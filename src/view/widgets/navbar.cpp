/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "navbar.h"

#include "asset_manager.h"
#include "bindings.h"
#include "linux_input.h"
#include "theme.h"
#include "ui_const.h"

#include <array>

namespace view::widgets {
namespace {

struct HintSpec {
    const char* key;
    const char* action;
};

std::array<HintSpec, 3> hints_for(model::AppPage page, bool editing) {
    switch (page) {
        case model::AppPage::Settings:
            if (editing) {
                return {{{"Z/C", "Change"}, {"OK", "Save"}, {"ESC", "Cancel"}}};
            }
            return {{{"F/X", "Field"}, {"OK", "Edit"}, {"ESC", "Back"}}};
        case model::AppPage::Test1:
        case model::AppPage::Test2:
            return {{{"", ""}, {"ESC", "Back"}, {"", ""}}};
        case model::AppPage::LoraError:
            return {{{"OK", "Retry"}, {"", ""}, {"ESC", "Continue"}}};
        case model::AppPage::Home:
        default:
            return {{{"Z/C", "Select"}, {"OK", "Open"}, {"ESC", "Exit"}}};
    }
}

} // namespace

NavBar::NavBar(lv_obj_t* parent, viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseWidgets(parent), view_model_(view_model), assets_(assets) {}

NavBar::~NavBar() {
    if (theme_button_) {
        platform::unregister_nav_button(2, theme_button_->root());
    }
    if (page_observer_) {
        lv_observer_remove(page_observer_);
        page_observer_ = nullptr;
    }
    if (theme_observer_) {
        lv_observer_remove(theme_observer_);
        theme_observer_ = nullptr;
    }
}

void NavBar::build() {
    if (core_obj_) {
        return;
    }

    core_obj_ = lv_obj_create(parent_);
    lv_obj_remove_style_all(core_obj_);
    lv_obj_set_size(core_obj_, LV_PCT(100), kNavBarHeight);
    lv_obj_align(core_obj_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(core_obj_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(core_obj_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(core_obj_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(core_obj_, 8, 0);
    reactive::bind_theme(core_obj_, view_model_.dark_mode_subject(), reactive::ThemeRole::Bar);

    auto* hints_bar = lv_obj_create(core_obj_);
    lv_obj_remove_style_all(hints_bar);
    lv_obj_set_size(hints_bar, 250, LV_PCT(100));
    lv_obj_set_flex_flow(hints_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hints_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(hints_bar, LV_OBJ_FLAG_SCROLLABLE);

    const lv_font_t* key_font = assets_.load_standard_font(14);
    if (!key_font) key_font = &lv_font_montserrat_14;
    const lv_font_t* action_font = assets_.load_standard_font(14, app::StandardFontWeight::Regular);
    if (!action_font) action_font = &lv_font_montserrat_14;

    for (int i = 0; i < 3; ++i) {
        auto* item = lv_obj_create(hints_bar);
        lv_obj_remove_style_all(item);
        lv_obj_set_size(item, LV_SIZE_CONTENT, LV_PCT(100));
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(item, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(item, 3, 0);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

        hint_keys_[i] = lv_label_create(item);
        lv_label_set_text(hint_keys_[i], "");
        lv_obj_set_style_text_font(hint_keys_[i], key_font, 0);

        hint_actions_[i] = lv_label_create(item);
        lv_label_set_text(hint_actions_[i], "");
        lv_obj_set_style_text_font(hint_actions_[i], action_font, 0);
        reactive::bind_theme(hint_actions_[i], view_model_.dark_mode_subject(), reactive::ThemeRole::Text);
    }

    const auto light = view::palette(false).text;
    const auto dark = view::palette(true).text;
    auto* icon_font = assets_.load_font("Phosphor-Fill.ttf", 22);
    theme_button_ = std::make_unique<IconButton>(core_obj_, view_model_, 32, 24, "",
                                                 icon_font ? icon_font : &lv_font_montserrat_14,
                                                 light, dark, toggle_theme_cb, &view_model_, true);
    theme_button_->build();
    platform::register_nav_button(2, theme_button_->root());

    page_observer_ = reactive::observe_obj(core_obj_, view_model_.current_page_subject(), page_changed_cb, this);
    theme_observer_ = reactive::observe_obj(core_obj_, view_model_.dark_mode_subject(), theme_changed_cb, this);
    reactive::observe_obj(core_obj_, view_model_.settings_edit_active_subject(), page_changed_cb, this);
    reactive::observe_obj(core_obj_, view_model_.node_edit_active_subject(), page_changed_cb, this);

    update_hints();
    update_theme_icon();
}

void NavBar::update_hints() {
    const bool editing =
        lv_subject_get_int(view_model_.settings_edit_active_subject()) != 0 ||
        lv_subject_get_int(view_model_.node_edit_active_subject()) != 0;
    const auto hints = hints_for(view_model_.current_page(), editing);
    const auto palette = view::palette(view_model_.is_dark_mode());
    for (int i = 0; i < 3; ++i) {
        lv_label_set_text(hint_keys_[i], hints[i].key);
        lv_label_set_text(hint_actions_[i], hints[i].action);
        lv_obj_set_style_text_color(hint_keys_[i], palette.primary, 0);
    }
}

void NavBar::update_theme_icon() {
    if (!theme_button_) {
        return;
    }
    theme_button_->set_text(view_model_.is_dark_mode() ? ICON_SUN : ICON_MOON);
}

void NavBar::page_changed_cb(lv_observer_t* observer, lv_subject_t* subject) {
    LV_UNUSED(subject);
    auto* nav = static_cast<NavBar*>(lv_observer_get_user_data(observer));
    if (nav) {
        nav->update_hints();
    }
}

void NavBar::theme_changed_cb(lv_observer_t* observer, lv_subject_t* subject) {
    LV_UNUSED(subject);
    auto* nav = static_cast<NavBar*>(lv_observer_get_user_data(observer));
    if (nav) {
        nav->update_hints();
        nav->update_theme_icon();
    }
}

void NavBar::toggle_theme_cb(lv_event_t* event) {
    auto* view_model = static_cast<viewmodel::BaseViewModel*>(lv_event_get_user_data(event));
    if (view_model) {
        view_model->toggle_dark_mode();
    }
}

} // namespace view::widgets
