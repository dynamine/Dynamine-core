#include "..\..\Public\WinService\IService.h"

#pragma comment(lib, "advapi32.lib")

IService::IService()
{
	// TODO:
}

BOOL IService::Install()
{
	// TODO:
	return FALSE;
}

BOOL IService::Uninstall()
{
	// TODO:
	return FALSE;
}

DWORD IService::Start()
{
	// TODO:
	return 0;
}

DWORD IService::Stop()
{
	// TODO:
	return 0;
}

VOID IService::SetServiceStatus(DWORD status, DWORD win32ExitCode=NULL, DWORD waitHint=NULL)
{
	static DWORD checkpoint = 1;

	status.dwCurrentState = status;

	// Optional status
	if (win32ExitCode)
		status.dwWin32ExitCode = win32ExitCode;
	if (waitHint)
		status.dwWaitHint = waitHint;
	
	// If the service is starting remote controls, else accept stop control
	if (status == SERVICE_START_PENDING)
		status.dwControlsAccepted = 0;
	else
		status.dwControlsAccepted = SERVICE_ACCEPT_STOP;

	// If running or stopped then reset checkpoint otherwise increment global one
	if ((status == SERVICE_RUNNING) || (status == SERVICE_STOPPED))
		status.dwCheckPoint = 0;
	else
		status.dwCheckPoint = checkpoint;
	
	// If the service is not actually running then we are done
	if (IsDebug())
		return;

	::SetServiceStatus(status_handle, &status);
}

DWORD IService::Run()
{
	// Service status to running
	SetServiceStatus(SERVICE_RUNNING);

	// Wait for the service exit event to be set before this method exits
	::WaitForSingleObject(exit_event, INFINITE);

	return 0;
}

BOOL WINAPI _consoleControlHandler(DWORD ctrlType)
{
	// If not any exit event then ignore
	if (!(ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT || ctrlType == CTRL_SHUTDOWN_EVENT))
		return FALSE;

	//onStop();

	return TRUE;
}

VOID IService::service_main(DWORD argc, LPTSTR * argv)
{
	// TODO: Check if we actually need to set this since our setServiceStatus function should handle it for us
	//_status.dwCurrentState = SERVICE_START_PENDING;

	if (IsDebug())
	{
		// Register the console control handler
		::SetConsoleCtrlHandler( (PHANDLER_ROUTINE)_consoleControlHandler, TRUE);
	}
	return;
}

DWORD IService::service_control_handler(DWORD serviceControl, DWORD eventType, LPVOID eventData)
{
	DWORD ret = NO_ERROR;

	switch (serviceControl)
	{
	case SERVICE_CONTROL_SHUTDOWN:
		onShutdown();
		break;
	case SERVICE_CONTROL_PAUSE:
		onPause();
		break;
	case SERVICE_CONTROL_CONTINUE:
		onContinue();
		break;
	case SERVICE_CONTROL_INTERROGATE:
		onInterrogate();
		break;
	case SERVICE_CONTROL_DEVICEEVENT:
		ret = onDeviceEvent(eventType, (PDEV_BROADCAST_HDR)eventData);
		break;
	case SERVICE_CONTROL_HARDWAREPROFILECHANGE:
		ret = onHardwareProfileChange(eventType);
		break;
	// Only on WINXP or higher
	#if (_WIN32_WINNT >= _WIN32_WINNT_WINXP)
	case SERVICE_CONTROL_SESSIONCHANGE:
		ret = onSessionChange(eventType, (PWTSSESSION_NOTIFICATION)eventData);
		break;
	#endif
	// Only on WS03 or higher
	#if (_WIN32_WINNT >= _WIN32_WINNT_WS03)
	case SERVICE_CONTROL_POWEREVENT:
		ret = onPowerEvent(eventType, (POWERBROADCAST_SETTING*)eventData);
		break;
	#endif
	// Only on Vista or higher
	#if (_WIN32_WINNT >= _WIN32_WINNT_VISTA)
	case SERVICE_CONTROL_PRESHUTDOWN:
		ret = onPreShutdown((LPSERVICE_PRESHUTDOWN_INFO)eventData);
		break;
	#endif
	default:
		onUnknownRequest(serviceControl);
	}

	return ret;
}

BOOL IService::_consoleControlHandler(DWORD ctrlType)
{
	// If not any exit event then ignore
	if (!(ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT || ctrlType == CTRL_SHUTDOWN_EVENT))
		return FALSE;

	onStop();

	return FALSE;
}
