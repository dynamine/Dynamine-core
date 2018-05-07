#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <memory>
#include "Public/WinService/DynamineDaemon.h"

#pragma comment(lib, "advapi32.lib")


int __cdecl wmain(int argc, PWSTR argv[])
{
	auto daemon = std::make_shared<DynamineDaemon>();
	daemon->Run(                        // Installation Params:
		SERVICE_START_TYPE,				// Service start type
		PWSTR(SERVICE_DEPENDENCIES),    // Service dependencies to install
		PWSTR(SERVICE_ACCOUNT),         // Account to install service under
		PWSTR(SERVICE_PASSWORD)         // Password for account
	); 

	return 0;
}


