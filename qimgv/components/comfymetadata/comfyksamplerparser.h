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
    // and merges both branches of ConditioningCombine.
    static QString resolveConditioningText(const QJsonObject &prompt,
                                            const QJsonValue &conditioningInput,
                                            QSet<QString> visited = {});
    // Resolves a text-producing input that may be a literal string or a link
    // into a node that builds the string dynamically (concatenation,
    // find/replace, etc.). Collects every string value reachable from the
    // node.
    static QString resolveTextLink(const QJsonObject &prompt, const QJsonValue &v,
                                    QSet<QString> &visited);
    static QJsonObject findMainKSamplerNode(const QJsonObject &prompt, QString &outId);
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
    static bool isLink(const QJsonValue &v);
};
