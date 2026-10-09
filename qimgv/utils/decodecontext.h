#pragma once

#include <stop_token>

#include "utils/displaypipeline.h"

struct DecodeContext {
    std::stop_token cancellationToken;
    // What the decoded image is prepared for (ImageStatic).
    DisplayPipeline displayPipeline = DisplayPipeline::Cpu;

    [[nodiscard]] bool isCancellationRequested() const noexcept
    {
        return cancellationToken.stop_requested();
    }
};
