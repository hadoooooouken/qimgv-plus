#include "comfyksamplerparser.h"
#include "pngtextchunkreader.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QSet>
#include <functional>

bool ComfyKSamplerParser::isLink(const QJsonValue &v)
{
    // A link to another node in the ComfyUI JSON looks like ["12", 0]
    return v.isArray() && v.toArray().size() == 2 && v.toArray().at(1).isDouble();
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
        if (srcInputs.contains("ckpt_name"))
            return srcInputs.value("ckpt_name").toString();
        if (srcInputs.contains("unet_name"))
            return srcInputs.value("unet_name").toString();

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

QString ComfyKSamplerParser::resolveConditioningText(const QJsonObject &prompt,
                                                       const QJsonValue &conditioningInput,
                                                       QSet<QString> visited)
{
    if (!isLink(conditioningInput))
        return {};

    QJsonArray link = conditioningInput.toArray();
    QString srcId = link.at(0).toVariant().toString();

    if (!prompt.contains(srcId) || visited.contains(srcId))
        return {};
    visited.insert(srcId);

    QJsonObject srcNode = prompt.value(srcId).toObject();
    QString classType = srcNode.value("class_type").toString();
    QJsonObject srcInputs = srcNode.value("inputs").toObject();

    // The actual prompt text lives on the CLIPTextEncode node(s). Covers the
    // plain CLIPTextEncode as well as SDXL's text_g/text_l variant.
    if (classType.contains("CLIPTextEncode", Qt::CaseInsensitive)) {
        QStringList parts;
        for (const char *key : { "text", "text_g", "text_l" }) {
            QSet<QString> textVisited;
            QString text = resolveTextLink(prompt, srcInputs.value(key), textVisited);
            if (!text.isEmpty())
                parts << text;
        }
        return parts.join(QStringLiteral(" "));
    }

    // ConditioningCombine merges two branches — resolve and join both.
    if (classType.contains("ConditioningCombine", Qt::CaseInsensitive)) {
        QString a = resolveConditioningText(prompt, srcInputs.value("conditioning_1"), visited);
        QString b = resolveConditioningText(prompt, srcInputs.value("conditioning_2"), visited);
        if (!a.isEmpty() && !b.isEmpty())
            return a + QStringLiteral(" ") + b;
        return a.isEmpty() ? b : a;
    }

    // Other pass-through nodes (ControlNetApply, ConditioningSetArea,
    // ConditioningZeroOut, Switch/Mux nodes, etc.) carry the chain forward
    // through a single conditioning-shaped input.
    QJsonValue nextLink = pickPassThroughLink(srcInputs, QStringLiteral("conditioning"));
    if (isLink(nextLink))
        return resolveConditioningText(prompt, nextLink, visited);

    return {};
}

QString ComfyKSamplerParser::resolveTextLink(const QJsonObject &prompt,
                                              const QJsonValue &v,
                                              QSet<QString> &visited)
{
    if (v.isString())
        return v.toString();

    if (!isLink(v))
        return {};

    QJsonArray link = v.toArray();
    QString srcId = link.at(0).toVariant().toString();
    if (!prompt.contains(srcId) || visited.contains(srcId))
        return {};
    visited.insert(srcId);

    QJsonObject srcInputs = prompt.value(srcId).toObject().value("inputs").toObject();

    QStringList parts;
    for (auto it = srcInputs.constBegin(); it != srcInputs.constEnd(); ++it) {
        if (it.value().isString() && !it.value().toString().isEmpty()) {
            parts << it.value().toString();
        } else if (isLink(it.value())) {
            QString sub = resolveTextLink(prompt, it.value(), visited);
            if (!sub.isEmpty())
                parts << sub;
        }
    }
    return parts.join(QStringLiteral(" "));
}

QJsonObject ComfyKSamplerParser::findMainKSamplerNode(const QJsonObject &prompt, QString &outId)
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
    // i.e. the last one in the chain. Ancestors are searched recursively
    // across ALL of a node's input links (not just latent_image), because
    // intermediate nodes (LatentUpscale, VAEEncode/VAEDecode, etc.) often sit
    // between two KSamplers, so a direct latent_image->latent_image link may
    // not exist. A KSampler can never appear on a "foreign" path (via
    // model/positive/negative) since it doesn't produce MODEL/CONDITIONING
    // outputs — so traversing all links is safe and produces no false positives.
    std::function<void(const QJsonObject &, QSet<QString> &, QSet<QString> &)> collectAncestors =
        [&](const QJsonObject &nodeInputs, QSet<QString> &visited, QSet<QString> &found) {
            for (auto it = nodeInputs.constBegin(); it != nodeInputs.constEnd(); ++it) {
                if (!isLink(it.value()))
                    continue;
                QString srcId = it.value().toArray().at(0).toVariant().toString();
                if (visited.contains(srcId))
                    continue;
                visited.insert(srcId);

                if (candidateIds.contains(srcId)) {
                    found.insert(srcId);
                    continue; // don't recurse past a KSampler ancestor, not needed here
                }
                if (!prompt.contains(srcId))
                    continue;
                QJsonObject srcNode = prompt.value(srcId).toObject();
                collectAncestors(srcNode.value("inputs").toObject(), visited, found);
            }
        };

    QSet<QString> usedAsInputBySomeoneElse;
    for (const QString &id : candidateIds) {
        QJsonObject inputs = prompt.value(id).toObject().value("inputs").toObject();
        QSet<QString> visited;
        QSet<QString> ancestors;
        collectAncestors(inputs, visited, ancestors);
        usedAsInputBySomeoneElse.unite(ancestors);
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
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(promptJson, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return std::unexpected(
            QStringLiteral("JSON parse error: %1 (offset %2)")
                .arg(err.errorString())
                .arg(err.offset));

    QJsonObject prompt = doc.object();

    QString mainId;
    QJsonObject ks = findMainKSamplerNode(prompt, mainId);
    if (ks.isEmpty())
        return std::unexpected(QStringLiteral("No KSampler/KSamplerAdvanced node found in the graph"));

    QJsonObject inputs = ks.value("inputs").toObject();

    KSamplerInfo info;
    info.sourceNodeId = mainId;

    // seed / noise_seed — both variants occur (KSampler / KSamplerAdvanced)
    QJsonValue seedVal = inputs.contains("seed") ? inputs.value("seed")
                                                  : inputs.value("noise_seed");
    info.seed = static_cast<qint64>(seedVal.toDouble());

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

    // Positive / negative prompt text — walk up the conditioning links
    info.positivePrompt = resolveConditioningText(prompt, inputs.value("positive"));
    info.negativePrompt = resolveConditioningText(prompt, inputs.value("negative"));

    return info;
}
