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
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <string>
#include <vector>
#endif

#ifndef APP_LORA_SPI_DEVICE
#define APP_LORA_SPI_DEVICE ""
#endif

namespace platform {
namespace {

#if !USE_DESKTOP
constexpr std::uint8_t kSx126xGetStatus = 0xC0;

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
    close(fd);

    if (!ok) {
        error = std::string("SPI transaction failed on ") + device + ": " + std::strerror(errno);
        return false;
    }

    // SX126x status: bits [6:4] carry the chip mode. A present radio answers
    // with a mode in 2..6 (standby/FS/RX/TX); a floating MISO reads 0x00/0xFF.
    const std::uint8_t status = rx[1];
    const std::uint8_t chip_mode = (status >> 4U) & 0x07U;
    if (chip_mode < 2 || chip_mode > 6) {
        char detail[64];
        std::snprintf(detail, sizeof(detail), "SX1262 silent on %s (status 0x%02X)",
                      device.c_str(), status);
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
    const auto devices = spi_devices();
    if (devices.empty()) {
        status.present = false;
        status.working = false;
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

} // namespace platform
