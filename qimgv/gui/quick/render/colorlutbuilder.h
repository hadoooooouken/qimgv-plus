#pragma once

#include <QByteArray>
#include <QColorSpace>
#include <QObject>
#include <QString>
#include <QThreadPool>
#include <memory>
#include <vector>

#include "gui/quick/render/colortransformplan.h"

// Builds the 3D colour lookup tables of ColorTransformKind::Lut on a worker
// thread and caches them by (source, target) ICC profile identity.
//
// GUI thread only. find() never blocks; request() starts at most one build
// per pair. A pair stays an active task, identified by its task id, until
// its completion arrives on the GUI thread; the completion checks that id
// before it touches the cache. A completion that arrives after the builder
// was destroyed is dropped. Every build ends with lutReady() or lutFailed().
class ColorLutBuilder : public QObject {
  Q_OBJECT

public:
  // Pairs remembered; the viewer alternates between few colour spaces.
  static constexpr std::size_t kCacheCapacity = 4;

  // pool runs the builds; the global pool when null.
  explicit ColorLutBuilder(QObject *parent = nullptr,
                           QThreadPool *pool = nullptr);
  ~ColorLutBuilder() override;

  // The cached table for the pair, or null.
  [[nodiscard]] std::shared_ptr<const ColorLut>
  find(const QColorSpace &source, const QColorSpace &target);
  // Starts building the pair's table unless it is cached, being built or
  // failed before (reported once through lutFailed()).
  void request(const QColorSpace &source, const QColorSpace &target);
  [[nodiscard]] bool isBuilding() const;

signals:
  // A requested table is now cached; find() returns it.
  void lutReady();
  void lutFailed(const QString &message);

private:
  struct Key {
    QByteArray source;
    QByteArray target;

    friend bool operator==(const Key &, const Key &) = default;
  };
  struct Entry {
    Key key;
    std::shared_ptr<const ColorLut> lut;
  };
  struct ActiveTask {
    Key key;
    quint64 id = 0;
  };
  // Lives as long as the builder; completions posted by workers check it on
  // the GUI thread before touching the builder.
  struct Lifetime {};

  [[nodiscard]] static Key keyOf(const QColorSpace &source,
                                 const QColorSpace &target);
  void finish(quint64 taskId, std::expected<ColorLut, QString> result);

  QThreadPool *mPool = nullptr;
  std::vector<Entry> mCache;
  std::vector<ActiveTask> mActive;
  std::vector<Key> mFailed;
  quint64 mNextTaskId = 0;
  std::shared_ptr<Lifetime> mLifetime;
};
