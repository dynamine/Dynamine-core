#include "../../Public/WinService/DynamineDaemon.h"

DynamineDaemon::DynamineDaemon()
	: IService(PWSTR(SERVICE_NAME), PWSTR(DISPLAY_NAME), SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_PAUSE_CONTINUE, NULL)
{
	daemon_thread_ = INVALID_HANDLE_VALUE;
	exit_code_ = 1;
	printf("DynamineDaemon Constructed\n");
}

DynamineDaemon::~DynamineDaemon()
{
	if (daemon_thread_ != INVALID_HANDLE_VALUE)
		CloseHandle(daemon_thread_);
}


VOID DynamineDaemon::OnStart(DWORD argc, LPTSTR* argv)
{
	SetState(SERVICE_RUNNING);

	// Create Daemon Thread
	daemon_thread_ = CreateThread(NULL, NULL, LPTHREAD_START_ROUTINE(MainThread), this, NULL, NULL);

	// Wait here for daemon thread to complete
	WaitForSingleObject(daemon_thread_, INFINITE);

	printf("DynamineDaemon OnStart()\n");

	SetStatusStopped(0x0);
}

VOID DynamineDaemon::OnStop()
{
	TerminateThread(daemon_thread_, 0x0);
	printf("DynamineDaemon OnStop()\n");
}

DWORD WINAPI DynamineDaemon::MainThread(LPVOID thread_data)
{
	auto parent = static_cast<DynamineDaemon*>(thread_data);

	parent->DaemonThread();

	printf("DynamineDaemon In Thread!\n");

	return 0;
}

// -----------------------------------------------------------------------------
// Real work done below

VOID DynamineDaemon::DaemonThread()
{
	printf("DynamineDaemon In Deep Thread!\n");
	for(;;) {}
}

