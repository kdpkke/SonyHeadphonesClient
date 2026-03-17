// SDL_Renderer backend from https://github.com/ocornut/imgui/blob/master/examples/example_sdl3_sdlrenderer3
#include <cstdio>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_main.h>

#include "Platform/Platform.hpp"
#ifdef _WIN32
#include "Platform/Windows/ImmersiveColor.h"
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "Fonts/PlexSansIcon.h"
#include <algorithm>
// Implemented by Client.cpp
extern bool clientShouldExit();

bool gShouldClose = false;

SDL_Window* gWindow = nullptr;
SDL_Renderer* gRenderer = nullptr;

#ifdef _WIN32

RTL_OSVERSIONINFOW g_windowsVersionInfo;
bool g_micaSupported;

typedef LONG NTSTATUS, *PNTSTATUS;
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS (0x00000000)
#endif

typedef NTSTATUS (WINAPI *RtlGetVersion_t)(PRTL_OSVERSIONINFOW);

// https://stackoverflow.com/questions/36543301/detecting-windows-10-version/36543774#36543774
BOOL GetOSVersion(PRTL_OSVERSIONINFOW lpRovi)
{
    HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
    if (hMod)
    {
        RtlGetVersion_t fxPtr = (RtlGetVersion_t)GetProcAddress(hMod, "RtlGetVersion");
        if (fxPtr)
        {
            lpRovi->dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
            if (STATUS_SUCCESS == fxPtr(lpRovi))
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

typedef bool (WINAPI *ShouldAppsUseDarkMode_t)(); // 132
bool ShouldAppsUseDarkMode()
{
    static ShouldAppsUseDarkMode_t fn = (ShouldAppsUseDarkMode_t)-1;
    if (fn == (ShouldAppsUseDarkMode_t)-1)
    {
        HMODULE h = LoadLibraryW(L"uxtheme.dll");
        if (h)
        {
            fn = (ShouldAppsUseDarkMode_t)GetProcAddress(h, MAKEINTRESOURCEA(132));
        }
    }
    return fn ? fn() : false /* Pre-1809 behavior */;
}

void CheckMicaSupported()
{
    g_micaSupported = IsCompositionActive() && g_windowsVersionInfo.dwBuildNumber >= 22523;
}

static ImVec4 HueSatShift(const ImVec4& col, float deltaH, float deltaS)
{
    float h, s, v;
    ImGui::ColorConvertRGBtoHSV(col.x, col.y, col.z, h, s, v);
    h = fmodf(h + deltaH, 1.0f);
    s = std::clamp(s + deltaS, 0.0f, 1.0f);
    float r, g, b;
    ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);
    return ImVec4(r, g, b, col.w);
}

static void ApplyAccentColorsToImGuiStyle(bool dark)
{
    // Accent colors are only supported on Windows 8+
    if (g_windowsVersionInfo.dwBuildNumber < 9200)
        return;

    ImVec4 accentColor = ImColor(CImmersiveColor::GetColor(IMCLR_SystemAccent));

    // Convert accent color to hue for reference
    float accentH, accentS, accentV;
    ImGui::ColorConvertRGBtoHSV(accentColor.x, accentColor.y, accentColor.z, accentH, accentS, accentV);

    ImGuiStyle& style = ImGui::GetStyle();

    // Get base color hue
    float baseH, baseS, baseV;
    ImGui::ColorConvertRGBtoHSV(
        style.Colors[ImGuiCol_TextLink].x,
        style.Colors[ImGuiCol_TextLink].y,
        style.Colors[ImGuiCol_TextLink].z,
        baseH, baseS, baseV);

    // Compute hue delta to match system accent hue
    float deltaH = accentH - baseH;
    if (deltaH > 0.5f) deltaH -= 1.0f;
    else if (deltaH < -0.5f) deltaH += 1.0f;

    float deltaS = accentS - baseS;

    // Apply hue shift to relevant colors
    if (dark)
    {
        static constexpr ImGuiCol kColorsToShiftDark[] = {
            ImGuiCol_FrameBg,
            ImGuiCol_FrameBgHovered,
            ImGuiCol_FrameBgActive,
            ImGuiCol_TitleBgActive,
            ImGuiCol_CheckMark,
            ImGuiCol_SliderGrab,
            ImGuiCol_SliderGrabActive,
            ImGuiCol_Button,
            ImGuiCol_ButtonHovered,
            ImGuiCol_ButtonActive,
            ImGuiCol_Header,
            ImGuiCol_HeaderHovered,
            ImGuiCol_HeaderActive,
            ImGuiCol_SeparatorHovered,
            ImGuiCol_SeparatorActive,
            ImGuiCol_ResizeGrip,
            ImGuiCol_ResizeGripHovered,
            ImGuiCol_ResizeGripActive,
            ImGuiCol_TabHovered,
            ImGuiCol_Tab,
            ImGuiCol_TabSelected,
            ImGuiCol_TabSelectedOverline,
            ImGuiCol_TabDimmed,
            ImGuiCol_TabDimmedSelected,
            ImGuiCol_TextLink,
        };
        for (auto col : kColorsToShiftDark)
        {
            style.Colors[col] = HueSatShift(style.Colors[col], deltaH, deltaS);
        }
    }
    else
    {
        static constexpr ImGuiCol kColorsToShiftLight[] = {
            ImGuiCol_FrameBgHovered,
            ImGuiCol_FrameBgActive,
            ImGuiCol_TitleBgActive,
            ImGuiCol_CheckMark,
            ImGuiCol_SliderGrab,
            ImGuiCol_SliderGrabActive,
            ImGuiCol_Button,
            ImGuiCol_ButtonHovered,
            ImGuiCol_ButtonActive,
            ImGuiCol_Header,
            ImGuiCol_HeaderHovered,
            ImGuiCol_HeaderActive,
            ImGuiCol_SeparatorHovered,
            ImGuiCol_SeparatorActive,
            ImGuiCol_ResizeGrip,
            ImGuiCol_ResizeGripHovered,
            ImGuiCol_ResizeGripActive,
            ImGuiCol_TabHovered,
            ImGuiCol_Tab,
            ImGuiCol_TabSelected,
            ImGuiCol_TabSelectedOverline,
            ImGuiCol_TabDimmed,
            ImGuiCol_TabDimmedSelected,
            ImGuiCol_TextLink,
        };
        for (auto col : kColorsToShiftLight)
        {
            style.Colors[col] = HueSatShift(style.Colors[col], deltaH, deltaS);
        }
    }
}

void UpdateWindowDwmAttributes(HWND hwnd)
{
    BOOL dark = ShouldAppsUseDarkMode();

    if (IsCompositionActive())
    {
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

        if (g_micaSupported)
        {
            DWM_SYSTEMBACKDROP_TYPE backdrop = DWMSBT_MAINWINDOW;
            DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
        }
    }

    if (dark)
    {
        ImGui::StyleColorsDark();
    }
    else
    {
        ImGui::StyleColorsLight();
    }
    ApplyAccentColorsToImGuiStyle(dark);
}

static bool WindowsMessageHook(void* userdata, MSG* msg)
{
    switch (msg->message)
    {
    case WM_SETTINGCHANGE:
        {
            if (msg->lParam && wcscmp((LPCWSTR)msg->lParam, L"ImmersiveColorSet") == 0)
            {
                UpdateWindowDwmAttributes(msg->hwnd);
            }
            break;
        }
    case WM_DWMCOMPOSITIONCHANGED:
        {
            CheckMicaSupported();
            break;
        }
    }
    return true; // let SDL continue processing
}

#endif

void mainLoop()
{
    ImGuiIO& io = ImGui::GetIO();
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT)
            gShouldClose = true;
        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(gWindow))
            gShouldClose = true;
    }
    if (SDL_GetWindowFlags(gWindow) & SDL_WINDOW_MINIMIZED)
    {
        SDL_Delay(10);
        return;
    }
    // Start the Dear ImGui frame
    {
        // Platform font loading - if available
        // This is only done once per session. See @ref clientPlatformLocateFontBinary for more info.
        static int platformFontSize = 0;
        if (!platformFontSize)
        {
            const char* fontData = nullptr;
            platformFontSize = clientPlatformLocateFontBinary(&fontData);
            if (platformFontSize)
            {
                SDL_Log("Loading platform font of size %d bytes", platformFontSize);
                ImFontConfig merge_config{};
                merge_config.MergeMode = true;
                // XXX: PlexSansIcon covered latin-1 pages. New ones won't overwrite them.
                // External fonts are meant to cover missing glyphs e.g. CJK ones anyway - so this is fine.
                io.Fonts->AddFontFromMemoryTTF((void*)fontData, platformFontSize, 15.0f, &merge_config);
            }
        }
        // New frame
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }    
    gShouldClose |= clientShouldExit();
    // Rendering
    {
        ImGui::Render();
        SDL_SetRenderScale(gRenderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 0);
        SDL_RenderClear(gRenderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), gRenderer);
        SDL_RenderPresent(gRenderer);
    }
#ifdef EMSCRIPTEN
    if (gShouldClose)
        emscripten_cancel_main_loop();
#endif
}

int main(int, char**)
{
    clientPlatformInit();
#ifdef _WIN32
    GetOSVersion(&g_windowsVersionInfo);
#endif
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    gWindow = SDL_CreateWindow(
        "SonyHeadphonesClient",
        800, 600,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
#ifdef _WIN32
        | SDL_WINDOW_TRANSPARENT
#endif
    );
    if (!gWindow)
    {
        SDL_Log("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return 1;
    }
    gRenderer = SDL_CreateRenderer(gWindow, nullptr);
    if (!gRenderer)
    {
        SDL_Log("Error: SDL_CreateRenderer()\n");
        return 1;
    }
    // Setup Dear ImGui context
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
    }
#ifdef _WIN32
    {
        HWND hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(gWindow), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        if (hwnd)
        {
            CheckMicaSupported();
            UpdateWindowDwmAttributes(hwnd);
        }
        SDL_SetWindowsMessageHook(WindowsMessageHook, nullptr);
    }
#endif
    ImGuiIO& io = ImGui::GetIO();
    // Setup Default Dear ImGui styles
#ifndef _WIN32
    ImGui::StyleColorsDark();
#endif
    auto& style = ImGui::GetStyle();
    style.FrameRounding = 8.0f;
    style.CircleTessellationMaxError = 0.01f;
    style.FramePadding = ImVec2(8.0f, 8.0f);
    // Setup Platform/Renderer backends
    {
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
        io.ConfigErrorRecoveryEnableAssert = true; // Don't assert on errors
        ImGui_ImplSDL3_InitForSDLRenderer(gWindow, gRenderer);
        ImGui_ImplSDLRenderer3_Init(gRenderer);
    }
    // Load our default font
    {
        io.Fonts->Clear();
        io.Fonts->AddFontFromMemoryCompressedBase85TTF(kEmbedFontPlexSansIcon, 15.0f);
    }
    // Main loop

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, 0, 1);
#else
    while (!gShouldClose)
        mainLoop();
#endif

    // Cleanup
    {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        SDL_DestroyRenderer(gRenderer);
        SDL_DestroyWindow(gWindow);
        SDL_Quit();

        clientPlatformDestroy();
    }
    return 0;
}
