/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_widget.h"
#include "base_model.h"
#include "base_viewmodel.h"
#include "icon_button.h"

#include <array>
#include <memory>

namespace app {
class AssetManager;
}

namespace view::widgets {

// Bottom hint bar: context-sensitive key hints on the left and a theme toggle
// on the right. Key routing is handled centrally by the application input
// controller; the theme icon is the one real nav button (key `6`).
class NavBar : public BaseWidgets {
public:
    NavBar(lv_obj_t* parent, viewmodel::BaseViewModel& view_model, app::AssetManager& assets);
    ~NavBar() override;

    void build() override;

private:
    void update_hints();
    void update_theme_icon();

    static void page_changed_cb(lv_observer_t* observer, lv_subject_t* subject);
    static void theme_changed_cb(lv_observer_t* observer, lv_subject_t* subject);
    static void toggle_theme_cb(lv_event_t* event);

    viewmodel::BaseViewModel& view_model_;
    app::AssetManager& assets_;
    std::array<lv_obj_t*, 3> hint_keys_{};
    std::array<lv_obj_t*, 3> hint_actions_{};
    std::unique_ptr<IconButton> theme_button_;
    lv_observer_t* page_observer_{nullptr};
    lv_observer_t* theme_observer_{nullptr};
};

} // namespace view::widgets
