#ifndef __TWITCHAPI_H__
#define __TWITCHAPI_H__

#include "../Networking/NomSocketManager.h"
#include "../Networking/NomWebSocket.h"
#include "../TwitchAPI/ChannelPointRewardRedemption.h"
#include <atomic>
#include <thread>
#include <memory>
#include <map>
#include <functional>
#include <chrono>

namespace NomBotCore {
	class TwitchAPI {
	public:

		std::string SubscriptionTypeToString(int type) {
			switch (type) {
			case SubscriptionType::ChannelPointsCustomRewardRedemptionAdd:
				return "channel.channel_points_custom_reward_redemption.add";
			case SubscriptionType::AutomodMessageHold:
				return "automod.message.hold";
			default:
				return "unknown";
			}
		}

		enum SubscriptionType {
			ChannelPointsCustomRewardRedemptionAdd,
			// This exsist so that when websocket connects it can subscribe to this so the session dosent end, it will never be used otherwise
			AutomodMessageHold
		};

		struct EventSubSubscription {
			std::string id;
			std::string status;
			std::string type;
			std::string version;
			std::string condition;
			std::string created_at;
			std::function<void(const ChannelPointRewardRedemption)> callback;
			bool Subscibed = false;

			EventSubSubscription() : id(""), status(""), type(""), version("1"), condition(""), created_at(""), callback(nullptr), Subscibed(false) {}
		};

		TwitchAPI();
		~TwitchAPI();

		void Run(std::atomic<bool>& running);
		void WebSocketThreadFunc(std::atomic<bool>& running);

		int Initialize();
		int Authenticate();
		int GetAccessToken();
		int RefreshAccessToken();
		int EnableWebSocket(bool enable);
		int IsWebSocketEnabled() const { return m_IsWebSocketEnabled; }
		int ConnectWebSocket();
		int CheckIfRefreshNeeded();
		int AddEventSubSubscription(SubscriptionType type, std::function<void(const ChannelPointRewardRedemption&)> callback = nullptr);
		int RemoveEventSubSubscription(SubscriptionType type);
		int SubscribetoUnSubscribedEvents();
		int IsSubscribedToEvent(SubscriptionType type);
	private:
		NomSocketManager* m_SocketManager = nullptr;
		NomWebSocket* m_WebSocket = nullptr;
		char* m_ClientID = nullptr;
		char* m_ClientSecret = nullptr;
		std::atomic<bool>* m_WebSocketRunning;
		std::vector<std::thread> internalThreads;
		void StartInternalThread();
		bool m_IsWebSocketEnabled = false;
		char* m_ChannelID = nullptr;

		std::map<std::string, EventSubSubscription*> m_Subscriptions;
	protected:
		char* m_AuthCode;
		char* m_AccessToken = nullptr;
		char* m_RefreshToken;
		int m_ExpiresIn;
		std::vector<char*> m_Scopes;
		char* m_TokenType;
		char* m_WebSocketSessionID = nullptr;
		char* m_ScopesString = nullptr;
	};
}

#endif