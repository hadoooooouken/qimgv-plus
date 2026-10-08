#pragma once

// Shared by the snapshot-fed bridges: stores next into current and emits the
// bridge's notify signal only when the value actually changed.
template <typename Value, typename Bridge>
void publishIfChanged(Value &current, const Value &next, Bridge &bridge,
                      void (Bridge::*notify)()) {
  if (current == next)
    return;
  current = next;
  (bridge.*notify)();
}
