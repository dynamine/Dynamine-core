#pragma once
#include <minwindef.h>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <minwinbase.h>
#include <loguru.hpp>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")

template<class Owner>
class TcpServer
{
	// OnClientConnect function that Owner must have
	typedef DWORD (WINAPI Owner::*OnClientConnect)(SOCKET);

	struct ThreadData
	{
		Owner* instance;
		OnClientConnect callback;
		SOCKET client_socket;
	};

public:
	TcpServer(PCSTR port)
	{
		// Initialize base data
		server_socket_ = INVALID_SOCKET;
		socket_addrinfo_ = NULL;
		initialized_ = FALSE;
		running_ = FALSE;
		terminate_ = FALSE;

		InitializeCriticalSection(&mutex_);

		// Setup WSA data
		if (WSAStartup(MAKEWORD(2, 2), &wsa_data_) != 0)
		{
			LOG_F(ERROR, "WSAStartup Failure: %08X", WSAGetLastError());
			goto FAILED;
		}

		// Setup address hints to get teh hostname
		ZeroMemory(&socket_addrhints_, sizeof(struct addrinfo));
		socket_addrhints_.ai_family = AF_INET;
		socket_addrhints_.ai_socktype = SOCK_STREAM;
		socket_addrhints_.ai_protocol = IPPROTO_TCP;
		socket_addrhints_.ai_flags = AI_PASSIVE;

		// Get server address and port, we know that we want 0.0.0.0 and port,
		// however this is a safer route to go since hostname finding is not guranteed
		if ((getaddrinfo(NULL, port, &socket_addrhints_, &socket_addrinfo_)) != 0)
		{
			LOG_F(ERROR, "getaddrinfo() Failure: %08X", GetLastError());
			goto FAILED;
		}

		// Create our server socket
		if ((server_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
		{
			LOG_F(ERROR, "ServerSocket creation failed: %08X", WSAGetLastError());
			goto FAILED;
		}

		// Bind socket to hostname:port
		if ((bind(server_socket_, socket_addrinfo_->ai_addr, int(socket_addrinfo_->ai_addrlen))) == SOCKET_ERROR)
		{
			LOG_F(ERROR, "Failed to bind to 0.0.0.0:%s with error: %08X", port, WSAGetLastError());
			goto FAILED;
		}

		// We did it, and are ready to start listening
		initialized_ = TRUE;
		return;

	FAILED:
		Cleanup();
	}

	~TcpServer()
	{
		// Cleanup socket items
		Cleanup();

		// Remote critical section
		DeleteCriticalSection(&mutex_);
	}

	static DWORD WINAPI ThreadOnClientConnect(LPVOID data)
	{
		ThreadData* td = static_cast<ThreadData*>(data);

		return (td->instance->*td->callback)(td->client_socket);
	}

	VOID Start(Owner* owner, OnClientConnect client_connect)
	{
		// Start listening on socket for requests
		if ((listen(server_socket_, SOMAXCONN)) == SOCKET_ERROR)
		{
			LOG_F(ERROR, "Listening on socket failed with: %08X", WSAGetLastError());
			goto FAILED;
		}

		// Server is now running
		running_ = TRUE;

		// Lets accept some client sockets
		while (!IsTerminated())
		{
			// Passing null for now on the sockaddr_in items since they are not needed
			SOCKET client_socket = accept(server_socket_, NULL, NULL);

			// Pass this thread data for the caller to process
			ThreadData* thread_data = new ThreadData;
			thread_data->instance = owner;
			thread_data->client_socket = client_socket;
			thread_data->callback = client_connect;
			// Handle the client socket and let the server forget about it
			HANDLE handle = CreateThread(NULL, NULL, ThreadOnClientConnect, LPVOID(thread_data), NULL, NULL);
			CloseHandle(handle);
		}

	FAILED:
		running_ = FALSE;
		Cleanup();
	}

	VOID Stop()
	{
		EnterCriticalSection(&mutex_);
		terminate_ = TRUE;
		LeaveCriticalSection(&mutex_);
	}

	BOOL IsRunning() { return running_; }
	BOOL IsInitialized() { return initialized_; }
	BOOL IsTerminated()
	{
		// TODO: Test and verify termination
		BOOL res;
		EnterCriticalSection(&mutex_);
		res = terminate_;
		LeaveCriticalSection(&mutex_);
		return res;
	}

protected:
	VOID Cleanup()
	{
		// If we have a server socket and WSA has been initialized then clean it up
		if (server_socket_ != INVALID_SOCKET && WSAGetLastError() != WSANOTINITIALISED)
			WSACleanup();

		// If we have a server socket then clean it up
		if (server_socket_ != INVALID_SOCKET)
			closesocket(server_socket_);

		// if there is a pointer to addrinfo then clean it up
		if (socket_addrinfo_)
			freeaddrinfo(socket_addrinfo_);
	}

private:
	SOCKET                server_socket_;     // TCP server socket
	WSADATA                    wsa_data_;     // WSA data for windows socket
	struct addrinfo*    socket_addrinfo_;     // Win32 socket address information from getaddrinfo()
	struct addrinfo    socket_addrhints_;     // Give known info into getaddrinfo()
	BOOL                      terminate_;     // Should terminate the server immediatly
	BOOL                    initialized_;     // Did initialization succeed
	BOOL                        running_;     // Gives status of server
	CRITICAL_SECTION              mutex_;     // Lets us lock some stuff down
};
