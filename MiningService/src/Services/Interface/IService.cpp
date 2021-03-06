#include "IService.h"

// TODO: Check if we can nil this
#pragma comment(lib, "advapi32.lib")

// Create our singleton instance
IService *IService::instance_;

// TODO: Figure out how to move this to Tools and fix cyclic deps
LPSTR* CommandLineToArgvA(LPSTR lpCmdLine, INT *pNumArgs)
{
	int retval;
	retval = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, lpCmdLine, -1, NULL, 0);
	if (!SUCCEEDED(retval))
		return NULL;

	LPWSTR lpWideCharStr = (LPWSTR)malloc(retval * sizeof(WCHAR));
	if (lpWideCharStr == NULL)
		return NULL;

	retval = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, lpCmdLine, -1, lpWideCharStr, retval);
	if (!SUCCEEDED(retval))
	{
		free(lpWideCharStr);
		return NULL;
	}

	int numArgs;
	LPWSTR* args;
	args = CommandLineToArgvW(lpWideCharStr, &numArgs);
	free(lpWideCharStr);
	if (args == NULL)
		return NULL;

	int storage = numArgs * sizeof(LPSTR);
	for (int i = 0; i < numArgs; ++i)
	{
		BOOL lpUsedDefaultChar = FALSE;
		retval = WideCharToMultiByte(CP_ACP, 0, args[i], -1, NULL, 0, NULL, &lpUsedDefaultChar);
		if (!SUCCEEDED(retval))
		{
			LocalFree(args);
			return NULL;
		}

		storage += retval;
	}

	LPSTR* result = (LPSTR*)LocalAlloc(LMEM_FIXED, storage);
	if (result == NULL)
	{
		LocalFree(args);
		return NULL;
	}

	int bufLen = storage - numArgs * sizeof(LPSTR);
	LPSTR buffer = ((LPSTR)result) + numArgs * sizeof(LPSTR);
	for (int i = 0; i < numArgs; ++i)
	{
		_ASSERTE(bufLen > 0);
		BOOL lpUsedDefaultChar = FALSE;
		retval = WideCharToMultiByte(CP_ACP, 0, args[i], -1, buffer, bufLen, NULL, &lpUsedDefaultChar);
		if (!SUCCEEDED(retval))
		{
			LocalFree(result);
			LocalFree(args);
			return NULL;
		}

		result[i] = buffer;
		buffer += retval;
		bufLen -= retval;
	}

	LocalFree(args);

	*pNumArgs = numArgs;
	return result;
}

IService::IService(PTCHAR name, PTCHAR display_name, DWORD accepted_controls)
{
	// Clear memory
	::ZeroMemory(PVOID(&status_), sizeof(SERVICE_STATUS));
	::ZeroMemory(PVOID(service_name_), sizeof(service_name_));
	::ZeroMemory(PVOID(display_name_), sizeof(display_name_));

	// Initialize variables
	InitializeCriticalSection(&status_mutex_);

	::strcpy_s(service_name_, name);
	::strcpy_s(display_name_, display_name);

	status_handle_ = NULL;
	exit_event_ = NULL;
	debug_ = FALSE;
	install_ = FALSE;
	uninstall_ = FALSE;

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

BOOL IService::Install(CONST DWORD start_type, PTSTR dependencies, PTSTR account, PTSTR password)
{
	TCHAR path[MAX_PATH];
	SC_HANDLE service_manager = NULL;
	SC_HANDLE service = NULL;
	BOOL ret = FALSE;

	if (GetModuleFileName(NULL, path, MAX_PATH) == 0)
	{
		LOG_F(ERROR, "GetModuleFileName failed w/err 0x%08lx", GetLastError());
		goto Cleanup;
	}

	// Open the local default service control manager database
	service_manager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE);
	if (service_manager == NULL) 
	{
		LOG_F(ERROR, "OpenSCManager failed w/err 0x%08lx", GetLastError());
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
		LOG_F(ERROR, "CreateService failed w/err 0x%08lx", GetLastError());
		goto Cleanup;
	}

	ret = TRUE;
	LOG_F(INFO, "%s is installed.", service_name_);

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
		LOG_F(ERROR, "OpenSCManager failed w/err 0x%08lx", GetLastError());
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
		LOG_F(ERROR, "OpenService failed w/err 0x%08lx", GetLastError());
		goto Cleanup;
	}

	// Try to stop the service
	if (ControlService(service, SERVICE_CONTROL_STOP, &service_status))
	{
		LOG_F(INFO, "Stopping %s.", service_name_);
		Sleep(1000);

		while (QueryServiceStatus(service, &service_status))
		{
			if (service_status.dwCurrentState == SERVICE_STOP_PENDING)
			{
				printf(".");
				Sleep(1000);
			}
			else break;
		}

		if (service_status.dwCurrentState == SERVICE_STOPPED)
			LOG_F(INFO, "%s is stopped.", service_name_);
		else
			LOG_F(ERROR, "%s failed to stop.", service_name_);
	}

	// Now remove the service by calling DeleteService.
	if (!DeleteService(service))
	{
		LOG_F(ERROR, "DeleteService failed w/err 0x%08lx", GetLastError());
		goto Cleanup;
	}

	ret = TRUE;
	LOG_F(INFO, "%s is removed.", service_name_);

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

DWORD IService::Run(CONST DWORD start_type, PTSTR dependencies, PTSTR account, PTSTR password)
{
	// Get command line arguments and set configs with them
	int num_args = 0;
	// Get unicode command line arguments just in case
	LPTSTR* tchar_cmd_args = CommandLineToArgvA(::GetCommandLineA(), &num_args);
	// Loop over arguments
	for (int i = 0; i < num_args; i++)
	{
		LPTSTR current_arg = tchar_cmd_args[i];
		// If argument is a flag passed to the service then check it
		if (current_arg[0] == L'/' || current_arg[0] == L'-')
		{
			// If the flag is debug then set the service to debug mode
			if (::_strcmpi(&current_arg[1], "debug") == 0)
				debug_ = TRUE;

			if (::_strcmpi(&current_arg[1], "install") == 0)
				install_ = TRUE;
			
			if (::_strcmpi(&current_arg[1], "uninstall") == 0)
				uninstall_ = TRUE;

		}
	}

	if (install_)
		return Install(start_type, dependencies, account, password);

	if (uninstall_)
		return Uninstall();

	// If debugging as a console application we cannot attatch to the SVC_MGR so just run servce_main
	if (debug_)
	{
		service_main(DWORD(num_args), tchar_cmd_args);
		OnStop();
	}
	// This is what must run when the program is ran in a service context
	else 
	{
		SERVICE_TABLE_ENTRY service_table[] =
		{   
			{ service_name_, service_main },
			{ NULL, NULL }
		};
		if (::StartServiceCtrlDispatcher(service_table) == 0)
			status_.dwWin32ExitCode = GetLastError();
	}

	::LocalFree(tchar_cmd_args);

	return status_.dwWin32ExitCode;
}

VOID IService::SetState(DWORD status)
{
	EnterCriticalSection(&status_mutex_);

	_set_status(status);

	LeaveCriticalSection(&status_mutex_);
}

VOID IService::SetStatusStopped(DWORD exit_code, BOOL specific)
{
	EnterCriticalSection(&status_mutex_);

	if (specific)
	{
		status_.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
		status_.dwServiceSpecificExitCode = exit_code;
	}
	else
		status_.dwWin32ExitCode = exit_code;

	_set_status(SERVICE_STOPPED);

	LeaveCriticalSection(&status_mutex_);
}

VOID IService::SetHintTime(DWORD runtime_ms)
{
	EnterCriticalSection(&status_mutex_);

	// Update status
	status_.dwCheckPoint++;
	status_.dwWaitHint = runtime_ms;

	// Apply status to service
	::SetServiceStatus(status_handle_, &status_);

	// Reset wait hint which won't apply until next ::SetServiceStatus call
	status_.dwWaitHint = 0;

	LeaveCriticalSection(&status_mutex_);
}

VOID IService::CheckIn()
{
	EnterCriticalSection(&status_mutex_);

	status_.dwCheckPoint++;
	::SetServiceStatus(status_handle_, &status_);

	LeaveCriticalSection(&status_mutex_);
}

VOID IService::WaitForExitEvent()
{
	::WaitForSingleObject(exit_event_, INFINITE);
}

VOID IService::SetExitEvent()
{
	// Update service status
	SetState(SERVICE_STOP_PENDING);

	// Set our exit event so the service knows to exit
	::SetEvent(exit_event_);
}


// -------------------------------------------------------------------
// Set as a static reference for the windows kernel calls
// -------------------------------------------------------------------
// What is called in the windows kernel on the singleton

VOID IService::service_main(DWORD argc, LPTSTR* argv)
{
	_ASSERTE(instance_ != NULL);

	instance_->_service_main(argc, argv);
}

BOOL WINAPI IService::console_control_handler(DWORD ctrl_type)
{
	return instance_->_console_control_handler(ctrl_type);
}

DWORD IService::service_control_handler(DWORD service_control, DWORD event_type, LPVOID event_data, LPVOID service_context)
{
	return reinterpret_cast<IService*>(service_context)->_service_control_handler(service_control, event_type, event_data);
}

// -------------------------------------------------------------------


// -------------------------------------------------------------------
// Actual implementation called from singleton instance

VOID IService::_service_main(DWORD argc, LPTSTR* argv)
{

	if (IsDebug())
	{
		// Register the console control handler
		if(::SetConsoleCtrlHandler(PHANDLER_ROUTINE(console_control_handler), TRUE) == 0)
		{
			LOG_F(ERROR, "Failed to register ConsoleCtrlHandler!");
			return;
		}
		LOG_F(INFO, "Press Ctrl+C or Ctrl+Break to quit...");
	}
	else
	{
		// register the service handler routine
		status_handle_ = ::RegisterServiceCtrlHandlerEx(service_name_, LPHANDLER_FUNCTION_EX(service_control_handler), (LPVOID)instance_);
		if (status_handle_ == NULL)
		{
			SetStatusStopped(ERROR_INVALID_HANDLE);
			return;
		}
	}

	// Start the service
	SetState(SERVICE_START_PENDING);
	OnStart(argc, argv);
}

BOOL IService::_console_control_handler(DWORD ctrl_type)
{
	// If not any exit event then ignore
	if (!(
		ctrl_type == CTRL_C_EVENT || 
		ctrl_type == CTRL_BREAK_EVENT || 
		ctrl_type == CTRL_SHUTDOWN_EVENT ||
		ctrl_type == CTRL_CLOSE_EVENT))
		return FALSE;

	OnStop();

	return TRUE;
}

DWORD IService::_service_control_handler(DWORD service_control, DWORD event_type, LPVOID event_data)
{
	DWORD ret = NO_ERROR;

	switch (service_control)
	{
	case SERVICE_CONTROL_STOP:
		if (status_.dwControlsAccepted & SERVICE_ACCEPT_STOP) {
			SetState(SERVICE_STOP_PENDING);
			OnStop();
		}
		break;
	case SERVICE_CONTROL_SHUTDOWN:
		OnShutdown();
		break;
	case SERVICE_CONTROL_PAUSE:
		OnPause();
		break;
	case SERVICE_CONTROL_CONTINUE:
		OnContinue();
		break;
	case SERVICE_CONTROL_INTERROGATE:
		OnInterrogate();
		break;
	case SERVICE_CONTROL_DEVICEEVENT:
		ret = OnDeviceEvent(event_type, static_cast<PDEV_BROADCAST_HDR>(event_data));
		break;
	case SERVICE_CONTROL_HARDWAREPROFILECHANGE:
		ret = OnHardwareProfileChange(event_type);
		break;
		// Only on WINXP or higher
#if (_WIN32_WINNT >= _WIN32_WINNT_WINXP)
	case SERVICE_CONTROL_SESSIONCHANGE:
		ret = OnSessionChange(event_type, static_cast<PWTSSESSION_NOTIFICATION>(event_data));
		break;
#endif
		// Only on WS03 or higher
#if (_WIN32_WINNT >= _WIN32_WINNT_WS03)
	case SERVICE_CONTROL_POWEREVENT:
		ret = OnPowerEvent(event_type, static_cast<POWERBROADCAST_SETTING*>(event_data));
		break;
#endif
		// Only on Vista or higher
#if (_WIN32_WINNT >= _WIN32_WINNT_VISTA)
	case SERVICE_CONTROL_PRESHUTDOWN:
		ret = OnPreShutdown(static_cast<LPSERVICE_PRESHUTDOWN_INFO>(event_data));
		break;
#endif
	default:
		OnUnknownRequest(service_control);
	}

	return ret;
}

// -------------------------------------------------------------------


// -------------------------------------------------------------------
// Internal helper functions

VOID IService::_set_status(DWORD status)
{
	_ASSERTE(instance_ != NULL);

	instance_->status_.dwCurrentState = status;
	instance_->status_.dwCheckPoint = 0;
	instance_->status_.dwWaitHint = 0;

	::SetServiceStatus(instance_->status_handle_, &instance_->status_);
}

// -------------------------------------------------------------------