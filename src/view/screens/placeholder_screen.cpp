/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "placeholder_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"

#include <utility>

namespace screen {

PlaceholderScreen::PlaceholderScreen(viewmodel::BaseViewModel& view_model,
                                     app::AssetManager& assets,
                                     std::string title)
    : BaseScreen(view_model, assets), title_(std::move(title)) {
    init();
}

void PlaceholderScreen::build_content(lv_obj_t* content) {
    auto* group = lv_obj_create(content);
    lv_obj_remove_style_all(group);
    lv_obj_set_size(group, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(group, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(group, 4, 0);
    lv_obj_clear_flag(group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(group);

    auto* title = lv_label_create(group);
    lv_label_set_text(title, title_.c_str());
    const lv_font_t* title_font = assets().load_standard_font(18);
    lv_obj_set_style_text_font(title, title_font ? title_font : &lv_font_montserrat_18, 0);
    reactive::bind_theme(title, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    auto* subtitle = lv_label_create(group);
    lv_label_set_text(subtitle, "Coming soon");
    const lv_font_t* subtitle_font = assets().load_standard_font(12, app::StandardFontWeight::Regular);
    lv_obj_set_style_text_font(subtitle, subtitle_font ? subtitle_font : &lv_font_montserrat_12, 0);
    reactive::bind_theme(subtitle, view_model().dark_mode_subject(), reactive::ThemeRole::Text);
}

} // namespace screen
