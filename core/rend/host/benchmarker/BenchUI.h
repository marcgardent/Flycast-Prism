#pragma once
#include <SDL2/SDL.h>
#include "imgui.h"
#include <vector>
#include <string>

class BenchUI {
public:
    BenchUI() : window(nullptr), renderer(nullptr), fontTexture(nullptr), uiScale(1.0f) {}
    ~BenchUI() { shutdown(); }

    bool init(const char* title, int width, int height) {
        // Detect DPI Scale
        float ddpi, hdpi, vdpi;
        if (SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi) == 0) {
            uiScale = (hdpi / 96.0f) * 2.0f; // Boost x2
            if (uiScale < 2.0f) uiScale = 2.0f;
        } else {
            uiScale = 2.0f;
        }

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); // Linear filtering for AA

        window = SDL_CreateWindow(title, 
                                  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  (int)(width * uiScale), (int)(height * uiScale), 
                                  SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        if (!window) return false;

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) {
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        }
        if (!renderer) return false;

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        ImGui::GetStyle().ScaleAllSizes(uiScale);
        io.FontGlobalScale = uiScale;

        // Create font texture
        unsigned char* pixels;
        int w, h;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
        
        fontTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, w, h);
        SDL_UpdateTexture(fontTexture, nullptr, pixels, w * 4);
        SDL_SetTextureBlendMode(fontTexture, SDL_BLENDMODE_BLEND);
        io.Fonts->SetTexID((ImTextureID)fontTexture);

        return true;
    }

    void shutdown() {
        if (fontTexture) {
            SDL_DestroyTexture(fontTexture);
            fontTexture = nullptr;
        }
        if (renderer) {
            SDL_DestroyRenderer(renderer);
            renderer = nullptr;
        }
        if (window) {
            SDL_DestroyWindow(window);
            window = nullptr;
        }
        if (ImGui::GetCurrentContext()) {
            ImGui::DestroyContext();
        }
    }

    void handleEvent(SDL_Event& event) {
        ImGuiIO& io = ImGui::GetIO();
        if (event.type == SDL_MOUSEMOTION) {
            io.AddMousePosEvent((float)event.motion.x, (float)event.motion.y);
        } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
            int button = -1;
            if (event.button.button == SDL_BUTTON_LEFT) button = 0;
            if (event.button.button == SDL_BUTTON_RIGHT) button = 1;
            if (event.button.button == SDL_BUTTON_MIDDLE) button = 2;
            if (button != -1) io.AddMouseButtonEvent(button, event.type == SDL_MOUSEBUTTONDOWN);
        } else if (event.type == SDL_MOUSEWHEEL) {
            io.AddMouseWheelEvent((float)event.wheel.x, (float)event.wheel.y);
        }
    }

    void addLog(const std::string& log) {
        logs.push_back(log);
        if (logs.size() > 500) logs.erase(logs.begin());
    }

    int render(const std::vector<std::unique_ptr<TestCase>>& allTests, int currentIdx, uint32_t frameCount, uint32_t totalFrames) {
        ImGuiIO& io = ImGui::GetIO();
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        io.DisplaySize = ImVec2((float)w, (float)h);
        io.DeltaTime = 1.0f / 60.0f;
        ImGui::NewFrame();

        float padding = 10.0f * uiScale;
        float infoHeight = 150.0f * uiScale; // Increased for combo

        // UI Definition
        ImGui::SetNextWindowPos(ImVec2(padding, padding), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2((float)w - 2 * padding, infoHeight), ImGuiCond_Always);
        ImGui::Begin("Control Panel", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
        
        // Test Selection Combo
        int nextIdx = currentIdx;
        if (ImGui::BeginCombo("Select Test", allTests[currentIdx]->getId().c_str())) {
            for (int i = 0; i < (int)allTests.size(); i++) {
                bool isSelected = (i == currentIdx);
                if (ImGui::Selectable(allTests[i]->getId().c_str(), isSelected)) {
                    nextIdx = i;
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Name: %s", allTests[currentIdx]->getName().c_str());
        ImGui::TextWrapped("Desc: %s", allTests[currentIdx]->getDescription().c_str());
        ImGui::Text("Frames: %u / %u", frameCount, totalFrames);
        ImGui::SameLine(ImGui::GetWindowWidth() - 80 * uiScale);
        if (ImGui::Button("Quit", ImVec2(70 * uiScale, 0))) {
            nextIdx = -1; // Special signal to quit
        }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(padding, infoHeight + 2 * padding), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2((float)w - 2 * padding, (float)h - (infoHeight + 3 * padding)), ImGuiCond_Always);
        ImGui::Begin("Logs", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
        for (const auto& log : logs) {
            ImGui::TextUnformatted(log.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        ImGui::End();

        ImGui::Render();

        // Render Draw Data using SDL_Renderer
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);

        ImDrawData* draw_data = ImGui::GetDrawData();
        for (int n = 0; n < draw_data->CmdListsCount; n++) {
            const ImDrawList* cmd_list = draw_data->CmdLists[n];
            const ImDrawVert* vtx_buffer = cmd_list->VtxBuffer.Data;
            const ImDrawIdx* idx_buffer = cmd_list->IdxBuffer.Data;

            for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
                const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
                if (pcmd->UserCallback) {
                    pcmd->UserCallback(cmd_list, pcmd);
                } else {
                    SDL_Rect r = { (int)pcmd->ClipRect.x, (int)pcmd->ClipRect.y, (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y) };
                    SDL_RenderSetClipRect(renderer, &r);

                    // Convert ImGui vertices to SDL_Vertex
                    std::vector<SDL_Vertex> sdl_vertices(pcmd->ElemCount);
                    std::vector<int> sdl_indices(pcmd->ElemCount);

                    for (unsigned int i = 0; i < pcmd->ElemCount; i++) {
                        ImDrawIdx idx = idx_buffer[pcmd->IdxOffset + i];
                        const ImDrawVert& v = vtx_buffer[idx];
                        SDL_Vertex& sv = sdl_vertices[i];
                        sv.position.x = v.pos.x;
                        sv.position.y = v.pos.y;
                        sv.tex_coord.x = v.uv.x;
                        sv.tex_coord.y = v.uv.y;
                        sv.color.r = (v.col >> 0) & 0xFF;
                        sv.color.g = (v.col >> 8) & 0xFF;
                        sv.color.b = (v.col >> 16) & 0xFF;
                        sv.color.a = (v.col >> 24) & 0xFF;
                        sdl_indices[i] = (int)i;
                    }

                    SDL_RenderGeometry(renderer, (SDL_Texture*)pcmd->GetTexID(), sdl_vertices.data(), (int)sdl_vertices.size(), sdl_indices.data(), (int)sdl_indices.size());
                }
            }
        }
        SDL_RenderSetClipRect(renderer, nullptr);
        SDL_RenderPresent(renderer);

        return nextIdx;
    }

private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* fontTexture;
    std::vector<std::string> logs;
    float uiScale;
};
