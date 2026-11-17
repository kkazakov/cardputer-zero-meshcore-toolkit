/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_viewmodel.h"

#include "meshcore_protocol.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
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


uint32_t node_color(uint8_t type, bool dark) {
    switch (type) {
        case platform::meshcore::kAdvTypeRepeater: return dark ? 0x4DA8FF : 0x1F6FB2;
        case platform::meshcore::kAdvTypeChat: return dark ? 0x53C06B : 0x2E7D32;
        case platform::meshcore::kAdvTypeRoom: return dark ? 0xF0A020 : 0xB26A00;
        case platform::meshcore::kAdvTypeSensor: return dark ? 0x35C9C9 : 0x007A7A;
        default: return dark ? 0x9AA0A6 : 0x5F6B7A;
    }
}

void format_age(char* out, size_t capacity, uint32_t age_ms) {
    const uint32_t seconds = age_ms / 1000U;
    if (seconds < 60U) {
        std::snprintf(out, capacity, "%us", static_cast<unsigned>(seconds));
    } else if (seconds < 3600U) {
        std::snprintf(out, capacity, "%um", static_cast<unsigned>(seconds / 60U));
    } else {
        std::snprintf(out, capacity, "%uh", static_cast<unsigned>(seconds / 3600U));
    }
}

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
      settings_edit_active_subject_(false),
      node_edit_active_subject_(false),
      node_edit_cursor_subject_(0),
      lora_ok_subject_(false),
      lora_error_subject_(""),
      discover_summary_subject_("Idle"),
      discover_list_subject_(""),
      discover_scanning_subject_(false),
      discover_scroll_subject_(0),
      spectrum_peak_subject_(0),
      spectrum_floor_subject_(0),
      spectrum_channel_subject_(0),
      spectrum_busy_subject_(0),
      spectrum_view_subject_(0),
      spectrum_window_subject_(0),
      spectrum_status_subject_("") {}

BaseViewModel::~BaseViewModel() {
    shutdown_radio();
}

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
lv_subject_t* BaseViewModel::settings_edit_active_subject() { return settings_edit_active_subject_.native(); }
lv_subject_t* BaseViewModel::node_edit_active_subject() { return node_edit_active_subject_.native(); }
lv_subject_t* BaseViewModel::node_edit_cursor_subject() { return node_edit_cursor_subject_.native(); }
lv_subject_t* BaseViewModel::lora_ok_subject() { return lora_ok_subject_.native(); }
lv_subject_t* BaseViewModel::lora_error_subject() { return lora_error_subject_.native(); }
lv_subject_t* BaseViewModel::discover_summary_subject() { return discover_summary_subject_.native(); }
lv_subject_t* BaseViewModel::discover_list_subject() { return discover_list_subject_.native(); }
lv_subject_t* BaseViewModel::discover_scanning_subject() { return discover_scanning_subject_.native(); }
lv_subject_t* BaseViewModel::discover_scroll_subject() { return discover_scroll_subject_.native(); }
lv_subject_t* BaseViewModel::spectrum_peak_subject() { return spectrum_peak_subject_.native(); }
lv_subject_t* BaseViewModel::spectrum_floor_subject() { return spectrum_floor_subject_.native(); }
lv_subject_t* BaseViewModel::spectrum_channel_subject() { return spectrum_channel_subject_.native(); }
lv_subject_t* BaseViewModel::spectrum_busy_subject() { return spectrum_busy_subject_.native(); }
lv_subject_t* BaseViewModel::spectrum_view_subject() { return spectrum_view_subject_.native(); }
lv_subject_t* BaseViewModel::spectrum_window_subject() { return spectrum_window_subject_.native(); }
lv_subject_t* BaseViewModel::spectrum_status_subject() { return spectrum_status_subject_.native(); }

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
    if (page == model_.current_page()) {
        return;
    }

    stop_feature_sessions();
    model_.set_current_page(page);

    if (page == model::AppPage::Discover) {
        start_discover();
    } else if (page == model::AppPage::Spectrum) {
        start_spectrum();
    }
    publish_all();
}

void BaseViewModel::show_home() { set_current_page(model::AppPage::Home); }
void BaseViewModel::show_settings() { set_current_page(model::AppPage::Settings); }
void BaseViewModel::show_discover() { set_current_page(model::AppPage::Discover); }
void BaseViewModel::show_spectrum() { set_current_page(model::AppPage::Spectrum); }
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
        case 0: show_spectrum(); break;
        case 1: show_discover(); break;
        case 2: show_settings(); break;
        default: show_home(); break;
    }
}

void BaseViewModel::move_settings_focus(int delta) {
    if (model_.settings_edit_active()) {
        return;
    }
    model_.move_settings_focus(delta);
    publish_mesh();
}

bool BaseViewModel::settings_edit_active() const {
    return model_.settings_edit_active();
}

void BaseViewModel::begin_settings_edit() {
    if (model_.settings_edit_active()) {
        return;
    }
    // Snapshot so ESC can restore the value the edit started from.
    mesh_before_edit_ = model_.mesh_config();
    model_.set_settings_edit_active(true);
    publish_mesh();
}

void BaseViewModel::commit_settings_edit() {
    if (!model_.settings_edit_active()) {
        return;
    }
    model_.set_settings_edit_active(false);
    publish_mesh();
}

void BaseViewModel::cancel_settings_edit() {
    if (!model_.settings_edit_active()) {
        return;
    }
    model_.set_mesh_config(mesh_before_edit_);
    model_.set_settings_edit_active(false);
    publish_mesh();
}

void BaseViewModel::adjust_focused_value(int delta) {
    if (!model_.settings_edit_active()) {
        return; // L/R only change values once OK opened the row
    }

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
        case model::SettingsField::SpreadingFactor: {
            const int old_default_preamble = model::meshcore_preamble_for_sf(mesh.sf);
            mesh.region = model::Region::Custom;
            mesh.sf = clamp(mesh.sf + delta, 7, 12);
            if (mesh.preamble_len == old_default_preamble) {
                mesh.preamble_len = model::meshcore_preamble_for_sf(mesh.sf);
            }
            break;
        }
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

void BaseViewModel::insert_node_char(char character) {
    if (character == '\0' || std::strchr(kNodeCharset, character) == nullptr) {
        return; // only characters the config format accepts
    }

    auto& mesh = model_.mutable_mesh_config();
    const int len = static_cast<int>(std::strlen(mesh.node_name));
    if (len >= model::kMaxNodeName) {
        return;
    }

    int cursor = model_.node_edit_cursor();
    if (cursor > len) {
        cursor = len;
    }

    for (int i = len; i >= cursor; --i) {
        mesh.node_name[i + 1] = mesh.node_name[i];
    }
    mesh.node_name[cursor] = character;
    model_.set_node_edit_cursor(cursor + 1);
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
    if (model_.current_page() == model::AppPage::Discover) {
        start_discover();
    } else if (model_.current_page() == model::AppPage::Spectrum) {
        start_spectrum();
    }
}

void BaseViewModel::set_lora_status(bool ok, const char* error) {
    lora_ok_subject_.set(ok);
    lora_error_subject_.set(error ? error : "");
}

void BaseViewModel::request_quit() {
    quit_requested_subject_.set(true);
}

void BaseViewModel::publish_all() {
    title_subject_.set(page_title(model_.current_page()));
    dark_mode_subject_.set(model_.dark_mode());
    current_page_subject_.set(page_to_int(model_.current_page()));
    carousel_selection_subject_.set(model_.carousel_selection());
    settings_focus_subject_.set(model_.settings_focus());
    if (discover_) {
        publish_discover(true);
    }
}


bool BaseViewModel::ensure_radio(std::string& error) {
    if (!radio_) {
        radio_ = std::make_unique<platform::MeshRadio>();
    }
    if (radio_->ready()) {
        return true;
    }
    if (radio_->initialize(model_.mesh_config(), error)) {
        lora_ok_subject_.set(true);
        lora_error_subject_.set("");
        return true;
    }
    return false;
}

void BaseViewModel::stop_feature_sessions() {
    if (discover_) {
        discover_->stop();
        discover_.reset();
    }
    if (spectrum_) {
        spectrum_->stop();
        spectrum_.reset();
    }
    // The snapshot points into the scanner's arrays; clear it before the timer
    // of a still-alive screen can read it during the deferred page switch.
    spectrum_snapshot_ = {};
    if (radio_) {
        radio_->standby();
    }
    discover_scroll_ = 0;
    discover_published_revision_ = 0;
    spectrum_published_revision_ = 0;
    discover_list_subject_.set("");
    discover_summary_subject_.set("Idle");
    discover_scanning_subject_.set(false);
    discover_scroll_subject_.set(0);
    spectrum_status_subject_.set("Idle");
}

void BaseViewModel::start_discover() {
    stop_feature_sessions();

    std::string error;
    if (!ensure_radio(error)) {
        const std::string summary = "Radio unavailable: " + error;
        discover_summary_subject_.set(summary.c_str());
        discover_list_subject_.set("LoRa hat is not responding\nESC to go back");
        discover_scanning_subject_.set(false);
        return;
    }

    discover_ = std::make_unique<platform::DiscoverScanner>(*radio_);
    discover_->start(model_.mesh_config(), lv_tick_get());
    discover_scroll_ = 0;
    discover_scroll_subject_.set(0);
    publish_discover(true);
}

void BaseViewModel::start_spectrum() {
    stop_feature_sessions();

    std::string error;
    if (!ensure_radio(error)) {
        const std::string status = "Radio unavailable: " + error;
        spectrum_status_subject_.set(status.c_str());
        return;
    }

    spectrum_ = std::make_unique<platform::SpectrumScanner>(*radio_);
    spectrum_->start(model_.mesh_config(), lv_tick_get());
    publish_spectrum(true);
}

void BaseViewModel::shutdown_radio() {
    stop_feature_sessions();
    if (radio_) {
        radio_->shutdown();
        radio_.reset();
    }
}

void BaseViewModel::poll_discover() {
    if (!discover_) {
        return;
    }
    discover_->tick(lv_tick_get());
    publish_discover(false);
}

void BaseViewModel::toggle_discover_scan() {
    if (!discover_ || !discover_->active()) {
        start_discover();
        return;
    }
    discover_->set_scanning(!discover_->scanning());
    publish_discover(true);
}

void BaseViewModel::scroll_discover(int delta) {
    if (!discover_) {
        return;
    }
    const int rows = static_cast<int>(discover_->node_count());
    const int maximum = std::max(0, rows - kDiscoverVisibleRows);
    discover_scroll_ = std::max(0, std::min(maximum, discover_scroll_ + delta));
    discover_scroll_subject_.set(discover_scroll_);
    publish_discover(true);
}

void BaseViewModel::publish_discover(bool force) {
    if (!discover_) {
        return;
    }

    const uint32_t now = lv_tick_get();
    const uint32_t revision = discover_->revision();
    if (!force && revision == discover_published_revision_ &&
        (now - last_discover_publish_ms_) < 1000U) {
        return;
    }
    discover_published_revision_ = revision;
    last_discover_publish_ms_ = now;

    const auto& stats = discover_->stats();
    const bool dark = model_.dark_mode();
    char summary[320];
    if (!discover_->error().empty()) {
        std::snprintf(summary, sizeof(summary), "Radio error: %.200s", discover_->error().c_str());
    } else {
        std::snprintf(summary, sizeof(summary), "%s%s | %d nearby (%d rpt, %d comp)",
                      stats.simulated ? "SIM | " : "",
                      stats.scanning ? "Scanning..." : "Paused",
                      stats.total, stats.repeaters, stats.companions);
    }
    discover_summary_subject_.set(summary);
    discover_scanning_subject_.set(stats.scanning);

    std::array<size_t, platform::DiscoverScanner::kMaxNodes> order{};
    const size_t count = std::min(discover_->node_count(), order.size());
    for (size_t i = 0; i < count; ++i) {
        order[i] = i;
    }
    const auto* nodes = discover_->nodes();
    std::sort(order.begin(), order.begin() + static_cast<std::ptrdiff_t>(count),
              [nodes](size_t left, size_t right) {
                  if (nodes[left].rssi != nodes[right].rssi) {
                      return nodes[left].rssi > nodes[right].rssi;
                  }
                  return nodes[left].last_ms > nodes[right].last_ms;
              });

    const int maximum = std::max(0, static_cast<int>(count) - kDiscoverVisibleRows);
    discover_scroll_ = std::max(0, std::min(maximum, discover_scroll_));
    const size_t first = static_cast<size_t>(discover_scroll_);
    const size_t shown = std::min<size_t>(count - first, kDiscoverVisibleRows);
    std::array<char, 900> rows{};
    size_t used = 0;
    for (size_t i = 0; i < shown && used < rows.size() - 96; ++i) {
        const auto& node = nodes[order[first + i]];
        char name[40];
        if (node.name_valid && node.name[0] != '\0') {
            platform::meshcore::copy_display_name(name, sizeof(name), node.name, 18);
        } else {
            std::snprintf(name, sizeof(name), "Node %02X%02X", node.pubkey[0], node.pubkey[1]);
        }
        char age[12];
        format_age(age, sizeof(age), now - node.last_ms);
        char path[12];
        if (node.hops == 0) {
            std::snprintf(path, sizeof(path), "direct");
        } else {
            std::snprintf(path, sizeof(path), "%uh", static_cast<unsigned>(node.hops));
        }
        const int written = std::snprintf(
            rows.data() + used, rows.size() - used,
            "#%06X %s#  %s %ddBm %.1f %s | %s\n",
            static_cast<unsigned>(node_color(node.node_type, dark)),
            name,
            platform::meshcore::node_type_short(node.node_type),
            static_cast<int>(node.rssi),
            static_cast<double>(node.snr_q4) / 4.0,
            path,
            age);
        if (written <= 0) {
            break;
        }
        if (used + static_cast<size_t>(written) >= rows.size()) {
            used = rows.size() - 1;
            break;
        }
        used += static_cast<size_t>(written);
    }

    if (used == 0) {
        const char* message = discover_->error().empty()
                                  ? (stats.scanning ? "Scanning... nothing has answered yet"
                                                    : "Paused | press OK to scan")
                                  : "Radio unavailable | press OK to retry";
        discover_list_subject_.set(message);
    } else {
        rows[used - 1] = '\0'; // trim the trailing newline
        discover_list_subject_.set(rows.data());
    }

    discover_scroll_subject_.set(discover_scroll_);
}

void BaseViewModel::poll_spectrum() {
    if (!spectrum_) {
        return;
    }
    spectrum_->tick(lv_tick_get());
    publish_spectrum(false);
}

void BaseViewModel::publish_spectrum(bool force) {
    if (!spectrum_) {
        return;
    }
    spectrum_snapshot_ = spectrum_->snapshot();
    const uint32_t revision = spectrum_snapshot_.plot_revision * 31U +
                              spectrum_snapshot_.waterfall_revision;
    if (!force && revision == spectrum_published_revision_) {
        return;
    }
    spectrum_published_revision_ = revision;

    spectrum_peak_subject_.set(spectrum_snapshot_.peak_dbm);
    spectrum_floor_subject_.set(spectrum_snapshot_.floor_dbm);
    spectrum_channel_subject_.set(spectrum_snapshot_.channel_dbm);
    spectrum_busy_subject_.set(spectrum_snapshot_.busy_percent);
    spectrum_view_subject_.set(spectrum_snapshot_.channel_view ? 1 : 0);
    spectrum_window_subject_.set(spectrum_snapshot_.window);

    if (!spectrum_->error().empty()) {
        const std::string status = "Radio error: " + spectrum_->error();
        spectrum_status_subject_.set(status.c_str());
    } else {
        spectrum_status_subject_.set("");
    }
}

void BaseViewModel::toggle_spectrum_view() {
    if (!spectrum_) {
        return;
    }
    spectrum_->set_channel_view(!spectrum_->channel_view(), lv_tick_get());
    publish_spectrum(true);
}

void BaseViewModel::cycle_spectrum_window(int delta) {
    if (!spectrum_) {
        return;
    }
    spectrum_->cycle_window(delta, lv_tick_get());
    publish_spectrum(true);
}

const platform::SpectrumSnapshot& BaseViewModel::spectrum_snapshot() const {
    return spectrum_snapshot_;
}

const char* BaseViewModel::page_title(model::AppPage page) {
    switch (page) {
        case model::AppPage::Settings: return "Settings";
        case model::AppPage::Discover: return "Discover";
        case model::AppPage::Spectrum: return "Spectrum";
        case model::AppPage::LoraError: return "LoRa error";
        case model::AppPage::Home:
        default: return "Meshcore Toolkit";
    }
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
    settings_edit_active_subject_.set(model_.settings_edit_active());
    node_edit_active_subject_.set(model_.node_edit_active());
    node_edit_cursor_subject_.set(model_.node_edit_cursor());
}

} // namespace viewmodel
