/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */



#pragma once

#include <cstdint>

namespace view {

// some font constants
constexpr const char* ICON_SIGN_OUT           = "\uE42A";
constexpr const char* ICON_TEXT_BOLD          = "\uE5BE";
constexpr const char* ICON_MOON               = "\uE330";
constexpr const char* ICON_SUN                = "\uE474";
constexpr const char* ICON_SQUARE_ARROW_LEFT  = "\uE074";
constexpr const char* ICON_SQUARE_ARROW_RIGHT = "\uE076";
constexpr const char* ICON_MINUS              = "\uE32A";
constexpr const char* ICON_PLUS               = "\uE3D4";
constexpr const char* ICON_INFO               = "\uE2CE";
constexpr const char* ICON_WIFI_NONE          = "\uE4F0";
constexpr const char* ICON_WIFI_LOW           = "\uE4EC";
constexpr const char* ICON_WIFI_MEDIUM        = "\uE4EE";
constexpr const char* ICON_WIFI_HIGH          = "\uE4EA";
constexpr const char* ICON_ETHERNET           = "\uEDDE";
constexpr const char* ICON_BAT_FULL           = "\uE7C4";
constexpr const char* ICON_BAT_HIGH           = "\uE7C2";
constexpr const char* ICON_BAT_MEDIUM         = "\uE7C0";
constexpr const char* ICON_BAT_LOW            = "\uE7BE";
constexpr const char* ICON_BAT_EMPTY          = "\uE7C6";
constexpr const char* ICON_BAT_CHARGING       = "\uE0BC";

namespace color {

// Naive UI common color tokens, flattened to solid RGB values for LVGL.
namespace light {
constexpr uint32_t kPrimary      = 0x18a058;
constexpr uint32_t kInfo         = 0x2080f0;
constexpr uint32_t kSuccess      = 0x18a058;
constexpr uint32_t kWarning      = 0xf0a020;
constexpr uint32_t kError        = 0xd03050;
constexpr uint32_t kBody         = 0xffffff;
constexpr uint32_t kCard         = 0xffffff;
constexpr uint32_t kAction       = 0xfafafc;
constexpr uint32_t kButton       = 0xf5f5f5;
constexpr uint32_t kBorder       = 0xe0e0e6;
constexpr uint32_t kTextPrimary  = 0x1f2225;
constexpr uint32_t kTextDisabled = 0xc2c2c2;
} // namespace light

namespace dark {
// Palette shared with CardputerZero Mouse Jiggler / Ultimate-Remote's Linux
// renderer: deep navy canvas, panel cards, mint accent.
constexpr uint32_t kPrimary      = 0x88f0b3; // mint accent
constexpr uint32_t kInfo         = 0x80baff; // blue
constexpr uint32_t kSuccess      = 0x88f0b3;
constexpr uint32_t kWarning      = 0xffd18b; // amber
constexpr uint32_t kError        = 0xf29ec0; // pink
constexpr uint32_t kBody         = 0x0b111a; // canvas
constexpr uint32_t kCard         = 0x1b2b37; // raised card
constexpr uint32_t kAction       = 0x141f2d; // panel
constexpr uint32_t kButton       = 0x1b2b37; // focused row / button
constexpr uint32_t kBorder       = 0x27374a;
constexpr uint32_t kTextPrimary  = 0xedf3fa;
constexpr uint32_t kTextDisabled = 0x95a6bb;
} // namespace dark

} // namespace color

} // namespace view
