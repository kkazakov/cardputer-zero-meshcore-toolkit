/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "lora_hat.h"

#include <cstdint>
#include <cstdio>

#if !USE_DESKTOP
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>
#endif

#ifndef APP_LORA_SPI_DEVICE
#define APP_LORA_SPI_DEVICE ""
#endif

// EXT 5V rail exposed as an LED-class brightness file on the CardputerZero.
#ifndef APP_LORA_POWER_PATH
#define APP_LORA_POWER_PATH "/sys/class/leds/ext_5v_out/brightness"
#endif

namespace platform {
namespace {

#if !USE_DESKTOP
constexpr std::uint8_t kSx126xGetStatus = 0xC0;
// The hat needs the rail up before the radio answers; the vendor app uses the
// same 150 ms settle time.
constexpr useconds_t kPowerSettleUs = 150000;

// Previous rail value so shutdown can put it back. -1 = never touched.
int g_saved_power = -1;

bool read_sysfs_value(const char* path, int& value) {
    const int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return false;
    }
    char buffer[16] = {};
    const auto count = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (count <= 0) {
        return false;
    }
    value = std::atoi(buffer);
    return true;
}

bool write_sysfs_value(const char* path, int value, std::string& error) {
    const int fd = open(path, O_WRONLY);
    if (fd < 0) {
        error = std::string(path) + ": " + std::strerror(errno);
        return false;
    }
    char buffer[16] = {};
    const int length = std::snprintf(buffer, sizeof(buffer), "%d\n", value);
    const auto written = write(fd, buffer, static_cast<size_t>(length));
    close(fd);
    if (written != length) {
        error = std::string(path) + ": write failed";
        return false;
    }
    return true;
}

// Powers the EXT 5V rail. Returns false with a human-readable reason when the
// rail exists but cannot be driven (e.g. wrong group permissions).
bool enable_hat_power(std::string& error) {
    const char* path = APP_LORA_POWER_PATH;
    if (access(path, F_OK) != 0) {
        // No such rail on this board; let the SPI probe decide.
        return true;
    }

    int current = 0;
    if (read_sysfs_value(path, current) && g_saved_power < 0) {
        g_saved_power = current;
    }
    if (current == 0 && !write_sysfs_value(path, 1, error)) {
        error = std::string("cannot enable EXT 5V (") + error + ")";
        return false;
    }

    usleep(kPowerSettleUs);
    return true;
}

std::vector<std::string> spi_devices() {
    std::vector<std::string> devices;

    const char* configured = APP_LORA_SPI_DEVICE;
    if (configured[0] != '\0') {
        devices.push_back(configured);
        return devices;
    }

    auto* dir = opendir("/dev");
    if (!dir) {
        return devices;
    }
    while (auto* entry = readdir(dir)) {
        if (std::strncmp(entry->d_name, "spidev", 6) == 0) {
            devices.push_back(std::string("/dev/") + entry->d_name);
        }
    }
    closedir(dir);
    return devices;
}

bool sx126x_responds(const std::string& device, std::string& error) {
    const int fd = open(device.c_str(), O_RDWR);
    if (fd < 0) {
        error = std::string("cannot open ") + device + ": " + std::strerror(errno);
        return false;
    }

    std::uint8_t mode = SPI_MODE_0;
    std::uint8_t bits = 8;
    std::uint32_t speed = 1000000;
    ioctl(fd, SPI_IOC_WR_MODE, &mode);
    ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);

    std::uint8_t tx[2] = {kSx126xGetStatus, 0x00};
    std::uint8_t rx[2] = {0x00, 0x00};
    spi_ioc_transfer transfer{};
    transfer.tx_buf = reinterpret_cast<unsigned long>(tx);
    transfer.rx_buf = reinterpret_cast<unsigned long>(rx);
    transfer.len = sizeof(tx);
    transfer.speed_hz = speed;
    transfer.bits_per_word = bits;

    const bool ok = ioctl(fd, SPI_IOC_MESSAGE(1), &transfer) >= 0;
    const int saved_errno = errno;
    close(fd);

    if (!ok) {
        error = std::string("SPI transaction failed on ") + device + ": " + std::strerror(saved_errno);
        return false;
    }

    // SX126x status: bits [6:4] carry the chip mode. A powered radio answers
    // with 2..6 (STDBY_RC/STDBY_XOSC/FS/RX/TX); an unpowered one reads 0x00.
    const std::uint8_t status = rx[1];
    const std::uint8_t chip_mode = (status >> 4U) & 0x07U;
    if (chip_mode < 2 || chip_mode > 6) {
        char detail[128];
        std::snprintf(detail, sizeof(detail),
                      "SX1262 silent on %s (status 0x%02X)", device.c_str(), status);
        error = detail;
        return false;
    }
    return true;
}
#endif

} // namespace

LoRaHatStatus probe_lora_hat() {
    LoRaHatStatus status;

#if USE_DESKTOP
    status.present = true;
    status.working = true;
    status.device = "simulator";
    return status;
#else
    std::string power_error;
    if (!enable_hat_power(power_error)) {
        status.present = true;
        status.error = power_error;
        return status;
    }

    const auto devices = spi_devices();
    if (devices.empty()) {
        status.present = false;
        status.error = "No SPI device found";
        return status;
    }

    for (const auto& device : devices) {
        std::string error;
        if (sx126x_responds(device, error)) {
            status.present = true;
            status.working = true;
            status.device = device;
            return status;
        }
        // Keep the most recent error for reporting.
        status.error = error;
        status.device = device;
    }

    status.present = true; // an spidev node exists, but the radio did not answer
    status.working = false;
    return status;
#endif
}

void release_lora_hat() {
#if !USE_DESKTOP
    if (g_saved_power < 0) {
        return;
    }
    std::string ignored;
    write_sysfs_value(APP_LORA_POWER_PATH, g_saved_power, ignored);
    g_saved_power = -1;
#endif
}

} // namespace platform
