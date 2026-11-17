/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace platform {

// RAII wrapper around one Linux GPIO character-device line (uAPI v2).
// The radio's RESET/BUSY/DIO1 lines live directly on the CM0 GPIO header on
// the CardputerZero; there is no libgpiod runtime dependency.
class GpioLine {
public:
    enum class Direction {
        Input,
        Output,
    };

    GpioLine();
    ~GpioLine();

    GpioLine(const GpioLine&) = delete;
    GpioLine& operator=(const GpioLine&) = delete;
    GpioLine(GpioLine&& other) noexcept;
    GpioLine& operator=(GpioLine&& other) noexcept;

    bool open(const char* chip_path,
              unsigned int offset,
              Direction direction,
              bool initial_high,
              std::string& error);
    void close();

    bool is_open() const;
    unsigned int offset() const;
    bool set(bool high);
    bool get(bool& high) const;

private:
    int chip_fd_{-1};
    int line_fd_{-1};
    unsigned int offset_{0};
};

} // namespace platform
