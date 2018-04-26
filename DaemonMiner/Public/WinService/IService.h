#pragma once

#include <Windows.h>
#include <winsvc.h>
#include <tchar.h>
#include <dbt.h>
#include <crtdbg.h>
#include <cstdarg>
#include <strsafe.h>
#include <string>

#include "../Logger/Logger.h"

// Can't forward declare just wstring since it's a typedef of std:basic_string<wchar> :/
//using namespace std;

class IService
{
public:
	// Only one service object in the process
	IService(CONST PWCHAR name, CONST PWCHAR display_name, CONST DWORD accepted_controls, Logger * log_instance);

	virtual ~IService();

	// Control methods
	
	BOOL Install(CONST DWORD start_type, PWSTR dependencies, PWSTR account, PWSTR password);

	BOOL Uninstall();

	DWORD Start();


	VOID SetStatus(DWORD status, DWORD win32_exit_code, DWORD wait_hint);
	BOOL IsDebug() { return debug_; }
	Logger* GetLogger() { return logger_; }
	
	// Overridden with code that runs as a service
	virtual DWORD Run();

	// Service event handler functions to be overriden by parent class
	virtual VOID onShutdown()				{}
	virtual VOID onPause()					{}
	virtual VOID onContinue()				{}
	virtual VOID onInterrogate()			{}

	virtual VOID onUnknownRequest(DWORD control) {}

	virtual DWORD onDeviceEvent(DWORD dbt, PDEV_BROADCAST_HDR hdr)	{ return NO_ERROR; }
	virtual DWORD onHardwareProfileChange(DWORD dbt)				{ return NO_ERROR; }

	// Windows version specific events
	#if (_WIN32_WINNT >= _WIN32_WINNT_WINXP)
	virtual DWORD onSessionChange(DWORD event, PWTSSESSION_NOTIFICATION session) { return NO_ERROR; }
	#endif
	#if (_WIN32_WINNT >= _WIN32_WINNT_WS03)
	virtual DWORD onPowerEvent(DWORD event, POWERBROADCAST_SETTING* setting) { return NO_ERROR; }
	#endif
	#if (_WIN32_WINNT >= _WIN32_WINNT_VISTA)
	virtual DWORD onPreShutdown(LPSERVICE_PRESHUTDOWN_INFO info) { return NO_ERROR; }
	#endif

	virtual VOID onStop()
	{
		// Update service status
		SetStatus(SERVICE_STOP_PENDING, NULL, NULL);

		// Set our exit event so the service knows to exit
		::SetEvent(exit_event_);
	}

protected:
	// Static is called to access this function from the singleton
	static VOID WINAPI service_main(DWORD argc, LPTSTR* argv);
	

	// Static is called to access this function from the singleton
	static BOOL WINAPI console_control_handler(DWORD ctrl_type);
	

	static DWORD WINAPI service_control_handler(DWORD service_control, DWORD event_type, LPVOID event_data, LPVOID service_context);
	
	// Singleton instance
	static IService* instance_;

	
    WCHAR                       service_name_[WCHAR_MAX];    // Service name
	WCHAR                       display_name_[WCHAR_MAX];    // Service display name
	SERVICE_STATUS_HANDLE       status_handle_;              // Service status handle
	HANDLE                      exit_event_;                 // Exit interrupt handle to signal the service to exit
	SERVICE_STATUS              status_;                     // Service's status
	BOOL                        debug_;                      // Enables extra logging and doesn't attatch the service into dispatchTable
	Logger*                     logger_;                     // Logger to send all the logs to
	CRITICAL_SECTION            status_mutex_;               // Critical Section mutex
private:
	// Internal functions that can only be accessed to provide static functions with functionality
	VOID _service_main(DWORD argc, LPTSTR* argv);
	BOOL _console_control_handler(DWORD ctrlType);
	DWORD _service_control_handler(DWORD service_control, DWORD event_type, LPVOID event_data);

	// Singleton Modifiers
	IService();
	IService(const IService&);
	void operator=(const IService&);
};