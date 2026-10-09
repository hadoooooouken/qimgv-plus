#include "colorlutbuilder.h"

#include <QCoreApplication>
#include <QDebug>
#include <algorithm>
#include <utility>

using namespace Qt::StringLiterals;

ColorLutBuilder::ColorLutBuilder(QObject *parent, QThreadPool *pool)
    : QObject(parent),
      mPool(pool ? pool : QThreadPool::globalInstance()),
      mLifetime(std::make_shared<Lifetime>()) {}

// Builds still running finish on their worker; their completions find the
// lifetime token expired and are dropped.
ColorLutBuilder::~ColorLutBuilder() = default;

ColorLutBuilder::Key ColorLutBuilder::keyOf(const QColorSpace &source,
                                            const QColorSpace &target) {
  return Key{source.iccProfile(), target.iccProfile()};
}

std::shared_ptr<const ColorLut>
ColorLutBuilder::find(const QColorSpace &source, const QColorSpace &target) {
  const Key key = keyOf(source, target);
  const auto entry = std::ranges::find(mCache, key, &Entry::key);
  if (entry == mCache.end())
    return nullptr;
  // Most recently used last.
  std::rotate(entry, entry + 1, mCache.end());
  return mCache.back().lut;
}

void ColorLutBuilder::request(const QColorSpace &source,
                              const QColorSpace &target) {
  Key key = keyOf(source, target);
  if (std::ranges::contains(mCache, key, &Entry::key) ||
      std::ranges::contains(mActive, key, &ActiveTask::key) ||
      std::ranges::contains(mFailed, key))
    return;

  const quint64 taskId = ++mNextTaskId;
  mActive.push_back(ActiveTask{std::move(key), taskId});
  const std::weak_ptr<Lifetime> lifetime = mLifetime;
  mPool->start([this, lifetime, taskId, source, target]() {
    std::expected<ColorLut, QString> result = buildColorLut(source, target);
    QCoreApplication *application = QCoreApplication::instance();
    if (!application) {
      qCritical() << "ColorLutBuilder: no application to deliver a colour "
                     "lookup table to";
      return;
    }
    // The application object outlives the builder; the builder itself is
    // only touched on the GUI thread once the token proves it alive.
    QMetaObject::invokeMethod(
        application,
        [this, lifetime, taskId, result = std::move(result)]() mutable {
          if (lifetime.lock())
            finish(taskId, std::move(result));
        },
        Qt::QueuedConnection);
  });
}

void ColorLutBuilder::finish(quint64 taskId,
                             std::expected<ColorLut, QString> result) {
  const auto task = std::ranges::find(mActive, taskId, &ActiveTask::id);
  if (task == mActive.end()) {
    // Every started task stays active until this completion; an unknown id
    // is a logic error, and its result cannot be attributed to a pair.
    qCritical() << "ColorLutBuilder: completion of unknown task" << taskId;
    return;
  }
  Key key = std::move(task->key);
  mActive.erase(task);

  if (!result) {
    mFailed.push_back(std::move(key));
    emit lutFailed(u"Cannot build a colour lookup table: %1"_s.arg(
        result.error()));
    return;
  }
  if (mCache.size() >= kCacheCapacity)
    mCache.erase(mCache.begin());
  mCache.push_back(Entry{std::move(key), std::make_shared<const ColorLut>(
                                             std::move(*result))});
  emit lutReady();
}

bool ColorLutBuilder::isBuilding() const { return !mActive.empty(); }
