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

	BOOL install();

	BOOL uninstall();

	DWORD start();

	DWORD stop();

	VOID setServiceStatus(DWORD status, DWORD win32ExitCode, DWORD waitHint);
	BOOL isDebug() { return _debug; }
	Logger* getLogger() { return _logger; }
	
	virtual DWORD execute();

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
		setServiceStatus(SERVICE_STOP_PENDING, NULL, NULL);

		// Set our exit event so the service knows to exit
		::SetEvent(_exitEvent);
	}

protected:
	VOID WINAPI _serviceMain(DWORD argc, LPTSTR* argv);
	DWORD WINAPI _serviceControlHandler(DWORD serviceControl, DWORD eventType, LPVOID eventData);

	WCHAR						_serviceName[WCHAR_MAX];	// Service name
	SERVICE_STATUS_HANDLE		_statusHandle;				// Service status handle
	HANDLE						_exitEvent;					// Exit interrupt handle to signal the service to exit
	SERVICE_STATUS				_status;						// Service's status
	BOOL						_debug;						// Enables extra logging and doesn't attatch the service into dispatchTable
	Logger*						_logger;					// Logger to send all the logs to

private:
	//BOOL WINAPI _consoleControlHandler(DWORD ctrlType);

};