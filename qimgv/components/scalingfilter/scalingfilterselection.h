#pragma once

#include "settings_types.h"

// The viewer's scaling-filter shortcuts (toggleScalingFilter,
// cycleScalingFilter), as decisions independent of any UI.
namespace ScalingFilterSelection {

// toggleScalingFilter: switches between the configured filter and nearest
// neighbour; from any filter other than the configured one it returns to the
// configured one.
[[nodiscard]] ScalingFilter toggled(ScalingFilter current, ScalingFilter configured);

// cycleScalingFilter: the next filter in ScalingFilter order, wrapping after
// the last one.
[[nodiscard]] ScalingFilter next(ScalingFilter current);

} // namespace ScalingFilterSelection
