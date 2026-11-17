/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"

#include "lvgl.h"

namespace screen {

// Full-screen error shown when the LoRa hat cannot be detected at startup.
class LoraErrorScreen : public BaseScreen {
public:
    LoraErrorScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
    static void error_changed(lv_observer_t* observer, lv_subject_t* subject);

    lv_obj_t* error_label_{nullptr};
};

} // namespace screen
