#pragma once
#include "app_update.h"
#include "ui_markdown.h"
#include "ui_modal_transition.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
struct StudioState;
namespace UiAppUpdate {
struct State {
    AppUpdate::Client client;
    UiMarkdown::Document markdown;
    bool markdownReady = true;
    bool requested = false;
    bool open = false;
    Ui::ModalTransition transition;
    std::string installError;
};
void RenderMenuBar(State&);
void RenderDialog(State&, StudioState&, HWND);
}
