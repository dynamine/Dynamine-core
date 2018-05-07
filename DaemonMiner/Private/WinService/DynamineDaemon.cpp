#include "../../Public/WinService/DynamineDaemon.h"

DynamineDaemon::DynamineDaemon()
	: IService(PWSTR(SERVICE_NAME), PWSTR(DISPLAY_NAME), SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_PAUSE_CONTINUE, NULL)
{
	printf("DynamineDaemon Constructed\n");
}

VOID DynamineDaemon::OnStart(DWORD argc, LPTSTR* argv)
{
	printf("DynamineDaemon OnStart()\n");

	SetStatusStopped(0x0);
}

VOID DynamineDaemon::OnStop()
{
	printf("DynamineDaemon OnStop()\n");
}
