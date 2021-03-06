#define NOMINMAX                    // Disable min/max from windows.h

#define _WINSOCKAPI_                // Disable windows.h including winsock apis

#define _CRT_SECURE_NO_WARNINGS     // Disable errors on deprecated functions

#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <memory>
#include "Services/MiningService.hpp"

#pragma comment(lib, "advapi32.lib")

#pragma warning(disable:4996)		// _CRT_SECURE_NO_WARNINGS

#define LOGURU_IMPLEMENTATION 1
#include <loguru.hpp>

int __cdecl _tmain(int argc, PTSTR argv[])
{

	loguru::init(argc, argv);

	// Put every log message in "everything.log":
	loguru::add_file("everything.log", loguru::Append, loguru::Verbosity_MAX);

	// Only log INFO, WARNING, ERROR and FATAL to "latest_readable.log":
	loguru::add_file("latest_readable.log", loguru::Truncate, loguru::Verbosity_INFO);

	loguru::g_stderr_verbosity = 10;

	auto daemon = std::make_shared<MiningService>();
	daemon->Run(                        // Installation Params:
		SERVICE_START_TYPE,				// Service start type
		PTSTR(SERVICE_DEPENDENCIES),    // Service dependencies to install
		PTSTR(SERVICE_ACCOUNT),         // Account to install service under
		PTSTR(SERVICE_PASSWORD)         // Password for account
	); 

	return 0;
}


