#include "nompch.h"
#include "NomWebSocket.h"
#include "../Core/Logging/ImGuiLog.h"

namespace NomBotCore {
	std::map<std::string, std::string> m_HandshakeHeaders;
	std::mutex socketMutex;

	std::string base64_encode(const unsigned char* input, int length) {
		BIO* bmem = nullptr;
		BIO* b64 = nullptr;
		BUF_MEM* bptr = nullptr;
		b64 = BIO_new(BIO_f_base64());
		bmem = BIO_new(BIO_s_mem());
		b64 = BIO_push(b64, bmem);
		//BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // Ignore newlines - write everything in one line
		BIO_write(b64, input, length);
		BIO_flush(b64);
		BIO_get_mem_ptr(b64, &bptr);
		std::string encodedData(bptr->data, bptr->length - 1);
		BIO_free_all(b64);
		return encodedData;
	}

	std::string generateWebSocketAcceptKey(const std::string& clientKey) {
		const std::string magicString = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
		std::string combinedKey = clientKey + magicString;
		unsigned char sha1Hash[SHA_DIGEST_LENGTH];
		SHA1((const unsigned char*)combinedKey.c_str(), combinedKey.length(), sha1Hash);
		return base64_encode(sha1Hash, SHA_DIGEST_LENGTH);
	}

	NomWebSocket::NomWebSocket(NomSocketManager& socketManager)
		: m_SocketManager(socketManager) // Use member initializer list
	{
		m_SocketManager.CreateSocket(WEBOCKETNAME, 1, 1); // TCP, IPv4
	}

	NomWebSocket::~NomWebSocket()
	{
		m_SocketManager.CloseSocket(WEBOCKETNAME);
		m_SocketManager.RemoveSocket(WEBOCKETNAME);
	}

	int NomWebSocket::HandleWebSocketHandshake(bool sslData)
	{
		char buffer[4096] = { 0 };
		ImGuiLogManager::AddLog("WebSocket", "Waiting to receive WebSocket handshake request...", LogSeverity::Info);
		int bytesReceived = m_SocketManager.ReceiveData(WEBOCKETNAME, buffer, sizeof(buffer) - 1, sslData);
		if (bytesReceived <= 0) {
			ImGuiLogManager::AddLog("WebSocket", "Failed to receive WebSocket handshake request. Socket may not be connected or client did not send data.", LogSeverity::Error);
			return -1;
		}
		std::string request(buffer, bytesReceived);
		ImGuiLogManager::AddLog("WebSocket", "Received WebSocket handshake request:\n" + request, LogSeverity::Info);

		size_t keyPos = request.find("Sec-WebSocket-Accept: ");
		if (keyPos == std::string::npos) {
			ImGuiLogManager::AddLog("WebSocket", "Sec-WebSocket-Accept header not found.", LogSeverity::Error);
			return -1;
		}
		size_t keyEnd = request.find("\r\n", keyPos);
		if (keyEnd == std::string::npos) {
			ImGuiLogManager::AddLog("WebSocket", "Malformed Sec-WebSocket-Accept header.", LogSeverity::Error);
			return -1;
		}
		return 0;
	}

	int NomWebSocket::ConnectWebSocket(const char* address, int port, bool sslData)
	{
		int result = m_SocketManager.ConnectSocket(WEBOCKETNAME, address, port, sslData);
		if (result < 0) {
			ImGuiLogManager::AddLog("WebSocket", "Failed to connect to " + std::string(address) + ":" + std::to_string(port), LogSeverity::Error);
			m_SocketManager.CloseSocket(WEBOCKETNAME);
			m_SocketManager.RemoveSocket(WEBOCKETNAME);
			return -1;
		}
		std::string generatedKey = "x3JJHMbDL1EzLkh9GBhXDw=="; // TODO: In a real implementation, generate a random base64-encoded key
		std::string handshakeRequest =
			"GET /ws HTTP/1.1\r\n"
			"Host: " + std::string(address) + "\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Key: " + generatedKey + "\r\n"
			"Sec-WebSocket-Version: 13\r\n";
		for (const auto& header : m_HandshakeHeaders) {
			handshakeRequest += std::string(header.first) + ": " + std::string(header.second) + "\r\n";
			ImGuiLogManager::AddLog("WebSocket", "Added custom header: " + std::string(header.first) + ": " + std::string(header.second), LogSeverity::Info);
		}
		handshakeRequest += "\r\n";
		m_SocketManager.SendData(WEBOCKETNAME, handshakeRequest.c_str(), handshakeRequest.length(), sslData);
		result = HandleWebSocketHandshake(sslData);
		if (result < 0) {
			ImGuiLogManager::AddLog("WebSocket", "WebSocket handshake failed with " + std::string(address) + ":" + std::to_string(port), LogSeverity::Error);
			m_SocketManager.CloseSocket(WEBOCKETNAME);
			m_SocketManager.RemoveSocket(WEBOCKETNAME);
			return -1;
		}
		ImGuiLogManager::AddLog("WebSocket", "WebSocket connection established with " + std::string(address) + ":" + std::to_string(port), LogSeverity::Info);
		return 0;
	}

	int NomWebSocket::SendWebSocketFrame(const char* data, int length, bool sslData)
	{
		std::lock_guard<std::mutex> lock(socketMutex);
		std::vector<unsigned char> frame;
		frame.push_back(0x81); // FIN + text frame

		unsigned char maskBit = 0x80; // Client-to-server frames must be masked
		if (length <= 125) {
			frame.push_back(static_cast<unsigned char>(length | maskBit));
		} else if (length <= 65535) {
			frame.push_back(126 | maskBit);
			frame.push_back((length >> 8) & 0xFF);
			frame.push_back(length & 0xFF);
		} else {
			frame.push_back(127 | maskBit);
			for (int i = 7; i >= 0; --i) {
				frame.push_back((length >> (8 * i)) & 0xFF);
			}
		}

		unsigned char maskingKey[4];
		for (int i = 0; i < 4; ++i) {
			maskingKey[i] = rand() % 256;
			frame.push_back(maskingKey[i]);
		}

		for (int i = 0; i < length; ++i) {
			frame.push_back(data[i] ^ maskingKey[i % 4]);
		}

		int bytesSent = m_SocketManager.SendData(WEBOCKETNAME, reinterpret_cast<const char*>(frame.data()), frame.size(), sslData);
		if (bytesSent <= 0) {
			ImGuiLogManager::AddLog("WebSocket", "Failed to send WebSocket frame. Socket may not be connected.", LogSeverity::Error);
			return -1;
		}
		ImGuiLogManager::AddLog("WebSocket", "Sent WebSocket frame with " + std::to_string(bytesSent) + " bytes.", LogSeverity::Info);
		ImGuiLogManager::AddLog("WebSocket", std::string("Data: ") + std::string(data, bytesSent), LogSeverity::Info);
		return bytesSent;
	}

	const char* NomWebSocket::ReceiveWebSocketFrame(bool sslData)
	{
		std::lock_guard<std::mutex> lock(socketMutex);
		unsigned char header[2];
		int bytesReceived = m_SocketManager.ReceiveData(WEBOCKETNAME, reinterpret_cast<char*>(header), 2, sslData);
		if (bytesReceived <= 0) {
			ImGuiLogManager::AddLog("WebSocket", "Failed to receive WebSocket frame header.", LogSeverity::Error);
			return nullptr;
		}
		bool fin = (header[0] & 0x80) != 0;
		unsigned char opcode = header[0] & 0x0F;
		bool masked = (header[1] & 0x80) != 0;
		uint64_t payloadLength = header[1] & 0x7F;
		if (payloadLength == 126) {
			unsigned char extended[2];
			bytesReceived = m_SocketManager.ReceiveData(WEBOCKETNAME, reinterpret_cast<char*>(extended), 2, sslData);
			if (bytesReceived <= 0) {
				ImGuiLogManager::AddLog("WebSocket", "Failed to receive WebSocket frame extended payload length.", LogSeverity::Error);
				return nullptr;
			}
			payloadLength = (extended[0] << 8) | extended[1];
		} else if (payloadLength == 127) {
			unsigned char extended[8];
			bytesReceived = m_SocketManager.ReceiveData(WEBOCKETNAME, reinterpret_cast<char*>(extended), 8, sslData);
			if (bytesReceived <= 0) {
				ImGuiLogManager::AddLog("WebSocket", "Failed to receive WebSocket frame extended payload length.", LogSeverity::Error);
				return nullptr;
			}
			payloadLength = 0;
			for (int i = 0; i < 8; ++i) {
				payloadLength = (payloadLength << 8) | extended[i];
			}
		}
		unsigned char maskingKey[4];
		if (masked) {
			bytesReceived = m_SocketManager.ReceiveData(WEBOCKETNAME, reinterpret_cast<char*>(maskingKey), 4, sslData);
			if (bytesReceived <= 0) {
				ImGuiLogManager::AddLog("WebSocket", "Failed to receive WebSocket frame masking key.", LogSeverity::Error);
				return nullptr;
			}
		}
		std::vector<unsigned char> payload(payloadLength);
		size_t totalReceived = 0;
		while (totalReceived < payloadLength) {
			bytesReceived = m_SocketManager.ReceiveData(WEBOCKETNAME, reinterpret_cast<char*>(payload.data() + totalReceived), payloadLength - totalReceived, sslData);
			if (bytesReceived <= 0) {
				ImGuiLogManager::AddLog("WebSocket", "Failed to receive WebSocket frame payload data.", LogSeverity::Error);
				return nullptr;
			}
			totalReceived += bytesReceived;
		}
		if (masked) {
			for (size_t i = 0; i < payloadLength; ++i) {
				payload[i] ^= maskingKey[i % 4];
			}
		}

		if (opcode == 0x9) { // Ping frame
			ImGuiLogManager::AddLog("WebSocket", "Received PING frame. Sending PONG response.", LogSeverity::Info);
			SendPongFrame(reinterpret_cast<const char*>(payload.data()), payloadLength);
			return nullptr; // No application data to return
		} else if (opcode == 0xA) { // Pong frame
			ImGuiLogManager::AddLog("WebSocket", "Received PONG frame.", LogSeverity::Info);
			return nullptr; // No application data to return
		} else if (opcode == 0x8) { // Connection close frame
			ImGuiLogManager::AddLog("WebSocket", "Received CLOSE frame. Closing connection.", LogSeverity::Info);
			m_SocketManager.CloseSocket(WEBOCKETNAME);
			m_SocketManager.RemoveSocket(WEBOCKETNAME);
			return nullptr;
		}

		char* message = new char[payloadLength + 1];
		memcpy(message, payload.data(), payloadLength);
		message[payloadLength] = '\0';
		return message;
	}

	int NomWebSocket::SetHandshakeHeader(const std::string& key, const std::string& value) {
		m_HandshakeHeaders[key] = value;
		return 0;
	}

	int NomWebSocket::SendPongFrame(const char* pingPayload, size_t payloadLen)
	{
		unsigned char pongFrame[2 + 4 + 125]; // Header + mask + max payload
		size_t frameSize = 0;
		pongFrame[frameSize++] = 0x80 | 0x0A; // FIN + opcode for Pong

		unsigned char maskBit = 0x80;
		pongFrame[frameSize++] = static_cast<unsigned char>(payloadLen | maskBit);

		unsigned char maskingKey[4];
		for (int i = 0; i < 4; ++i) {
			maskingKey[i] = rand() % 256;
			pongFrame[frameSize++] = maskingKey[i];
		}

		for (size_t i = 0; i < payloadLen; ++i) {
			pongFrame[frameSize++] = pingPayload[i] ^ maskingKey[i % 4];
		}

		int bytesSent = m_SocketManager.SendData(WEBOCKETNAME, reinterpret_cast<const char*>(pongFrame), frameSize, true);
		if (bytesSent <= 0) {
			ImGuiLogManager::AddLog("WebSocket", "Failed to send PONG frame. Socket may not be connected.", LogSeverity::Error);
			return -1;
		}
		ImGuiLogManager::AddLog("WebSocket", "Sent PONG frame with " + std::to_string(bytesSent) + " bytes.", LogSeverity::Info);
		return bytesSent;
	}
}