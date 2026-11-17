/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "home_screen.h"

#include "asset_manager.h"
#include "base_model.h"
#include "bindings.h"
#include "theme.h"

#include <array>

namespace screen {
namespace {

struct CarouselItem {
    const char* title;
    const char* subtitle;
};

constexpr std::array<CarouselItem, model::kCarouselItemCount> kItems = {{
    {"Settings", "Radio & node"},
    {"Test 1", "Placeholder"},
    {"Test 2", "Placeholder"},
}};

constexpr int kCenterCardX = 87;
constexpr int kCenterCardY = 4;
constexpr int kCenterCardW = 146;
constexpr int kCenterCardH = 88;
constexpr int kSideCardW   = 71;
constexpr int kSideCardH   = 66;
constexpr int kSideCardY   = 16;
constexpr int kLeftCardX   = 7;
constexpr int kRightCardX  = 242;
constexpr int kCardRadius  = 11;
constexpr int kDotActiveW  = 14;
constexpr int kDotIdleW    = 5;
constexpr int kDotH        = 4;

lv_color_t accent_color(const view::ThemePalette& palette, int index) {
    switch (index) {
        case 1: return palette.info;
        case 2: return palette.warning;
        default: return palette.primary;
    }
}

} // namespace

HomeScreen::HomeScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void HomeScreen::build_content(lv_obj_t* content) {
    const lv_font_t* center_title_font = assets().load_standard_font(16);
    if (!center_title_font) center_title_font = &lv_font_montserrat_16;
    const lv_font_t* subtitle_font = assets().load_standard_font(10, app::StandardFontWeight::Regular);
    if (!subtitle_font) subtitle_font = &lv_font_montserrat_10;
    const lv_font_t* side_font = assets().load_standard_font(12);
    if (!side_font) side_font = &lv_font_montserrat_12;

    const auto build_card = [&](int x, int y, int w, int h, Card& card,
                                const lv_font_t* title_font, bool show_subtitle) {
        card.container = lv_obj_create(content);
        lv_obj_remove_style_all(card.container);
        lv_obj_set_size(card.container, w, h);
        lv_obj_set_pos(card.container, x, y);
        lv_obj_clear_flag(card.container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(card.container, kCardRadius, 0);
        lv_obj_set_style_border_width(card.container, 1, 0);
        lv_obj_set_style_border_opa(card.container, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_opa(card.container, LV_OPA_COVER, 0);

        card.title = lv_label_create(card.container);
        lv_label_set_long_mode(card.title, LV_LABEL_LONG_DOT);
        lv_obj_set_width(card.title, w - 8);
        lv_obj_set_style_text_font(card.title, title_font, 0);
        lv_obj_set_style_text_align(card.title, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(card.title, LV_ALIGN_TOP_MID, 0, show_subtitle ? 12 : 20);

        card.subtitle = lv_label_create(card.container);
        lv_label_set_long_mode(card.subtitle, LV_LABEL_LONG_DOT);
        lv_obj_set_width(card.subtitle, w - 8);
        lv_obj_set_style_text_font(card.subtitle, subtitle_font, 0);
        lv_obj_set_style_text_align(card.subtitle, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(card.subtitle, LV_ALIGN_TOP_MID, 0, show_subtitle ? 34 : -40);
        if (!show_subtitle) {
            lv_obj_add_flag(card.subtitle, LV_OBJ_FLAG_HIDDEN);
        }
    };

    build_card(kLeftCardX, kSideCardY, kSideCardW, kSideCardH, left_, side_font, false);
    build_card(kCenterCardX, kCenterCardY, kCenterCardW, kCenterCardH, center_,
               center_title_font, true);
    build_card(kRightCardX, kSideCardY, kSideCardW, kSideCardH, right_, side_font, false);

    auto* dots_bar = lv_obj_create(content);
    lv_obj_remove_style_all(dots_bar);
    lv_obj_set_size(dots_bar, LV_SIZE_CONTENT, kDotH);
    lv_obj_align(dots_bar, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_flex_flow(dots_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots_bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dots_bar, 7, 0);
    lv_obj_clear_flag(dots_bar, LV_OBJ_FLAG_SCROLLABLE);
    for (auto& dot : dots_) {
        dot = lv_obj_create(dots_bar);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, kDotIdleW, kDotH);
        lv_obj_set_style_radius(dot, 2, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    }

    reactive::observe_obj(root(), view_model().carousel_selection_subject(),
                          selection_changed, this);
    reactive::observe_obj(root(), view_model().dark_mode_subject(), theme_changed, this);
    reactive::observe_obj(root(), view_model().lora_ok_subject(), theme_changed, this);
    render();
}

void HomeScreen::style_card(const Card& card, lv_color_t accent, bool center) {
    const auto palette = view::palette(view_model().is_dark_mode());
    lv_obj_set_style_bg_color(card.container, palette.surface, 0);
    lv_obj_set_style_border_color(card.container, center ? accent : palette.border, 0);
    lv_obj_set_style_border_width(card.container, center ? 2 : 1, 0);
    lv_obj_set_style_text_color(card.title, center ? palette.text : palette.text_disabled, 0);
    lv_obj_set_style_text_color(card.subtitle, palette.text_disabled, 0);
}

void HomeScreen::render() {
    const int selected = view_model().carousel_selection();
    const int count = model::kCarouselItemCount;
    const int center_index = selected;
    const int left_index = (selected - 1 + count) % count;
    const int right_index = (selected + 1) % count;

    const bool lora_ok = lv_subject_get_int(view_model().lora_ok_subject()) != 0;
    const auto subtitle_for = [&](int index) {
        // The Settings card doubles as the radio status indicator.
        if (index == 0 && !lora_ok) {
            return "Radio offline";
        }
        return kItems[index].subtitle;
    };

    lv_label_set_text(center_.title, kItems[center_index].title);
    lv_label_set_text(center_.subtitle, subtitle_for(center_index));
    lv_label_set_text(left_.title, kItems[left_index].title);
    lv_label_set_text(right_.title, kItems[right_index].title);

    const auto palette = view::palette(view_model().is_dark_mode());
    style_card(center_, accent_color(palette, center_index), true);
    style_card(left_, palette.border, false);
    style_card(right_, palette.border, false);

    for (int i = 0; i < count; ++i) {
        const bool active = i == selected;
        lv_obj_set_width(dots_[i], active ? kDotActiveW : kDotIdleW);
        lv_obj_set_style_bg_color(dots_[i],
                                  active ? accent_color(palette, selected) : palette.border, 0);
    }
}

void HomeScreen::selection_changed(lv_observer_t* observer, lv_subject_t* subject) {
    LV_UNUSED(subject);
    auto* screen = static_cast<HomeScreen*>(lv_observer_get_user_data(observer));
    if (screen) {
        screen->render();
    }
}

void HomeScreen::theme_changed(lv_observer_t* observer, lv_subject_t* subject) {
    LV_UNUSED(subject);
    auto* screen = static_cast<HomeScreen*>(lv_observer_get_user_data(observer));
    if (screen) {
        screen->render();
    }
}

} // namespace screen
