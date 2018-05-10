#pragma once
#include "Interface/IService.h"
#include "../Network/TcpServer.hpp"
#include <loguru.hpp>
#include <json.hpp>

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

CONST BYTE CMD_START_MINER[MAXCHAR] = "start-miner";
CONST BYTE CMD_STOP_MINER[MAXCHAR] = "stop-miner";
CONST BYTE CMD_RESOURCES[MAXCHAR] = "resources";
CONST BYTE CMD_STATS[MAXCHAR] = "stats";

class MiningService : public IService
 {
public:
	MiningService();
	~MiningService();

protected:
	VOID OnStart(DWORD argc, LPTSTR* argv) override;
	VOID OnStop() override;

	VOID DaemonThread();

	DWORD OnClientConnect(SOCKET client_socket);

	static DWORD WINAPI MainThread(LPVOID thread_data);

private:
	HANDLE                        daemon_thread_;         // Daemon thread handle where the actual work happens
	int                               exit_code_;			// Windows exit code
	TcpServer<MiningService>*        cmd_server_;         // Command TCP Server to communicate with the daemon
 };
