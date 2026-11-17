/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"

#include "lvgl.h"

namespace screen {

// Main sliding-menu (carousel) screen: three horizontally arranged cards with a
// center accent card, side cards, and a dot indicator. Mirrors the
// cardputer-zero-mouse-emu home layout.
class HomeScreen : public BaseScreen {
public:
    HomeScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;

    struct Card {
        lv_obj_t* container{nullptr};
        lv_obj_t* title{nullptr};
        lv_obj_t* subtitle{nullptr};
        lv_obj_t* accent_bar{nullptr};
    };

    void render();
    void style_card(const Card& card, lv_color_t accent, bool center);

    static void selection_changed(lv_observer_t* observer, lv_subject_t* subject);
    static void theme_changed(lv_observer_t* observer, lv_subject_t* subject);

    Card left_;
    Card center_;
    Card right_;
    lv_obj_t* dots_[3]{};
};

} // namespace screen
