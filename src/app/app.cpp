/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "app.h"

#include "application_config.h"
#include "asset_manager.h"
#include "help_popup.h"
#include "logger.h"
#include "linux_input.h"
#include "lora_hat.h"
#include "screenshot_service.h"
#include "screen_manager.h"
#include "base_viewmodel.h"
#include "theme.h"
#include "toast.h"

#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#if USE_DESKTOP
#include "desktop_simulator_frame.h"
#endif

#if !USE_DESKTOP
#if APP_USE_DRM
#include "src/drivers/display/drm/lv_linux_drm.h"
#else
#include "src/drivers/display/fb/lv_linux_fbdev.h"
#endif
#endif

#ifndef APP_FRAMEBUFFER_DEVICE
#define APP_FRAMEBUFFER_DEVICE "/dev/fb0"
#endif

#ifndef APP_DRM_DEVICE
#define APP_DRM_DEVICE "/dev/dri/card0"
#endif

#ifndef APP_DRM_CONNECTOR_ID
#define APP_DRM_CONNECTOR_ID -1
#endif

#ifndef APP_CONFIG_FILE
#define APP_CONFIG_FILE "meshcore-toolkit.conf"
#endif

namespace app {
namespace {

void quit_requested_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* running = static_cast<bool*>(lv_observer_get_user_data(observer));
    if (running && lv_subject_get_int(subject)) {
        *running = false;
    }
}

struct ConfigPersistence {
    viewmodel::BaseViewModel* view_model = nullptr;
    std::string config_path;
    bool last_dark_mode = false;
};

void persist_config(ConfigPersistence& persistence) {
    ApplicationConfig config;
    config.dark_mode = persistence.view_model->is_dark_mode();
    config.mesh = persistence.view_model->mesh_config();
    std::string error;
    if (save_application_config(persistence.config_path, config, error)) {
        LOG_INFO("saved config: {}", persistence.config_path);
    } else {
        LOG_WARN("failed to save config {}: {}", persistence.config_path, error);
    }
}

void persist_dark_mode_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* persistence = static_cast<ConfigPersistence*>(lv_observer_get_user_data(observer));
    if (!persistence || !persistence->view_model) {
        return;
    }
    const bool dark_mode = lv_subject_get_int(subject) != 0;
    if (dark_mode == persistence->last_dark_mode) {
        return;
    }
    persistence->last_dark_mode = dark_mode;
    persist_config(*persistence);
}

std::string writable_config_path() {
#if USE_DESKTOP
    return APP_CONFIG_FILE;
#else
    if (const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME")) {
        const std::filesystem::path root(xdg_config_home);
        if (!root.empty() && root.is_absolute()) {
            return (root / "meshcore-toolkit" / "meshcore-toolkit.conf").string();
        }
    }
    if (const char* home = std::getenv("HOME")) {
        const std::filesystem::path root(home);
        if (!root.empty() && root.is_absolute()) {
            return (root / ".config" / "meshcore-toolkit" / "meshcore-toolkit.conf").string();
        }
    }
    return APP_CONFIG_FILE;
#endif
}

// Central key router. The platform layer forwards every key press here first;
// returning true consumes the key so it never reaches the (now minimal) nav
// button dispatch.
class InputController {
public:
    InputController(viewmodel::BaseViewModel& view_model,
                    AssetManager& assets,
                    std::function<bool()> retry_lora)
        : view_model_(view_model),
          toast_(std::make_unique<view::widgets::Toast>(lv_layer_top(), view_model, assets)),
          help_popup_(std::make_unique<view::widgets::HelpPopup>(lv_layer_top(), view_model, assets)),
          retry_lora_(std::move(retry_lora)) {
        toast_->build();
        help_popup_->build();
        platform::set_global_key_listener(global_key_listener, this);
    }

    ~InputController() {
        platform::clear_global_key_listener(global_key_listener, this);
    }

private:
    static bool is_left(uint32_t key) {
        return key == 'z' || key == 'Z' || key == LV_KEY_LEFT;
    }
    static bool is_right(uint32_t key) {
        return key == 'c' || key == 'C' || key == LV_KEY_RIGHT;
    }

    bool handle_key(uint32_t key, bool long_pressed) {
        if (key == platform::kKeyPrintScreen && !long_pressed) {
            const auto result = platform::screenshot::capture_active_screen();
            if (result.success) {
                LOG_INFO("screenshot saved: {}", result.path);
                toast_->show_highlighted("Saved to ", result.path, "",
                                         view::widgets::ToastTone::Success, 5200, true);
            } else {
                LOG_WARN("screenshot failed: {}", result.error);
                toast_->show("Screenshot failed: " + result.error,
                             view::widgets::ToastTone::Error, 3200);
            }
            return true;
        }

        if (key == platform::kKeyHelp && !long_pressed) {
            toast_->hide();
            if (help_popup_->visible()) {
                help_popup_->hide();
            } else {
                help_popup_->show(view_model_.current_page());
            }
            return true;
        }

        if (help_popup_->visible()) {
            if (key == LV_KEY_ESC && !long_pressed) {
                help_popup_->hide();
            }
            return true;
        }

        if (long_pressed) {
            return false;
        }

        switch (view_model_.current_page()) {
            case model::AppPage::Home:
                return handle_home(key);
            case model::AppPage::Settings:
                return handle_settings(key);
            case model::AppPage::Test1:
            case model::AppPage::Test2:
                if (key == LV_KEY_ESC) {
                    view_model_.show_home();
                    return true;
                }
                return false;
            case model::AppPage::LoraError:
                if (key == LV_KEY_ENTER) {
                    retry_lora_();
                    return true;
                }
                if (key == LV_KEY_ESC) {
                    // Degraded mode: the radio is unusable, but the settings
                    // editor stays reachable so the node can still be configured.
                    view_model_.show_home();
                    return true;
                }
                return false;
        }
        return false;
    }

    bool handle_home(uint32_t key) {
        if (is_left(key)) {
            view_model_.move_carousel(-1);
            return true;
        }
        if (is_right(key)) {
            view_model_.move_carousel(1);
            return true;
        }
        if (key == LV_KEY_ENTER) {
            view_model_.activate_selection();
            return true;
        }
        if (key == LV_KEY_ESC) {
            view_model_.request_quit();
            return true;
        }
        return false;
    }

    bool handle_settings(uint32_t key) {
        const bool editing = lv_subject_get_int(view_model_.node_edit_active_subject()) != 0;

        if (editing) {
            if (key == LV_KEY_ENTER) {
                view_model_.toggle_node_edit(); // commit
                return true;
            }
            if (key == LV_KEY_ESC) {
                view_model_.cancel_node_edit();
                return true;
            }
            if (key == LV_KEY_LEFT) {
                view_model_.move_node_cursor(-1);
                return true;
            }
            if (key == LV_KEY_RIGHT) {
                view_model_.move_node_cursor(1);
                return true;
            }
            if (key == LV_KEY_UP) {
                view_model_.cycle_node_char(1);
                return true;
            }
            if (key == LV_KEY_DOWN) {
                view_model_.cycle_node_char(-1);
                return true;
            }
            if (key == LV_KEY_BACKSPACE) {
                view_model_.backspace_node_char();
                return true;
            }
            return false;
        }

        if (key == LV_KEY_ESC) {
            view_model_.show_home();
            return true;
        }
        if (key == LV_KEY_UP) {
            view_model_.move_settings_focus(-1);
            return true;
        }
        if (key == LV_KEY_DOWN) {
            view_model_.move_settings_focus(1);
            return true;
        }
        if (key == LV_KEY_LEFT) {
            view_model_.adjust_focused_value(-1);
            return true;
        }
        if (key == LV_KEY_RIGHT) {
            view_model_.adjust_focused_value(1);
            return true;
        }
        if (key == LV_KEY_ENTER &&
            lv_subject_get_int(view_model_.settings_focus_subject()) ==
                static_cast<int>(model::SettingsField::NodeName)) {
            view_model_.toggle_node_edit(); // begin editing
            return true;
        }
        return false;
    }

    static bool global_key_listener(uint32_t key,
                                    const char*,
                                    bool long_pressed,
                                    void* user_data) {
        auto* controller = static_cast<InputController*>(user_data);
        return controller && controller->handle_key(key, long_pressed);
    }

    viewmodel::BaseViewModel& view_model_;
    std::unique_ptr<view::widgets::Toast> toast_;
    std::unique_ptr<view::widgets::HelpPopup> help_popup_;
    std::function<bool()> retry_lora_;
};

#if !USE_DESKTOP
lv_display_t* init_device_display() {
#if APP_USE_DRM
    auto* display = lv_linux_drm_create();
    if (!display) {
        return nullptr;
    }
    if (lv_linux_drm_set_file(display, APP_DRM_DEVICE, APP_DRM_CONNECTOR_ID) != LV_RESULT_OK) {
        lv_display_delete(display);
        return nullptr;
    }
    platform::init_key_input(display);
    return display;
#else
    auto* display = lv_linux_fbdev_create();
    if (!display) {
        return nullptr;
    }
    if (lv_linux_fbdev_set_file(display, APP_FRAMEBUFFER_DEVICE) != LV_RESULT_OK) {
        lv_display_delete(display);
        return nullptr;
    }
    platform::init_key_input(display);
    return display;
#endif
}
#endif

} // namespace

int Application::run() {
    logger::Logger::init();
    logger::Logger::set_tag("meshcore-toolkit");

    lv_init();

    AssetManager assets;
    for (const auto& root : assets.roots()) {
        LOG_INFO("asset root: {}", root.string());
    }

    viewmodel::BaseViewModel view_model;
    const std::string user_config_path = writable_config_path();
    std::string loaded_config_path = APP_CONFIG_FILE;
    if (user_config_path != APP_CONFIG_FILE) {
        std::error_code filesystem_error;
        if (std::filesystem::is_regular_file(user_config_path, filesystem_error)) {
            loaded_config_path = user_config_path;
        }
    }

    ApplicationConfig config;
    std::string config_error;
    if (load_application_config(loaded_config_path, config, config_error)) {
        view_model.set_dark_mode(config.dark_mode);
        view_model.set_mesh_config(config.mesh);
        LOG_INFO("loaded config: {} (dark_mode={})",
                 loaded_config_path,
                 config.dark_mode ? "yes" : "no");
    } else {
        LOG_WARN("failed to load config {}: {}; using defaults", loaded_config_path, config_error);
    }

#if USE_DESKTOP
    DesktopSimulatorFrame simulator_frame(assets);
    auto* display = simulator_frame.display();
#else
    auto* display = init_device_display();
#endif
    if (!display) {
        LOG_ERROR("failed to initialize display");
        return 1;
    }

    auto* standard_font = assets.load_standard_font(14);
    view::apply_lvgl_theme(display,
                           view_model.is_dark_mode(),
                           standard_font ? standard_font : LV_FONT_DEFAULT);

#if USE_DESKTOP
    simulator_frame.bind_dark_mode(view_model.dark_mode_subject());
#endif

    ScreenManager screen_manager(view_model, assets);
    screen_manager.start();

    auto probe_lora = [&]() {
        const auto status = platform::probe_lora_hat();
        view_model.set_lora_status(status.working, status.error.c_str());
        if (status.working) {
            LOG_INFO("LoRa hat OK ({})", status.device);
            view_model.show_home();
        } else {
            LOG_ERROR("LoRa hat unavailable: {}", status.error);
            view_model.show_lora_error();
        }
        return status.working;
    };

    InputController input_controller(view_model, assets, probe_lora);

    ConfigPersistence persistence{&view_model, user_config_path, view_model.is_dark_mode()};
    auto* dark_mode_observer = lv_subject_add_observer(view_model.dark_mode_subject(),
                                                        persist_dark_mode_observer,
                                                        &persistence);

    bool running = true;
    auto* quit_observer = lv_subject_add_observer(view_model.quit_requested_subject(),
                                                   quit_requested_observer,
                                                   &running);

    probe_lora();

    LOG_INFO("LVGL app started at {}x{}", lv_display_get_horizontal_resolution(display),
             lv_display_get_vertical_resolution(display));
    while (running
#if USE_DESKTOP
           && simulator_frame.process_events()
#endif
    ) {
        lv_timer_handler();
        lv_delay_ms(5);
    }

    persist_config(persistence);
    platform::release_lora_hat();

    if (quit_observer) {
        lv_observer_remove(quit_observer);
    }
    if (dark_mode_observer) {
        lv_observer_remove(dark_mode_observer);
    }

    return 0;
}

} // namespace app
