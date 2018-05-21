#include "MiningService.hpp"
#include "../Network/TcpConnection.hpp"
#include "../Network/TcpClient.hpp"

MiningService::MiningService()
	: IService(PWSTR(SERVICE_NAME), PWSTR(DISPLAY_NAME), SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_PAUSE_CONTINUE)
{
	// Initialize dependencies
	miner_dispatch_thread_ = INVALID_HANDLE_VALUE;
	cmd_thread_ = INVALID_HANDLE_VALUE;
	exit_code_ = 1;

	// Create a TCPServer to handle commands to the miner
	cmd_server_ = new TcpServer<MiningService>(PORT);

	miner_cmd_server_ = new TcpServer<MiningService>(COM_PORT);

	// Acknowledge that we made it to the logs
	LOG_F(INFO, "MiningService Constructed");
}

MiningService::~MiningService()
{
	delete cmd_server_;
	delete miner_cmd_server_;

	// If the daemon thread hasnt been cleaned up do it now
	if (miner_dispatch_thread_ != INVALID_HANDLE_VALUE)
		CloseHandle(miner_dispatch_thread_);

	if(cmd_thread_ != INVALID_HANDLE_VALUE)
		CloseHandle(cmd_thread_);
}

VOID MiningService::OnStart(DWORD argc, LPTSTR* argv)
{
	LOG_F(INFO, "Set state running...");
	SetState(SERVICE_RUNNING);

	// Create Miner dispatch thread
	LOG_F(INFO, "Creating daemon thread...");
	miner_dispatch_thread_ = CreateThread(NULL, NULL, LPTHREAD_START_ROUTINE(MinerDispatchProxy), this, NULL, NULL);

	LOG_F(INFO, "Creating command thread...");
	cmd_thread_ = CreateThread(NULL, NULL, LPTHREAD_START_ROUTINE(CommandThreadProxy), this, NULL, NULL);

	LOG_F(INFO, "Started mining service!");
	// Wait here for daemon thread to complete
	WaitForSingleObject(miner_dispatch_thread_, INFINITE);

	SetStatusStopped(0x0);

	LOG_F(INFO, "MiningService OnStart() completed");
}

VOID MiningService::OnStop()
{
	// Stop the server
	LOG_F(INFO, "Stopping command server...");
	cmd_server_->Stop();

	LOG_F(INFO, "Stopping com server...");
	miner_cmd_server_->Stop();

	// Terminate command thread
	LOG_F(INFO, "Terminating command thread...");
	TerminateThread(cmd_thread_, 0x0);

	// Terminate daemon thread
	LOG_F(INFO, "Terminating daemon thread...");
	TerminateThread(miner_dispatch_thread_, 0x0);

	LOG_F(INFO, "MiningService OnStop() Completed");
}

PCHAR* MiningService::GetDevices()
{
	// Run this command:
	// wmic path win32_VideoController get name
    /* Returns this:
       Name
       NVIDIA GeForce GTX 1080
     */
	// Parse out every line with "NVIDIA GeForce" and then combine the rest in lowercase

	return nullptr;
}

DWORD WINAPI MiningService::MinerThreadProxy(LPVOID thread_data)
{
	auto parent = static_cast<MiningService*>(thread_data);

	parent->MinerThread();
	return 0;
}

DWORD WINAPI MiningService::MinerDispatchProxy(LPVOID thread_data)
{
	auto parent = static_cast<MiningService*>(thread_data);

	parent->MinerComThread();

	return 0;
}

DWORD WINAPI MiningService::CommandThreadProxy(LPVOID thread_data)
{
	auto parent = static_cast<MiningService*>(thread_data);

	parent->CommandServerThread();

	return 0;
}

// -----------------------------------------------------------------------------
// Real work done below

DWORD WINAPI MiningService::MinerDispatchThread(SOCKET client_socket)
{
	// Handles what to do when we receive a miner command
	LOG_F(INFO, "Received miner command");
	TcpConnection client(client_socket);
	Packet* pack;
	Packet* start = new Packet(PCHAR("startMiner"), PCHAR("{ \"result\": \"success\"}"));

	while(client_socket != INVALID_SOCKET)
	{
		pack = new Packet;
		LOG_F(INFO, "Waiting to receive communication...");
		client.RecvData(pack);
		LOG_F(INFO, "Comm Got: %s", pack->command);
		client.SendData(start);
		LOG_F(INFO, "Sent success");
		delete pack;
	}

	delete start;

	client.Close(true);
	LOG_F(INFO, "Closing com connection");

	return 0x0;
}

VOID MiningService::MinerThread()
{
	
}

VOID MiningService::CommandServerThread()
{
	LOG_F(INFO, "Starting TCP command server on port %s\n", PORT);

	// Start TCP server and when a client connects, dispatch info to the
	// MiningService::OnClientConnect function that will be ran on
	// a thread so that we can take N amount of clients
	cmd_server_->Start(this, &MiningService::OnClientConnect);
}

VOID MiningService::MinerComThread()
{
	LOG_F(INFO, "Starting communication TCP command server on port %s\n", COM_PORT);

	// Start TCP server and when a client connects, dispatch info to the
	// MiningService::MinerDispatchThread function that will be ran on
	// a thread so that we can take N amount of clients
	miner_cmd_server_->Start(this, &MiningService::MinerDispatchThread);
}

// When a client connects to the Command TCP server this is the filtering it goes through
DWORD WINAPI MiningService::OnClientConnect(SOCKET client_socket)
{
	LOG_F(INFO, "CLIENT CONNECTED!");
	TcpConnection client(client_socket);

	TcpClient miner_client(const_cast<PSTR>(COM_PORT));
	TcpConnection miner_com(miner_client.GetSocket());

	Packet* pack;
	Packet* recvpack;
	Packet* resources = new Packet(PCHAR("resources"), PCHAR("{ \"resources\": [\"localhost.gpu0.GTX1080\"] }"));
	Packet* stats = new Packet(PCHAR("hashRate"), PCHAR("{}"));
	Packet* start = new Packet(PCHAR("startMiner"), PCHAR("{ \"result\": \"success\"}"));
	Packet* stop = new Packet(PCHAR("stopMiner"), PCHAR("{ \"result\": \"failure\"}"));

	// Once we establish a connection with this client
	// it is expecting us to hold that connection open for more commands
	while(client_socket != INVALID_SOCKET)
	{
		// TODO: fix json null data being sent at the end
		pack = new Packet;
		recvpack = new Packet;

		LOG_F(INFO, "Waiting to receive packet...");
		client.RecvData(pack);
		LOG_F(INFO, "Got: %s", pack->command);

		if (strcmp(pack->command, CMD_RESOURCES) == 0)
		{
			client.SendData(resources);
			LOG_F(INFO, "Sent resources");
		}

		if(strcmp(pack->command, CMD_STATS) == 0)
		{
			int rnum = rand() % 2000 + 1000;
			stats->data = json({
				{"resource", "localhost.gpu0.GTX1080"},
				{"hashRate", std::to_string(rnum)}
				});
			client.SendData(stats);
			LOG_F(INFO, "Sent hashRate");
		}

		if(strcmp(pack->command, CMD_START_MINER) == 0)
		{
			miner_com.SendData(pack);
			miner_com.RecvData(recvpack);
			client.SendData(start);
			
			LOG_F(INFO, "Started mining");
		}

		if(strcmp(pack->command, CMD_STOP_MINER) == 0)
		{
			client.SendData(stop);
			LOG_F(INFO, "Stopped mining");
		}

		delete pack;
		delete recvpack;
		Sleep(500);
	}
	client.Close(true);
	LOG_F(INFO, "CLIENT DISCONNECTED");

	delete resources;
	delete stats;
	delete start;
	delete stop;

	return 0x0;
}

