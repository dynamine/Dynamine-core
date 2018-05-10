#pragma once

#define _WINSOCKAPI_ // Disable windows.h including winsock
#include <Windows.h>
#include <winsvc.h>
#include <tchar.h>
#include <dbt.h>
#include <crtdbg.h>
#include <cstdarg>
#include <strsafe.h>
#include <string>

class IService
{
public:
	// Only one service object in the process
	IService(PWCHAR name, PWCHAR display_name, DWORD accepted_controls);

	virtual ~IService();

	// -------------------------------------------------------------------
	// External methods
	BOOL Install(CONST DWORD start_type, PWSTR dependencies, PWSTR account, PWSTR password);

	BOOL Uninstall();

	DWORD Run(CONST DWORD start_type, PWSTR dependencies, PWSTR account, PWSTR password);

	VOID SetState(DWORD status);

	VOID SetStatusStopped(DWORD exit_code, BOOL specific=FALSE);

	VOID SetHintTime(DWORD runtime_ms);

	VOID CheckIn();

	VOID WaitForExitEvent();

	VOID SetExitEvent();

	BOOL IsDebug() CONST       { return debug_; }

	BOOL IsInstall() CONST     { return install_; }

	BOOL IsUninstall() CONST   { return uninstall_; }

	// -------------------------------------------------------------------

	// -------------------------------------------------------------------
	// Implementation functions
	virtual VOID OnStart(DWORD argc, LPTSTR* argv)                  {}
	virtual VOID OnStop()                                           {}
	// -------------------------------------------------------------------

	// -------------------------------------------------------------------
	// Service event handler functions to be overriden by parent class
	virtual VOID OnShutdown()                                       {}
	virtual VOID OnPause() { SetState(SERVICE_PAUSED); }
	virtual VOID OnContinue() { SetState(SERVICE_RUNNING); }
	virtual VOID OnInterrogate()                                    {}
	virtual VOID OnUnknownRequest(DWORD control)                    {}

	virtual DWORD OnDeviceEvent(DWORD dbt, PDEV_BROADCAST_HDR hdr)	{ return NO_ERROR; }
	virtual DWORD OnHardwareProfileChange(DWORD dbt)				{ return NO_ERROR; }

	// Windows version specific events
	#if (_WIN32_WINNT >= _WIN32_WINNT_WINXP)
	virtual DWORD OnSessionChange(DWORD event, PWTSSESSION_NOTIFICATION session) { return NO_ERROR; }
	#endif
	#if (_WIN32_WINNT >= _WIN32_WINNT_WS03)
	virtual DWORD OnPowerEvent(DWORD event, POWERBROADCAST_SETTING* setting) { return NO_ERROR; }
	#endif
	#if (_WIN32_WINNT >= _WIN32_WINNT_VISTA)
	virtual DWORD OnPreShutdown(LPSERVICE_PRESHUTDOWN_INFO info) { return NO_ERROR; }
	#endif
	// -------------------------------------------------------------------
	

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
	BOOL                        install_;                    // Flag to set that this run is an installation
	BOOL                        uninstall_;                  
	BOOL                        debug_;                      // Enables extra logging and doesn't attatch the service into dispatchTable
	CRITICAL_SECTION            status_mutex_;               // Critical Section mutex

private:
	// Internal functions that can only be accessed to provide static functions with functionality
	VOID _service_main(DWORD argc, LPTSTR* argv);
	BOOL _console_control_handler(DWORD ctrlType);
	DWORD _service_control_handler(DWORD service_control, DWORD event_type, LPVOID event_data);

	// Internal function wrappers
	VOID _set_status(DWORD status);

	// Singleton Modifiers
	IService();
	IService(const IService&);
	void operator=(const IService&);
};