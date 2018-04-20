#pragma once

#include <Windows.h>
#include <winsvc.h>
#include <tchar.h>
#include <dbt.h>
#include <crtdbg.h>
#include <stdarg.h>
#include <strsafe.h>

#include "../Logger/Logger.h"

class IService
{
public:
	IService();

	BOOL Install();

	BOOL Uninstall();

	DWORD Start();

	DWORD Stop();

	VOID SetServiceStatus(DWORD status, DWORD win32ExitCode, DWORD waitHint);
	BOOL IsDebug() { return debug; }
	Logger* GetLogger() { return logger; }
	
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
		SetServiceStatus(SERVICE_STOP_PENDING, NULL, NULL);

		// Set our exit event so the service knows to exit
		::SetEvent(exit_event);
	}

protected:
	VOID WINAPI service_main(DWORD argc, LPTSTR* argv);
	DWORD WINAPI service_control_handler(DWORD serviceControl, DWORD eventType, LPVOID eventData);

	WCHAR						service_name[WCHAR_MAX];	// Service name
	SERVICE_STATUS_HANDLE		status_handle;				// Service status handle
	HANDLE						exit_event;					// Exit interrupt handle to signal the service to exit
	SERVICE_STATUS				status;						// Service's status
	BOOL						debug;						// Enables extra logging and doesn't attatch the service into dispatchTable
	Logger*						logger;					// Logger to send all the logs to

private:
	BOOL WINAPI console_control_handler(DWORD ctrlType);
	IService(const IService&);
	IService& operator=(const IService&);
};