#include "MiningService.hpp"
#include "../Network/TcpConnection.hpp"

MiningService::MiningService()
	: IService(PWSTR(SERVICE_NAME), PWSTR(DISPLAY_NAME), SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_PAUSE_CONTINUE)
{
	// Initialize dependencies
	daemon_thread_ = INVALID_HANDLE_VALUE;
	cmd_thread_ = INVALID_HANDLE_VALUE;
	exit_code_ = 1;

	// Create a TCPServer to handle commands to the miner
	cmd_server_ = new TcpServer<MiningService>(PORT);

	// Acknowledge that we made it to the logs
	LOG_F(INFO, "MiningService Constructed");
}

MiningService::~MiningService()
{
	delete cmd_server_;

	// If the daemon thread hasnt been cleaned up do it now
	if (daemon_thread_ != INVALID_HANDLE_VALUE)
		CloseHandle(daemon_thread_);

	if(cmd_thread_ != INVALID_HANDLE_VALUE)
		CloseHandle(cmd_thread_);
}

VOID MiningService::OnStart(DWORD argc, LPTSTR* argv)
{
	LOG_F(INFO, "Set state running...");
	SetState(SERVICE_RUNNING);

	// Create Daemon Thread
	LOG_F(INFO, "Creating daemon thread...");
	daemon_thread_ = CreateThread(NULL, NULL, LPTHREAD_START_ROUTINE(MiningThread), this, NULL, NULL);

	LOG_F(INFO, "Creating command thread...");
	cmd_thread_ = CreateThread(NULL, NULL, LPTHREAD_START_ROUTINE(CommandThread), this, NULL, NULL);

	LOG_F(INFO, "Started mining service!");
	// Wait here for daemon thread to complete
	WaitForSingleObject(daemon_thread_, INFINITE);

	SetStatusStopped(0x0);

	LOG_F(INFO, "MiningService OnStart() completed");
}

VOID MiningService::OnStop()
{
	// Stop the server
	LOG_F(INFO, "Stopping command server...");
	cmd_server_->Stop();

	// Terminate command thread
	LOG_F(INFO, "Terminating command thread...");
	TerminateThread(cmd_thread_, 0x0);

	// Terminate daemon thread
	LOG_F(INFO, "Terminating daemon thread...");
	TerminateThread(daemon_thread_, 0x0);

	LOG_F(INFO, "MiningService OnStop() Completed");
}

DWORD WINAPI MiningService::MiningThread(LPVOID thread_data)
{
	auto parent = static_cast<MiningService*>(thread_data);

	parent->DaemonThread();

	return 0;
}

DWORD WINAPI MiningService::CommandThread(LPVOID thread_data)
{
	auto parent = static_cast<MiningService*>(thread_data);

	parent->CommandServerThread();

	return 0;
}

// -----------------------------------------------------------------------------
// Real work done below

VOID MiningService::DaemonThread()
{
	LOG_F(INFO, "MiningService In Deep Thread!");

	// Setup mining workers here to poll for start/stop commands
	for(;;) {}
}

VOID MiningService::CommandServerThread()
{
	LOG_F(INFO, "Starting TCP command server on port %s\n", PORT);
	cmd_server_->Start(this, &MiningService::OnClientConnect);
}

// When a client connects to the Command TCP server this is the filtering it goes through
DWORD WINAPI MiningService::OnClientConnect(SOCKET client_socket)
{
	LOG_F(INFO, "CLIENT CONNECTED!");
	TcpConnection client(client_socket);

	Packet* pack;
	Packet* resources = new Packet(PCHAR("resources"), PCHAR("{ \"resources\": {\"localhost.gpu0\": \"GTX 1080\"}}"));

	while(client_socket != INVALID_SOCKET)
	{
		pack = new Packet;
		LOG_F(INFO, "Waiting to receive packet...");
		client.RecvData(pack);
		LOG_F(INFO, "Got: %s", pack->command);

		if (strcmp(pack->command, CMD_RESOURCES) == 0)
		{
			client.SendData(resources);
			LOG_F(INFO, "Sent resources");
		}
		delete pack;
		Sleep(500);
	}
	client.Close(true);
	LOG_F(INFO, "CLIENT DISCONNECTED");
	return 0x0;
}

