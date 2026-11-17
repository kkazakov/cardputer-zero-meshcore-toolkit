/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "application_config.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace app {
namespace {

std::string trim(std::string value) {
    const auto not_space = [](unsigned char ch) { return std::isspace(ch) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool parse_bool(const std::string& value, bool& result) {
    const auto normalized = lowercase(trim(value));
    if (normalized == "yes" || normalized == "true" || normalized == "1") {
        result = true;
        return true;
    }
    if (normalized == "no" || normalized == "false" || normalized == "0") {
        result = false;
        return true;
    }
    return false;
}

bool parse_int(const std::string& value, int& result) {
    try {
        std::size_t used = 0;
        const auto parsed = std::stol(trim(value), &used);
        if (used == 0) {
            return false;
        }
        result = static_cast<int>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

const char* region_key(model::Region region) {
    switch (region) {
        case model::Region::UsCanada:  return "us_canada";
        case model::Region::Australia: return "australia";
        case model::Region::NewZealand: return "new_zealand";
        case model::Region::Eu433:     return "eu433";
        case model::Region::Custom:    return "custom";
        case model::Region::Europe:
        default:                       return "europe";
    }
}

bool parse_region(const std::string& value, model::Region& region) {
    const auto normalized = lowercase(trim(value));
    if (normalized == "europe") region = model::Region::Europe;
    else if (normalized == "us_canada" || normalized == "uscanada") region = model::Region::UsCanada;
    else if (normalized == "australia") region = model::Region::Australia;
    else if (normalized == "new_zealand" || normalized == "newzealand") region = model::Region::NewZealand;
    else if (normalized == "eu433" || normalized == "eu_433") region = model::Region::Eu433;
    else if (normalized == "custom") region = model::Region::Custom;
    else return false;
    return true;
}

void set_node_name(model::MeshConfig& mesh, const std::string& value) {
    const auto name = trim(value);
    std::strncpy(mesh.node_name, name.c_str(), model::kNodeNameBuffer - 1);
    mesh.node_name[model::kNodeNameBuffer - 1] = '\0';
}

} // namespace

bool load_application_config(const std::string& path,
                             ApplicationConfig& config,
                             std::string& error) {
    error.clear();
    std::ifstream input(path);
    if (!input) {
        error = "could not open file";
        return false;
    }

    std::string section;
    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '#' || line.front() == ';') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            section = lowercase(trim(line.substr(1, line.size() - 2)));
            continue;
        }

        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const auto key = lowercase(trim(line.substr(0, separator)));
        const auto value = trim(line.substr(separator + 1));

        if (section == "application" && key == "dark_mode") {
            bool parsed = false;
            if (parse_bool(value, parsed)) {
                config.dark_mode = parsed;
            }
            continue;
        }

        if (section != "mesh") {
            continue;
        }

        int parsed = 0;
        if (key == "node_name") {
            set_node_name(config.mesh, value);
        } else if (key == "region") {
            model::Region region = model::Region::Europe;
            if (parse_region(value, region)) {
                apply_region_preset(config.mesh, region);
            }
        } else if (key == "freq" && parse_int(value, parsed)) {
            config.mesh.freq_hz = static_cast<std::uint32_t>(parsed);
        } else if (key == "sf" && parse_int(value, parsed)) {
            config.mesh.sf = parsed;
        } else if (key == "bw_index" && parse_int(value, parsed)) {
            config.mesh.bw_index = parsed;
        } else if (key == "cr" && parse_int(value, parsed)) {
            config.mesh.cr = parsed;
        } else if (key == "power" && parse_int(value, parsed)) {
            config.mesh.tx_power_dbm = parsed;
        } else if (key == "sync" && parse_int(value, parsed)) {
            config.mesh.sync_word = parsed;
        } else if (key == "preamble" && parse_int(value, parsed)) {
            config.mesh.preamble_len = parsed;
        }
    }

    return true;
}

bool save_application_config(const std::string& path,
                             const ApplicationConfig& config,
                             std::string& error) {
    error.clear();

    const std::filesystem::path config_path(path);
    const auto parent = config_path.parent_path();
    if (!parent.empty()) {
        std::error_code filesystem_error;
        std::filesystem::create_directories(parent, filesystem_error);
        if (filesystem_error) {
            error = "could not create parent directory: " + filesystem_error.message();
            return false;
        }
    }

    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        error = "could not open file for writing";
        return false;
    }

    output << "[application]\n";
    output << "dark_mode=" << (config.dark_mode ? "yes" : "no") << "\n\n";

    output << "[mesh]\n";
    output << "node_name=" << config.mesh.node_name << "\n";
    output << "region=" << region_key(config.mesh.region) << "\n";
    output << "freq=" << config.mesh.freq_hz << "\n";
    output << "sf=" << config.mesh.sf << "\n";
    output << "bw_index=" << config.mesh.bw_index << "\n";
    output << "cr=" << config.mesh.cr << "\n";
    output << "power=" << config.mesh.tx_power_dbm << "\n";
    output << "sync=" << config.mesh.sync_word << "\n";
    output << "preamble=" << config.mesh.preamble_len << "\n";

    if (!output) {
        error = "failed while writing file";
        return false;
    }

    return true;
}

} // namespace app
