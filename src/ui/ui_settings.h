#pragma once
#include "studio_preferences.h"
#include "i18n.h"
#include "ui_modal_transition.h"
#include <string>

struct StudioState;
namespace UiSettings {
struct State {
    bool requested = false;
    bool open = false;
    int category = 0;
    StudioPreferences::Preferences draft;
    i18n::Language language = i18n::Language::English;
    Ui::ModalTransition transition;
    std::string error;
    // Empty uses the normal config; headless regressions supply an isolated file.
    std::string configPath;
};
void RenderMenuBar(State& state);
void RenderDialog(State& state, StudioState& studio);
} // namespace UiSettings
