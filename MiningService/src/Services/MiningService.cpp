#include "MiningService.hpp"

MiningService::MiningService()
	: IService(PWSTR(SERVICE_NAME), PWSTR(DISPLAY_NAME), SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_PAUSE_CONTINUE)
{
	// Initialize dependencies
	daemon_thread_ = INVALID_HANDLE_VALUE;
	exit_code_ = 1;

	// Create a TCPServer to handle commands to the miner
	cmd_server_ = new TCPServer<MiningService>(PORT);

	// Acknowledge that we made it to the logs
	LOG_F(INFO, "MiningService Constructed\n");
}

MiningService::~MiningService()
{
	// TODO: If this causes a heap corruption then the dtor is already being called by C++ for us
	delete cmd_server_;

	// If the daemon thread hasnt been cleaned up do it now
	if (daemon_thread_ != INVALID_HANDLE_VALUE)
		CloseHandle(daemon_thread_);
}

VOID MiningService::OnStart(DWORD argc, LPTSTR* argv)
{
	LOG_F(INFO, "Set state running...\n");
	SetState(SERVICE_RUNNING);

	// Create Daemon Thread
	LOG_F(INFO, "Creating daemon thread...\n");
	daemon_thread_ = CreateThread(NULL, NULL, LPTHREAD_START_ROUTINE(MainThread), this, NULL, NULL);

	LOG_F(INFO, "Started mining service!\n");
	// Wait here for daemon thread to complete
	WaitForSingleObject(daemon_thread_, INFINITE);

	SetStatusStopped(0x0);

	LOG_F(INFO, "MiningService OnStart() completed\n");
}

VOID MiningService::OnStop()
{
	// Stop the server
	LOG_F(INFO, "Stopping command server...\n");
	cmd_server_->Stop();

	// Terminate daemon thread
	LOG_F(INFO, "Terminating daemon thread...\n");
	TerminateThread(daemon_thread_, 0x0);

	LOG_F(INFO, "MiningService OnStop() Completed\n");
}

DWORD WINAPI MiningService::MainThread(LPVOID thread_data)
{
	auto parent = static_cast<MiningService*>(thread_data);

	LOG_F(INFO, "MiningService In Thread!\n");
	parent->DaemonThread();

	return 0;
}

// -----------------------------------------------------------------------------
// Real work done below

VOID MiningService::DaemonThread()
{
	LOG_F(INFO, "MiningService In Deep Thread!\n");
	for(;;) {}
}

// When a client connects to the Command TCP server this is the filtering it goes through
DWORD MiningService::OnClientConnect(SOCKET client_socket)
{

	return 0x0;
}

