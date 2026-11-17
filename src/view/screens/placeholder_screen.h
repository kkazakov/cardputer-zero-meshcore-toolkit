/*
 * SPDX-FileCopyrightText: 2026 Meshcore Toolkit contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"

#include <string>

namespace screen {

// Simple placeholder page used by the "Test 1" and "Test 2" carousel entries.
class PlaceholderScreen : public BaseScreen {
public:
    PlaceholderScreen(viewmodel::BaseViewModel& view_model,
                      app::AssetManager& assets,
                      std::string title);

private:
    void build_content(lv_obj_t* content) override;

    std::string title_;
};

} // namespace screen
