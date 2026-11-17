/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace platform {

// The Cap LoRa-1262 puts its antenna switch (FM8625H SX_ANT_SW) on P0 of a
// PI4IOE5V6408 I2C expander at 0x43 on /dev/i2c-1. P0 must be driven high
// before any RF path is usable. Cap LoRa868 has no expander at all, so a
// missing device is a warning, not a fatal error.
//
// enable_rf_switch() snapshots the whole P0-related register state and
// restore_rf_switch() puts those bits back on shutdown.
bool enable_rf_switch(std::string& detail);
void restore_rf_switch();
bool rf_switch_available();
const std::string& rf_switch_detail();

} // namespace platform
