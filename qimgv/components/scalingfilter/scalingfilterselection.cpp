#include "scalingfilterselection.h"

namespace {
constexpr ScalingFilter kFirstFilter = QI_FILTER_NEAREST;
constexpr ScalingFilter kLastFilter = QI_FILTER_MKS2021_GPU;
} // namespace

ScalingFilter ScalingFilterSelection::toggled(ScalingFilter current, ScalingFilter configured) {
    return current == configured ? QI_FILTER_NEAREST : configured;
}

ScalingFilter ScalingFilterSelection::next(ScalingFilter current) {
    if (current >= kLastFilter || current < kFirstFilter)
        return kFirstFilter;
    return static_cast<ScalingFilter>(static_cast<int>(current) + 1);
}
