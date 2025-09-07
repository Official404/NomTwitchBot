#ifndef __NOMWEBSOCKET_H__
#define __NOMWEBSOCKET_H__

#include "NomSocketManager.h"
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <string>

#define WEBOCKETNAME "WebSocket"

namespace NomBotCore {
	std::string base64_encode(const unsigned char* input, int length);
	std::string generateWebSocketAcceptKey(const std::string& clientKey);

	class NomWebSocket {
	public:
		NomWebSocket(NomSocketManager& socketManager);
		~NomWebSocket();
		int HandleWebSocketHandshake(bool sslData = false);
		int ConnectWebSocket(const char* address, int port, bool sslData = false);
		int SendWebSocketFrame(const char* data, int length, bool sslData = false);
		const char* ReceiveWebSocketFrame(bool sslData = false);
		int SetHandshakeHeader(const std::string& key, const std::string& value);
		int SendPongFrame(const char* pingPayload, size_t payloadLen);
	private:
		NomSocketManager& m_SocketManager;
	};
}

#endif