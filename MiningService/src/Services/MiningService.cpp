#include "MiningService.hpp"

MiningService::MiningService()
	: IService(PWSTR(SERVICE_NAME), PWSTR(DISPLAY_NAME), SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_PAUSE_CONTINUE)
{
	daemon_thread_ = INVALID_HANDLE_VALUE;
	exit_code_ = 1;
	printf("MiningService Constructed\n");
}

MiningService::~MiningService()
{
	if (daemon_thread_ != INVALID_HANDLE_VALUE)
		CloseHandle(daemon_thread_);
}


VOID MiningService::OnStart(DWORD argc, LPTSTR* argv)
{
	SetState(SERVICE_RUNNING);

	// Create Daemon Thread
	daemon_thread_ = CreateThread(NULL, NULL, LPTHREAD_START_ROUTINE(MainThread), this, NULL, NULL);

	// Wait here for daemon thread to complete
	WaitForSingleObject(daemon_thread_, INFINITE);

	printf("MiningService OnStart()\n");

	SetStatusStopped(0x0);
}

VOID MiningService::OnStop()
{
	TerminateThread(daemon_thread_, 0x0);
	printf("MiningService OnStop()\n");
}

DWORD WINAPI MiningService::MainThread(LPVOID thread_data)
{
	auto parent = static_cast<MiningService*>(thread_data);

	parent->DaemonThread();

	printf("MiningService In Thread!\n");

	return 0;
}

// -----------------------------------------------------------------------------
// Real work done below

VOID MiningService::DaemonThread()
{
	printf("MiningService In Deep Thread!\n");
	for(;;) {}
}

