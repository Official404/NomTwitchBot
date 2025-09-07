#ifndef __CORE_H__
#define __CORE_H__
#include "../TwitchAPI/TwitchAPI.h"
#include "../TwitchAPI/ChannelPointRewardRedemption.h"
#include <atomic>
#include <thread>
#include <memory>

namespace NomBotCore {
	class BotCore {
	public:
		BotCore();
		~BotCore();

		void StartTwitchAPI();
		void StopTwitchAPI();
		bool IsWebSocketEnabled() const { return m_TwitchAPI->IsWebSocketEnabled(); }
		void EnableWebSocket(bool enable);
		void SubscrubeToEvent(TwitchAPI::SubscriptionType type, std::function<void(const ChannelPointRewardRedemption&)> callback = nullptr);
		void UnsubscribeFromEvent(TwitchAPI::SubscriptionType type);
		bool IsSubscribedToEvent(TwitchAPI::SubscriptionType type);
	private:
		std::unique_ptr<TwitchAPI> m_TwitchAPI;
		std::thread* m_TwitchAPIThread = nullptr;
		std::atomic<bool> m_TwitchAPIRunning{false};
	};
}

#endif