#include "..\..\Public\WinService\IService.h"

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

VOID IService::SetServiceStatus(DWORD status)
{
	// TODO:
	return;
}

VOID IService::_serviceMain(DWORD argc, LPTSTR * argv)
{
	// TODO:
	return;
}

DWORD IService::_serviceControlHandler(DWORD serviceControl, DWORD eventType, LPVOID eventData)
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
#if (_WIN32_WINNT >= _WIN32_WINNT_WINXP)
	case SERVICE_CONTROL_SESSIONCHANGE:
		ret = onSessionChange(eventType, (PWTSSESSION_NOTIFICATION)eventData);
		break;
#endif
#if (_WIN32_WINNT >= _WIN32_WINNT_WS03)
	case SERVICE_CONTROL_POWEREVENT:
		ret = onPowerEvent(eventType, (POWERBROADCAST_SETTING*)eventData);
		break;
#endif
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
