/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "gpio_line.h"

#if !USE_DESKTOP

#include <cerrno>
#include <cstring>
#include <linux/gpio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace platform {
namespace {

std::string ioctl_error(const char* what) {
    return std::string(what) + ": " + std::strerror(errno);
}

} // namespace

GpioLine::GpioLine() = default;

GpioLine::~GpioLine() {
    close();
}

GpioLine::GpioLine(GpioLine&& other) noexcept
    : chip_fd_(other.chip_fd_), line_fd_(other.line_fd_), offset_(other.offset_) {
    other.chip_fd_ = -1;
    other.line_fd_ = -1;
    other.offset_ = 0;
}

GpioLine& GpioLine::operator=(GpioLine&& other) noexcept {
    if (this != &other) {
        close();
        chip_fd_ = other.chip_fd_;
        line_fd_ = other.line_fd_;
        offset_ = other.offset_;
        other.chip_fd_ = -1;
        other.line_fd_ = -1;
        other.offset_ = 0;
    }
    return *this;
}

bool GpioLine::open(const char* chip_path,
                    unsigned int offset,
                    Direction direction,
                    bool initial_high,
                    std::string& error) {
    close();

    const char* path = chip_path && chip_path[0] ? chip_path : "/dev/gpiochip0";
    chip_fd_ = ::open(path, O_RDWR | O_CLOEXEC);
    if (chip_fd_ < 0) {
        error = std::string("cannot open ") + path + ": " + std::strerror(errno);
        return false;
    }

    gpio_v2_line_request request{};
    request.offsets[0] = offset;
    request.num_lines = 1;
    std::strncpy(request.consumer, "meshcore-toolkit", sizeof(request.consumer) - 1);
    request.config.flags = direction == Direction::Output ? GPIO_V2_LINE_FLAG_OUTPUT
                                                          : GPIO_V2_LINE_FLAG_INPUT;
    if (direction == Direction::Output) {
        request.config.num_attrs = 1;
        request.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
        request.config.attrs[0].attr.values = initial_high ? 1U : 0U;
        request.config.attrs[0].mask = 1U;
    }

    if (::ioctl(chip_fd_, GPIO_V2_GET_LINE_IOCTL, &request) < 0) {
        error = ioctl_error("GPIO_V2_GET_LINE_IOCTL");
        close();
        return false;
    }

    line_fd_ = request.fd;
    offset_ = offset;
    return true;
}

void GpioLine::close() {
    if (line_fd_ >= 0) {
        ::close(line_fd_);
        line_fd_ = -1;
    }
    if (chip_fd_ >= 0) {
        ::close(chip_fd_);
        chip_fd_ = -1;
    }
    offset_ = 0;
}

bool GpioLine::is_open() const {
    return line_fd_ >= 0;
}

unsigned int GpioLine::offset() const {
    return offset_;
}

bool GpioLine::set(bool high) {
    if (line_fd_ < 0) {
        return false;
    }
    gpio_v2_line_values values{};
    values.bits = high ? 1U : 0U;
    values.mask = 1U;
    return ::ioctl(line_fd_, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) == 0;
}

bool GpioLine::get(bool& high) const {
    if (line_fd_ < 0) {
        return false;
    }
    gpio_v2_line_values values{};
    values.mask = 1U;
    if (::ioctl(line_fd_, GPIO_V2_LINE_GET_VALUES_IOCTL, &values) < 0) {
        return false;
    }
    high = (values.bits & 1U) != 0;
    return true;
}

} // namespace platform

#else

namespace platform {

GpioLine::GpioLine() = default;
GpioLine::~GpioLine() = default;

GpioLine::GpioLine(GpioLine&& other) noexcept
    : chip_fd_(other.chip_fd_), line_fd_(other.line_fd_), offset_(other.offset_) {
    other.chip_fd_ = -1;
    other.line_fd_ = -1;
    other.offset_ = 0;
}

GpioLine& GpioLine::operator=(GpioLine&& other) noexcept {
    if (this != &other) {
        chip_fd_ = other.chip_fd_;
        line_fd_ = other.line_fd_;
        offset_ = other.offset_;
        other.chip_fd_ = -1;
        other.line_fd_ = -1;
        other.offset_ = 0;
    }
    return *this;
}

bool GpioLine::open(const char*, unsigned int, Direction, bool, std::string& error) {
    error = "GPIO lines are only available in the device build";
    return false;
}

void GpioLine::close() {
    chip_fd_ = -1;
    line_fd_ = -1;
    offset_ = 0;
}

bool GpioLine::is_open() const { return false; }
unsigned int GpioLine::offset() const { return offset_; }
bool GpioLine::set(bool) { return false; }
bool GpioLine::get(bool&) const { return false; }

} // namespace platform

#endif
