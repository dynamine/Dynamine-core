#pragma once


#include <minwindef.h>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <minwinbase.h>
#include <loguru.hpp>
#include <minwinbase.h>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")



class TcpClient
{

public:
	TcpClient(PSTR port, PSTR hostname=NULL)
	{
		// Initialize base data
		client_socket_ = INVALID_SOCKET;
		initialized_ = FALSE;
		socket_addrinfo_ = NULL;

		// Setup WSA data
		if (WSAStartup(MAKEWORD(2, 2), &wsa_data_) != 0)
		{
			LOG_F(ERROR, "WSAStartup Failure: %08X", WSAGetLastError());
			goto FAILED;
		}

		// Create our server socket
		if ((client_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
		{
			LOG_F(ERROR, "ClientSocket creation failed: %08X", WSAGetLastError());
			goto FAILED;
		}

		// TODO: Figure out how to specify hostname
		ZeroMemory(&socket_addrhints_, sizeof(struct addrinfo));
		socket_addrhints_.ai_family = AF_INET;
		socket_addrhints_.ai_socktype = SOCK_STREAM;
		socket_addrhints_.ai_protocol = IPPROTO_TCP;

		if ((getaddrinfo(NULL, port, &socket_addrhints_, &socket_addrinfo_)) != 0)
		{
			LOG_F(ERROR, "getaddrinfo() Failure: %08X", GetLastError());
			goto FAILED;
		}

		if(connect(client_socket_, socket_addrinfo_->ai_addr, int(socket_addrinfo_->ai_addrlen)) != 0)
		{
			LOG_F(ERROR, "Failed to connect to the server: %08X", GetLastError());
			goto FAILED;
		}

		// We did it, and are ready to start listening
		initialized_ = TRUE;
		return;

	FAILED:
		Cleanup();
	}

	~TcpClient()
	{
		// Cleanup socket items
		Cleanup();
	}

	BOOL IsInitialized() { return initialized_; }
	SOCKET GetSocket() { return client_socket_; }

protected:
	VOID Cleanup()
	{
		// If we have a server socket and WSA has been initialized then clean it up
		if (client_socket_ != INVALID_SOCKET && WSAGetLastError() != WSANOTINITIALISED)
			WSACleanup();

		// If we have a server socket then clean it up
		if (client_socket_ != INVALID_SOCKET)
		{
			closesocket(client_socket_);
			client_socket_ = INVALID_SOCKET;
		}

		if(socket_addrinfo_ != NULL)
			freeaddrinfo(socket_addrinfo_);
	}

private:
	SOCKET                client_socket_;     // TCP client socket
	WSADATA                    wsa_data_;     // WSA data for windows socket
	struct addrinfo*    socket_addrinfo_;     // Win32 socket address information from getaddrinfo()
	struct addrinfo    socket_addrhints_;     // Give known info into getaddrinfo()
	BOOL                    initialized_;     // Did initialization succeed
};
