#include "MiningService.hpp"
#include "../Tools/Tools.h"
#include "../Network/TcpClient.hpp"
#include "easywsclient.hpp"

using easywsclient::WebSocket;

MiningService::MiningService()
	: IService(PTSTR(SERVICE_NAME), PTSTR(DISPLAY_NAME), SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_PAUSE_CONTINUE)
{
	// Initialize dependencies
	miner_dispatch_thread_ = INVALID_HANDLE_VALUE;
	cmd_thread_ = INVALID_HANDLE_VALUE;
	exit_code_ = 1;

	// Create a TCPServer to handle commands to the miner
	cmd_server_ = new TcpServer<MiningService>(PORT);

	// TODO: Cutting feature for now but could use this for relaying api calls nicely
	//miner_cmd_server_ = new TcpServer<MiningService>(COM_PORT);

	// Acknowledge that we made it to the logs
	LOG_F(INFO, "MiningService Constructed");
}

MiningService::~MiningService()
{
	delete cmd_server_;
	//delete miner_cmd_server_;

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
	//LOG_F(INFO, "Creating daemon thread...");
	//miner_dispatch_thread_ = CreateThread(NULL, NULL, LPTHREAD_START_ROUTINE(MinerDispatchProxy), this, NULL, NULL);

	LOG_F(INFO, "Creating command thread...");
	cmd_thread_ = CreateThread(NULL, NULL, LPTHREAD_START_ROUTINE(CommandThreadProxy), this, NULL, NULL);

	LOG_F(INFO, "Started mining service!");
	// Wait here for daemon thread to complete
	WaitForSingleObject(cmd_thread_, INFINITE);

	SetStatusStopped(0x0);

	LOG_F(INFO, "MiningService OnStart() completed");
}

VOID MiningService::OnStop()
{
	// Stop the server
	LOG_F(INFO, "Stopping command server...");
	cmd_server_->Stop();

	//LOG_F(INFO, "Stopping com server...");
	//miner_cmd_server_->Stop();

	LOG_F(INFO, "Terminating miners...");
	for (auto miner : miners_)
	{
		StopMiner(miner.first);
		Delete(miner.second);
	}

	// Terminate command thread
	LOG_F(INFO, "Terminating command thread...");
	TerminateThread(cmd_thread_, 0x0);

	// Terminate daemon thread
	//LOG_F(INFO, "Terminating daemon thread...");
	//TerminateThread(miner_dispatch_thread_, 0x0);

	LOG_F(INFO, "MiningService OnStop() Completed");
}


// Proxies to start class methods as threads

DWORD WINAPI MiningService::MinerThreadProxy(LPVOID thread_data)
{
	auto data = static_cast<MinerThreadData*>(thread_data);

	data->instance->MinerThread(data->miner);
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

// TODO: Not used currently
VOID MiningService::MinerComThread()
{
	LOG_F(INFO, "Starting communication TCP command server on port %s\n", COM_PORT);

	// Start TCP server and when a client connects, dispatch info to the
	// MiningService::MinerDispatchThread function that will be ran on
	// a thread so that we can take N amount of clients
	miner_cmd_server_->Start(this, &MiningService::MinerDispatchThread);
}

DWORD WINAPI MiningService::MinerDispatchThread(SOCKET client_socket)
{
	// Handles what to do when we receive a miner command
	LOG_F(INFO, "Received miner command");
	TcpConnection client(client_socket);
	Packet* pack;
	Packet* start = new Packet(PCHAR("startMiner"), PCHAR("{ \"result\": \"success\"}"));

	while (client_socket != INVALID_SOCKET)
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
// ----------------------------------------

BOOL MiningService::StartMiner(Miner* miner)
{
	MinerThreadData* thread_data = new MinerThreadData;
	thread_data->instance = this;
	thread_data->miner = miner;


	miner->thread = CreateThread(NULL, NULL, LPTHREAD_START_ROUTINE(MinerThreadProxy), thread_data, NULL, NULL);

	if (miner->thread == INVALID_HANDLE_VALUE)
		return FALSE;

	DWORD exit_code;

	// Wait for our error timeout
	Sleep(40500);

	// Failed to get Process state, try to kill and return failure
	if (GetExitCodeProcess(miner->process->hProcess, &exit_code) == FALSE || (exit_code != STILL_ACTIVE && exit_code != 0))
	{
		StopMiner(miner->resource);
		return FALSE;
	}

	return TRUE;
}

BOOL MiningService::StopMiner(tstring resource)
{

	if (miners_.count(resource) == 0)
	{
		LOG_F(INFO, "Failed to stop mining resource %s, nothing mining on it!", resource.c_str());
		return FALSE;
	}

	Miner* miner_data = miners_.at(resource);

	if (TerminateProcess(miner_data->process->hProcess, 0x0) == FALSE)
	{
		LOG_F(INFO, "Failed to terminate miner process for resource %s!", resource.c_str());
		return FALSE;
	}

	return TRUE;
}

// For CCMiner, this will return the cuda devices and their id number
std::vector<std::string> MiningService::GetDevices()
{
	STARTUPINFO si;
	SECURITY_ATTRIBUTES saAttr;
	PROCESS_INFORMATION pi;
	HANDLE child_stdout_read = NULL, child_stdout_write = NULL;
	HANDLE child_stderr_read = NULL, child_stderr_write = NULL;

	std::vector<std::string> devices;

	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	// Create a pipe for the child process's STDOUT. 
	if (!CreatePipe(&child_stdout_read, &child_stdout_write, &saAttr, 0))
	{
		LOG_F(ERROR, "Failed to create stdout handle");
		return devices;
	}

	if (!CreatePipe(&child_stderr_read, &child_stderr_write, &saAttr, 0))
	{
		LOG_F(ERROR, "Failed to create stdout handle");
		return devices;
	}


	// Ensure the read handle to the pipe for STDOUT is not inherited.
	if (!SetHandleInformation(child_stdout_read, HANDLE_FLAG_INHERIT, 0))
	{
		LOG_F(ERROR, "Failed to set read handle settings");
		return devices;
	}

	// Ensure the read handle to the pipe for STDOUT is not inherited.
	if (!SetHandleInformation(child_stderr_read, HANDLE_FLAG_INHERIT, 0))
	{
		LOG_F(ERROR, "Failed to set read handle settings");
		return devices;
	}

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.hStdError = child_stderr_write;
	si.hStdOutput = child_stdout_write;
	si.dwFlags |= STARTF_USESTDHANDLES;

	ZeroMemory(&pi, sizeof(pi));

	PTSTR cmd = PTSTR("Miners//ccminer//ccminer-x64.exe -n");

	// Start the child process. 
	if (!CreateProcess(NULL,            // No module name (use command line)
		cmd,                            // Command line
		NULL,                           // Process handle not inheritable
		NULL,                           // Thread handle not inheritable
		TRUE,                          // Set handle inheritance to FALSE
		0,                              // No creation flags
		NULL,                           // Use parent's environment block
		NULL,                           // Use parent's starting directory 
		&si,                            // Pointer to STARTUPINFO structure
		&pi)                            // Pointer to PROCESS_INFORMATION structure
		)
	{
		LOG_F(ERROR, "CreateProcess failed (%d).\n", GetLastError());
		return devices;
	}

	CloseHandle(child_stdout_write);
	CloseHandle(child_stderr_write);

	// Read output of get devices
	// TODO: This needs heavy refactoring to account for weird cases
	// this is only covering the happy path right now
	DWORD dwRead;
	CHAR chBuf[4096];
	memset(chBuf, 0, 4096);
	BOOL bSuccess = FALSE;

	std::string output = "", error = "";
	for (;;) {
		bSuccess = ReadFile(child_stdout_read, chBuf, 4096, &dwRead, NULL);
		if (!bSuccess || dwRead == 0) break;

		std::string s(chBuf, dwRead);
		output += s;
	}

	dwRead = 0;
	for (;;) {
		bSuccess = ReadFile(child_stderr_read, chBuf, 4096, &dwRead, NULL);
		if (!bSuccess || dwRead == 0) break;

		std::string s(chBuf, dwRead);
		error += s;
	}


	std::istringstream devices_input(error);

	for (std::string line; std::getline(devices_input, line); ) {
		std::string device = "";
		size_t found = line.find("GPU #");

		if (found == std::string::npos)
			continue;
		// Only deal with up to 10 gpus for now
		device += line[5] + std::string("@");

		// Assume we always find this if we found GPU output
		found = line.find('@');
		// Gets the device name string
		device += line.substr(8, found - 8);

		LOG_F(INFO, "Found device %s", device.c_str());

		// Since this is a c string, the first char at the starting address will be converted to
		// an int. This happens to be a number always in our device string
		devices.push_back(device);
	}

	// Close process and thread handles. 
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return devices;
}

int MiningService::GetHashrate(tstring resource)
{
	Miner* miner = miners_.at(resource);

	EnterCriticalSection(&(miner->mutex));

	//WebSocket::pointer ws = WebSocket::from_url(strcat())

	/*TcpClient api_client(PTSTR("4050"));
	TcpConnection api(api_client.GetSocket());

	PTSTR cmd = PTSTR("summary");

	api.Write(cmd, sizeof(cmd));

	char data[PACKET_SIZE];
	api.Read(data, PACKET_SIZE);

	LOG_F(INFO, "%s", data);
	*/

	LeaveCriticalSection(&(miner->mutex));
	
	return 0;
}

VOID MiningService::MinerThread(Miner* miner)
{
	STARTUPINFO si;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(miner->process, sizeof(*(miner->process)));

	// Start the child process. 
	if (!CreateProcess(NULL,            // No module name (use command line)
		miner->command,                 // Command line
		NULL,                           // Process handle not inheritable
		NULL,                           // Thread handle not inheritable
		FALSE,                          // Set handle inheritance to FALSE
		0,                              // No creation flags
		NULL,                           // Use parent's environment block
		NULL,                           // Use parent's starting directory 
		&si,                            // Pointer to STARTUPINFO structure
		miner->process)                 // Pointer to PROCESS_INFORMATION structure
		)
	{
		LOG_F(ERROR, "CreateProcess failed (%d).\n", GetLastError());
		return;
	}

	// Wait until child process exits.
	WaitForSingleObject(miner->process->hProcess, INFINITE);

	LOG_F(INFO, "Stopping miner on resource %s", miner->resource.c_str());

	// Close process and thread handles. 
	CloseHandle(miner->process->hProcess);
	CloseHandle(miner->process->hThread);
}

VOID MiningService::CommandServerThread()
{
	LOG_F(INFO, "Starting TCP command server on port %s\n", PORT);

	// Start TCP server and when a client connects, dispatch info to the
	// MiningService::OnClientConnect function that will be ran on
	// a thread so that we can take N amount of clients
	cmd_server_->Start(this, &MiningService::OnClientConnect);
}


// When a client connects to the Command TCP server this is the filtering it goes through
DWORD WINAPI MiningService::OnClientConnect(SOCKET client_socket)
{
	LOG_F(INFO, "CLIENT CONNECTED!");
	TcpConnection client(client_socket);

	TcpClient miner_client(const_cast<PSTR>(COM_PORT));
	TcpConnection miner_com(miner_client.GetSocket());

	Packet *request = NULL, 
           *response = NULL,
	       *recvpack = NULL;

	Packet* stats = new Packet(PCHAR("hashRate"), PCHAR("{}"));
	Packet* success = new Packet(PCHAR("startMiner"), PCHAR("{ \"result\": \"success\", \"resource\": \"none\"}"));
	Packet* failure = new Packet(PCHAR("stopMiner"), PCHAR("{ \"result\": \"failure\", \"resource\": \"none\"}"));

	// Once we establish a connection with this client
	// it is expecting us to hold that connection open for more commands
	while(client_socket != INVALID_SOCKET)
	{
		// TODO: fix json null data being sent at the end
		request = new Packet;
		recvpack = new Packet;

		LOG_F(INFO, "Waiting to receive packet...");
		client.RecvData(request);
		LOG_F(INFO, "Got: %s", request->command);

		if (strcmp(request->command, CMD_RESOURCES) == 0)
		{
			devices_ = GetDevices();

			response = success;
			response->command = PTCHAR(CMD_RESOURCES);
			response->data = json({
				{ "resources", devices_ }
				});
			
			client.SendData(response);
			LOG_F(INFO, "Sent resources");
		}
		else if(strcmp(request->command, CMD_STATS) == 0)
		{
			std::string resource_id = request->data["resource"].get<std::string>();

			GetHashrate(resource_id);
			int rnum = 1300;

			stats->data = json({
				{"resource", "0@SM 6.1 Gigabyte GTX 1080"},
				{"hashRate", std::to_string(rnum)}
				});
			client.SendData(stats);
			LOG_F(INFO, "Sent hashRate");
		}
		else if(strcmp(request->command, CMD_START_MINER) == 0)
		{
			std::string resource;
			std::string miner = request->data["miner_binary"].get<std::string>();
			std::map<std::string, std::string> miner_args = request->data["miner_args"].get<std::map<std::string, std::string>>();

			std::string miner_start_command;
			miner_start_command += "Miners//ccminer//" + miner;

			// Build the miner command
			for(auto arg : miner_args)
			{
				// If the var is our resource then capture it
				if (arg.first == "-d")
					resource = arg.second;

				miner_start_command += " " + arg.first + " " + arg.second;
			}

			std::string port = std::to_string(API_BASE_PORT + miners_.size());

			// Add API gateway
			miner_start_command += " -b " + std::string(API_BASE_HOST) + ":" + port + " -r 3 --retry-pause=5 --timeout=10";

			LOG_F(INFO, "Miner cmd: %s", miner_start_command.c_str());

			Miner* miner_data = new Miner;
			miner_data->command = _strdup(miner_start_command.c_str());
			miner_data->resource = _strdup(resource.c_str());
			miner_data->running = FALSE;
			miner_data->api_gateway_port = _strdup(port.c_str());

			if(miners_.count(miner_data->resource) != 0)
			{
				StopMiner(miner_data->resource);
				Miner* old_miner = miners_.at(miner_data->resource);
				Delete(old_miner);
				miners_.erase(miners_.find(miner_data->resource));
			}

			miners_.insert(std::pair<tstring, Miner*>(miner_data->resource, miner_data));

			response = (StartMiner(miner_data) != 0) ? success : failure;

			response->command = PTCHAR(CMD_START_MINER);
			response->data["resource"] = devices_[atoi(resource.c_str())];

			//miner_com.SendData(pack);
			//miner_com.RecvData(recvpack);
			client.SendData(response);
			
			LOG_F(INFO, "Started mining");
		}
		else if(strcmp(request->command, CMD_STOP_MINER) == 0)
		{
			std::string str_resource = request->data["resource"].get<std::string>();
			PTSTR resource = _strdup(str_resource.c_str());

			response = StopMiner(resource) ? success : failure;

			response->command = PTCHAR(CMD_STOP_MINER);
			response->data["resource"] = devices_[atoi(resource)];

			client.SendData(response);
			LOG_F(INFO, "Stopped mining");
		}
		else
		{
			LOG_F(INFO, "UNKNOWN COMMAND RECEIVED %s, ", request->command);
		}

		// TODO: THIS IS AWFUL, fix with proper memory management in each command
		Delete(success);
		Delete(failure);

		success = new Packet(PCHAR("startMiner"), PCHAR("{ \"result\": \"success\", \"resource\": \"none\"}"));
		failure = new Packet(PCHAR("stopMiner"), PCHAR("{ \"result\": \"failure\", \"resource\": \"none\"}"));

		Delete(request);
		Delete(recvpack);
		Sleep(500);
	}
	client.Close(true);
	LOG_F(INFO, "CLIENT DISCONNECTED");

	Delete(stats);
	Delete(success);
	Delete(failure);
	Delete(request);
	Delete(recvpack);

	return 0x0;
}

