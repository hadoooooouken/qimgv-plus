#include "resizedialogmodel.h"

#include <QtGlobal>
#include <algorithm>
#include <array>

namespace {
using namespace Qt::StringLiterals;

constexpr double kMinimumPercent = 1.0;
constexpr double kMaximumPercent = 1600.0;
constexpr double kInitialPercent = 100.0;
constexpr double kPercentScale = 100.0;
constexpr int kMinimumSide = 1;
constexpr int kMaximumSide = 65535;

// Filters in the order of ResizeDialogModel::Filter.
constexpr std::array kFilters{
    QI_FILTER_NEAREST,
    QI_FILTER_BILINEAR,
    QI_FILTER_SMART,
    QI_FILTER_MKS2021,
};
constexpr auto kDefaultFilter = ResizeDialogModel::Filter::MagicKernelSharp2021;

struct CommonSize {
    int width;
    int height;
    QLatin1StringView label;
};

constexpr std::array kCommonSizes{
    CommonSize{1280, 720, "1280 x 720"_L1},
    CommonSize{1366, 768, "1366 x 768"_L1},
    CommonSize{1440, 900, "1440 x 900"_L1},
    CommonSize{1440, 1050, "1440 x 1050"_L1},
    CommonSize{1600, 1200, "1600 x 1200"_L1},
    CommonSize{1920, 1080, "1920 x 1080 (FullHD)"_L1},
    CommonSize{1920, 1200, "1920 x 1200 (FullHD)"_L1},
    CommonSize{2560, 1080, "2560 x 1080"_L1},
    CommonSize{2560, 1440, "2560 x 1440"_L1},
    CommonSize{2560, 1600, "2560 x 1600"_L1},
    CommonSize{3840, 1600, "3840 x 1600 (UW 4K)"_L1},
    CommonSize{3840, 2160, "3840 x 2160 (UHD-1)"_L1},
};

constexpr int kNoCommonSize = -1;

int clampSide(int side) {
    return std::clamp(side, kMinimumSide, kMaximumSide);
}
} // namespace

ResizeDialogModel::ResizeDialogModel(QObject *parent) : DialogSession(parent) {}

int ResizeDialogModel::originalWidth() const {
    return mInput.originalSize.width();
}

int ResizeDialogModel::originalHeight() const {
    return mInput.originalSize.height();
}

bool ResizeDialogModel::byPercent() const {
    return mByPercent;
}

double ResizeDialogModel::percent() const {
    return mPercent;
}

double ResizeDialogModel::minimumPercent() const {
    return kMinimumPercent;
}

double ResizeDialogModel::maximumPercent() const {
    return kMaximumPercent;
}

int ResizeDialogModel::targetWidth() const {
    return mTarget.width();
}

int ResizeDialogModel::targetHeight() const {
    return mTarget.height();
}

QSize ResizeDialogModel::targetSize() const {
    return mTarget;
}

int ResizeDialogModel::minimumSide() const {
    return kMinimumSide;
}

int ResizeDialogModel::maximumSide() const {
    return kMaximumSide;
}

bool ResizeDialogModel::keepAspectRatio() const {
    return mKeepAspectRatio;
}

int ResizeDialogModel::filterIndex() const {
    return mFilterIndex;
}

QStringList ResizeDialogModel::commonSizes() const {
    QStringList labels;
    labels.reserve(static_cast<qsizetype>(kCommonSizes.size()));
    for (const CommonSize &size : kCommonSizes)
        labels.append(size.label);
    return labels;
}

int ResizeDialogModel::commonSizeIndex() const {
    return mCommonSizeIndex;
}

bool ResizeDialogModel::upscaylAvailable() const {
    return !mInput.upscaylModels.isEmpty();
}

bool ResizeDialogModel::upscaylApplies() const {
    return mTarget.width() > mInput.originalSize.width() ||
           mTarget.height() > mInput.originalSize.height();
}

bool ResizeDialogModel::useUpscayl() const {
    return mUseUpscayl;
}

QStringList ResizeDialogModel::upscaylModels() const {
    return mInput.upscaylModels;
}

int ResizeDialogModel::upscaylModelIndex() const {
    return mUpscaylModelIndex;
}

std::optional<ResizeRequest> ResizeDialogModel::result() const {
    return mResult;
}

std::optional<ResizePreferences> ResizeDialogModel::preferencesToStore() const {
    return mPreferences;
}

bool ResizeDialogModel::start(const ResizeDialogInput &input) {
    if (!canStart("resize"))
        return false;
    mResult.reset();
    mPreferences.reset();
    mInput = input;
    mByPercent = true;
    mPercent = kInitialPercent;
    mTarget = input.originalSize;
    mKeepAspectRatio = true;
    mLastEdited = Side::Width;
    mFilterIndex = static_cast<int>(kDefaultFilter);
    mCommonSizeIndex = kNoCommonSize;
    mUseUpscayl = upscaylAvailable() && input.useUpscayl;
    const qsizetype storedModel = input.upscaylModels.indexOf(input.upscaylModel);
    mUpscaylModelIndex = !upscaylAvailable() ? -1 : static_cast<int>(qMax(storedModel, 0));
    emit stateChanged();
    open();
    return true;
}

void ResizeDialogModel::setByPercent(bool byPercent) {
    mByPercent = byPercent;
    if (mByPercent) {
        mKeepAspectRatio = true;
        applyPercent();
    }
    emit stateChanged();
}

void ResizeDialogModel::setPercent(double percent) {
    mByPercent = true;
    mKeepAspectRatio = true;
    mPercent = std::clamp(percent, kMinimumPercent, kMaximumPercent);
    applyPercent();
    emit stateChanged();
}

void ResizeDialogModel::setTargetWidth(int width) {
    mByPercent = false;
    applySide(Side::Width, width);
    emit stateChanged();
}

void ResizeDialogModel::setTargetHeight(int height) {
    mByPercent = false;
    applySide(Side::Height, height);
    emit stateChanged();
}

void ResizeDialogModel::setKeepAspectRatio(bool keep) {
    // Percent mode always keeps the aspect ratio.
    if (mByPercent)
        keep = true;
    mKeepAspectRatio = keep;
    mCommonSizeIndex = kNoCommonSize;
    applySide(mLastEdited, mLastEdited == Side::Width ? mTarget.width() : mTarget.height());
    emit stateChanged();
}

void ResizeDialogModel::selectCommonSize(int index) {
    if (index < 0 || index >= static_cast<int>(kCommonSizes.size())) {
        mCommonSizeIndex = kNoCommonSize;
        scaleOriginalTo(mInput.originalSize, mKeepAspectRatio ? Qt::KeepAspectRatio
                                                              : Qt::IgnoreAspectRatio);
    } else {
        mByPercent = false;
        mCommonSizeIndex = index;
        const CommonSize &size = kCommonSizes[static_cast<std::size_t>(index)];
        scaleOriginalTo(QSize(size.width, size.height),
                        mKeepAspectRatio ? Qt::KeepAspectRatio : Qt::IgnoreAspectRatio);
    }
    emit stateChanged();
}

void ResizeDialogModel::fitDesktop() {
    mByPercent = false;
    scaleOriginalTo(mInput.desktopSize, Qt::KeepAspectRatio);
    emit stateChanged();
}

void ResizeDialogModel::fillDesktop() {
    mByPercent = false;
    scaleOriginalTo(mInput.desktopSize, Qt::KeepAspectRatioByExpanding);
    emit stateChanged();
}

void ResizeDialogModel::reset() {
    mCommonSizeIndex = kNoCommonSize;
    setTarget(mInput.originalSize);
    emit stateChanged();
}

void ResizeDialogModel::setFilterIndex(int index) {
    if (index < 0 || index >= static_cast<int>(kFilters.size()))
        index = static_cast<int>(kDefaultFilter);
    mFilterIndex = index;
    emit stateChanged();
}

void ResizeDialogModel::setUseUpscayl(bool use) {
    mUseUpscayl = upscaylAvailable() && use;
    emit stateChanged();
}

void ResizeDialogModel::setUpscaylModelIndex(int index) {
    if (upscaylAvailable() && index >= 0 && index < mInput.upscaylModels.size())
        mUpscaylModelIndex = index;
    emit stateChanged();
}

void ResizeDialogModel::accept() {
    if (!isOpen())
        return;
    if (mTarget != mInput.originalSize) {
        const bool useUpscayl = effectiveUseUpscayl();
        const QString model = selectedUpscaylModel();
        mResult = ResizeRequest{
            .size = mTarget,
            .filter = kFilters[static_cast<std::size_t>(mFilterIndex)],
            .useUpscayl = useUpscayl,
            .upscaylModel = model,
        };
        mPreferences = ResizePreferences{.useUpscayl = useUpscayl, .upscaylModel = model};
    }
    conclude();
}

void ResizeDialogModel::reject() {
    if (!isOpen())
        return;
    mResult.reset();
    mPreferences.reset();
    conclude();
}

void ResizeDialogModel::applyPercent() {
    const double scale = mPercent / kPercentScale;
    // Truncated like the widget dialog's integer conversion.
    setTarget(QSize(static_cast<int>(mInput.originalSize.width() * scale),
                    static_cast<int>(mInput.originalSize.height() * scale)));
}

void ResizeDialogModel::applySide(Side side, int value) {
    mLastEdited = side;
    QSize target = mTarget;
    // Single precision, like the widget dialog.
    const QSize original = mInput.originalSize;
    if (side == Side::Width) {
        target.setWidth(value);
        if (mKeepAspectRatio && original.width() > 0) {
            const float factor = static_cast<float>(value) / original.width();
            target.setHeight(static_cast<int>(original.height() * factor));
        }
    } else {
        target.setHeight(value);
        if (mKeepAspectRatio && original.height() > 0) {
            const float factor = static_cast<float>(value) / original.height();
            target.setWidth(static_cast<int>(original.width() * factor));
        }
    }
    setTarget(target);
}

void ResizeDialogModel::scaleOriginalTo(QSize bounds, Qt::AspectRatioMode mode) {
    setTarget(mInput.originalSize.scaled(bounds, mode));
}

void ResizeDialogModel::setTarget(QSize size) {
    mTarget = QSize(clampSide(size.width()), clampSide(size.height()));
}

bool ResizeDialogModel::effectiveUseUpscayl() const {
    // A stored "Use Upscayl" must not send a downscale through the much
    // slower AI path.
    return upscaylAvailable() && mUseUpscayl && upscaylApplies();
}

QString ResizeDialogModel::selectedUpscaylModel() const {
    if (mUpscaylModelIndex < 0 || mUpscaylModelIndex >= mInput.upscaylModels.size())
        return {};
    return mInput.upscaylModels.at(mUpscaylModelIndex);
}
