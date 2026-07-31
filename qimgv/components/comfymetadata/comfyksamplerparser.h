#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QSet>
#include <expected>

struct KSamplerInfo
{
    // Order matches the desired display order: Checkpoint, CLIP, VAE,
    // Sampler, Scheduler, Seed, CFG, Denoise, Steps, LoRA.
    QString    modelName;      // ckpt_name / unet_name (Checkpoint)
    QString    clipName;       // clip_name / clip_name1 (CLIP)
    QString    vaeName;        // vae_name (VAE)
    QString samplerName;       // Sampler
    QString scheduler;         // Scheduler
    qint64  seed        = 0;   // Seed
    double  cfg           = 0.0; // CFG
    double  denoise      = 1.0;  // Denoise
    int     steps        = 0;    // Steps
    QStringList loraNames;     // LoRA — chain of LoraLoader nodes (may be empty)
    QString positivePrompt;   // Positive prompt text
    // Ids (with class_type, e.g. "499 (Florence2Run)") of nodes that were
    // reachable while resolving positivePrompt but contributed no text —
    // most commonly a node whose real value is a runtime-computed output
    // that isn't recorded anywhere in the static "prompt" JSON. Surface
    // this separately from positivePrompt in the UI (e.g. as a warning
    // badge); do not append it into positivePrompt itself, since that
    // string is likely to be copied back into ComfyUI verbatim.
    QStringList unresolvedPromptNodeIds;
    QString sourceNodeId; // id of the KSampler node the parameters were taken from
};

class ComfyKSamplerParser
{
public:
    // Full path: reads the PNG, extracts the "prompt" chunk, parses it.
    // On error returns a human-readable description of the cause (for display in the UI).
    static std::expected<KSamplerInfo, QString> parseFromPng(const QString &pngPath);

    // If the JSON is already available as a QByteArray (ComfyUI's "prompt" chunk).
    static std::expected<KSamplerInfo, QString> parseFromJson(const QByteArray &promptJson);

private:
    // Walks backward through a conditioning input (positive) to find
    // the originating CLIPTextEncode node(s) and return their text. Follows
    // simple pass-through nodes (ControlNetApply, ConditioningSetArea, etc.)
    // and merges both branches of ConditioningCombine. Any node ids passed
    // back via outUnresolvedNodeIds by the underlying resolveTextLink() call
    // are forwarded unchanged.
    static std::expected<QString, QString>
        resolveConditioningText(const QJsonObject &prompt,
                                const QJsonValue &conditioningInput,
                                QStringList &outUnresolvedNodeIds);
    // Resolves a text-producing input that may be a literal string or a link
    // into a node that builds the string dynamically (concatenation,
    // find/replace, etc.). Collects every string-typed, non-empty input
    // whose key name looks content-shaped (contains "text", "string",
    // "value", "prompt", or "caption", and isn't one of the small set of
    // known instruction-argument names such as "text_input" or "query") on
    // every node reachable that way. Classification is by input key name
    // only — never by class_type — since the ComfyUI custom-node ecosystem
    // has an unenumerable long tail of node names, while widget-argument
    // naming conventions are comparatively small and shared.
    //
    // A node reachable this way that yields no text at all (no content-shaped
    // input present, or the only one found was empty) is a case where the
    // real value is very likely a runtime-computed output that the static
    // "prompt" JSON never recorded in the first place (e.g. an image
    // captioner). Rather than fabricate something from that node's
    // unrelated configuration inputs, such node ids are appended to
    // outUnresolvedNodeIds instead of being folded into the returned text.
    static std::expected<QString, QString>
        resolveTextLink(const QJsonObject &prompt, const QJsonValue &v,
                        QStringList &outUnresolvedNodeIds);
    static std::expected<QJsonObject, QString>
        findMainKSamplerNode(const QJsonObject &prompt, QString &outId);
    static QString resolveModelChain(const QJsonObject &prompt, const QJsonValue &modelInput,
                                      QStringList &loraNames);
    static QString findLoaderValue(const QJsonObject &prompt,
                                    const QStringList &classSubstrings,
                                    const QStringList &valueKeys);
    // Resolves a numeric input (used for seed) that may be a literal number
    // or a link into a node that produces it dynamically: a literal-holding
    // node (PrimitiveInt/PrimitiveNode, "Seed (rgthree)", etc. — exposed
    // under a "seed"/"noise_seed"/"value" key) or a simple two-operand
    // arithmetic node (e.g. rgthree/mikey "Simple Math" evaluating an
    // expression like "a+b" against its own inputs). Only single-operator
    // two-operand expressions are evaluated; anything more complex falls
    // back through the generic literal/pass-through handling.
    static double resolveSeedValue(const QJsonObject &prompt, const QJsonValue &v,
                                    QSet<QString> &visited);
    // Best-effort link to follow through an unrecognized node (Reroute,
    // Switch/Mux nodes, etc.) when no known key (e.g. "model"/"conditioning")
    // is present. Prefers preferredKey if it's a link; otherwise, if a
    // literal boolean selector is present alongside on_true/on_false inputs,
    // follows the selected branch; otherwise picks the first link-valued
    // input whose key doesn't look like a selector/control value.
    // This can't resolve switches whose selector is itself fed by another
    // node - that would require running the graph, not just reading it.
    static QJsonValue pickPassThroughLink(const QJsonObject &srcInputs,
                                           const QString &preferredKey);
    static std::expected<void, QString> validatePromptGraph(const QJsonObject &prompt);
    static bool isLink(const QJsonValue &v);
};
