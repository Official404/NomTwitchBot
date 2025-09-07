#include "nompch.h"
#include "TwitchAPI.h"
#include "../Core/Logging/ImGuiLog.h"
#include "../Core/JSONParser/JsonParser.h"
#include "ChannelPointRewardRedemption.h"

namespace NomBotCore {
	TwitchAPI::TwitchAPI()
	{
		m_ScopesString = "chat:read+moderator:manage:automod+channel:read:redemptions";
		AddEventSubSubscription(SubscriptionType::AutomodMessageHold, nullptr);
	}

	TwitchAPI::~TwitchAPI()
	{
		if (m_SocketManager) {
			m_SocketManager->CloseAllSockets();
			delete m_SocketManager;
			m_SocketManager = nullptr;
		}
		if (m_WebSocket) {
			delete m_WebSocket;
			m_WebSocket = nullptr;
		}
		if (m_ClientID) {
			delete[] m_ClientID;
			m_ClientID = nullptr;
		}
		if (m_ClientSecret) {
			delete[] m_ClientSecret;
			m_ClientSecret = nullptr;
		}
	}

	void TwitchAPI::Run(std::atomic<bool>& running)
	{
		Initialize();
		Authenticate();
		while (running) {
			CheckIfRefreshNeeded();
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		m_IsWebSocketEnabled = false;

		for (auto& t : internalThreads) {
			if (t.joinable()) {
				t.join();
			}
		}
	}

	void TwitchAPI::WebSocketThreadFunc(std::atomic<bool>& running)
	{
		if (m_AccessToken) {
			std::string authHeader = std::string("Bearer ") + m_AccessToken;
			ImGuiLogManager::AddLog("TwitchAPI", "Setting WebSocket Authorization header: " + authHeader, LogSeverity::Info);
			m_WebSocket->SetHandshakeHeader("Authorization", authHeader.c_str());
		}
		else {
			ImGuiLogManager::AddLog("TwitchAPI", "Access token is not available. Cannot set Authorization header for WebSocket.", LogSeverity::Error);
		}
		int result = m_WebSocket->ConnectWebSocket("eventsub.wss.twitch.tv", 443, true);
		if (result < 0) {
			m_IsWebSocketEnabled = false;
			return;
		}
		while (running && m_IsWebSocketEnabled) {
			const char* message = m_WebSocket->ReceiveWebSocketFrame(true);
			if (message) {
				ImGuiLogManager::AddLog("WebSocket", std::string("Received WebSocket message: ") + message, LogSeverity::Info);
				size_t pos = 0;
				std::shared_ptr<JsonValue> json = JsonParser::Parse(message, pos);
				if (json) {
					json->DumpToLog();
					if (json->type == JsonValue::Type::Object) {
						auto metadatait = json->objectValues.find("metadata");
						if (metadatait != json->objectValues.end() && metadatait->second->type == JsonValue::Type::Object) {
							auto msgIt = metadatait->second->objectValues.find("message_type");
							if (msgIt != metadatait->second->objectValues.end() && msgIt->second->type == JsonValue::Type::String) {
								std::string messageType = msgIt->second->stringValue;
								ImGuiLogManager::AddLog("TwitchAPI", "WebSocket message type: " + messageType, LogSeverity::Info);
								if (messageType == "session_welcome") {
									ImGuiLogManager::AddLog("TwitchAPI", "WebSocket session welcome received.", LogSeverity::Info);
									auto payloadit = json->objectValues.find("payload");
									if (payloadit != json->objectValues.end() && payloadit->second->type == JsonValue::Type::Object) {
										auto sessionIt = payloadit->second->objectValues.find("session");
										if (sessionIt != payloadit->second->objectValues.end() && sessionIt->second->type == JsonValue::Type::Object) {
											auto idIt = sessionIt->second->objectValues.find("id");
											if (idIt != sessionIt->second->objectValues.end() && idIt->second->type == JsonValue::Type::String) {
												m_WebSocketSessionID = new char[idIt->second->stringValue.length() + 1];
												strcpy(m_WebSocketSessionID, idIt->second->stringValue.c_str());
												ImGuiLogManager::AddLog("TwitchAPI", "WebSocket session ID: " + std::string(m_WebSocketSessionID), LogSeverity::Info);
											}
											else {
												ImGuiLogManager::AddLog("TwitchAPI", "Session ID field missing or not a string in WebSocket welcome message.", LogSeverity::Error);
											}
										}
										else {
											ImGuiLogManager::AddLog("TwitchAPI", "Session field missing or not an object in WebSocket welcome message.", LogSeverity::Error);
										}
									}
									else {
										ImGuiLogManager::AddLog("TwitchAPI", "Payload field missing or not an object in WebSocket message.", LogSeverity::Error);
									}
									// Subscribes to the AutomodMessageHold to keep the session alive
									SubscribetoUnSubscribedEvents();
								}
								else if (messageType == "notification") {
									// Handle notification
									ImGuiLogManager::AddLog("TwitchAPI", "WebSocket notification received.", LogSeverity::Info);
									auto payloadit = json->objectValues.find("payload");
									if (payloadit != json->objectValues.end() && payloadit->second->type == JsonValue::Type::Object) {
										auto eventIt = payloadit->second->objectValues.find("event");
										if (eventIt != payloadit->second->objectValues.end() && eventIt->second->type == JsonValue::Type::Object) {
											auto typeIt = payloadit->second->objectValues.find("subscription");
											if (typeIt != payloadit->second->objectValues.end() && typeIt->second->type == JsonValue::Type::Object) {
												auto typeFieldIt = typeIt->second->objectValues.find("type");
												if (typeFieldIt != typeIt->second->objectValues.end() && typeFieldIt->second->type == JsonValue::Type::String) {
													std::string eventType = typeFieldIt->second->stringValue;
													ImGuiLogManager::AddLog("TwitchAPI", "Event type: " + eventType, LogSeverity::Info);
													auto subIt = m_Subscriptions.find(eventType);
													if (subIt != m_Subscriptions.end() && subIt->second->callback) {
														// Call the registered callback with the event data as a JSON string
														auto eventData = eventIt->second;
														ChannelPointRewardRedemption redemption;
														if (eventType == "channel.channel_points_custom_reward_redemption.add") {
															auto obj = eventData->objectValues;
															auto idIt = obj.find("id");
															if (idIt != obj.end() && idIt->second->type == JsonValue::Type::String) {
																redemption.id = idIt->second->stringValue;
															}
															auto userIdIt = obj.find("user_id");
															if (userIdIt != obj.end() && userIdIt->second->type == JsonValue::Type::String) {
																redemption.user_id = userIdIt->second->stringValue;
															}
															auto userLoginIt = obj.find("user_login");
															if (userLoginIt != obj.end() && userLoginIt->second->type == JsonValue::Type::String) {
																redemption.user_login = userLoginIt->second->stringValue;
															}
															auto userNameIt = obj.find("user_name");
															if (userNameIt != obj.end() && userNameIt->second->type == JsonValue::Type::String) {
																redemption.user_name = userNameIt->second->stringValue;
															}
															auto channelIdIt = obj.find("broadcaster_user_id");
															if (channelIdIt != obj.end() && channelIdIt->second->type == JsonValue::Type::String) {
																redemption.channel_id = channelIdIt->second->stringValue;
															}
															auto channelLoginIt = obj.find("broadcaster_user_login");
															if (channelLoginIt != obj.end() && channelLoginIt->second->type == JsonValue::Type::String) {
																redemption.channel_login = channelLoginIt->second->stringValue;
															}
															auto channelNameIt = obj.find("broadcaster_user_name");
															if (channelNameIt != obj.end() && channelNameIt->second->type == JsonValue::Type::String) {
																redemption.channel_name = channelNameIt->second->stringValue;
															}
															auto redeemedAtIt = obj.find("redeemed_at");
															if (redeemedAtIt != obj.end() && redeemedAtIt->second->type == JsonValue::Type::String) {
																redemption.redeemed_at = redeemedAtIt->second->stringValue;
															}
															auto rewardIdIt = obj.find("reward");
															if (rewardIdIt != obj.end() && rewardIdIt->second->type == JsonValue::Type::Object) {
																auto rewardObj = rewardIdIt->second->objectValues;
																auto rewardIdFieldIt = rewardObj.find("id");
																if (rewardIdFieldIt != rewardObj.end() && rewardIdFieldIt->second->type == JsonValue::Type::String) {
																	redemption.reward_id = rewardIdFieldIt->second->stringValue;
																}
																auto rewardTitleFieldIt = rewardObj.find("title");
																if (rewardTitleFieldIt != rewardObj.end() && rewardTitleFieldIt->second->type == JsonValue::Type::String) {
																	redemption.reward_title = rewardTitleFieldIt->second->stringValue;
																}
																auto rewardCostFieldIt = rewardObj.find("cost");
																if (rewardCostFieldIt != rewardObj.end() && rewardCostFieldIt->second->type == JsonValue::Type::Number) {
																	redemption.reward_cost = std::to_string(static_cast<int32_t>(rewardCostFieldIt->second->numberValue));
																}
																auto promptIt = rewardObj.find("prompt");
																if (promptIt != rewardObj.end() && promptIt->second->type == JsonValue::Type::String) {
																	redemption.prompt = promptIt->second->stringValue;
																}
															}
															auto statusIt = obj.find("status");
															if (statusIt != obj.end() && statusIt->second->type == JsonValue::Type::String) {
																redemption.status = statusIt->second->stringValue;
															}
															// user_input is optional
															auto userInputIt = obj.find("user_input");
															if (userInputIt != obj.end() && userInputIt->second->type == JsonValue::Type::String) {
																try {
																	redemption.user_input = userInputIt->second->stringValue;
																}
																catch (const std::exception&) {
																	redemption.user_input = "";
																}
															}
															else {
																redemption.user_input = ""; // Default value if not provided
															}
														}
														subIt->second->callback(redemption);
														ImGuiLogManager::AddLog("TwitchAPI", "Invoked callback for event type: " + eventType, LogSeverity::Info);
													}
													else {
														ImGuiLogManager::AddLog("TwitchAPI", "No callback registered for event type: " + eventType, LogSeverity::Warning);
													}
												}
												else {
													ImGuiLogManager::AddLog("TwitchAPI", "Type field missing or not a string in subscription object.", LogSeverity::Error);
												}
											}
											else {
												ImGuiLogManager::AddLog("TwitchAPI", "Subscription field missing or not an object in payload.", LogSeverity::Error);
											}
										}
										else {
											ImGuiLogManager::AddLog("TwitchAPI", "Event field missing or not an object in payload.", LogSeverity::Error);
										}
									}
									else {
										ImGuiLogManager::AddLog("TwitchAPI", "Payload field missing or not an object in WebSocket message.", LogSeverity::Error);
									}
								}
								else if (messageType == "revocation") {
									// Handle revocation
									ImGuiLogManager::AddLog("TwitchAPI", "WebSocket revocation received.", LogSeverity::Warning);
								}
								else if (messageType == "keepalive" || messageType == "session_keepalive") {
									// Handle keepalive
									ImGuiLogManager::AddLog("TwitchAPI", "WebSocket keepalive received.", LogSeverity::Info);
								}
								else {
									ImGuiLogManager::AddLog("TwitchAPI", "Unknown WebSocket message type: " + messageType, LogSeverity::Warning);
								}
							}
							else {
								ImGuiLogManager::AddLog("TwitchAPI", "message_type field missing or not a string in metadata.", LogSeverity::Error);
							}
						}
						else {
							ImGuiLogManager::AddLog("TwitchAPI", "metadata field missing or not an object in WebSocket message.", LogSeverity::Error);
						}
					}
					else {
						ImGuiLogManager::AddLog("TwitchAPI", "Expected JSON object at root of WebSocket message.", LogSeverity::Error);
					}
				}
				else {
					ImGuiLogManager::AddLog("TwitchAPI", "Failed to parse WebSocket message as JSON.", LogSeverity::Error);
					std::string hex;
					for (size_t i = 0; i < strlen(message); ++i)
						hex += " " + std::to_string((unsigned char)message[i]);
					ImGuiLogManager::AddLog("WebSocket", "Raw WebSocket frame (hex):" + hex, LogSeverity::Info);
				}
				// Clean up
				delete[] message;
			}
			if (m_WebSocketSessionID)
				SubscribetoUnSubscribedEvents();
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

    int TwitchAPI::SubscribetoUnSubscribedEvents()
    {
		for (auto& [type, subscription] : m_Subscriptions) {
			if (!subscription->Subscibed) {
				if (!m_WebSocketSessionID) {
					ImGuiLogManager::AddLog("TwitchAPI", "WebSocket session ID is not set. Cannot subscribe to events.", LogSeverity::Error);
					return -1;
				}
				if (!m_ChannelID) {
					ImGuiLogManager::AddLog("TwitchAPI", "Channel ID is not set. Cannot subscribe to events.", LogSeverity::Error);
					return -1;
				}
				std::string header;
				std::string body;

				ImGuiLogManager::AddLog("TwitchAPI", "Subscribing to event: " + type, LogSeverity::Info);
				if (type == "automod.message.hold") {
					body = "{"
						"\"type\": \"automod.message.hold\","
						"\"version\": \"1\","
						"\"condition\": {"
						"\"broadcaster_user_id\": \"" + std::string(m_ChannelID) + "\","
						"\"moderator_user_id\": \"" + std::string(m_ChannelID) + "\""
						"},"
						"\"transport\": {"
						"\"method\": \"websocket\","
						"\"session_id\": \"" + m_WebSocketSessionID + "\""
						"}"
						"}";
					header = "POST /helix/eventsub/subscriptions HTTP/1.1\r\n"
						"Host: api.twitch.tv\r\n"
						"Client-ID: " + std::string(m_ClientID) + "\r\n"
						"Authorization: Bearer " + std::string(m_AccessToken) + "\r\n"
						"Content-Type: application/json\r\n"
						"Content-Length: " + std::to_string(body.length()) + "\r\n"
						"\r\n";
				}

				if (type == "channel.channel_points_custom_reward_redemption.add") {
					body = "{"
						"\"type\": \"channel.channel_points_custom_reward_redemption.add\","
						"\"version\": \"1\","
						"\"condition\": {"
						"\"broadcaster_user_id\": \"" + std::string(m_ChannelID) + "\""
						"},"
						"\"transport\": {"
						"\"method\": \"websocket\","
						"\"session_id\": \"" + m_WebSocketSessionID + "\""
						"}"
						"}";
					header = "POST /helix/eventsub/subscriptions HTTP/1.1\r\n"
						"Host: api.twitch.tv\r\n"
						"Client-ID: " + std::string(m_ClientID) + "\r\n"
						"Authorization: Bearer " + std::string(m_AccessToken) + "\r\n"
						"Content-Type: application/json\r\n"
						"Content-Length: " + std::to_string(body.length()) + "\r\n"
						"\r\n";
				}
				std::string request = header + body;
				if (!m_SocketManager->GetSocketStatus("Client"))
					m_SocketManager->ConnectSocket("Client", "api.twitch.tv", 443, true);

				
				int result = m_SocketManager->SendData("Client", request.c_str(), request.length(), true);
				if (result < 0) {
					ImGuiLogManager::AddLog("TwitchAPI", "Failed to send subscription request for event: " + type, LogSeverity::Error);
					return -1;
				}
				char response[4096] = { 0 };
				int bytesReceived = m_SocketManager->ReceiveData("Client", response, sizeof(response) - 1, true);
				if (bytesReceived <= 0) {
					ImGuiLogManager::AddLog("TwitchAPI", "Failed to receive response for subscription request for event: " + type, LogSeverity::Error);
					return -1;
				}
				response[bytesReceived] = '\0';
				m_SocketManager->CloseSocket("Client");
				ImGuiLogManager::AddLog("TwitchAPI", std::string("Received response for subscription request:\n") + response, LogSeverity::Info);
				
				// Mark as subscribed if we got a 202 Accepted response
				if (strstr(response, "202 Accepted")) {
					subscription->Subscibed = true;
					ImGuiLogManager::AddLog("TwitchAPI", "Successfully subscribed to event: " + type, LogSeverity::Info);
					return 0;
				} else {
					ImGuiLogManager::AddLog("TwitchAPI", "Failed to subscribe to event: " + type, LogSeverity::Error);
					return -1;
				}
			}
		}
		return 0;
    }

	int TwitchAPI::IsSubscribedToEvent(SubscriptionType type)
	{
		std::string typeStr = SubscriptionTypeToString(type);
		auto it = m_Subscriptions.find(typeStr);
		if (it != m_Subscriptions.end()) {
			return it->second->Subscibed ? 1 : 0;
		}
		return -1;
	}

	void TwitchAPI::StartInternalThread()
	{
		internalThreads.emplace_back([]() {
			// Placeholder for internal thread tasks
			std::this_thread::sleep_for(std::chrono::seconds(1));
		});
	}

	int TwitchAPI::Initialize()
	{
		m_SocketManager = new NomSocketManager();
		m_SocketManager->CreateSocket("Client", 1, 1);
		m_SocketManager->CreateSocket("Listener", 1, 1);

		m_SocketManager->BindSocket("Listener", "127.0.0.1", 3000);
		m_SocketManager->Listen("Listener", SOMAXCONN);

		m_WebSocket = new NomWebSocket(*m_SocketManager);

		// Get Client-ID from environment variable
		char* clientId = nullptr;
		size_t clientIdLen = 0;
		if (_dupenv_s(&clientId, &clientIdLen, "CLIENT_ID") != 0 || clientId == nullptr) {
			ImGuiLogManager::AddLog("TwitchAPI", "Environment variable CLIENT_ID not found!", LogSeverity::Error);
			m_SocketManager->CloseAllSockets();
			delete m_SocketManager;
			return 1;
		}
		else {
			ImGuiLogManager::AddLog("TwitchAPI", std::string("ClientID: ") + clientId, LogSeverity::Info);
			m_ClientID = new char[strlen(clientId) + 1];
			strcpy_s(m_ClientID, strlen(clientId) + 1, clientId);
		}
		free(clientId);

		// Get Client-Secret from environment variable
		char* clientSecret = nullptr;
		size_t clientSecretLen = 0;
		if (_dupenv_s(&clientSecret, &clientSecretLen, "CLIENT_SECRET") != 0 || clientSecret == nullptr) {
			ImGuiLogManager::AddLog("TwitchAPI", "Environment variable CLIENT_SECRET not found!", LogSeverity::Error);
			m_SocketManager->CloseAllSockets();
			delete m_SocketManager;
			delete[] m_ClientID; // Clean up
			return 1;
		}
		else {
			ImGuiLogManager::AddLog("TwitchAPI", std::string("ClientSecret: ") + std::string(clientSecret).substr(0, 4) + "****", LogSeverity::Info);
			m_ClientSecret = new char[strlen(clientSecret) + 1];
			strcpy_s(m_ClientSecret, strlen(clientSecret) + 1, clientSecret);
		}
		free(clientSecret);
		return 0;
	}

	int TwitchAPI::Authenticate()
	{
		char link[256];
		sprintf_s(link, "https://id.twitch.tv/oauth2/authorize?response_type=code&client_id=%s&redirect_uri=http://localhost:3000&scope=%s", m_ClientID, m_ScopesString);
		ShellExecuteA(NULL, "open", link, NULL, NULL, SW_SHOWNORMAL);

		char clientAddress[INET_ADDRSTRLEN];
		int clientPort = 0;
		m_SocketManager->AcceptConnection("Listener", clientAddress, &clientPort);
		ImGuiLogManager::AddLog("TwitchAPI", std::string("Accepted connection from ") + clientAddress + ":" + std::to_string(clientPort), LogSeverity::Info);

		char buffer[4096] = { 0 };
		int bytesReceived = m_SocketManager->ReceiveData(ACCEPTED_CONNECTION, buffer, sizeof(buffer) - 1);
		while (bytesReceived < 0) {
			int bytesReceived = m_SocketManager->ReceiveData(ACCEPTED_CONNECTION, buffer, sizeof(buffer) - 1);
			Sleep(100);
		}

		if (bytesReceived > 0) {
			buffer[bytesReceived] = '\0'; // Null-terminate the received data
			ImGuiLogManager::AddLog("TwitchAPI", std::string("Received data: ") + buffer, LogSeverity::Info);
			char* getLine = strstr(buffer, "GET /?code=");
			if (getLine) {
				char* codeStart = getLine + strlen("GET /?code=");
				char* codeEnd = strchr(codeStart, '&');
				char code[256] = { 0 };
				if (codeEnd) {
					size_t codeLen = codeEnd - codeStart;
					if (codeLen < sizeof(code)) {
						strncpy(code, codeStart, codeLen);
						code[codeLen] = '\0';
						m_AuthCode = new char[strlen(code) + 1];
						strcpy_s(m_AuthCode, strlen(code) + 1, code);
						ImGuiLogManager::AddLog("TwitchAPI", std::string("Extracted OAuth code: ") + m_AuthCode, LogSeverity::Info);
					}
				}
				else {
					codeEnd = strchr(codeStart, ' ');
					if (codeEnd) {
						size_t codeLen = codeEnd - codeStart;
						if (codeLen < sizeof(code)) {
							strncpy(code, codeStart, codeLen);
							code[codeLen] = '\0';
							m_AuthCode = new char[strlen(code) + 1];
							strcpy_s(m_AuthCode, strlen(code) + 1, code);
							ImGuiLogManager::AddLog("TwitchAPI", std::string("Extracted OAuth code: ") + m_AuthCode, LogSeverity::Info);
						}
					}
				}
			}
			else {
				ImGuiLogManager::AddLog("TwitchAPI", "No OAuth code found in the request!", LogSeverity::Error);
				return 1;
			}
		}

		const char* httpResponse =
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/html\r\n"
			"Connection: close\r\n"
			"\r\n"
			"<html><body><h1>You can now close this window.</h1></body></html>";
		m_SocketManager->SendData(ACCEPTED_CONNECTION, httpResponse, strlen(httpResponse));
		m_SocketManager->RemoveSocket(ACCEPTED_CONNECTION);
		if (m_AuthCode != nullptr) {
			GetAccessToken();
		} else {
			ImGuiLogManager::AddLog("TwitchAPI", "Authorization code is null, cannot get access token!", LogSeverity::Error);
			return 1;
		}

		// Get Channel ID
		m_SocketManager->ConnectSocket("Client", "api.twitch.tv", 443, true);
		char httpRequest[1024];
		sprintf_s(httpRequest,
			"GET /helix/users HTTP/1.1\r\n"
			"Host: api.twitch.tv\r\n"
			"Client-ID: %s\r\n"
			"Authorization: Bearer %s\r\n"
			"User-Agent: NomBotCore/1.0\r\n"
			"Connection: close\r\n"
			"\r\n",
			m_ClientID, m_AccessToken);
		m_SocketManager->SendData("Client", httpRequest, strlen(httpRequest), true);
		char userBuffer[8192] = { 0 };
		int userBytesReceived = m_SocketManager->ReceiveData("Client", userBuffer, sizeof(userBuffer) - 1, true);
		while (userBytesReceived < 0) {
			int userBytesReceived = m_SocketManager->ReceiveData("Client", userBuffer, sizeof(userBuffer) - 1, true);
			Sleep(100);
		}
		if (userBytesReceived > 0) {
			userBuffer[userBytesReceived] = '\0'; // Null-terminate the received data
			ImGuiLogManager::AddLog("TwitchAPI", std::string("Received user data: ") + userBuffer, LogSeverity::Info);
			// Parse JSON response to extract id
			const char* idKey = "\"id\":\"";
			char* idStart = strstr(userBuffer, idKey);
			if (idStart) {
				idStart += strlen(idKey);
				char* idEnd = strchr(idStart, '"');
				if (idEnd) {
					size_t idLen = idEnd - idStart;
					char* id = new char[idLen + 1];
					strncpy_s(id, idLen + 1, idStart, idLen);
					id[idLen] = '\0';
					m_ChannelID = new char[strlen(id) + 1];
					strcpy_s(m_ChannelID, strlen(id) + 1, id);
					ImGuiLogManager::AddLog("TwitchAPI", std::string("Channel ID: ") + std::string(id), LogSeverity::Info);
					delete[] id;
				}
			}
			else {
				ImGuiLogManager::AddLog("TwitchAPI", "No channel ID found in response.", LogSeverity::Error);
				return 1;
			}
		}
		else {
			ImGuiLogManager::AddLog("TwitchAPI", "Failed to receive user data.", LogSeverity::Error);
			return 1;
		}

		m_SocketManager->CloseSocket("Client");

		return 0;
	}

	int TwitchAPI::GetAccessToken()
	{
		// Exchange the authorization code for an access token
		const char* host = "id.twitch.tv";
		char postData[512] = { 0 };
		if (m_AuthCode == nullptr) {
			ImGuiLogManager::AddLog("TwitchAPI", "Authorization code is null, cannot get access token!", LogSeverity::Error);
			return 1;
		}
		sprintf_s(postData, "client_id=%s&client_secret=%s&code=%s&grant_type=authorization_code&redirect_uri=http://localhost:3000", m_ClientID, m_ClientSecret, m_AuthCode);
		char httpRequest[1024];
		sprintf_s(httpRequest,
			"POST /oauth2/token HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Content-Type: application/x-www-form-urlencoded\r\n"
			"Content-Length: %zu\r\n"
			"Connection: close\r\n"
			"\r\n"
			"%s",
			host, strlen(postData), postData);
		m_SocketManager->ConnectSocket("Client", host, 443, true);
		m_SocketManager->SendData("Client", httpRequest, strlen(httpRequest), true);
		char buffer[8192] = { 0 };
		int bytesReceived = m_SocketManager->ReceiveData("Client", buffer, sizeof(buffer) - 1, true);
		while (bytesReceived < 0) {
			int bytesReceived = m_SocketManager->ReceiveData("Client", buffer, sizeof(buffer) - 1, true);
			Sleep(100);
		}
		if (bytesReceived > 0) {
			buffer[bytesReceived] = '\0'; // Null-terminate the received data
			ImGuiLogManager::AddLog("TwitchAPI", std::string("Received data: ") + buffer, LogSeverity::Info);
			// Parse JSON response to extract access_token
			const char* accessTokenKey = "\"access_token\":\"";
			char* accessTokenStart = strstr(buffer, accessTokenKey);
			if (accessTokenStart) {
				accessTokenStart += strlen(accessTokenKey);
				char* accessTokenEnd = strchr(accessTokenStart, '"');
				if (accessTokenEnd) {
					size_t accessTokenLen = accessTokenEnd - accessTokenStart;
					char* accessToken = new char[accessTokenLen + 1];
					strncpy_s(accessToken, accessTokenLen + 1, accessTokenStart, accessTokenLen);
					accessToken[accessTokenLen] = '\0';
					m_AccessToken = new char[strlen(accessToken) + 1];
					strcpy_s(m_AccessToken, strlen(accessToken) + 1, accessToken);
					ImGuiLogManager::AddLog("TwitchAPI", std::string("Access Token: ") + std::string(accessToken).substr(0, 4) + "****", LogSeverity::Info);
					delete[] accessToken;
				}
			}
			else {
				ImGuiLogManager::AddLog("TwitchAPI", "No access token found in response.", LogSeverity::Error);
				return 1;
			}
			// Parse JSON response to extract expires_in
			const char* refreshToken = "\"refresh_token\":\"";
			char* refreshTokenStart = strstr(buffer, refreshToken);
			if (refreshTokenStart) {
				refreshTokenStart += strlen(refreshToken);
				char* refreshTokenEnd = strchr(refreshTokenStart, '"');
				if (refreshTokenEnd) {
					size_t refreshTokenLen = refreshTokenEnd - refreshTokenStart;
					char* refresh_Token = new char[refreshTokenLen + 1];
					strncpy_s(refresh_Token, refreshTokenLen + 1, refreshTokenStart, refreshTokenLen);
					refresh_Token[refreshTokenLen] = '\0';
					m_RefreshToken = new char[strlen(refresh_Token) + 1];
					strcpy_s(m_RefreshToken, strlen(refresh_Token) + 1, refresh_Token);
					ImGuiLogManager::AddLog("TwitchAPI", std::string("Refresh Token: ") + std::string(refresh_Token).substr(0, 4) + "****", LogSeverity::Info);
					delete[] refresh_Token;
				}
			}
			else {
				ImGuiLogManager::AddLog("TwitchAPI", "No refresh token found in response.", LogSeverity::Error);
				return 1;
			}
			// Parse JSON response to extract expires_in
			const char* expiresInKey = "\"expires_in\":";
			char* expiresInStart = strstr(buffer, expiresInKey);
			if (expiresInStart) {
				expiresInStart += strlen(expiresInKey);
				char* expiresInEnd = strchr(expiresInStart, ',');
				if (expiresInEnd) {
					size_t expiresInLen = expiresInEnd - expiresInStart;
					char* expiresInStr = new char[expiresInLen + 1];
					strncpy_s(expiresInStr, expiresInLen + 1, expiresInStart, expiresInLen);
					expiresInStr[expiresInLen] = '\0';
					int expiresIn = atoi(expiresInStr);
					m_ExpiresIn = expiresIn;
					ImGuiLogManager::AddLog("TwitchAPI", std::string("Expires In: ") + std::to_string(expiresIn) + " seconds", LogSeverity::Info);
					delete[] expiresInStr;
				}
			}
			else {
				ImGuiLogManager::AddLog("TwitchAPI", "No expires_in found in response.", LogSeverity::Error);
				return 1;
			}
			// Parse JSON response to extract scope
			const char* scopeKey = "\"scope\":[";
			char* scopeStart = strstr(buffer, scopeKey);
			if (scopeStart) {
				scopeStart += strlen(scopeKey);
				char* scopeEnd = strchr(scopeStart, ']');
				if (scopeEnd) {
					size_t scopeLen = scopeEnd - scopeStart;
					char* scopeStr = new char[scopeLen + 1];
					strncpy_s(scopeStr, scopeLen + 1, scopeStart, scopeLen);
					scopeStr[scopeLen] = '\0';
					char* p = scopeStr;
					while (*p) {
						char* quoteStart = strchr(p, '"');
						if (!quoteStart) break;
						char* quoteEnd = strchr(quoteStart + 1, '"');
						if (!quoteEnd) break;
						size_t len = quoteEnd - quoteStart - 1;
						char* scopeItem = new char[len + 1];
						strncpy_s(scopeItem, len + 1, quoteStart + 1, len);	
						scopeItem[len] = '\0';
						m_Scopes.push_back(scopeItem);
						p = quoteEnd + 1;
					}
					ImGuiLogManager::AddLog("TwitchAPI", "Scopes:", LogSeverity::Info);
					for (const char* s : m_Scopes) ImGuiLogManager::AddLog("TwitchAPI", std::string(" - ") + s, LogSeverity::Info);
					delete[] scopeStr;
				}
			}
			else {
				ImGuiLogManager::AddLog("TwitchAPI", "No scope found in response.", LogSeverity::Error);
				return 1;
			}
			// Parse JSON response to extract token_type
			const char* tokenTypeKey = "\"token_type\":\"";
			char* tokenTypeStart = strstr(buffer, tokenTypeKey);
			if (tokenTypeStart) {
				tokenTypeStart += strlen(tokenTypeKey);
				char* tokenTypeEnd = strchr(tokenTypeStart, '"');
				if (tokenTypeEnd) {
					size_t tokenTypeLen = tokenTypeEnd - tokenTypeStart;
					char* tokenTypeStr = new char[tokenTypeLen + 1];
					strncpy_s(tokenTypeStr, tokenTypeLen + 1, tokenTypeStart, tokenTypeLen);
					tokenTypeStr[tokenTypeLen] = '\0';
					m_TokenType = new char[strlen(tokenTypeStr) + 1];
					strcpy_s(m_TokenType, strlen(tokenTypeStr) + 1, tokenTypeStr);
					ImGuiLogManager::AddLog("TwitchAPI", std::string("Token Type: ") + m_TokenType, LogSeverity::Info);
					delete[] tokenTypeStr;
				}
			}
			else {
				ImGuiLogManager::AddLog("TwitchAPI", "No token_type found in response.", LogSeverity::Error);
				return 1;
			}
			m_SocketManager->CloseSocket("Client");
			if (m_AuthCode != nullptr) {
				delete[] m_AuthCode;
				m_AuthCode = nullptr;
			}
		}

		return 0;
	}

	int TwitchAPI::RefreshAccessToken()
	{
		// Refresh the access token using the refresh token
		if (m_RefreshToken == nullptr) {
			ImGuiLogManager::AddLog("TwitchAPI", "Refresh token is null, cannot refresh access token!", LogSeverity::Error);
			return 1;
		}
		const char* host = "id.twitch.tv";
		char postData[512] = { 0 };
		sprintf_s(postData, "client_id=%s&client_secret=%s&refresh_token=%s&grant_type=refresh_token", m_ClientID, m_ClientSecret, m_RefreshToken);
		char httpRequest[1024];
		sprintf_s(httpRequest,
			"POST /oauth2/token HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Content-Type: application/x-www-form-urlencoded\r\n"
			"Content-Length: %zu\r\n"
			"Connection: close\r\n"
			"\r\n"
			"%s",
			host, strlen(postData), postData);
		m_SocketManager->ConnectSocket("Client", host, 443, true);
		m_SocketManager->SendData("Client", httpRequest, strlen(httpRequest), true);
		char buffer[8192] = { 0 };
		int bytesReceived = m_SocketManager->ReceiveData("Client", buffer, sizeof(buffer) - 1, true);
		while (bytesReceived < 0) {
			int bytesReceived = m_SocketManager->ReceiveData("Client", buffer, sizeof(buffer) - 1, true);
			Sleep(100);
		}
		// Removes old data from previous token
		if (m_AccessToken != nullptr) {
			delete[] m_AccessToken;
			m_AccessToken = nullptr;
		}
		if (m_RefreshToken != nullptr) {
			delete[] m_RefreshToken;
			m_RefreshToken = nullptr;
		}
		if (m_TokenType != nullptr) {
			delete[] m_TokenType;
			m_TokenType = nullptr;
		}
		for (char* s : m_Scopes) {
			delete[] s;
		}
		m_Scopes.clear();
		m_ExpiresIn = 0;

		if (bytesReceived > 0) {
			buffer[bytesReceived] = '\0'; // Null-terminate the received data
			ImGuiLogManager::AddLog("TwitchAPI", std::string("Received data: ") + buffer, LogSeverity::Info);
			// Parse JSON response to extract access_token
			const char* accessTokenKey = "\"access_token\":\"";
			char* accessTokenStart = strstr(buffer, accessTokenKey);
			if (accessTokenStart) {
				accessTokenStart += strlen(accessTokenKey);
				char* accessTokenEnd = strchr(accessTokenStart, '"');
				if (accessTokenEnd) {
					size_t accessTokenLen = accessTokenEnd - accessTokenStart;
					char* accessToken = new char[accessTokenLen + 1];
					strncpy_s(accessToken, accessTokenLen + 1, accessTokenStart, accessTokenLen);
					accessToken[accessTokenLen] = '\0';
					if (m_AccessToken != nullptr) {
						delete[] m_AccessToken;
					}
					m_AccessToken = new char[strlen(accessToken) + 1];
					strcpy_s(m_AccessToken, strlen(accessToken) + 1, accessToken);
					ImGuiLogManager::AddLog("TwitchAPI", std::string("Access Token: ") + std::string(accessToken).substr(0, 4) + "****", LogSeverity::Info);
					delete[] accessToken;
				}
			}
			else {
				ImGuiLogManager::AddLog("TwitchAPI", "No access token found in response.", LogSeverity::Error);
				return 1;
			}
			// Parse JSON response to extract expires_in
			const char* expiresInKey = "\"expires_in\":";
			char* expiresInStart = strstr(buffer, expiresInKey);
			if (expiresInStart) {
				expiresInStart += strlen(expiresInKey);
				char* expiresInEnd = strchr(expiresInStart, ',');
				if (expiresInEnd) {
					size_t expiresInLen = expiresInEnd - expiresInStart;
					char* expiresInStr = new char[expiresInLen + 1];
					strncpy_s(expiresInStr, expiresInLen + 1, expiresInStart, expiresInLen);
					expiresInStr[expiresInLen] = '\0';
					int expiresIn = atoi(expiresInStr);
					m_ExpiresIn = expiresIn;
					ImGuiLogManager::AddLog("TwitchAPI", std::string("Expires In: ") + std::to_string(expiresIn) + " seconds", LogSeverity::Info);
					delete[] expiresInStr;
				}
			}
			else {
				ImGuiLogManager::AddLog("TwitchAPI", "No expires_in found in response.", LogSeverity::Error);
				return 1;
			}
			// Parse JSON response to extract scope
			const char* scopeKey = "\"scope\":[";
			char* scopeStart = strstr(buffer, scopeKey);
			if (scopeStart) {
				scopeStart += strlen(scopeKey);
				char* scopeEnd = strchr(scopeStart, ']');
				if (scopeEnd) {
					size_t scopeLen = scopeEnd - scopeStart;
					char* scopeStr = new char[scopeLen + 1];
					strncpy_s(scopeStr, scopeLen + 1, scopeStart, scopeLen);
					scopeStr[scopeLen] = '\0';
					// Clear existing scopes
					for (char* s : m_Scopes) {
						delete[] s;
					}
					m_Scopes.clear();
					char* p = scopeStr;
					while (*p) {
						char* quoteStart = strchr(p, '"');
						if (!quoteStart) break;
						char* quoteEnd = strchr(quoteStart + 1, '"');
						if (!quoteEnd) break;
						size_t len = quoteEnd - quoteStart - 1;
						char* scopeItem = new char[len + 1];
						strncpy_s(scopeItem, len + 1, quoteStart + 1, len);
						scopeItem[len] = '\0';
						m_Scopes.push_back(scopeItem);
						p = quoteEnd + 1;
					}
					ImGuiLogManager::AddLog("TwitchAPI", "Scopes:", LogSeverity::Info);
					for (const char* s : m_Scopes) ImGuiLogManager::AddLog("TwitchAPI", std::string(" - ") + s, LogSeverity::Info);
					delete[] scopeStr;
				}
			}
			else {
				ImGuiLogManager::AddLog("TwitchAPI", "No scope found in response.", LogSeverity::Error);
				return 1;
			}
			// Parse JSON response to extract token_type
			const char* tokenTypeKey = "\"token_type\":\"";
			char* tokenTypeStart = strstr(buffer, tokenTypeKey);
			if (tokenTypeStart) {
				tokenTypeStart += strlen(tokenTypeKey);
				char* tokenTypeEnd = strchr(tokenTypeStart, '"');
				if (tokenTypeEnd) {
					size_t tokenTypeLen = tokenTypeEnd - tokenTypeStart;
					char* tokenTypeStr = new char[tokenTypeLen + 1];
					strncpy_s(tokenTypeStr, tokenTypeLen + 1, tokenTypeStart, tokenTypeLen);
					tokenTypeStr[tokenTypeLen] = '\0';
					if (m_TokenType != nullptr) {
						delete[] m_TokenType;
					}
					m_TokenType = new char[strlen(tokenTypeStr) + 1];
					strcpy_s(m_TokenType, strlen(tokenTypeStr) + 1, tokenTypeStr);
					ImGuiLogManager::AddLog("TwitchAPI", std::string("Token Type: ") + m_TokenType, LogSeverity::Info);
					delete[] tokenTypeStr;
				}
			}
			else {
				ImGuiLogManager::AddLog("TwitchAPI", "No token_type found in response.", LogSeverity::Error);
				return 1;
			}
			// Note: Twitch may return a new refresh token; handle it if present
			const char* refreshTokenKey = "\"refresh_token\":\"";
			char* refreshTokenStart = strstr(buffer, refreshTokenKey);
			if (refreshTokenStart) {
				refreshTokenStart += strlen(refreshTokenKey);
				char* refreshTokenEnd = strchr(refreshTokenStart, '"');
				if (refreshTokenEnd) {
					size_t refreshTokenLen = refreshTokenEnd - refreshTokenStart;
					char* refresh_Token = new char[refreshTokenLen + 1];
					strncpy_s(refresh_Token, refreshTokenLen + 1, refreshTokenStart, refreshTokenLen);
					refresh_Token[refreshTokenLen] = '\0';
					if (m_RefreshToken != nullptr) {
						delete[] m_RefreshToken;
					}
					m_RefreshToken = new char[strlen(refresh_Token) + 1];
					strcpy_s(m_RefreshToken, strlen(refresh_Token) + 1, refresh_Token);
					ImGuiLogManager::AddLog("TwitchAPI", std::string("Refresh Token: ") + std::string(refresh_Token).substr(0, 4) + "****", LogSeverity::Info);
					delete[] refresh_Token;
				}
			}
		}
		m_SocketManager->CloseSocket("Client");
	}

	int TwitchAPI::EnableWebSocket(bool enable)
	{
		//Start new internal thread if (enable) else stop thread
		if (m_AccessToken == nullptr) {
			ImGuiLogManager::AddLog("TwitchAPI", "Access token is null, cannot enable WebSocket!", LogSeverity::Error);
			return 1;
		}
		m_IsWebSocketEnabled = enable;
		if (enable) {
			std::atomic<bool>* running = new std::atomic<bool>(true);
			m_WebSocketRunning = running;
			internalThreads.emplace_back(&TwitchAPI::WebSocketThreadFunc, this, std::ref(*running));
		}
		else {
			if (m_WebSocketRunning) {
				*m_WebSocketRunning = false;
				delete m_WebSocketRunning;
				m_WebSocketRunning = nullptr;
			}
		}
		return 0;
	}

	int TwitchAPI::ConnectWebSocket()
	{
		const char* wsAddress = "wss://eventsub.wss.twitch.tv";
		int wsPort = 443;
		if (m_WebSocket == nullptr) {
			ImGuiLogManager::AddLog("TwitchAPI", "WebSocket is not initialized!", LogSeverity::Error);
			return -1;
		}
		int result = m_WebSocket->ConnectWebSocket(wsAddress, wsPort, true);
		if (result < 0) {
			ImGuiLogManager::AddLog("TwitchAPI", "Failed to connect to WebSocket " + std::string(wsAddress) + ":" + std::to_string(wsPort), LogSeverity::Error);
			return -1;
		}
		ImGuiLogManager::AddLog("TwitchAPI", "WebSocket connection established with " + std::string(wsAddress) + ":" + std::to_string(wsPort), LogSeverity::Info);
		return 0;
	}

	int TwitchAPI::CheckIfRefreshNeeded()
	{
		if (m_ExpiresIn < 300) { 
			ImGuiLogManager::AddLog("TwitchAPI", "Access token is about to expire, refreshing...", LogSeverity::Warning);
			RefreshAccessToken();
		}
		return 0;
	}

	int TwitchAPI::AddEventSubSubscription(SubscriptionType type, std::function<void(const ChannelPointRewardRedemption&)> callback)
	{
		// Placeholder for adding EventSub subscription
		EventSubSubscription* sub = new EventSubSubscription();
		switch (type) {
		case SubscriptionType::AutomodMessageHold:
				sub->type = "automod.message.hold";
				sub->version = "1";
				break;
		case SubscriptionType::ChannelPointsCustomRewardRedemptionAdd:
				sub->type = "channel.channel_points_custom_reward_redemption.add";
				sub->version = "1";
				break;
		}
		sub->callback = callback;
		m_Subscriptions[SubscriptionTypeToString(type)] = sub;
		ImGuiLogManager::AddLog("TwitchAPI", "Adding EventSub subscription of type: " + SubscriptionTypeToString(type), LogSeverity::Info);
		return 0;
	}

	int TwitchAPI::RemoveEventSubSubscription(SubscriptionType type)
	{
		auto it = m_Subscriptions.find(SubscriptionTypeToString(type));
		if (it != m_Subscriptions.end()) {
			delete it->second;
			m_Subscriptions.erase(it);
			ImGuiLogManager::AddLog("TwitchAPI", "Removed EventSub subscription of type: " + SubscriptionTypeToString(type), LogSeverity::Info);
			return 0;
		}
		ImGuiLogManager::AddLog("TwitchAPI", "No subscription found for type: " + SubscriptionTypeToString(type), LogSeverity::Warning);
		return 1;
	}
}