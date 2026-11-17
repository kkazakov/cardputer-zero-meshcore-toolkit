/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "discover_screen.h"

#include "asset_manager.h"
#include "base_viewmodel.h"
#include "bindings.h"
#include "theme.h"

namespace screen {

DiscoverScreen::DiscoverScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

DiscoverScreen::~DiscoverScreen() {
    if (poll_timer_) {
        lv_timer_delete(poll_timer_);
        poll_timer_ = nullptr;
    }
}

void DiscoverScreen::build_content(lv_obj_t* content) {
    const auto palette = view::palette(view_model().is_dark_mode());
    const lv_font_t* small = assets().load_standard_font(10, app::StandardFontWeight::Regular);
    if (!small) small = &lv_font_montserrat_10;
    const lv_font_t* label_font = assets().load_standard_font(10);
    if (!label_font) label_font = &lv_font_montserrat_10;
    const lv_font_t* button_font = assets().load_standard_font(10);
    if (!button_font) button_font = &lv_font_montserrat_10;

    status_ = lv_label_create(content);
    lv_label_set_text(status_, "Starting\\xE2\\x80\\xA6");
    lv_obj_set_width(status_, 246);
    lv_label_set_long_mode(status_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(status_, small, 0);
    lv_obj_set_style_text_color(status_, palette.text_disabled, 0);
    lv_obj_set_style_text_opa(status_, LV_OPA_70, 0);
    lv_obj_set_pos(status_, 6, 3);
    reactive::bind_theme(status_, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
    lv_label_bind_text(status_, view_model().discover_summary_subject(), nullptr);

    scan_button_ = lv_obj_create(content);
    lv_obj_remove_style_all(scan_button_);
    lv_obj_set_size(scan_button_, 56, 18);
    lv_obj_set_pos(scan_button_, 258, 1);
    lv_obj_clear_flag(scan_button_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(scan_button_, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    reactive::bind_theme(scan_button_, view_model().dark_mode_subject(), reactive::ThemeRole::Button);
    lv_obj_add_event_cb(scan_button_, scan_clicked, LV_EVENT_CLICKED, this);

    scan_label_ = lv_label_create(scan_button_);
    lv_label_set_text(scan_label_, "Stop");
    lv_obj_set_style_text_font(scan_label_, button_font, 0);
    lv_obj_center(scan_label_);

    list_container_ = lv_obj_create(content);
    lv_obj_remove_style_all(list_container_);
    lv_obj_set_size(list_container_, 316, 90);
    lv_obj_set_pos(list_container_, 2, 20);
    lv_obj_set_style_pad_all(list_container_, 0, 0);
    lv_obj_set_style_pad_row(list_container_, 0, 0);
    lv_obj_set_scroll_dir(list_container_, LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(list_container_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(list_container_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(list_container_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(list_container_, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    list_font_ = assets().load_standard_font(12);
    if (!list_font_) list_font_ = &lv_font_montserrat_12;

    list_ = lv_label_create(list_container_);
    lv_label_set_recolor(list_, true);
    lv_label_set_long_mode(list_, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(list_, 306);
    lv_obj_clear_flag(list_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_text_font(list_, list_font_, 0);
    lv_obj_set_style_text_line_space(list_, 2, 0);
    lv_obj_set_style_text_color(list_, palette.text, 0);
    reactive::bind_theme(list_, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
    lv_label_bind_text(list_, view_model().discover_list_subject(), nullptr);

    reactive::observe_obj(scan_button_, view_model().discover_scanning_subject(),
                          scanning_changed, this);
    update_scan_label();

    poll_timer_ = lv_timer_create(poll_timer_cb, 100, this);
}

void DiscoverScreen::update_scan_label() {
    if (!scan_label_) {
        return;
    }
    const bool scanning = lv_subject_get_int(view_model().discover_scanning_subject()) != 0;
    lv_label_set_text(scan_label_, scanning ? "Stop" : "Scan");
}

void DiscoverScreen::scan_clicked(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    auto* discover = static_cast<DiscoverScreen*>(lv_event_get_user_data(event));
    if (discover) {
        discover->view_model().toggle_discover_scan();
    }
}

void DiscoverScreen::scanning_changed(lv_observer_t* observer, lv_subject_t*) {
    auto* discover = static_cast<DiscoverScreen*>(lv_observer_get_user_data(observer));
    if (discover) {
        discover->update_scan_label();
    }
}

void DiscoverScreen::poll_timer_cb(lv_timer_t* timer) {
    auto* discover = static_cast<DiscoverScreen*>(lv_timer_get_user_data(timer));
    if (discover) {
        discover->view_model().poll_discover();
    }
}

} // namespace screen
