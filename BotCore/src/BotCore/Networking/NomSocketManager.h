#ifndef __NOMSOCKETMANAGER_H__
#define __NOMSOCKETMANAGER_H__
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <winsock.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define ACCEPTED_CONNECTION "AcceptedConnection"

namespace NomBotCore {
	struct NomSocket {
		char* name;
		int id;
		int type; // 1 = TCP, 2 = UDP
		int protocol; // 1 = IPv4, 2 = IPv6
		int status; // 1 = connected, 0 = disconnected
		char* host;
		char* address;
		int port;
		SOCKET* socket;
		SSL* ssl;

		NomSocket(int socketId, int socketType, int socketProtocol, const char* socketAddress, int socketPort)
			: id(socketId), type(socketType), protocol(socketProtocol), status(0), port(socketPort), socket(nullptr), address(nullptr), ssl(nullptr) {
			address = new char[strlen(socketAddress) + 1];
			strcpy_s(address, strlen(socketAddress) + 1, socketAddress);
		}
		NomSocket(int socketId, int socketType, int socketProtocol)
			: id(socketId), type(socketType), protocol(socketProtocol), status(0), socket(nullptr), address(nullptr), ssl(nullptr) {}
		~NomSocket() {
			if (address) delete[] address;
			if (name) delete[] name;
			if (socket) delete socket;
			if (ssl) SSL_free(ssl);
		}
		//Get socket by id
		int getId() const { return id; }
		int getType() const { return type; }
		int getProtocol() const { return protocol; }
		int getStatus() const { return status; }
		const char* getAddress() const { return address; }
		int getPort() const { return port; }
		void setStatus(int socketStatus) { status = socketStatus; }
	};

	class NomSocketManager {
	public:
		NomSocketManager();
		~NomSocketManager();
		int Initialize();
		int BindSocket(const char* socketName, const char* address, int port);
		int AcceptConnection(const char* socketName, char* clientAddress, int* clientPort);
		int CreateSocket(const char* name, int type, int protocol);
		int ConnectSocket(const char* socketName, const char* address, int port, bool sslData = false);
		int SendData(const char* socketName, const char* data, int length, bool sslData = false);
		int ReceiveData(const char* socketName, char* buffer, int length, bool sslData = false);
		int Listen(const char* socketName, int backlog);
		int CloseSocket(const char* socketName);
		int CloseAllSockets();
		int RemoveSocket(const char* socketName);
		int GetSocketStatus(const char* socketName);
		int SendPongFrame();
	private:
		int socketCount;
		int maxSockets;
		std::vector<NomSocket*> sockets;

		int BindSocket(int socketId, const char* address, int port);
		int AcceptConnection(int socketId, char* clientAddress, int* clientPort);
		int ConnectSocket(int socketId, const char* address, int port, bool sslData = false);
		int SendData(int socketId, const char* data, int length, bool sslData = false);
		int ReceiveData(int socketId, char* buffer, int length, bool sslData = false);
		int Listen(int socketId, int backlog);
		int CloseSocket(int socketId);
		int RemoveSocket(int socketId);

		int SocketNameToId(const char* name);
	protected:
		SSL_CTX* sslCtx;
	};
}

#endif 
