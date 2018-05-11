#pragma once
#include "Interface/IService.h"
#include "../Network/TcpServer.hpp"
#include <loguru.hpp>
#include <json.hpp>
#include <string>
#include <random>

// For json
using namespace nlohmann;

// Internal service name
#define SERVICE_NAME L"DynamineDaemon"

// External service name
#define DISPLAY_NAME L"Dynamine Mining Daemon"

// Service start options
#define SERVICE_START_TYPE       SERVICE_DEMAND_START

// List of service dependencies - "dep1\0dep2\0\0"
#define SERVICE_DEPENDENCIES     L""

// The name of the account under which the service should run
#define SERVICE_ACCOUNT          L"NT AUTHORITY\\LocalService"

// The password to the service account name
#define SERVICE_PASSWORD         NULL

// Command server port
#define PORT  "4001"

static const char* CMD_START_MINER = "start-miner";
static const char* CMD_STOP_MINER  =  "stop-miner";
static const char* CMD_RESOURCES   =  "resources";
static const char* CMD_STATS       =  "hashRate";

class MiningService : public IService
 {
	struct Miner
	{
		std::string command;   // start, stop, restart etc.
		std::string resource;  // localhost.gpu0
		std::string state;
		HANDLE      thread_handle;
		PROCESS_INFORMATION* miner_process;
	};

public:
	MiningService();
	~MiningService();

	DWORD WINAPI OnClientConnect(SOCKET client_socket);

protected:
	VOID OnStart(DWORD argc, LPTSTR* argv) override;
	VOID OnStop() override;

	VOID DaemonThread();
	VOID MinerThread();
	VOID CommandServerThread();

	static DWORD WINAPI MinerThreadProxy(LPVOID thread_data);
	static DWORD WINAPI MiningThread(LPVOID thread_data);
	static DWORD WINAPI CommandThread(LPVOID thread_data);

private:
	HANDLE                           cmd_thread_;         // Command thread handle where commands to control daemon thread occur
	HANDLE                        daemon_thread_;         // Daemon thread handle where the actual work happens
	int                               exit_code_;	      // Windows exit code
	TcpServer<MiningService>*        cmd_server_;         // Command TCP Server to communicate with the daemon
	CRITICAL_SECTION					  mutex_;         // lock read write between threads
	Miner*                               miners_;         // Miner configurations

 };
