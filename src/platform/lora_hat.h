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

// Probes for the CardputerZero LoRa hat (SX1262 on the SPI bus).
//
// The hat is powered from the EXT 5V rail, which is off by default: the
// SX1262 stays silent (MISO reads 0x00) until that rail is enabled. The probe
// therefore powers the rail, waits for it to settle, and then reads the SX1262
// status byte. On desktop builds this returns a clearly-simulated result.
LoRaHatStatus probe_lora_hat();

// Powers the EXT 5V rail for a radio session and remembers the previous value
// so release_lora_hat() can restore it. Idempotent; returns false (with a
// human-readable reason) when the rail exists but cannot be driven.
bool enable_lora_power(std::string& error);

// SPI node that answered the last successful probe (empty before a probe).
// The radio driver reuses it instead of poking every spidev node again.
std::string detected_lora_spi_device();

// Restores the EXT 5V rail to the value it had before probe_lora_hat().
// Call on shutdown so the port is not left powered.
void release_lora_hat();

} // namespace platform
