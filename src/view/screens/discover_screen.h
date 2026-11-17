/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"

#include "lvgl.h"

namespace screen {

// Active MeshCore node discovery. The list is a single recolored LVGL label
// (cheap to rebuild and touch-friendly) inside a scroll container; Up/Down
// move the scroll offset by one row, OK toggles the sweep, ESC leaves.
class DiscoverScreen : public BaseScreen {
public:
    DiscoverScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);
    ~DiscoverScreen() override;

private:
    void build_content(lv_obj_t* content) override;
    void update_scan_label();

    static void scan_clicked(lv_event_t* event);
    static void scanning_changed(lv_observer_t* observer, lv_subject_t* subject);
    static void poll_timer_cb(lv_timer_t* timer);

    lv_obj_t* status_{nullptr};
    lv_obj_t* list_container_{nullptr};
    lv_obj_t* list_{nullptr};
    lv_obj_t* scan_button_{nullptr};
    lv_obj_t* scan_label_{nullptr};
    const lv_font_t* list_font_{nullptr};
    lv_timer_t* poll_timer_{nullptr};
};

} // namespace screen
