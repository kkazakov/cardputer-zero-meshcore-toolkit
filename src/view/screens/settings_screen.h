/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"

#include "lvgl.h"

#include <array>

namespace screen {

// Scrollable list of every primary MeshCore radio setting. Left/Right edit the
// focused value; the node-name row opens an inline character editor with OK.
class SettingsScreen : public BaseScreen {
public:
    SettingsScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;

    struct Row {
        lv_obj_t* container{nullptr};
        lv_obj_t* name{nullptr};
        lv_obj_t* value{nullptr};
    };

    void refresh();
    static void refresh_cb(lv_observer_t* observer, lv_subject_t* subject);

    lv_obj_t* list_{nullptr};
    std::array<Row, model::kSettingsFieldCount> rows_{};
};

} // namespace screen
