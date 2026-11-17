/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_viewmodel.h"

#include <algorithm>
#include <cstring>

namespace viewmodel {
namespace {

int page_to_int(model::AppPage page) {
    return static_cast<int>(page);
}

int clamp(int value, int low, int high) {
    return std::max(low, std::min(high, value));
}

int wrap(int value, int count) {
    const int wrapped = value % count;
    return wrapped < 0 ? wrapped + count : wrapped;
}

constexpr const char* kNodeCharset =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -_.";

} // namespace

BaseViewModel::BaseViewModel()
    : title_subject_(model_.app_title()),
      dark_mode_subject_(model_.dark_mode()),
      current_page_subject_(page_to_int(model_.current_page())),
      quit_requested_subject_(false),
      carousel_selection_subject_(model_.carousel_selection()),
      settings_focus_subject_(model_.settings_focus()),
      node_name_subject_(model_.mesh_config().node_name),
      region_subject_(static_cast<int>(model_.mesh_config().region)),
      freq_hz_subject_(static_cast<int32_t>(model_.mesh_config().freq_hz)),
      sf_subject_(model_.mesh_config().sf),
      bw_index_subject_(model_.mesh_config().bw_index),
      cr_subject_(model_.mesh_config().cr),
      tx_power_subject_(model_.mesh_config().tx_power_dbm),
      sync_word_subject_(model_.mesh_config().sync_word),
      preamble_subject_(model_.mesh_config().preamble_len),
      node_edit_active_subject_(false),
      node_edit_cursor_subject_(0),
      lora_ok_subject_(false),
      lora_error_subject_("") {}

BaseViewModel::~BaseViewModel() = default;

lv_subject_t* BaseViewModel::title_subject() { return title_subject_.native(); }
lv_subject_t* BaseViewModel::dark_mode_subject() { return dark_mode_subject_.native(); }
lv_subject_t* BaseViewModel::current_page_subject() { return current_page_subject_.native(); }
lv_subject_t* BaseViewModel::quit_requested_subject() { return quit_requested_subject_.native(); }
lv_subject_t* BaseViewModel::carousel_selection_subject() { return carousel_selection_subject_.native(); }
lv_subject_t* BaseViewModel::settings_focus_subject() { return settings_focus_subject_.native(); }
lv_subject_t* BaseViewModel::node_name_subject() { return node_name_subject_.native(); }
lv_subject_t* BaseViewModel::region_subject() { return region_subject_.native(); }
lv_subject_t* BaseViewModel::freq_hz_subject() { return freq_hz_subject_.native(); }
lv_subject_t* BaseViewModel::sf_subject() { return sf_subject_.native(); }
lv_subject_t* BaseViewModel::bw_index_subject() { return bw_index_subject_.native(); }
lv_subject_t* BaseViewModel::cr_subject() { return cr_subject_.native(); }
lv_subject_t* BaseViewModel::tx_power_subject() { return tx_power_subject_.native(); }
lv_subject_t* BaseViewModel::sync_word_subject() { return sync_word_subject_.native(); }
lv_subject_t* BaseViewModel::preamble_subject() { return preamble_subject_.native(); }
lv_subject_t* BaseViewModel::node_edit_active_subject() { return node_edit_active_subject_.native(); }
lv_subject_t* BaseViewModel::node_edit_cursor_subject() { return node_edit_cursor_subject_.native(); }
lv_subject_t* BaseViewModel::lora_ok_subject() { return lora_ok_subject_.native(); }
lv_subject_t* BaseViewModel::lora_error_subject() { return lora_error_subject_.native(); }

bool BaseViewModel::is_dark_mode() const {
    return model_.dark_mode();
}

void BaseViewModel::set_dark_mode(bool enabled) {
    model_.set_dark_mode(enabled);
    publish_all();
}

void BaseViewModel::toggle_dark_mode() {
    model_.toggle_dark_mode();
    publish_all();
}

model::MeshConfig BaseViewModel::mesh_config() const {
    return model_.mesh_config();
}

model::AppPage BaseViewModel::current_page() const {
    return model_.current_page();
}

void BaseViewModel::set_current_page(model::AppPage page) {
    model_.set_current_page(page);
    publish_all();
}

void BaseViewModel::show_home() { set_current_page(model::AppPage::Home); }
void BaseViewModel::show_settings() { set_current_page(model::AppPage::Settings); }
void BaseViewModel::show_test1() { set_current_page(model::AppPage::Test1); }
void BaseViewModel::show_test2() { set_current_page(model::AppPage::Test2); }
void BaseViewModel::show_lora_error() { set_current_page(model::AppPage::LoraError); }

int BaseViewModel::carousel_selection() const {
    return model_.carousel_selection();
}

void BaseViewModel::move_carousel(int delta) {
    model_.move_carousel(delta);
    publish_mesh();
}

void BaseViewModel::activate_selection() {
    switch (model_.carousel_selection()) {
        case 0: show_settings(); break;
        case 1: show_test1(); break;
        case 2: show_test2(); break;
        default: show_home(); break;
    }
}

void BaseViewModel::move_settings_focus(int delta) {
    model_.move_settings_focus(delta);
    publish_mesh();
}

void BaseViewModel::adjust_focused_value(int delta) {
    auto& mesh = model_.mutable_mesh_config();
    const auto field = static_cast<model::SettingsField>(model_.settings_focus());

    switch (field) {
        case model::SettingsField::Region: {
            const int next = wrap(static_cast<int>(mesh.region) + delta, model::kRegionCount);
            apply_region_preset(mesh, static_cast<model::Region>(next));
            break;
        }
        case model::SettingsField::Frequency:
            mesh.region = model::Region::Custom;
            mesh.freq_hz = static_cast<std::uint32_t>(clamp(
                static_cast<int>(mesh.freq_hz) + delta * 1000000, 300000000, 940000000));
            break;
        case model::SettingsField::SpreadingFactor:
            mesh.region = model::Region::Custom;
            mesh.sf = clamp(mesh.sf + delta, 7, 12);
            break;
        case model::SettingsField::Bandwidth:
            mesh.region = model::Region::Custom;
            mesh.bw_index = clamp(mesh.bw_index + delta, 0, model::kBandwidthCount - 1);
            break;
        case model::SettingsField::CodingRate:
            mesh.region = model::Region::Custom;
            mesh.cr = clamp(mesh.cr + delta, 5, 8);
            break;
        case model::SettingsField::TxPower:
            mesh.tx_power_dbm = clamp(mesh.tx_power_dbm + delta, -9, 22);
            break;
        case model::SettingsField::SyncWord:
            mesh.sync_word = wrap(mesh.sync_word + delta, 256);
            break;
        case model::SettingsField::Preamble:
            mesh.preamble_len = clamp(mesh.preamble_len + delta * 16, 6, 65535);
            break;
        case model::SettingsField::NodeName:
        default:
            break;
    }

    publish_mesh();
}

void BaseViewModel::toggle_node_edit() {
    const bool active = model_.node_edit_active();
    if (!active) {
        std::strncpy(node_name_before_edit_.data(),
                     model_.mesh_config().node_name,
                     node_name_before_edit_.size() - 1);
        node_name_before_edit_[node_name_before_edit_.size() - 1] = '\0';
        model_.set_node_edit_cursor(static_cast<int>(std::strlen(model_.mesh_config().node_name)));
    }
    model_.set_node_edit_active(!active);
    publish_mesh();
}

void BaseViewModel::cancel_node_edit() {
    if (!model_.node_edit_active()) {
        return;
    }
    std::strncpy(model_.mutable_mesh_config().node_name,
                 node_name_before_edit_.data(),
                 model::kNodeNameBuffer - 1);
    model_.mutable_mesh_config().node_name[model::kNodeNameBuffer - 1] = '\0';
    model_.set_node_edit_active(false);
    publish_mesh();
}

void BaseViewModel::move_node_cursor(int delta) {
    model_.move_node_edit_cursor(delta);
    publish_mesh();
}

void BaseViewModel::cycle_node_char(int delta) {
    auto& mesh = model_.mutable_mesh_config();
    const int charset_len = static_cast<int>(std::strlen(kNodeCharset));
    const int len = static_cast<int>(std::strlen(mesh.node_name));
    int cursor = model_.node_edit_cursor();
    if (cursor > len) {
        cursor = len;
    }

    if (cursor == len && delta > 0 && len < model::kMaxNodeName) {
        mesh.node_name[cursor] = kNodeCharset[0];
        mesh.node_name[cursor + 1] = '\0';
    } else if (cursor < len) {
        int position = -1;
        for (int i = 0; i < charset_len; ++i) {
            if (kNodeCharset[i] == mesh.node_name[cursor]) {
                position = i;
                break;
            }
        }
        if (position < 0) {
            position = 0;
        }
        mesh.node_name[cursor] = kNodeCharset[wrap(position + delta, charset_len)];
    }

    publish_mesh();
}

void BaseViewModel::backspace_node_char() {
    auto& mesh = model_.mutable_mesh_config();
    const int len = static_cast<int>(std::strlen(mesh.node_name));
    const int cursor = model_.node_edit_cursor();
    if (cursor > 0 && cursor <= len) {
        for (int i = cursor - 1; i < len; ++i) {
            mesh.node_name[i] = mesh.node_name[i + 1];
        }
        model_.set_node_edit_cursor(cursor - 1);
    }
    publish_mesh();
}

void BaseViewModel::set_mesh_config(const model::MeshConfig& config) {
    model_.set_mesh_config(config);
    publish_mesh();
}

void BaseViewModel::set_lora_status(bool ok, const char* error) {
    lora_ok_subject_.set(ok);
    lora_error_subject_.set(error ? error : "");
}

void BaseViewModel::request_quit() {
    quit_requested_subject_.set(true);
}

void BaseViewModel::publish_all() {
    title_subject_.set(model_.app_title());
    dark_mode_subject_.set(model_.dark_mode());
    current_page_subject_.set(page_to_int(model_.current_page()));
    carousel_selection_subject_.set(model_.carousel_selection());
    settings_focus_subject_.set(model_.settings_focus());
}

void BaseViewModel::publish_mesh() {
    const auto& mesh = model_.mesh_config();
    node_name_subject_.set(mesh.node_name);
    region_subject_.set(static_cast<int>(mesh.region));
    freq_hz_subject_.set(static_cast<int32_t>(mesh.freq_hz));
    sf_subject_.set(mesh.sf);
    bw_index_subject_.set(mesh.bw_index);
    cr_subject_.set(mesh.cr);
    tx_power_subject_.set(mesh.tx_power_dbm);
    sync_word_subject_.set(mesh.sync_word);
    preamble_subject_.set(mesh.preamble_len);
    carousel_selection_subject_.set(model_.carousel_selection());
    settings_focus_subject_.set(model_.settings_focus());
    node_edit_active_subject_.set(model_.node_edit_active());
    node_edit_cursor_subject_.set(model_.node_edit_cursor());
}

} // namespace viewmodel
