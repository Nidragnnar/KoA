#include "Engine/Debug/DebugUI.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

namespace ke {

bool DebugUI::iniciar(SDL_Window* janela, SDL_GLContext contexto) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // não espalha imgui.ini pela pasta do jogo
    ImGui::StyleColorsDark();
    ImGui::GetStyle().WindowRounding = 2.0f;
    if (!ImGui_ImplSDL3_InitForOpenGL(janela, contexto)) return false;
    if (!ImGui_ImplOpenGL3_Init("#version 330")) return false;
    iniciado_ = true;
    return true;
}

void DebugUI::encerrar() {
    if (!iniciado_) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    iniciado_ = false;
}

void DebugUI::processar(const SDL_Event& e) {
    if (iniciado_) ImGui_ImplSDL3_ProcessEvent(&e);
}

void DebugUI::novoQuadro() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void DebugUI::renderizar() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool DebugUI::querTeclado() const { return iniciado_ && visivel && ImGui::GetIO().WantCaptureKeyboard; }

}
