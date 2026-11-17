/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "mesh_config.h"

namespace model {

enum class AppPage {
    Home      = 0,
    Settings  = 1,
    Test1     = 2,
    Test2     = 3,
    LoraError = 4,
};

constexpr int kCarouselItemCount = 3;

// Order of the rows shown on the Settings screen.
enum class SettingsField {
    NodeName       = 0,
    Region         = 1,
    Frequency      = 2,
    SpreadingFactor = 3,
    Bandwidth      = 4,
    CodingRate     = 5,
    TxPower        = 6,
    SyncWord       = 7,
    Preamble       = 8,
};

constexpr int kSettingsFieldCount = 9;

class BaseModel {
public:
    const char* app_title() const;

    bool dark_mode() const;
    void set_dark_mode(bool enabled);
    void toggle_dark_mode();

    const MeshConfig& mesh_config() const;
    void set_mesh_config(const MeshConfig& config);
    MeshConfig& mutable_mesh_config();

    AppPage current_page() const;
    void set_current_page(AppPage page);

    int carousel_selection() const;
    void set_carousel_selection(int index);
    void move_carousel(int delta);

    int settings_focus() const;
    void set_settings_focus(int index);
    void move_settings_focus(int delta);

    bool node_edit_active() const;
    void set_node_edit_active(bool active);
    int node_edit_cursor() const;
    void set_node_edit_cursor(int cursor);
    void move_node_edit_cursor(int delta);

private:
    bool dark_mode_             = false;
    AppPage current_page_       = AppPage::Home;
    MeshConfig mesh_            = default_mesh_config();
    int carousel_selection_     = 0;
    int settings_focus_         = 0;
    bool node_edit_active_      = false;
    int node_edit_cursor_       = 0;
};

} // namespace model
