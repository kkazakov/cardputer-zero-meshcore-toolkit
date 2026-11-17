/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "pi4io.h"

#if !USE_DESKTOP

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifndef APP_LORA_I2C_BUS
#define APP_LORA_I2C_BUS "/dev/i2c-1"
#endif
#ifndef APP_LORA_PI4IO_ADDRESS
#define APP_LORA_PI4IO_ADDRESS 0x43
#endif

namespace platform {
namespace {

constexpr const char* kBusPath = APP_LORA_I2C_BUS;
constexpr int kAddress = APP_LORA_PI4IO_ADDRESS;

constexpr uint8_t kRegDeviceId = 0x01;
constexpr uint8_t kRegPolarity = 0x02;
constexpr uint8_t kRegConfig = 0x03;
constexpr uint8_t kRegOutput = 0x05; // output state (0x01 is the device ID/control register)
constexpr uint8_t kRegHighZ = 0x07;

struct SwitchState {
    bool active{false};
    int fd{-1};
    uint8_t output{0};
    uint8_t polarity{0};
    uint8_t config{0};
    uint8_t high_z{0};
    std::string detail{"not checked"};
};

SwitchState g_state;

bool write_register(int fd, uint8_t reg, uint8_t value) {
    const uint8_t payload[2] = {reg, value};
    return ::write(fd, payload, sizeof(payload)) == static_cast<ssize_t>(sizeof(payload));
}

bool read_register(int fd, uint8_t reg, uint8_t& value) {
    if (::write(fd, &reg, 1) != 1) {
        return false;
    }
    return ::read(fd, &value, 1) == 1;
}

void close_bus() {
    if (g_state.fd >= 0) {
        ::close(g_state.fd);
        g_state.fd = -1;
    }
}

} // namespace

bool enable_rf_switch(std::string& detail) {
    if (g_state.active) {
        detail = g_state.detail;
        return true;
    }

    const int fd = ::open(kBusPath, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        g_state.detail = std::string("cannot open ") + kBusPath + ": " + std::strerror(errno);
        detail = g_state.detail;
        return false;
    }
    if (::ioctl(fd, I2C_SLAVE, kAddress) < 0) {
        g_state.detail = std::string("cannot select the RF switch on ") + kBusPath + ": " + std::strerror(errno);
        detail = g_state.detail;
        ::close(fd);
        return false;
    }

    SwitchState snapshot;
    snapshot.fd = fd;
    uint8_t device_id = 0;
    (void)device_id;
    if (!read_register(fd, kRegDeviceId, device_id) ||
        !read_register(fd, kRegOutput, snapshot.output) ||
        !read_register(fd, kRegPolarity, snapshot.polarity) ||
        !read_register(fd, kRegConfig, snapshot.config) ||
        !read_register(fd, kRegHighZ, snapshot.high_z)) {
        g_state.detail = "I2C RF switch did not answer (Cap LoRa868, or expander unavailable)";
        detail = g_state.detail;
        ::close(fd);
        return false;
    }

    const uint8_t output = static_cast<uint8_t>(snapshot.output | 0x01U);
    const uint8_t config = static_cast<uint8_t>(snapshot.config | 0x01U);
    const uint8_t high_z = static_cast<uint8_t>(snapshot.high_z & ~0x01U);
    // Polarity is already active-high on this board; only write 0x02 when it
    // actually needs changing. The reference controller's 0x02 write is
    // ignored by some expander revisions, so it must not gate the switch.
    const bool pol_ok = (snapshot.polarity & 0x01U) == 0 ||
                        write_register(fd, kRegPolarity,
                                       static_cast<uint8_t>(snapshot.polarity & ~0x01U));
    const bool hz_ok = write_register(fd, kRegHighZ, high_z);
    const bool out_ok = write_register(fd, kRegOutput, output);
    const bool cfg_ok = write_register(fd, kRegConfig, config);
    const bool written = hz_ok && pol_ok && out_ok && cfg_ok;

    uint8_t verify_output = 0;
    uint8_t verify_config = 0;
    const bool read_out = read_register(fd, kRegOutput, verify_output);
    const bool read_cfg = read_register(fd, kRegConfig, verify_config);
    const bool verified = written && read_out && read_cfg &&
                          (verify_output & 0x01U) != 0 && (verify_config & 0x01U) != 0;
    if (!verified) {
        g_state.detail = "I2C RF switch could not be enabled";
        detail = g_state.detail;
        ::close(fd);
        return false;
    }

    snapshot.active = true;
    snapshot.detail = "PI4IO 0x43 P0 high (RF switch on)";
    g_state = snapshot;
    detail = g_state.detail;
    return true;
}

void restore_rf_switch() {
    if (!g_state.active || g_state.fd < 0) {
        return;
    }

    // Only P0 belongs to us; leave the other pins as another client left them.
    uint8_t current = 0;
    auto restore_bit = [&](uint8_t reg, uint8_t saved) {
        if (!read_register(g_state.fd, reg, current)) {
            return;
        }
        const uint8_t value = static_cast<uint8_t>((current & ~0x01U) | (saved & 0x01U));
        (void)write_register(g_state.fd, reg, value);
    };
    restore_bit(kRegOutput, g_state.output);
    restore_bit(kRegPolarity, g_state.polarity);
    restore_bit(kRegConfig, g_state.config);
    restore_bit(kRegHighZ, g_state.high_z);

    close_bus();
    g_state.active = false;
    g_state.detail = "PI4IO state restored";
}

bool rf_switch_available() {
    return g_state.active;
}

const std::string& rf_switch_detail() {
    return g_state.detail;
}

} // namespace platform

#else

namespace platform {
namespace {
const std::string kDesktopDetail = "desktop simulator, no RF switch";
}

bool enable_rf_switch(std::string& detail) {
    detail = kDesktopDetail;
    return false;
}

void restore_rf_switch() {}
bool rf_switch_available() { return false; }
const std::string& rf_switch_detail() { return kDesktopDetail; }

} // namespace platform

#endif
