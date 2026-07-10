#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QSet>
#include <expected>

struct KSamplerInfo
{
    QString    modelName;      // ckpt_name / unet_name
    QString    clipName;       // clip_name / clip_name1
    QString    vaeName;        // vae_name
    QStringList loraNames;     // chain of LoraLoader nodes (may be empty)

    qint64  seed        = 0;
    int     steps        = 0;
    double  cfg           = 0.0;
    QString samplerName;
    QString scheduler;
    double  denoise      = 1.0;

    QString positivePrompt; // text resolved from the "positive" conditioning chain
    QString negativePrompt; // text resolved from the "negative" conditioning chain

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
    static QJsonObject findMainKSamplerNode(const QJsonObject &prompt, QString &outId);
    static QString resolveModelChain(const QJsonObject &prompt, const QJsonValue &modelInput,
                                      QStringList &loraNames);
    static QString findLoaderValue(const QJsonObject &prompt,
                                    const QStringList &classSubstrings,
                                    const QStringList &valueKeys);
    // Walks backward through a conditioning input (positive/negative) to find
    // the originating CLIPTextEncode node(s) and return their text. Follows
    // simple pass-through nodes (ControlNetApply, ConditioningSetArea, etc.)
    // and merges both branches of ConditioningCombine.
    static QString resolveConditioningText(const QJsonObject &prompt,
                                            const QJsonValue &conditioningInput,
                                            QSet<QString> visited = {});
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
    // Resolves a text-producing input that may be a literal string or a link
    // into a node that builds the string dynamically (concatenation,
    // find/replace, etc.). The exact input key names used by such nodes vary
    // per node pack and aren't known ahead of time, so instead of guessing
    // specific keys this collects every string value reachable from the
    // node - literal values held directly, plus whatever further link
    // inputs resolve to. This can pull in unrelated fragments (e.g. a
    // find/replace node's search/replacement arguments), but it beats
    // showing nothing for a workflow that builds its prompt dynamically.
    static QString resolveTextLink(const QJsonObject &prompt, const QJsonValue &v,
                                    QSet<QString> &visited);
    static bool isLink(const QJsonValue &v);
};
