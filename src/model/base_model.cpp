/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_model.h"

namespace model {

const char* BaseModel::app_title() const {
    return "Meshcore Toolkit";
}

bool BaseModel::dark_mode() const {
    return dark_mode_;
}

void BaseModel::set_dark_mode(bool enabled) {
    dark_mode_ = enabled;
}

void BaseModel::toggle_dark_mode() {
    dark_mode_ = !dark_mode_;
}

const MeshConfig& BaseModel::mesh_config() const {
    return mesh_;
}

void BaseModel::set_mesh_config(const MeshConfig& config) {
    mesh_ = config;
}

MeshConfig& BaseModel::mutable_mesh_config() {
    return mesh_;
}

AppPage BaseModel::current_page() const {
    return current_page_;
}

void BaseModel::set_current_page(AppPage page) {
    current_page_ = page;
}

int BaseModel::carousel_selection() const {
    return carousel_selection_;
}

void BaseModel::set_carousel_selection(int index) {
    const int wrapped = index % kCarouselItemCount;
    carousel_selection_ = wrapped < 0 ? wrapped + kCarouselItemCount : wrapped;
}

void BaseModel::move_carousel(int delta) {
    set_carousel_selection(carousel_selection_ + delta);
}

int BaseModel::settings_focus() const {
    return settings_focus_;
}

void BaseModel::set_settings_focus(int index) {
    if (index < 0) {
        settings_focus_ = 0;
    } else if (index >= kSettingsFieldCount) {
        settings_focus_ = kSettingsFieldCount - 1;
    } else {
        settings_focus_ = index;
    }
}

void BaseModel::move_settings_focus(int delta) {
    set_settings_focus(settings_focus_ + delta);
}

bool BaseModel::settings_edit_active() const {
    return settings_edit_active_;
}

void BaseModel::set_settings_edit_active(bool active) {
    settings_edit_active_ = active;
}

bool BaseModel::node_edit_active() const {
    return node_edit_active_;
}

void BaseModel::set_node_edit_active(bool active) {
    node_edit_active_ = active;
}

int BaseModel::node_edit_cursor() const {
    return node_edit_cursor_;
}

void BaseModel::set_node_edit_cursor(int cursor) {
    if (cursor < 0) {
        node_edit_cursor_ = 0;
    } else if (cursor > kMaxNodeName) {
        node_edit_cursor_ = kMaxNodeName;
    } else {
        node_edit_cursor_ = cursor;
    }
}

void BaseModel::move_node_edit_cursor(int delta) {
    set_node_edit_cursor(node_edit_cursor_ + delta);
}

} // namespace model
