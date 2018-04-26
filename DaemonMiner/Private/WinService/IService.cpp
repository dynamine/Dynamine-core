#include "..\..\Public\WinService\IService.h"

#pragma comment(lib, "advapi32.lib")

// Create our singleton instance
IService *IService::instance_;

IService::IService(CONST PWCHAR name, CONST PWCHAR display_name, CONST DWORD accepted_controls, Logger* log_instance)
{
	// Clear memory
	::ZeroMemory(PVOID(&status_), sizeof(SERVICE_STATUS));
	::ZeroMemory(PVOID(service_name_), sizeof(service_name_));
	::ZeroMemory(PVOID(display_name_), sizeof(display_name_));

	// Initialize variables
	InitializeCriticalSection(&status_mutex_);

	::wcscpy_s(service_name_, name);
	::wcscpy_s(display_name_, display_name);

	status_handle_ = NULL;
	exit_event_ = NULL;
	debug_ = FALSE;

	logger_ = log_instance;

	exit_event_ = ::CreateEvent(
		NULL,   // Event attributes
		TRUE,   // Allow manual reset
		FALSE,  // Initial state
		NULL    // Event Name
	);

	// This event is required to exit, make sure it exists
	_ASSERTE(exit_event_ != NULL);

	// Set our initial status
	status_.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	status_.dwCurrentState = SERVICE_STOPPED;

	// Set the accepted controls this service takes
	status_.dwControlsAccepted = accepted_controls;

	status_.dwWin32ExitCode = NO_ERROR;
	status_.dwServiceSpecificExitCode = 0;
	status_.dwCheckPoint = 0;
	status_.dwWaitHint = 0;

	// When the service is started then set the singleton instance to current one
	instance_ = this;
}

IService::~IService()
{
	DeleteCriticalSection(&status_mutex_);
}

BOOL IService::Install(CONST DWORD start_type, PWSTR dependencies, PWSTR account, PWSTR password)
{
	TCHAR path[MAX_PATH];
	SC_HANDLE service_manager = NULL;
	SC_HANDLE service = NULL;
	BOOL ret = FALSE;

	if (GetModuleFileName(NULL, path, MAX_PATH) == 0)
	{
		wprintf(L"GetModuleFileName failed w/err 0x%08lx\n", GetLastError());
		goto Cleanup;
	}

	// Open the local default service control manager database
	service_manager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE);
	if (service_manager == NULL) 
	{
		wprintf(L"OpenSCManager failed w/err 0x%08lx\n", GetLastError());
		goto Cleanup;
	}

	// Install the service into SCM by calling CreateService
	service = CreateService(
		service_manager,                // Service-manager database
		service_name_,                  // Name of service
		display_name_,                  // Name to display
		SERVICE_QUERY_STATUS,           // Desired access
		SERVICE_WIN32_OWN_PROCESS,      // Service type
		start_type,                     // Service start type
		SERVICE_ERROR_NORMAL,           // Error control type
		path,                           // Service's binary
		NULL,                           // No load ordering group
		NULL,                           // No tag identifier
		dependencies,                   // Dependencies
		account,                        // Service running account
		password                        // Password of the account
	);

	if (service == NULL)
	{
		wprintf(L"CreateService failed w/err 0x%08lx\n", GetLastError());
		goto Cleanup;
	}

	ret = TRUE;
	wprintf(L"%s is installed.\n", service_name_);

Cleanup:
	// Centralized cleanup for all allocated resources.
	if (service_manager)
	{
		CloseServiceHandle(service_manager);
		service_manager = NULL;
	}
	if (service)
	{
		CloseServiceHandle(service);
		service = NULL;
	}

	return ret;
}

BOOL IService::Uninstall()
{
	SC_HANDLE service_manager = NULL;
	SC_HANDLE service = NULL;
	SERVICE_STATUS service_status = {};
	BOOL ret = FALSE;

	// Open the local default service control manager database
	service_manager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (service_manager == NULL)
	{
		wprintf(L"OpenSCManager failed w/err 0x%08lx\n", GetLastError());
		goto Cleanup;
	}

	// Open the service with delete, stop, and query status permissions
	service = OpenService(
		service_manager, 
		service_name_, 
		SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE
	);

	if (service == NULL)
	{
		wprintf(L"OpenService failed w/err 0x%08lx\n", GetLastError());
		goto Cleanup;
	}

	// Try to stop the service
	if (ControlService(service, SERVICE_CONTROL_STOP, &service_status))
	{
		wprintf(L"Stopping %s.", service_name_);
		Sleep(1000);

		while (QueryServiceStatus(service, &service_status))
		{
			if (service_status.dwCurrentState == SERVICE_STOP_PENDING)
			{
				wprintf(L".");
				Sleep(1000);
			}
			else break;
		}

		if (service_status.dwCurrentState == SERVICE_STOPPED)
		{
			wprintf(L"\n%s is stopped.\n", service_name_);
		}
		else
		{
			wprintf(L"\n%s failed to stop.\n", service_name_);
		}
	}

	// Now remove the service by calling DeleteService.
	if (!DeleteService(service))
	{
		wprintf(L"DeleteService failed w/err 0x%08lx\n", GetLastError());
		goto Cleanup;
	}

	ret = TRUE;
	wprintf(L"%s is removed.\n", service_name_);

Cleanup:
	// Centralized cleanup for all allocated resources.
	if (service_manager)
	{
		CloseServiceHandle(service_manager);
		service_manager = NULL;
	}
	if (service)
	{
		CloseServiceHandle(service);
		service = NULL;
	}
	return ret;
}

DWORD IService::Start()
{
	// Get command line arguments and set configs with them
	int num_args = 0;
	// Get unicode command line arguments just in case
	LPWSTR* wchar_cmd_args = ::CommandLineToArgvW(::GetCommandLineW(), &num_args);
	// Loop over arguments
	for (int i = 0; i < num_args; i++) 
	{
		LPWSTR current_arg = wchar_cmd_args[i];
		// If argument is a flag passed to the service then check it
		if (current_arg[0] == L'/' || current_arg[0] == L'-') 
		{
			// If the flag is debug then set the service to debug mode
			if (::_wcsicmp(&current_arg[1], L"debug") == 0)
				debug_ = TRUE;
		}
	}

	// If we are debugging then just run the service_main, no need to attatch to SVC manager
	if (debug_)
	{
		IService::service_main(DWORD(num_args), wchar_cmd_args);
	}
	// This is the actual call that will happen that will run the service properly in windows
	else 
	{
		
		SERVICE_TABLE_ENTRYW st[] =
		{   
			{ service_name_, service_main },
			{ NULL, NULL }
		};
		if (::StartServiceCtrlDispatcherW(st) == 0)
			status_.dwWin32ExitCode = GetLastError();
	}

	::LocalFree(wchar_cmd_args);

	return status_.dwWin32ExitCode;
}

VOID IService::SetStatus(DWORD status_code, DWORD win32ExitCode=NULL, DWORD waitHint=NULL)
{
	EnterCriticalSection(&status_mutex_);
	static DWORD checkpoint = 1;

	status_.dwCurrentState = status_code;

	// Optional status
	if (win32ExitCode)
		status_.dwWin32ExitCode = win32ExitCode;
	if (waitHint)
		status_.dwWaitHint = waitHint;
	
	// If the service is starting remote controls, else accept stop control
	if (status_code == SERVICE_START_PENDING)
		status_.dwControlsAccepted = 0;
	else
		status_.dwControlsAccepted = SERVICE_ACCEPT_STOP;

	// If running or stopped then reset checkpoint otherwise increment global one
	if ((status_code == SERVICE_RUNNING) || (status_code == SERVICE_STOPPED))
		status_.dwCheckPoint = 0;
	else
		status_.dwCheckPoint = checkpoint;
	
	// If the service is not actually running then we are done
	if (IsDebug())
		return;

	::SetServiceStatus(status_handle_, &status_);
	LeaveCriticalSection(&status_mutex_);
}

DWORD IService::Run()
{
	// Service status to running
	SetStatus(SERVICE_RUNNING);

	// Wait for the service exit event to be set before this method exits
	::WaitForSingleObject(exit_event_, INFINITE);

	return 0;
}

DWORD IService::_service_control_handler(DWORD service_control, DWORD event_type, LPVOID event_data)
{
	_ASSERTE(instance_ != NULL);

	DWORD ret = NO_ERROR;

	switch (service_control)
	{
	case SERVICE_CONTROL_SHUTDOWN:
		instance_->onShutdown();
		break;
	case SERVICE_CONTROL_PAUSE:
		instance_->onPause();
		break;
	case SERVICE_CONTROL_CONTINUE:
		instance_->onContinue();
		break;
	case SERVICE_CONTROL_INTERROGATE:
		instance_->onInterrogate();
		break;
	case SERVICE_CONTROL_DEVICEEVENT:
		ret = instance_->onDeviceEvent(event_type, static_cast<PDEV_BROADCAST_HDR>(event_data));
		break;
	case SERVICE_CONTROL_HARDWAREPROFILECHANGE:
		ret = instance_->onHardwareProfileChange(event_type);
		break;
	// Only on WINXP or higher
	#if (_WIN32_WINNT >= _WIN32_WINNT_WINXP)
	case SERVICE_CONTROL_SESSIONCHANGE:
		ret = instance_->onSessionChange(event_type, static_cast<PWTSSESSION_NOTIFICATION>(event_data));
		break;
	#endif
	// Only on WS03 or higher
	#if (_WIN32_WINNT >= _WIN32_WINNT_WS03)
	case SERVICE_CONTROL_POWEREVENT:
		ret = instance_->onPowerEvent(event_type, static_cast<POWERBROADCAST_SETTING*>(event_data));
		break;
	#endif
	// Only on Vista or higher
	#if (_WIN32_WINNT >= _WIN32_WINNT_VISTA)
	case SERVICE_CONTROL_PRESHUTDOWN:
		ret = instance_->onPreShutdown(static_cast<LPSERVICE_PRESHUTDOWN_INFO>(event_data));
		break;
	#endif
	default:
		instance_->onUnknownRequest(service_control);
	}

	return ret;
}

// What is called in the windows kernel on the singleton
VOID IService::service_main(DWORD argc, LPTSTR* argv)
{
	instance_->_service_main(argc, argv);
}

// Actual implementation called from singleton
VOID IService::_service_main(DWORD argc, LPTSTR * argv)
{
	// TODO: Check if we actually need to set this since our setServiceStatus function should handle it for us
	status_.dwCurrentState = SERVICE_START_PENDING;

	if (IsDebug())
	{
		// Register the console control handler
		::SetConsoleCtrlHandler(PHANDLER_ROUTINE(IService::console_control_handler), TRUE);
		_putws(L"Press Ctrl+C or Ctrl+Break to quit...");
	}
	else 
{
		// register the service handler routine
		status_handle_ = ::RegisterServiceCtrlHandlerEx(service_name_, LPHANDLER_FUNCTION_EX(service_control_handler), (LPVOID)this);
		if (status_handle_ == NULL)
			return;
	}

	SetStatus(SERVICE_START_PENDING);

	status_.dwWin32ExitCode = S_OK;
	status_.dwCheckPoint = 0;
	status_.dwWaitHint = 0;

	// When the Run function returns, the service has stopped.
	status_.dwWin32ExitCode = Run();

	SetStatus(SERVICE_STOPPED);
}

// Set as a reference for the windows kernel calls
BOOL IService::console_control_handler(DWORD ctrl_type)
{
	return instance_->_console_control_handler(ctrl_type);
}

DWORD IService::service_control_handler(DWORD service_control, DWORD event_type, LPVOID event_data, LPVOID service_context)
{
	return reinterpret_cast<IService*>(service_context)->_service_control_handler(service_control, event_type, event_data);
}

// Actual implementation called from singleton
BOOL IService::_console_control_handler(DWORD ctrl_type)
{
	// If not any exit event then ignore
	if (!(ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT || ctrl_type == CTRL_SHUTDOWN_EVENT))
		return FALSE;

	onStop();

	return FALSE;
}
