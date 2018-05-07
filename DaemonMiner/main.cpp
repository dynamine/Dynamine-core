#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <memory>
#include "Public/WinService/DynamineDaemon.h"

#pragma comment(lib, "advapi32.lib")

#pragma warning(disable:4996)		// _CRT_SECURE_NO_WARNINGS

#define LOGURU_IMPLEMENTATION 1
#include "Public/Logger/Loguru.h"

int __cdecl _tmain(int argc, PWSTR argv[])
{
	//loguru::init(argc, argv);

	// Put every log message in "everything.log":
	loguru::add_file("everything.log", loguru::Append, loguru::Verbosity_MAX);

	// Only log INFO, WARNING, ERROR and FATAL to "latest_readable.log":
	loguru::add_file("latest_readable.log", loguru::Truncate, loguru::Verbosity_INFO);

	auto daemon = std::make_shared<DynamineDaemon>();
	daemon->Run(                        // Installation Params:
		SERVICE_START_TYPE,				// Service start type
		PWSTR(SERVICE_DEPENDENCIES),    // Service dependencies to install
		PWSTR(SERVICE_ACCOUNT),         // Account to install service under
		PWSTR(SERVICE_PASSWORD)         // Password for account
	); 

	return 0;
}


