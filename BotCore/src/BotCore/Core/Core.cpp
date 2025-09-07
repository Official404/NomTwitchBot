#include "nompch.h"
#include "Core.h"
#include "Logging/ImGuiLog.h"

namespace NomBotCore {
	BotCore::BotCore() 
	{
		m_TwitchAPI = std::make_unique<TwitchAPI>();
	}

	BotCore::~BotCore() 
	{
		StopTwitchAPI();
		m_TwitchAPI.reset();
	}

	void BotCore::StartTwitchAPI()
	{
		if (m_TwitchAPIThread && m_TwitchAPIThread->joinable()) {
			ImGuiLogManager::AddLog("BotCore", "TwitchAPI thread is already running!", LogSeverity::Warning);
			return;
		}
		m_TwitchAPIRunning = true;
		m_TwitchAPIThread = new std::thread([this]() {
			if (m_TwitchAPI) {
				m_TwitchAPI->Run(m_TwitchAPIRunning);
			}
		});
		ImGuiLogManager::AddLog("BotCore", "TwitchAPI thread started.", LogSeverity::Info);
	}

	void BotCore::StopTwitchAPI()
	{
		if (!m_TwitchAPIRunning) {
			ImGuiLogManager::AddLog("BotCore", "TwitchAPI thread is not running!", LogSeverity::Warning);
			return;
		}
		m_TwitchAPIRunning = false;
		if (m_TwitchAPIThread && m_TwitchAPIThread->joinable()) {
			m_TwitchAPIThread->join();
			delete m_TwitchAPIThread;
			m_TwitchAPIThread = nullptr;
		}
		ImGuiLogManager::AddLog("BotCore", "TwitchAPI thread stopped.", LogSeverity::Info);
	}

	void BotCore::EnableWebSocket(bool enable)
	{
		if (m_TwitchAPI) {
			m_TwitchAPI->EnableWebSocket(enable);
		}
		else {
			ImGuiLogManager::AddLog("BotCore", "TwitchAPI instance is null. Cannot enable/disable WebSocket.", LogSeverity::Error);
		}
	}

	void BotCore::SubscrubeToEvent(TwitchAPI::SubscriptionType type, std::function<void(const ChannelPointRewardRedemption&)> callback)
	{
		if (m_TwitchAPI) {
			int result = m_TwitchAPI->AddEventSubSubscription(type, callback);
			if (result != 0) {
				ImGuiLogManager::AddLog("BotCore", "Failed to add subscription to event type: " + std::to_string(type), LogSeverity::Error);
			}
			else {
				ImGuiLogManager::AddLog("BotCore", "Added Subscription to event type: " + std::to_string(type), LogSeverity::Info);
			}
		}
	}

	void BotCore::UnsubscribeFromEvent(TwitchAPI::SubscriptionType type)
	{
		if (m_TwitchAPI) {
			int result = m_TwitchAPI->RemoveEventSubSubscription(type);
			if (result != 0) {
				ImGuiLogManager::AddLog("BotCore", "Failed to remove subscription from event type: " + std::to_string(type), LogSeverity::Error);
			}
			else {
				ImGuiLogManager::AddLog("BotCore", "Removed Subscription from event type: " + std::to_string(type), LogSeverity::Info);
			}
		}
	}

	bool BotCore::IsSubscribedToEvent(TwitchAPI::SubscriptionType type)
	{
		if (m_TwitchAPI) {
			int result = m_TwitchAPI->IsSubscribedToEvent(type);
			if (result == 1) {
				return true;
			}
			else if (result == 0) {
				return false;
			}
			else {
				ImGuiLogManager::AddLog("BotCore", "Failed to check subscription status for event type: " + std::to_string(type), LogSeverity::Error);
				return false;
			}
		}
		return false;
	}
}