#pragma once

#include <stop_token>

struct DecodeContext {
    std::stop_token cancellationToken;

    [[nodiscard]] bool isCancellationRequested() const noexcept
    {
        return cancellationToken.stop_requested();
    }
};
