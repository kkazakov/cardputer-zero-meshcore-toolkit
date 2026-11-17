/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "settings_screen.h"

#include "asset_manager.h"
#include "base_model.h"
#include "bindings.h"
#include "mesh_config.h"
#include "theme.h"

#include <array>
#include <cstdio>
#include <cstring>

namespace screen {
namespace {

constexpr std::array<const char*, model::kSettingsFieldCount> kFieldNames = {{
    "Node name",
    "Region",
    "Frequency",
    "Spreading factor",
    "Bandwidth",
    "Coding rate",
    "TX power",
    "Sync word",
    "Preamble",
}};

constexpr int kRowHeight = 26;

} // namespace

SettingsScreen::SettingsScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void SettingsScreen::build_content(lv_obj_t* content) {
    const lv_font_t* name_font = assets().load_standard_font(12, app::StandardFontWeight::Regular);
    if (!name_font) name_font = &lv_font_montserrat_12;
    const lv_font_t* value_font = assets().load_standard_font(14);
    if (!value_font) value_font = &lv_font_montserrat_14;

    list_ = lv_obj_create(content);
    lv_obj_remove_style_all(list_);
    lv_obj_set_size(list_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list_, 4, 0);
    lv_obj_set_style_pad_row(list_, 2, 0);
    lv_obj_add_flag(list_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(list_, LV_DIR_VER);

    for (int i = 0; i < model::kSettingsFieldCount; ++i) {
        auto& row = rows_[i];
        row.container = lv_obj_create(list_);
        lv_obj_remove_style_all(row.container);
        lv_obj_set_size(row.container, LV_PCT(100), kRowHeight);
        lv_obj_set_flex_flow(row.container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row.container, LV_FLEX_ALIGN_SPACE_BETWEEN,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_hor(row.container, 6, 0);
        lv_obj_set_style_radius(row.container, 6, 0);
        lv_obj_clear_flag(row.container, LV_OBJ_FLAG_SCROLLABLE);

        row.name = lv_label_create(row.container);
        lv_label_set_text(row.name, kFieldNames[i]);
        lv_obj_set_style_text_font(row.name, name_font, 0);
        lv_obj_set_style_text_align(row.name, LV_TEXT_ALIGN_LEFT, 0);

        row.value = lv_label_create(row.container);
        lv_label_set_long_mode(row.value, LV_LABEL_LONG_DOT);
        lv_obj_set_width(row.value, 150);
        lv_obj_set_style_text_font(row.value, value_font, 0);
        lv_obj_set_style_text_align(row.value, LV_TEXT_ALIGN_RIGHT, 0);
    }

    lv_subject_t* subjects[] = {
        view_model().settings_focus_subject(),
        view_model().node_name_subject(),
        view_model().region_subject(),
        view_model().freq_hz_subject(),
        view_model().sf_subject(),
        view_model().bw_index_subject(),
        view_model().cr_subject(),
        view_model().tx_power_subject(),
        view_model().sync_word_subject(),
        view_model().preamble_subject(),
        view_model().node_edit_active_subject(),
        view_model().node_edit_cursor_subject(),
        view_model().dark_mode_subject(),
    };
    for (auto* subject : subjects) {
        reactive::observe_obj(root(), subject, refresh_cb, this);
    }

    refresh();
}

void SettingsScreen::refresh() {
    const int focus = lv_subject_get_int(view_model().settings_focus_subject());
    const bool dark = view_model().is_dark_mode();
    const auto palette = view::palette(dark);

    for (int i = 0; i < model::kSettingsFieldCount; ++i) {
        const bool focused = i == focus;
        lv_obj_set_style_bg_color(rows_[i].container, palette.button, 0);
        lv_obj_set_style_bg_opa(rows_[i].container, focused ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(rows_[i].name, focused ? palette.text : palette.text_disabled, 0);
        lv_obj_set_style_text_color(rows_[i].value, focused ? palette.text : palette.text, 0);
    }

    // Node name (with inline cursor while editing).
    char display[48];
    const char* name = lv_subject_get_string(view_model().node_name_subject());
    const bool editing = lv_subject_get_int(view_model().node_edit_active_subject()) != 0;
    if (editing && focus == static_cast<int>(model::SettingsField::NodeName)) {
        int cursor = lv_subject_get_int(view_model().node_edit_cursor_subject());
        const int len = static_cast<int>(std::strlen(name));
        if (cursor > len) cursor = len;
        int j = 0;
        for (int i = 0; i < cursor && j < 45; ++i) display[j++] = name[i];
        display[j++] = '|';
        for (int i = cursor; name[i] != '\0' && j < 45; ++i) display[j++] = name[i];
        display[j] = '\0';
    } else {
        std::strncpy(display, name, sizeof(display) - 1);
        display[sizeof(display) - 1] = '\0';
    }
    lv_label_set_text(rows_[0].value, display);

    lv_label_set_text(rows_[1].value,
                      model::region_name(static_cast<model::Region>(
                          lv_subject_get_int(view_model().region_subject()))));

    const auto freq = static_cast<unsigned>(lv_subject_get_int(view_model().freq_hz_subject()));
    lv_label_set_text_fmt(rows_[2].value, "%u.%03u MHz", freq / 1000000u, (freq % 1000000u) / 1000u);

    lv_label_set_text_fmt(rows_[3].value, "%d", static_cast<int>(lv_subject_get_int(view_model().sf_subject())));
    lv_label_set_text(rows_[4].value,
                      model::bandwidth_label(lv_subject_get_int(view_model().bw_index_subject())));
    lv_label_set_text_fmt(rows_[5].value, "4/%d", static_cast<int>(lv_subject_get_int(view_model().cr_subject())));
    lv_label_set_text_fmt(rows_[6].value, "%d dBm", static_cast<int>(lv_subject_get_int(view_model().tx_power_subject())));
    lv_label_set_text_fmt(rows_[7].value, "0x%02X", static_cast<unsigned>(lv_subject_get_int(view_model().sync_word_subject())));
    lv_label_set_text_fmt(rows_[8].value, "%d", static_cast<int>(lv_subject_get_int(view_model().preamble_subject())));

    lv_obj_scroll_to_view(rows_[focus].container, LV_ANIM_ON);
}

void SettingsScreen::refresh_cb(lv_observer_t* observer, lv_subject_t* subject) {
    LV_UNUSED(subject);
    auto* screen = static_cast<SettingsScreen*>(lv_observer_get_user_data(observer));
    if (screen) {
        screen->refresh();
    }
}

} // namespace screen
