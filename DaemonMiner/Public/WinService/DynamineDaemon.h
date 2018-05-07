#pragma once
#include "IService.h"

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

class DynamineDaemon : public IService
 {
public:
	DynamineDaemon();

protected:
	VOID OnStart(DWORD argc, LPTSTR* argv) override;
	VOID OnStop() override;
 };
