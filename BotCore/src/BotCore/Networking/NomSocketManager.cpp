#include "nompch.h"
#include "NomSocketManager.h"
#include "../Core/Logging/ImGuiLog.h"

namespace NomBotCore {
	NomSocketManager::NomSocketManager()
	{
		socketCount = 0;
		maxSockets = 16;
		sockets.resize(maxSockets, nullptr);
		Initialize();
	}

	NomSocketManager::~NomSocketManager()
	{
		for (int i = 0; i < socketCount; i++) {
			if (sockets[i] != nullptr) {
				if (sockets[i]->getStatus() == 1) {
					CloseSocket(sockets[i]->name);
				}
				delete sockets[i];
				sockets[i] = nullptr;
			}
		}
		sockets.clear();
		socketCount = 0;
		SSL_CTX_free(sslCtx);
		WSACleanup();
	}

	int NomSocketManager::Initialize()
	{
		// Initialize Winsock
		WSADATA wsaData;
		WORD wVersionRequested = MAKEWORD(2, 2);
		if (WSAStartup(wVersionRequested, &wsaData) != 0) {
			ImGuiLogManager::AddLog("Socket", "WSAStartup failed!", LogSeverity::Error);
			return 1;
		}
		else {
			ImGuiLogManager::AddLog("Socket", "WSAStartup succeeded.", LogSeverity::Info);
			ImGuiLogManager::AddLog("Socket", std::string("Winsock version: ") + std::to_string(HIBYTE(wsaData.wVersion)) + "." + std::to_string(LOBYTE(wsaData.wVersion)), LogSeverity::Info);
			ImGuiLogManager::AddLog("Socket", std::string("Description: ") + wsaData.szDescription, LogSeverity::Info);
			ImGuiLogManager::AddLog("Socket", std::string("System status: ") + wsaData.szSystemStatus, LogSeverity::Info);
		}
		// Initialize OpenSSL
		SSL_library_init();
		SSL_load_error_strings();
		OpenSSL_add_all_algorithms();
		sslCtx = SSL_CTX_new(TLS_client_method());
		return 0;
	}

	int NomSocketManager::CreateSocket(const char* name, int type, int protocol)
	{	
		if (socketCount >= maxSockets) {
			ImGuiLogManager::AddLog("Socket", "Maximum socket limit reached!", LogSeverity::Error);
			return -1;
		}
		NomSocket* newSocket = new NomSocket(socketCount, type, protocol);
		newSocket->name = new char[strlen(name) + 1];
		strcpy(newSocket->name, name);
		sockets[socketCount] = newSocket;
		ImGuiLogManager::AddLog("Socket", std::string("Created socket '") + name + "' with ID: " + std::to_string(socketCount), LogSeverity::Info);
		socketCount++;
		return 0;
	}

	int NomSocketManager::SocketNameToId(const char* name)
	{
		for (int i = 0; i < socketCount; i++) {
			if (sockets[i] != nullptr && strcmp(sockets[i]->name, name) == 0) {
				return sockets[i]->getId();
			}
		}
		ImGuiLogManager::AddLog("Socket", std::string("Socket with name '") + name + "' not found!", LogSeverity::Error);
		return -1;
	}

	int NomSocketManager::BindSocket(const char* socketName, const char* address, int port)
	{
		int socketId = SocketNameToId(socketName);
		if (socketId == -1) {
			return -1;
		}
		return BindSocket(socketId, address, port);
	}

	int NomSocketManager::BindSocket(int socketId, const char* address, int port)
	{
		if (socketId < 0 || socketId >= socketCount || sockets[socketId] == nullptr) {
			ImGuiLogManager::AddLog("Socket", "Invalid socket ID!", LogSeverity::Error);
			return -1;
		}
		NomSocket* nomSocket = sockets[socketId];
		if (nomSocket->getStatus() == 1) {
			ImGuiLogManager::AddLog("Socket", "Socket is already connected!", LogSeverity::Error);
			return -1;
		}
		// Create a socket based on type and protocol
		int af = (nomSocket->getProtocol() == 1) ? AF_INET : AF_INET6;
		int sockType = (nomSocket->getType() == 1) ? SOCK_STREAM : SOCK_DGRAM;
		int proto = (nomSocket->getType() == 1) ? IPPROTO_TCP : IPPROTO_UDP;
		SOCKET serverSocket = socket(af, sockType, proto);
		if (serverSocket == INVALID_SOCKET) {
			ImGuiLogManager::AddLog("Socket", "Socket creation failed.", LogSeverity::Error);
			return -1;
		}
		sockaddr_in serverAddr;
		serverAddr.sin_family = af;
		serverAddr.sin_port = htons(port);
		struct addrinfo hints = { 0 }, * res = nullptr;
		hints.ai_family = af;
		hints.ai_socktype = sockType;
		hints.ai_protocol = proto;
		int ret = getaddrinfo(address, NULL, &hints, &res);
		if (ret == 0 && res != nullptr) {
			sockaddr_in* addr_in = (sockaddr_in*)res->ai_addr;
			serverAddr.sin_addr = addr_in->sin_addr;
			if (nomSocket->address == nullptr)
				nomSocket->address = new char[INET_ADDRSTRLEN];
			inet_ntop(af, &serverAddr.sin_addr, nomSocket->address, INET_ADDRSTRLEN);
			freeaddrinfo(res);
		}
		else {
			ImGuiLogManager::AddLog("Socket", "getaddrinfo failed: ", LogSeverity::Error);
			closesocket(serverSocket);
			return -1;
		}
		if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
			ImGuiLogManager::AddLog("Socket", "Bind failed!", LogSeverity::Error);
			closesocket(serverSocket);
			return -1;
		}
		else {
			ImGuiLogManager::AddLog("Socket", std::string("Socket '") + nomSocket->name + "' bound to " + nomSocket->address + " on port " + std::to_string(port) + "!", LogSeverity::Info);
		}
		nomSocket->socket = new SOCKET(serverSocket);
		nomSocket->setStatus(1);
		return 0;
	}

	int NomSocketManager::AcceptConnection(const char* socketName, char* clientAddress, int* clientPort)
	{
		int socketId = SocketNameToId(socketName);
		if (socketId == -1) {
			return -1;
		}
		return AcceptConnection(socketId, clientAddress, clientPort);
	}

	int NomSocketManager::AcceptConnection(int socketId, char* clientAddress, int* clientPort)
	{
		if (socketId < 0 || socketId >= socketCount || sockets[socketId] == nullptr) {
			ImGuiLogManager::AddLog("Socket", "Invalid socket ID!", LogSeverity::Error);
			return -1;
		}
		NomSocket* nomSocket = sockets[socketId];
		if (nomSocket->getStatus() == 0) {
			ImGuiLogManager::AddLog("Socket", "Socket is not in listening state!", LogSeverity::Error);
			return -1;
		}
		sockaddr_in clientAddr;
		int addrLen = sizeof(clientAddr);
		SOCKET clientSocket = accept(*nomSocket->socket, (sockaddr*)&clientAddr, &addrLen);
		if (clientSocket == INVALID_SOCKET) {
			ImGuiLogManager::AddLog("Socket", "Accept failed!", LogSeverity::Error);
			return -1;
		}
		inet_ntop(AF_INET, &clientAddr.sin_addr, clientAddress, INET_ADDRSTRLEN);
		*clientPort = ntohs(clientAddr.sin_port);
		ImGuiLogManager::AddLog("Socket", std::string("Accepted connection from ") + clientAddress + ":" + std::to_string(*clientPort), LogSeverity::Info);
		// receve data from accepted connection


		CreateSocket(ACCEPTED_CONNECTION, 1, 1);
		int id = SocketNameToId(ACCEPTED_CONNECTION);
		if (sockets[id]->address) delete[] sockets[id]->address;
		sockets[id]->address = new char[strlen(clientAddress) + 1];
		strcpy(sockets[id]->address, clientAddress);
		sockets[id]->port = *clientPort;
		sockets[id]->socket = new SOCKET(clientSocket);
		sockets[id]->setStatus(1);
		return 0;
	}

	int NomSocketManager::ConnectSocket(int socketId, const char* address, int port, bool sslData)
	{
		if (socketId < 0 || socketId >= socketCount || sockets[socketId] == nullptr) {
			ImGuiLogManager::AddLog("Socket", "Invalid socket ID!", LogSeverity::Error);
			return -1;
		}
		NomSocket* nomSocket = sockets[socketId];
		nomSocket->host = new char[strlen(address) + 1];
		strcpy(nomSocket->host, address);
		if (nomSocket->getStatus() == 1) {
			ImGuiLogManager::AddLog("Socket", "Socket is already connected!", LogSeverity::Error);
			return -1;
		}
		// Create a socket based on type and protocol
		int af = (nomSocket->getProtocol() == 1) ? AF_INET : AF_INET6;
		int sockType = (nomSocket->getType() == 1) ? SOCK_STREAM : SOCK_DGRAM;
		int proto = (nomSocket->getType() == 1) ? IPPROTO_TCP : IPPROTO_UDP;
		SOCKET clientSocket = socket(af, sockType, proto);
		if (clientSocket == INVALID_SOCKET) {
			ImGuiLogManager::AddLog("Socket", "Socket creation failed.", LogSeverity::Error);
			return -1;
		}
		sockaddr_in serverAddr;
		serverAddr.sin_family = af;
		serverAddr.sin_port = htons(port);
		struct addrinfo hints = {0}, *res = nullptr;
		hints.ai_family = af;
		hints.ai_socktype = sockType;
		hints.ai_protocol = proto;
		int ret = getaddrinfo(address, NULL, &hints, &res);
		if (ret == 0 && res != nullptr) {
			sockaddr_in* addr_in = (sockaddr_in*)res->ai_addr;
			serverAddr.sin_addr = addr_in->sin_addr;
			if (nomSocket->address == nullptr)
				nomSocket->address = new char[INET_ADDRSTRLEN];
			inet_ntop(af, &serverAddr.sin_addr, nomSocket->address, INET_ADDRSTRLEN);
			freeaddrinfo(res);
		} else {
			ImGuiLogManager::AddLog("Socket", "getaddrinfo failed: ", LogSeverity::Error);
			closesocket(clientSocket);
			return -1;
		}
		if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
			ImGuiLogManager::AddLog("Socket", "Connection failed!", LogSeverity::Error);
			closesocket(clientSocket);
			return -1;
		} else {
			ImGuiLogManager::AddLog("Socket", std::string("Socket '") + nomSocket->name + "' connected to " + nomSocket->address + " on port " + std::to_string(port) + "!", LogSeverity::Info);
		}
		// handle ssl connection here if needed
		if (sslData) {
			SSL* ssl = SSL_new(sslCtx);
			SSL_set_fd(ssl, (int)clientSocket);
			SSL_set_tlsext_host_name(ssl, address);
			// Optionally, set default verify paths
			SSL_CTX_set_default_verify_paths(sslCtx);
			int ret = SSL_connect(ssl);
			if (ret <= 0) {
				int sslErr = SSL_get_error(ssl, ret);
				unsigned long errCode = ERR_get_error();
				ImGuiLogManager::AddLog("Socket", std::string("SSL connection failed! ") + ERR_error_string(errCode, nullptr) + " (SSL Error code: " + std::to_string(sslErr) + ")", LogSeverity::Error);
				SSL_free(ssl);
				closesocket(clientSocket);
				return -1;
			} else {
				ImGuiLogManager::AddLog("Socket", std::string("SSL connection established on socket '") + nomSocket->name + "'!", LogSeverity::Info);
			}
			nomSocket->ssl = ssl;
		} else {
			nomSocket->ssl = nullptr;
		}

		nomSocket->socket = new SOCKET(clientSocket);
		nomSocket->setStatus(1); // Mark as connected
		return 0;
	}

	int NomSocketManager::ConnectSocket(const char* socketName, const char* address, int port, bool sslData)
	{
		int socketId = SocketNameToId(socketName);
		if (socketId == -1) {
			return -1;
		}
		return ConnectSocket(socketId, address, port, sslData);
	}

	int NomSocketManager::SendData(int socketId, const char* data, int length, bool sslData)
	{
		if (socketId < 0 || socketId >= socketCount || sockets[socketId] == nullptr) {
			ImGuiLogManager::AddLog("Socket", "Invalid socket ID!", LogSeverity::Error);
			return -1;
		}
		NomSocket* nomSocket = sockets[socketId];
		if (nomSocket->getStatus() == 0) {
			ImGuiLogManager::AddLog("Socket", "Socket is not connected!", LogSeverity::Error);
			return -1;
		}
		// Send data through the socket
		int bytesSent = 0;
		if (sslData) {
			// TODO: Handle SNI if needed
			SSL_set_tlsext_host_name(nomSocket->ssl, nomSocket->host);
			bytesSent = SSL_write(nomSocket->ssl, data, length);
			if (bytesSent <= 0) {
				int sslErr = SSL_get_error(nomSocket->ssl, bytesSent);
				unsigned long errCode = ERR_get_error();
				ImGuiLogManager::AddLog("Socket", std::string("SSL Send failed! ") + ERR_error_string(errCode, nullptr) + " (SSL Error code: " + std::to_string(sslErr) + ")", LogSeverity::Error);
				return -1;
			}
		} else {
			if (nomSocket->ssl != nullptr) {
				ImGuiLogManager::AddLog("Socket", "Socket is using SSL, but sslData is false!", LogSeverity::Error);
				return -1;
			} else {
				bytesSent = send(*nomSocket->socket, data, length, 0);
				if (bytesSent == SOCKET_ERROR) {
					ImGuiLogManager::AddLog("Socket", "Send failed!", LogSeverity::Error);
					return -1;
				}
			}
		}
		ImGuiLogManager::AddLog("Socket", std::string("Sent ") + std::to_string(bytesSent) + " bytes on socket '" + nomSocket->name + "'", LogSeverity::Info);
		ImGuiLogManager::AddLog("Socket", std::string("Data: ") + std::string(data, bytesSent), LogSeverity::Info);
		return bytesSent;
	}

	int NomSocketManager::SendData(const char* socketName, const char* data, int length, bool sslData)
	{
		int socketId = SocketNameToId(socketName);
		if (socketId == -1) {
			return -1;
		}
		return SendData(socketId, data, length, sslData);
	}

	int NomSocketManager::ReceiveData(int socketId, char* buffer, int length, bool sslData)
	{
		if (socketId < 0 || socketId >= socketCount || sockets[socketId] == nullptr) {
			ImGuiLogManager::AddLog("Socket", "Invalid socket ID!", LogSeverity::Error);
			return -1;
		}
		NomSocket* nomSocket = sockets[socketId];
		if (nomSocket->getStatus() == 0) {
			ImGuiLogManager::AddLog("Socket", "Socket is not connected!", LogSeverity::Error);
			return -1;
		}
		// Receive data from the socket
		int bytesReceived = 0;
		if (sslData) {
			if (nomSocket->ssl == nullptr) {
				ImGuiLogManager::AddLog("Socket", "Socket is not using SSL, but sslData is true!", LogSeverity::Error);
				return -1;
			}
			SSL_set_tlsext_host_name(nomSocket->ssl, nomSocket->host);
			bytesReceived = SSL_read(nomSocket->ssl, buffer, length);
			if (bytesReceived <= 0) {
				int sslErr = SSL_get_error(nomSocket->ssl, bytesReceived);
				unsigned long errCode = ERR_get_error();
				ImGuiLogManager::AddLog("Socket", std::string("SSL Receive failed! ") + ERR_error_string(errCode, nullptr) + " (SSL Error code: " + std::to_string(sslErr) + ")", LogSeverity::Error);
				return -1;
			}
			return bytesReceived;
		} else {
			if (nomSocket->ssl != nullptr) {
				ImGuiLogManager::AddLog("Socket", "Socket is using SSL, but sslData is false!", LogSeverity::Error);
				return -1;
			} else {
				bytesReceived = recv(*nomSocket->socket, buffer, length, 0);
				if (bytesReceived == SOCKET_ERROR) {
					ImGuiLogManager::AddLog("Socket", "Receive failed!", LogSeverity::Error);
					return -1;
				}
			}
		}
		return bytesReceived;
	}

	int NomSocketManager::ReceiveData(const char* socketName, char* buffer, int length, bool sslData)
	{
		int socketId = SocketNameToId(socketName);
		if (socketId == -1) {
			return -1;
		}
		return ReceiveData(socketId, buffer, length, sslData);
	}

	int NomSocketManager::Listen(int socketId, int backlog)
	{
		if (socketId < 0 || socketId >= socketCount || sockets[socketId] == nullptr) {
			ImGuiLogManager::AddLog("Socket", "Invalid socket ID!", LogSeverity::Error);
			return -1;
		}
		NomSocket* nomSocket = sockets[socketId];
		if (nomSocket->getStatus() == 0) {
			ImGuiLogManager::AddLog("Socket", "Socket is not bound!", LogSeverity::Error);
			return -1;
		}
		// Listen on the socket
		if (listen(*nomSocket->socket, backlog) == SOCKET_ERROR) {
			ImGuiLogManager::AddLog("Socket", "Listen failed!", LogSeverity::Error);
			return -1;
		}
		else {
			ImGuiLogManager::AddLog("Socket", std::string("Socket '") + nomSocket->name + "' is now listening with a backlog of " + std::to_string(backlog) + "!", LogSeverity::Info);
		}
		return 0;
	}

	int NomSocketManager::Listen(const char* socketName, int backlog)
	{
		int socketId = SocketNameToId(socketName);
		if (socketId == -1) {
			return -1;
		}
		return Listen(socketId, backlog);
	}

	int NomSocketManager::CloseSocket(int socketId)
	{
		if (socketId < 0 || socketId >= socketCount || sockets[socketId] == nullptr) {
			ImGuiLogManager::AddLog("Socket", "Invalid socket ID!", LogSeverity::Error);
			return -1;
		}
		NomSocket* nomSocket = sockets[socketId];
		if (nomSocket->getStatus() == 0) {
			ImGuiLogManager::AddLog("Socket", "Socket is already disconnected!", LogSeverity::Error);
			return -1;
		}
		// Close the socket
		closesocket(*nomSocket->socket);
		SSL_free(nomSocket->ssl);
		nomSocket->ssl = nullptr;
		nomSocket->setStatus(0); // Mark as disconnected
		return 0;
	}

	int NomSocketManager::CloseSocket(const char* socketName)
	{
		int socketId = SocketNameToId(socketName);
		if (socketId == -1) {
			return -1;
		}
		return CloseSocket(socketId);
	}

	int NomSocketManager::RemoveSocket(const char* socketName)
	{
		int socketId = SocketNameToId(socketName);
		if (socketId == -1) {
			return -1;
		}
		return RemoveSocket(socketId);
	}

	int NomSocketManager::RemoveSocket(int socketId)
	{
		if (socketId < 0 || socketId >= socketCount || sockets[socketId] == nullptr) {
			ImGuiLogManager::AddLog("Socket", "Invalid socket ID!", LogSeverity::Error);
			return -1;
		}
		if (sockets[socketId]->getStatus() == 1) {
			CloseSocket(socketId);
		}
		delete sockets[socketId];
		sockets[socketId] = nullptr;
		socketCount--;
		return 0;
	}

	int NomSocketManager::GetSocketStatus(const char* socketName)
	{
		int socketId = SocketNameToId(socketName);
		if (socketId == -1) {
			return -1;
		}
		if (sockets[socketId] != nullptr) {
			return sockets[socketId]->getStatus();
		}
		return -1;
	}

	int NomSocketManager::CloseAllSockets()
	{
		for (int i = 0; i < socketCount; i++) {
			if (sockets[i] != nullptr && sockets[i]->getStatus() == 1) {
				CloseSocket(sockets[i]->name);
			}
		}
		return 0;
	}
}