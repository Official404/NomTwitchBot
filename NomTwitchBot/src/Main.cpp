#include "Main.h"
#include <windows.h>
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <sstream>
#include <d3d11.h>
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"

#include "SubscriptionFunctions/ChannelPointRewardRedemption.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGISwapChain* g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;
void CreateRenderTarget() {
	ID3D11Texture2D* pBackBuffer;
	g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
	pBackBuffer->Release();
}
void CleanupRenderTarget() {
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}
void CleanupDeviceD3D() {
	CleanupRenderTarget();
	if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}
bool CreateDeviceD3D(HWND hWnd) {
	// Setup swap chain
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2,
		D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
		return false;
	CreateRenderTarget();
	return true;
}
HWND hwnd;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();
	// Create application window
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"NomTwitchBot", NULL };
	RegisterClassEx(&wc);
	hwnd = CreateWindowW(wc.lpszClassName, L"NomTwitchBot", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

	if (hwnd == NULL) {
		NomBotCore::ImGuiLogManager::AddLog("Window", "Failed to create window!", NomBotCore::LogSeverity::Error);
		return 1;
	}
	ShowWindow(hwnd, SW_SHOWMAXIMIZED);
	UpdateWindow(hwnd);

	if (!CreateDeviceD3D(hwnd)) {
		NomBotCore::ImGuiLogManager::AddLog("Window", "Failed to create Direct3D device!", NomBotCore::LogSeverity::Error);
		CleanupDeviceD3D();
		return 1;
	}
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

	bool IsTwitchAPIEnabled = false;
	NomBotCore::BotCore botCore;
	MSG msg;
	while (true) {
		while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				break;
		}
		// Start ImGui frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("NomTwitchBot Control");
		if (ImGui::Button(IsTwitchAPIEnabled ? "Disable Twitch API" : "Enable Twitch API")) {
			if (IsTwitchAPIEnabled) {
				botCore.StopTwitchAPI();
				IsTwitchAPIEnabled = false;
			}
			else {
				botCore.StartTwitchAPI();
				IsTwitchAPIEnabled = true;
			}
		}
		ImGui::Text("Twitch API is %s", IsTwitchAPIEnabled ? "Enabled" : "Disabled");
		if (ImGui::Button("Quit")) {
			PostQuitMessage(0);
		}
		ImGui::End();
		if (IsTwitchAPIEnabled) {
			ImGui::Begin("Twitch Api Settings");
			ImGui::Text("WebSocket");
			ImGui::SameLine();
			if (botCore.IsWebSocketEnabled()) {
				if (ImGui::Button("Disable WebSocket")) {
					// This will be handled in the BotCore class
					botCore.EnableWebSocket(false);
				}
			}
			else {
				if (ImGui::Button("Enable WebSocket")) {
					// This will be handled in the BotCore class
					botCore.EnableWebSocket(true);
				}
			}
			ImGui::BeginChild("SubsciptionSettings");
			ImGui::Text("Event Subscriptions");
			ImGui::Separator();
			ImGui::Text("Channel point redemption:");
			ImGui::SameLine();
			if (botCore.IsSubscribedToEvent(NomBotCore::TwitchAPI::SubscriptionType::ChannelPointsCustomRewardRedemptionAdd)) {
				if (ImGui::Button("Unsubscribe")) {
					botCore.UnsubscribeFromEvent(NomBotCore::TwitchAPI::SubscriptionType::ChannelPointsCustomRewardRedemptionAdd);
				}
				ImGui::SameLine();
				ImGui::Text("Subscribed");
			}
			else {
				if (ImGui::Button("Subscribe")) {
					botCore.SubscrubeToEvent(
						NomBotCore::TwitchAPI::SubscriptionType::ChannelPointsCustomRewardRedemptionAdd,
						[](const NomBotCore::ChannelPointRewardRedemption& redemption) {
							NomTwitchBot::ChannelPointRewardRedemption::ProcessRedemption(redemption);
						}
					);
				}
				ImGui::SameLine();
				ImGui::Text("Not Subscribed");
			}
			ImGui::EndChild();
			ImGui::End();
		}
		ImGui::Begin("Application Logs:");
		for (const auto& logName : NomBotCore::ImGuiLogManager::GetLogNames()) {
			if (ImGui::CollapsingHeader(logName.c_str())) {
				const auto& entries = NomBotCore::ImGuiLogManager::GetLog(logName);
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
				ImGui::BeginChild(logName.c_str(), ImVec2(0, 300), false, ImGuiWindowFlags_HorizontalScrollbar);
				for (const auto& entry : entries) {
					ImVec4 color;
					switch (entry.severity) {
					case NomBotCore::LogSeverity::Info:    color = ImVec4(1, 1, 1, 1); break; // White
					case NomBotCore::LogSeverity::Warning: color = ImVec4(1, 1, 0, 1); break; // Yellow
					case NomBotCore::LogSeverity::Error:   color = ImVec4(1, 0, 0, 1); break; // Red
					}
					ImGui::PushStyleColor(ImGuiCol_Text, color);
					ImGui::TextUnformatted(entry.message.c_str());
					ImGui::PopStyleColor();
				}
				if (NomBotCore::ImGuiLogManager::GetScrollToBottom()) {
					ImGui::SetScrollHereY(1.0f);
					NomBotCore::ImGuiLogManager::AddLog("NomBot", "Rendered log: " + logName, NomBotCore::LogSeverity::Info);
					NomBotCore::ImGuiLogManager::AddLog("NomBot", "m_ScrollToBottom value: " + NomBotCore::ImGuiLogManager::GetScrollToBottom() ? "true" : "false", NomBotCore::LogSeverity::Info);
					NomBotCore::ImGuiLogManager::AddLog("NomBot", "Auto-scrolling to bottom of log.", NomBotCore::LogSeverity::Info);
					NomBotCore::ImGuiLogManager::SetScrollToBottom(false);
				}
				ImGui::EndChild();
				ImGui::PopStyleColor();
			}
			ImGui::Separator();
		}
		ImGui::End();

		// Rendering
		ImGui::Render();
		const float clear_color[4] = { 0.45f, 0.55f, 0.60f, 1.00f };
		g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
		g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		g_pSwapChain->Present(1, 0); // Present with vsync
		//g_pSwapChain->Present(0, 0); // Present without vsync
		if (msg.message == WM_QUIT)
			break;
	}
	// Cleanup
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	CleanupDeviceD3D();
	DestroyWindow(hwnd);
	UnregisterClass(wc.lpszClassName, wc.hInstance);
	NomBotCore::ImGuiLogManager::AddLog("NomBot", "Application exiting, dumping logs to file.", NomBotCore::LogSeverity::Info);
	NomBotCore::ImGuiLogManager::DumpAllLogsToFile("Logs");
	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}