/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "lora_error_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"

namespace screen {

LoraErrorScreen::LoraErrorScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void LoraErrorScreen::build_content(lv_obj_t* content) {
    auto* group = lv_obj_create(content);
    lv_obj_remove_style_all(group);
    lv_obj_set_size(group, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(group, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(group, 6, 0);
    lv_obj_clear_flag(group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(group);

    auto* mark = lv_label_create(group);
    lv_label_set_text(mark, "!");
    const lv_font_t* mark_font = assets().load_standard_font(26);
    lv_obj_set_style_text_font(mark, mark_font ? mark_font : &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(mark, view::palette(view_model().is_dark_mode()).error, 0);

    auto* title = lv_label_create(group);
    lv_label_set_text(title, "LoRa hat not working");
    const lv_font_t* title_font = assets().load_standard_font(16);
    lv_obj_set_style_text_font(title, title_font ? title_font : &lv_font_montserrat_16, 0);
    reactive::bind_theme(title, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    error_label_ = lv_label_create(group);
    lv_label_set_long_mode(error_label_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(error_label_, 280);
    lv_obj_set_style_text_align(error_label_, LV_TEXT_ALIGN_CENTER, 0);
    const lv_font_t* error_font = assets().load_standard_font(11, app::StandardFontWeight::Regular);
    lv_obj_set_style_text_font(error_label_, error_font ? error_font : &lv_font_montserrat_12, 0);
    reactive::bind_theme(error_label_, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    auto* hint = lv_label_create(group);
    lv_label_set_text(hint, "Check the hat is seated and powered, then retry");
    const lv_font_t* hint_font = assets().load_standard_font(11, app::StandardFontWeight::Regular);
    lv_obj_set_style_text_font(hint, hint_font ? hint_font : &lv_font_montserrat_12, 0);
    reactive::bind_theme(hint, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    reactive::observe_obj(root(), view_model().lora_error_subject(), error_changed, this);
    lv_label_set_text(error_label_, lv_subject_get_string(view_model().lora_error_subject()));
}

void LoraErrorScreen::error_changed(lv_observer_t* observer, lv_subject_t* subject) {
    auto* screen = static_cast<LoraErrorScreen*>(lv_observer_get_user_data(observer));
    if (screen && screen->error_label_) {
        lv_label_set_text(screen->error_label_, lv_subject_get_string(subject));
    }
}

} // namespace screen
