/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace platform {

struct LoRaHatStatus {
    bool present{false};
    bool working{false};
    std::string error;
    std::string device;
};

// Probes for the CardputerZero LoRa hat (SX1262 on the SPI bus). On desktop
// builds this returns a clearly-simulated "working" result so the UI can be
// exercised; on the device it opens a spidev node and reads the SX1262 status.
LoRaHatStatus probe_lora_hat();

} // namespace platform
