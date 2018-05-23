#pragma once
#include "Interface/IService.h"
#include "../Network/TcpServer.hpp"
#include <loguru.hpp>
#include <json.hpp>
#include <string>
#include <random>
#include <map>

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

static const char* CMD_START_MINER =  "startMiner";
static const char* CMD_STOP_MINER  =  "stopMiner";
static const char* CMD_RESOURCES   =  "resources";
static const char* CMD_STATS       =  "hashRate";

class MiningService : public IService
 {
	struct Miner
	{
		PTSTR                        command;  // command to run the miner
		PTSTR                       resource;  // localhost.gpu0
		PTSTR                          state;  // stopped, running
		HANDLE                        thread;  // handle to process thread
		PROCESS_INFORMATION*         process;  // Process info
		PTSTR                    api_gateway;  // http://localhost:4004

		~Miner()
		{
			free(command);
			free(resource);
			free(state);
			free(api_gateway);
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

	BOOL StartMiner(Miner* miner);

	PCHAR* GetDevices();

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
	std::map<PTCHAR, Miner*>              miners_;        // Miner configurations

 };
