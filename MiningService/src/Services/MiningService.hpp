#pragma once
#include "Interface/IService.h"
#include "../Network/TcpServer.hpp"
#include <loguru.hpp>
#include <json.hpp>
#include <string>
#include <map>
#include "../Network/TcpConnection.hpp"
#include <iostream>

// For json
using namespace nlohmann;

// Internal service name
#define SERVICE_NAME "DynamineDaemon"

// External service name
#define DISPLAY_NAME "Dynamine Mining Daemon"

// Service start options
#define SERVICE_START_TYPE       SERVICE_DEMAND_START

// List of service dependencies - "dep1\0dep2\0\0"
#define SERVICE_DEPENDENCIES     ""

// The name of the account under which the service should run
#define SERVICE_ACCOUNT          "NT AUTHORITY\\LocalService"

// The password to the service account name
#define SERVICE_PASSWORD         NULL

// Command server port
#define PORT  "4001"

// Internal daemon com port
#define COM_PORT "4002"

// Base API starting port
#define API_BASE_PORT 4050
#define API_BASE_HOST "127.0.0.1"

static const char* CMD_START_MINER =  "startMiner";
static const char* CMD_STOP_MINER  =  "stopMiner";
static const char* CMD_RESOURCES   =  "resources";
static const char* CMD_STATS       =  "hashRate";

// TODO: Place this somewhere better
typedef std::basic_string<TCHAR> tstring;

class MiningService : public IService
 {
	struct Miner
	{
		PTSTR                        command;  // command to run the miner
		tstring                     resource;  // localhost.gpu0
		BOOL                         running;  // stopped, running
		HANDLE                        thread;  // handle to process thread
		PROCESS_INFORMATION*         process;  // Process info
		PTSTR               api_gateway_port;  // 127.0.0.1:[4051]
		CRITICAL_SECTION		       mutex;  // Lock resources

		Miner()
		{
			InitializeCriticalSection(&mutex);
			process = new PROCESS_INFORMATION;
		}

		~Miner()
		{
			DeleteCriticalSection(&mutex);
			free(command);
			free(api_gateway_port);

			CloseHandle(thread);
			delete process;
		}
	};

	struct MinerThreadData
	{
		MiningService* instance;    // The current instance of mining service to be ran
		Miner* miner;              // The entire miner command formatted to run as a Process
	};

public:
	MiningService();
	~MiningService();

	DWORD WINAPI OnClientConnect(SOCKET client_socket);

protected:
	VOID OnStart(DWORD argc, LPTSTR* argv) override;
	VOID OnStop() override;
	VOID OnShutdown() override
	{
		OnStop();
	}

	BOOL StartMiner(Miner* miner);
	BOOL StopMiner(tstring resource);

	std::vector<std::string> GetDevices();

	// TODO: Implement this along with other setters/getters for thread locking
	Miner* GetMiner(tstring resource);
	BOOL AddMiner(Miner* miner);
	BOOL RemoveMiner(tstring resource);

	int GetHashrate(tstring resource);

	// Controls different instances of coin miner processes and
	// relays commands to their APIs
	DWORD WINAPI MinerDispatchThread(SOCKET client_socket);

	// Miner thread that holds the process to the miner
	VOID MinerThread(Miner* miner);

	// Handles TCP commands and runs the tcp server
	VOID CommandServerThread();

	// TODO: Deprecate this tcp server
	// Miner communication server thread that runs the TCP server
	VOID MinerComThread();

	// Proxies that are used to call the actual 
	static DWORD WINAPI MinerThreadProxy(LPVOID thread_data);
	static DWORD WINAPI MinerDispatchProxy(LPVOID thread_data);
	static DWORD WINAPI CommandThreadProxy(LPVOID thread_data);

private:
	HANDLE                           cmd_thread_;         // Command thread handle where commands to control daemon thread occur
	HANDLE                miner_dispatch_thread_;         // Daemon thread handle where the actual work happens
	int                               exit_code_;	      // Windows exit code
	TcpServer<MiningService>*        cmd_server_;         // Command TCP Server to communicate with the daemon
	TcpServer<MiningService>*  miner_cmd_server_;         // It sucks but this will let us scale for now
	CRITICAL_SECTION					  mutex_;         // lock read write to any miner information
	std::map<tstring, Miner*>            miners_;         // Miner configurations
	std::vector<std::string>            devices_;

 };
