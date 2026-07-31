#include "comfyksamplerparser.h"
#include "comfymetadatalimits.h"
#include "pngtextchunkreader.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QHash>
#include <QQueue>
#include <QSet>
#include <QVector>

namespace {
constexpr qsizetype kTextSeparatorCharacters = 1;

std::expected<void, QString> validateJsonNesting(const QByteArray &json)
{
    qsizetype nestingDepth = 0;
    bool insideString = false;
    bool escaped = false;

    for (const char byte : json) {
        if (insideString) {
            if (escaped) {
                escaped = false;
            } else if (byte == '\\') {
                escaped = true;
            } else if (byte == '"') {
                insideString = false;
            }
            continue;
        }

        if (byte == '"') {
            insideString = true;
        } else if (byte == '{' || byte == '[') {
            ++nestingDepth;
            if (nestingDepth > ComfyMetadataLimits::kMaxJsonNestingDepth) {
                return std::unexpected(
                    QStringLiteral("ComfyUI JSON exceeds the maximum nesting depth of %1")
                        .arg(ComfyMetadataLimits::kMaxJsonNestingDepth));
            }
        } else if ((byte == '}' || byte == ']') && nestingDepth > 0) {
            --nestingDepth;
        }
    }

    return {};
}

std::expected<void, QString> validateJsonValueCount(const QJsonObject &root)
{
    QVector<QJsonValue> pending;
    pending.append(QJsonValue(root));
    qsizetype valueCount = 0;

    while (!pending.isEmpty()) {
        const QJsonValue value = pending.takeLast();
        ++valueCount;
        if (valueCount > ComfyMetadataLimits::kMaxJsonValueCount) {
            return std::unexpected(
                QStringLiteral("ComfyUI JSON exceeds the maximum value count of %1")
                    .arg(ComfyMetadataLimits::kMaxJsonValueCount));
        }

        if (value.isObject()) {
            const QJsonObject object = value.toObject();
            for (auto it = object.constBegin(); it != object.constEnd(); ++it)
                pending.append(it.value());
        } else if (value.isArray()) {
            const QJsonArray array = value.toArray();
            for (const QJsonValue &entry : array)
                pending.append(entry);
        }
    }

    return {};
}

class BoundedTextCollector
{
public:
    std::expected<void, QString> append(const QString &text)
    {
        if (text.isEmpty())
            return {};

        const qsizetype separatorCharacters = mParts.isEmpty() ? 0 : kTextSeparatorCharacters;
        const qsizetype remainingCharacters =
            ComfyMetadataLimits::kMaxResolvedTextCharacters - mCharacterCount;
        if (separatorCharacters > remainingCharacters ||
            text.size() > remainingCharacters - separatorCharacters) {
            return std::unexpected(
                QStringLiteral("Resolved ComfyUI prompt text exceeds the %1-character limit")
                    .arg(ComfyMetadataLimits::kMaxResolvedTextCharacters));
        }

        mCharacterCount += separatorCharacters + text.size();
        mParts.append(text);
        return {};
    }

    QString joined() const
    {
        return mParts.join(QStringLiteral(" "));
    }

private:
    QStringList mParts;
    qsizetype mCharacterCount = 0;
};

// Input keys that structurally hold an instruction *given to* a text- or
// vision-generating node (a question, a referring expression, a system
// prompt) rather than text the node produced. Matched by exact name, not
// substring: unlike class_type, the argument name used for "an instruction
// fed to the model" is drawn from a small, conventional vocabulary shared
// across the ecosystem, so this list doesn't carry the same unenumerable
// long tail that class_type allow/deny lists do. Concrete example:
// Florence2Run's "text_input" holds the referring expression / DocVQA
// question passed in by the user, not the caption it generates.
bool isInstructionKeyName(const QString &key)
{
    static const QStringList kInstructionKeyNames = {
        QStringLiteral("text_input"),
        QStringLiteral("query"),
        QStringLiteral("question"),
        QStringLiteral("instruction"),
        QStringLiteral("system_prompt"),
    };
    for (const QString &known : kInstructionKeyNames) {
        if (key.compare(known, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

// An input is treated as potential prompt content purely by its key name —
// no class_type is consulted anywhere in this classification.
bool isContentKeyName(const QString &key)
{
    if (isInstructionKeyName(key))
        return false;

    static const QStringList kContentKeySubstrings = {
        QStringLiteral("text"),   QStringLiteral("string"), QStringLiteral("value"),
        QStringLiteral("prompt"), QStringLiteral("caption"),
    };
    for (const QString &sub : kContentKeySubstrings) {
        if (key.contains(sub, Qt::CaseInsensitive))
            return true;
    }
    return false;
}
}

bool ComfyKSamplerParser::isLink(const QJsonValue &v)
{
    // A link to another node in the ComfyUI JSON looks like ["12", 0]
    return v.isArray() && v.toArray().size() == 2 && v.toArray().at(1).isDouble();
}

std::expected<void, QString>
ComfyKSamplerParser::validatePromptGraph(const QJsonObject &prompt)
{
    if (prompt.size() > ComfyMetadataLimits::kMaxGraphNodeCount) {
        return std::unexpected(
            QStringLiteral("ComfyUI graph exceeds the maximum node count of %1")
                .arg(ComfyMetadataLimits::kMaxGraphNodeCount));
    }

    QHash<QString, QVector<QString>> dependencies;
    QHash<QString, qsizetype> incomingEdgeCounts;
    QHash<QString, qsizetype> graphDepths;
    qsizetype edgeCount = 0;

    for (auto it = prompt.constBegin(); it != prompt.constEnd(); ++it) {
        dependencies.insert(it.key(), {});
        incomingEdgeCounts.insert(it.key(), 0);
        graphDepths.insert(it.key(), 1);
    }

    for (auto nodeIt = prompt.constBegin(); nodeIt != prompt.constEnd(); ++nodeIt) {
        const QJsonObject inputs = nodeIt.value().toObject().value("inputs").toObject();
        QVector<QString> &nodeDependencies = dependencies[nodeIt.key()];

        for (auto inputIt = inputs.constBegin(); inputIt != inputs.constEnd(); ++inputIt) {
            if (!isLink(inputIt.value()))
                continue;

            const QString sourceId =
                inputIt.value().toArray().at(0).toVariant().toString();
            if (!prompt.contains(sourceId))
                continue;

            ++edgeCount;
            if (edgeCount > ComfyMetadataLimits::kMaxGraphEdgeCount) {
                return std::unexpected(
                    QStringLiteral("ComfyUI graph exceeds the maximum edge count of %1")
                        .arg(ComfyMetadataLimits::kMaxGraphEdgeCount));
            }

            nodeDependencies.append(sourceId);
            incomingEdgeCounts[sourceId] = incomingEdgeCounts.value(sourceId) + 1;
        }
    }

    QQueue<QString> readyNodes;
    for (auto it = incomingEdgeCounts.constBegin(); it != incomingEdgeCounts.constEnd(); ++it) {
        if (it.value() == 0)
            readyNodes.enqueue(it.key());
    }

    qsizetype processedNodeCount = 0;
    while (!readyNodes.isEmpty()) {
        const QString nodeId = readyNodes.dequeue();
        ++processedNodeCount;

        const qsizetype nodeDepth = graphDepths.value(nodeId);
        if (nodeDepth > ComfyMetadataLimits::kMaxGraphDepth) {
            return std::unexpected(
                QStringLiteral("ComfyUI graph exceeds the maximum depth of %1")
                    .arg(ComfyMetadataLimits::kMaxGraphDepth));
        }

        const QVector<QString> &nodeDependencies = dependencies.value(nodeId);
        for (const QString &sourceId : nodeDependencies) {
            graphDepths[sourceId] =
                qMax(graphDepths.value(sourceId), nodeDepth + 1);

            const qsizetype remainingIncomingEdges =
                incomingEdgeCounts.value(sourceId) - 1;
            incomingEdgeCounts[sourceId] = remainingIncomingEdges;
            if (remainingIncomingEdges == 0)
                readyNodes.enqueue(sourceId);
        }
    }

    if (processedNodeCount != prompt.size()) {
        return std::unexpected(
            QStringLiteral("ComfyUI graph contains a dependency cycle"));
    }

    return {};
}

std::expected<KSamplerInfo, QString> ComfyKSamplerParser::parseFromPng(const QString &pngPath)
{
    auto chunksResult = PngTextChunkReader::readTextChunks(pngPath);
    if (!chunksResult)
        return std::unexpected(chunksResult.error());

    const auto &chunks = *chunksResult;

    // "prompt" is the graph as it was actually executed, with real widget
    // values. "workflow" is the UI graph and also works, but "prompt" is
    // more reliable for parameters.
    QByteArray json = chunks.value("prompt");
    if (json.isEmpty())
        json = chunks.value("workflow");

    if (json.isEmpty())
        return std::unexpected(
            QStringLiteral("No ComfyUI metadata in this PNG ('prompt'/'workflow' chunks not found)"));

    return parseFromJson(json);
}

QString ComfyKSamplerParser::findLoaderValue(const QJsonObject &prompt,
                                              const QStringList &classSubstrings,
                                              const QStringList &valueKeys)
{
    for (auto it = prompt.constBegin(); it != prompt.constEnd(); ++it) {
        QJsonObject node = it.value().toObject();
        QString classType = node.value("class_type").toString();

        for (const QString &sub : classSubstrings) {
            if (!classType.contains(sub, Qt::CaseInsensitive))
                continue;

            QJsonObject inputs = node.value("inputs").toObject();
            for (const QString &key : valueKeys) {
                QJsonValue v = inputs.value(key);
                if (v.isString() && !v.toString().isEmpty())
                    return v.toString();
            }
        }
    }
    return {};
}

QString ComfyKSamplerParser::resolveModelChain(const QJsonObject &prompt,
                                                const QJsonValue &modelInputIn,
                                                QStringList &loraNames)
{
    QJsonValue modelInput = modelInputIn;

    // Walk backward along the model-input links until we hit a
    // checkpoint/unet loader or run out of chain. Collect LoRAs along the way.
    // "visited" guards against hanging on a broken/generated graph with a
    // cycle in the links (e.g. A.model -> B, B.model -> A).
    QSet<QString> visited;
    while (isLink(modelInput)) {
        QJsonArray link = modelInput.toArray();
        QString srcId = link.at(0).toVariant().toString();

        if (!prompt.contains(srcId) || visited.contains(srcId))
            break;
        visited.insert(srcId);

        QJsonObject srcNode = prompt.value(srcId).toObject();
        QString classType = srcNode.value("class_type").toString();
        QJsonObject srcInputs = srcNode.value("inputs").toObject();

        if (classType.contains("Lora", Qt::CaseInsensitive)) {
            QString loraName = srcInputs.value("lora_name").toString();
            if (!loraName.isEmpty())
                loraNames.prepend(loraName);

            // for LoraLoader / LoraLoaderModelOnly the model input is named "model"
            modelInput = srcInputs.value("model");
            continue;
        }

        // Reached a loader node — extract the name and stop
        static const QStringList loaderKeys = {"ckpt_name", "unet_name", "model_name", "checkpoint_name"};
        for (const QString &key : loaderKeys) {
            if (srcInputs.contains(key)) {
                return srcInputs.value(key).toString();
            }
        }

        // Unknown node type (Reroute, Switch/Mux, custom loader, etc.) —
        // try to continue through whatever plausible pass-through link is
        // available, otherwise stop and fall back to the node's title.
        QJsonValue nextLink = pickPassThroughLink(srcInputs, QStringLiteral("model"));
        if (isLink(nextLink)) {
            modelInput = nextLink;
            continue;
        }

        return srcNode.value("_meta").toObject().value("title").toString();
    }

    return {};
}

QJsonValue ComfyKSamplerParser::pickPassThroughLink(const QJsonObject &srcInputs,
                                                      const QString &preferredKey)
{
    if (!preferredKey.isEmpty() && isLink(srcInputs.value(preferredKey)))
        return srcInputs.value(preferredKey);

    // Boolean-selected mux/switch nodes (e.g. a generic Switch node with
    // on_true/on_false inputs): if a literal boolean selector is present,
    // follow the branch it points to.
    for (auto it = srcInputs.constBegin(); it != srcInputs.constEnd(); ++it) {
        if (!it.value().isBool())
            continue;
        QJsonValue branch = srcInputs.value(it.value().toBool() ? QStringLiteral("on_true")
                                                                  : QStringLiteral("on_false"));
        if (isLink(branch))
            return branch;
        break;
    }

    // Generic fallback: the first link-valued input whose key doesn't look
    // like a selector/control value rather than actual payload.
    static const QStringList selectorKeyHints = {
        QStringLiteral("boolean"), QStringLiteral("select"), QStringLiteral("index"),
        QStringLiteral("mode"), QStringLiteral("value")
    };
    for (auto it = srcInputs.constBegin(); it != srcInputs.constEnd(); ++it) {
        if (!isLink(it.value()))
            continue;
        bool looksLikeSelector = false;
        for (const QString &hint : selectorKeyHints) {
            if (it.key().contains(hint, Qt::CaseInsensitive)) {
                looksLikeSelector = true;
                break;
            }
        }
        if (!looksLikeSelector)
            return it.value();
    }

    return {};
}

double ComfyKSamplerParser::resolveSeedValue(const QJsonObject &prompt,
                                              const QJsonValue &v,
                                              QSet<QString> &visited)
{
    if (v.isDouble())
        return v.toDouble();

    if (!isLink(v))
        return 0;

    QJsonArray link = v.toArray();
    QString srcId = link.at(0).toVariant().toString();
    if (!prompt.contains(srcId) || visited.contains(srcId))
        return 0;
    visited.insert(srcId);

    QJsonObject srcNode = prompt.value(srcId).toObject();
    QString classType = srcNode.value("class_type").toString();
    QJsonObject srcInputs = srcNode.value("inputs").toObject();

    // Simple two-operand arithmetic nodes (e.g. rgthree/mikey "Simple Math")
    // combine two of their own inputs according to a widget-provided
    // expression such as "a+b". Only a single operator between two named
    // operands is handled; anything more elaborate falls through below.
    if (classType.contains("SimpleMath", Qt::CaseInsensitive) && srcInputs.value("value").isString()) {
        QString expr = srcInputs.value("value").toString().trimmed();
        static const QString ops = QStringLiteral("+-*/");
        for (QChar op : ops) {
            int idx = expr.indexOf(op);
            if (idx <= 0) // skip missing match and a leading unary sign
                continue;
            QString lhsName = expr.left(idx).trimmed();
            QString rhsName = expr.mid(idx + 1).trimmed();
            if (!srcInputs.contains(lhsName) || !srcInputs.contains(rhsName))
                continue;

            double a = resolveSeedValue(prompt, srcInputs.value(lhsName), visited);
            double b = resolveSeedValue(prompt, srcInputs.value(rhsName), visited);
            switch (op.toLatin1()) {
            case '+': return a + b;
            case '-': return a - b;
            case '*': return a * b;
            case '/': return b != 0 ? a / b : a;
            }
        }
    }

    // Literal-valued nodes: "Seed (rgthree)", PrimitiveInt/PrimitiveNode,
    // INT Constant, etc. all expose their number under one of these keys.
    for (const char *key : { "seed", "noise_seed", "value" }) {
        QJsonValue val = srcInputs.value(key);
        if (val.isDouble())
            return val.toDouble();
        if (isLink(val))
            return resolveSeedValue(prompt, val, visited);
    }

    // Unknown node type — fall back to whatever single link-valued input
    // looks like a pass-through (Reroute, etc.)
    QJsonValue nextLink = pickPassThroughLink(srcInputs, QString());
    if (isLink(nextLink))
        return resolveSeedValue(prompt, nextLink, visited);

    return 0;
}

std::expected<QString, QString>
ComfyKSamplerParser::resolveConditioningText(const QJsonObject &prompt,
                                              const QJsonValue &conditioningInput,
                                              QStringList &outUnresolvedNodeIds)
{
    QVector<QJsonValue> pending;
    pending.append(conditioningInput);
    QSet<QString> visited;
    BoundedTextCollector textCollector;

    while (!pending.isEmpty()) {
        const QJsonValue currentInput = pending.takeLast();
        if (!isLink(currentInput))
            continue;

        const QString sourceId =
            currentInput.toArray().at(0).toVariant().toString();
        if (!prompt.contains(sourceId) || visited.contains(sourceId))
            continue;
        visited.insert(sourceId);

        const QJsonObject sourceNode = prompt.value(sourceId).toObject();
        const QString classType = sourceNode.value("class_type").toString();
        const QJsonObject sourceInputs = sourceNode.value("inputs").toObject();

        // The actual prompt text lives on the CLIPTextEncode node(s). Covers the
        // plain CLIPTextEncode as well as SDXL's text_g/text_l variant.
        if (classType.contains("CLIPTextEncode", Qt::CaseInsensitive)) {
            for (const char *key : { "text", "text_g", "text_l" }) {
                auto text = resolveTextLink(prompt, sourceInputs.value(key), outUnresolvedNodeIds);
                if (!text)
                    return std::unexpected(text.error());

                auto appendResult = textCollector.append(*text);
                if (!appendResult)
                    return std::unexpected(appendResult.error());
            }
            continue;
        }

        // ConditioningCombine merges two branches and is expanded through the
        // bounded worklist in the same order as the previous recursive walk.
        if (classType.contains("ConditioningCombine", Qt::CaseInsensitive)) {
            pending.append(sourceInputs.value("conditioning_2"));
            pending.append(sourceInputs.value("conditioning_1"));
            continue;
        }

        // Other pass-through nodes (ControlNetApply, ConditioningSetArea,
        // ConditioningZeroOut, Switch/Mux nodes, etc.) carry the chain forward
        // through a single conditioning-shaped input.
        const QJsonValue nextLink =
            pickPassThroughLink(sourceInputs, QStringLiteral("conditioning"));
        if (isLink(nextLink))
            pending.append(nextLink);
    }

    return textCollector.joined();
}

std::expected<QString, QString>
ComfyKSamplerParser::resolveTextLink(const QJsonObject &prompt,
                                     const QJsonValue &v,
                                     QStringList &outUnresolvedNodeIds)
{
    QSet<QString> visited;
    BoundedTextCollector textCollector;

    // Returns whether `value` — and everything reachable from it — yielded
    // at least one non-empty string. This lets a builder node that
    // legitimately has no text of its own (StringConcatenate, Reroute,
    // whose content lives entirely in their children) be told apart from a
    // genuine dead end: a node with no content-shaped input at all, or one
    // whose only content-shaped input turned out to be empty.
    auto visit = [&](this auto &&self, const QJsonValue &value) -> std::expected<bool, QString> {
        if (value.isString()) {
            const QString text = value.toString();
            if (text.isEmpty())
                return false;
            auto appendResult = textCollector.append(text);
            if (!appendResult)
                return std::unexpected(appendResult.error());
            return true;
        }

        if (!isLink(value))
            return false;

        const QString sourceId = value.toArray().at(0).toVariant().toString();
        if (!prompt.contains(sourceId) || visited.contains(sourceId))
            return false;
        visited.insert(sourceId);

        const QJsonObject sourceNode = prompt.value(sourceId).toObject();
        const QJsonObject sourceInputs = sourceNode.value("inputs").toObject();

        // Classification is purely by input key name — class_type is never
        // consulted. See isContentKeyName()/isInstructionKeyName() above.
        bool yieldedAny = false;
        for (auto it = sourceInputs.constBegin(); it != sourceInputs.constEnd(); ++it) {
            if (!isContentKeyName(it.key()))
                continue;
            if (!((it.value().isString() && !it.value().toString().isEmpty()) ||
                  isLink(it.value())))
                continue;

            auto childYielded = self(it.value());
            if (!childYielded)
                return std::unexpected(childYielded.error());
            yieldedAny = yieldedAny || *childYielded;
        }

        if (!yieldedAny) {
            const QString classType = sourceNode.value("class_type").toString();
            outUnresolvedNodeIds.append(
                classType.isEmpty() ? sourceId
                                     : QStringLiteral("%1 (%2)").arg(sourceId, classType));
        }

        return yieldedAny;
    };

    auto result = visit(v);
    if (!result)
        return std::unexpected(result.error());

    return textCollector.joined();
}

std::expected<QJsonObject, QString>
ComfyKSamplerParser::findMainKSamplerNode(const QJsonObject &prompt, QString &outId)
{
    // Collect all candidates for the role of KSampler.
    QStringList candidateIds;
    for (auto it = prompt.constBegin(); it != prompt.constEnd(); ++it) {
        QString classType = it.value().toObject().value("class_type").toString();
        if (classType.contains("KSampler", Qt::CaseInsensitive) ||
            classType.contains("SamplerCustom", Qt::CaseInsensitive)) {
            candidateIds << it.key();
        }
    }

    if (candidateIds.isEmpty())
        return {};

    if (candidateIds.size() == 1) {
        outId = candidateIds.first();
        return prompt.value(outId).toObject();
    }

    // Multiple KSamplers (e.g. a hires-fix chain): the "main" one is
    // considered to be whichever is NOT an ancestor of another candidate —
    // i.e. the last one in the chain. Ancestors are searched iteratively
    // across ALL of a node's input links (not just latent_image), because
    // intermediate nodes (LatentUpscale, VAEEncode/VAEDecode, etc.) often sit
    // between two KSamplers, so a direct latent_image->latent_image link may
    // not exist. A KSampler can never appear on a "foreign" path (via
    // model/positive/negative) since it doesn't produce MODEL/CONDITIONING
    // outputs — so traversing all links is safe and produces no false positives.
    QSet<QString> candidateIdSet;
    for (const QString &id : candidateIds)
        candidateIdSet.insert(id);
    QSet<QString> usedAsInputBySomeoneElse;
    qsizetype traversalSteps = 0;

    for (const QString &id : candidateIds) {
        const QJsonObject inputs =
            prompt.value(id).toObject().value("inputs").toObject();
        QVector<QString> pendingNodeIds;
        for (auto it = inputs.constBegin(); it != inputs.constEnd(); ++it) {
            if (isLink(it.value()))
                pendingNodeIds.append(it.value().toArray().at(0).toVariant().toString());
        }

        QSet<QString> visited;
        while (!pendingNodeIds.isEmpty()) {
            ++traversalSteps;
            if (traversalSteps > ComfyMetadataLimits::kMaxGraphTraversalSteps) {
                return std::unexpected(
                    QStringLiteral("ComfyUI graph traversal exceeds the %1-step limit")
                        .arg(ComfyMetadataLimits::kMaxGraphTraversalSteps));
            }

            const QString sourceId = pendingNodeIds.takeLast();
            if (visited.contains(sourceId))
                continue;
            visited.insert(sourceId);

            if (candidateIdSet.contains(sourceId)) {
                usedAsInputBySomeoneElse.insert(sourceId);
                continue;
            }
            if (!prompt.contains(sourceId))
                continue;

            const QJsonObject sourceInputs =
                prompt.value(sourceId).toObject().value("inputs").toObject();
            for (auto it = sourceInputs.constBegin(); it != sourceInputs.constEnd(); ++it) {
                if (isLink(it.value())) {
                    pendingNodeIds.append(
                        it.value().toArray().at(0).toVariant().toString());
                }
            }
        }
    }

    for (const QString &id : candidateIds) {
        if (!usedAsInputBySomeoneElse.contains(id)) {
            outId = id;
            return prompt.value(id).toObject();
        }
    }

    outId = candidateIds.last();
    return prompt.value(outId).toObject();
}

std::expected<KSamplerInfo, QString> ComfyKSamplerParser::parseFromJson(const QByteArray &promptJson)
{
    if (promptJson.size() > ComfyMetadataLimits::kMaxMetadataBytes) {
        return std::unexpected(
            QStringLiteral("ComfyUI JSON exceeds the %1-byte limit")
                .arg(ComfyMetadataLimits::kMaxMetadataBytes));
    }

    auto nestingValidation = validateJsonNesting(promptJson);
    if (!nestingValidation)
        return std::unexpected(nestingValidation.error());

    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(promptJson, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return std::unexpected(
            QStringLiteral("JSON parse error: %1 (offset %2)")
                .arg(err.errorString())
                .arg(err.offset));

    QJsonObject prompt = doc.object();
    auto valueCountValidation = validateJsonValueCount(prompt);
    if (!valueCountValidation)
        return std::unexpected(valueCountValidation.error());

    auto graphValidation = validatePromptGraph(prompt);
    if (!graphValidation)
        return std::unexpected(graphValidation.error());

    QString mainId;
    auto ks = findMainKSamplerNode(prompt, mainId);
    if (!ks)
        return std::unexpected(ks.error());
    if (ks->isEmpty())
        return std::unexpected(QStringLiteral("No KSampler/KSamplerAdvanced node found in the graph"));

    QJsonObject inputs = ks->value("inputs").toObject();

    KSamplerInfo info;
    info.sourceNodeId = mainId;

    // seed / noise_seed — both variants occur (KSampler / KSamplerAdvanced).
    // The value itself may not be a literal: workflows often feed it through
    // a separate seed/primitive node, or even a small arithmetic node (e.g.
    // rgthree's "Simple Math" adding two seed sources together), so it has
    // to be resolved rather than read directly off the input.
    QJsonValue seedVal = inputs.contains("seed") ? inputs.value("seed")
                                                  : inputs.value("noise_seed");
    QSet<QString> seedVisited;
    info.seed = static_cast<qint64>(resolveSeedValue(prompt, seedVal, seedVisited));

    info.steps       = inputs.value("steps").toInt();
    info.cfg         = inputs.value("cfg").toDouble();
    info.samplerName = inputs.value("sampler_name").toString();
    info.scheduler    = inputs.value("scheduler").toString();
    info.denoise     = inputs.contains("denoise") ? inputs.value("denoise").toDouble() : 1.0;

    // Model + LoRA — walk up the graph along the "model" link
    info.modelName = resolveModelChain(prompt, inputs.value("model"), info.loraNames);

    // CLIP and VAE are usually loaded by separate nodes, not through the
    // model chain — search the whole graph by node type.
    info.clipName = findLoaderValue(prompt,
                                     { "CLIPLoader", "DualCLIPLoader" },
                                     { "clip_name", "clip_name1" });

    info.vaeName = findLoaderValue(prompt,
                                    { "VAELoader" },
                                    { "vae_name" });
    // Positive prompt – walk up the conditioning links
    QStringList unresolvedPromptNodeIds;
    auto positivePrompt =
        resolveConditioningText(prompt, inputs.value("positive"), unresolvedPromptNodeIds);
    if (!positivePrompt)
        return std::unexpected(positivePrompt.error());
    info.positivePrompt = *positivePrompt;
    info.unresolvedPromptNodeIds = unresolvedPromptNodeIds;

    return info;
}
