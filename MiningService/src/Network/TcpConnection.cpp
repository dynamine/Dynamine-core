#include "TcpConnection.hpp"
#include <iostream>
#include <minwinbase.h>

TcpConnection::TcpConnection(SOCKET socket)
{
	socket_fd_ = socket;
	status = connection_open;
	InitializeCriticalSection(&write_mutex_);

	read_buffer_size_ = 1024;
	read_bytes_ = 0;
}

TcpConnection::~TcpConnection()
{
	Close(true);
	DeleteCriticalSection(&write_mutex_);
}

BOOL TcpConnection::Write(PVOID data, size_t size)
{
	EnterCriticalSection(&write_mutex_);

	size_t leftOver = size;
	size_t wrote = 0;
	size_t sizeToWrite = 0;
	size_t bytesWritten = 0;

	while (leftOver > 0 && wrote < size)
	{
		sizeToWrite = std::min<size_t>(PACKET_SIZE, leftOver);
		bytesWritten = send(socket_fd_, (const char*)(wrote + (char*)data), sizeToWrite, 0);
		// if this is less than 0 it is a error
		if (bytesWritten < 0)
		{
			LeaveCriticalSection(&write_mutex_);
			return FALSE;
		}


		wrote += bytesWritten;
		if (bytesWritten > leftOver)
		{
			LeaveCriticalSection(&write_mutex_);
			return TRUE;
		}
		leftOver -= bytesWritten;
	}

	LeaveCriticalSection(&write_mutex_);
	return TRUE;
}

BOOL TcpConnection::Read(PVOID data, size_t size)
{
	size_t leftOver = size;
	size_t read = 0;
	size_t sizeToRead = 0;
	size_t bytesRead = 0;

	while (leftOver > 0 && read < size)
	{
		sizeToRead = std::min<size_t>(PACKET_SIZE, leftOver);
		bytesRead = recv(socket_fd_, (read + (char*)data), sizeToRead, 0);
		// if this is less than 0 it is a error
		if (bytesRead < 0)
			return FALSE;

		read += bytesRead;
		if (bytesRead > leftOver)
			return TRUE;
		leftOver -= bytesRead;
	}
	return TRUE;
}

VOID TcpConnection::Close(BOOL force)
{
	EnterCriticalSection(&write_mutex_);

	if (!force && ((status == connection_writing) || (status == connection_reading)))
	{
		status = connection_closing;
		LeaveCriticalSection(&write_mutex_);
		return;
	}

	if (socket_fd_ != INVALID_SOCKET)
		closesocket(socket_fd_);
	
	status = connection_closed;
	LeaveCriticalSection(&write_mutex_);
}

packet_accept_result TcpConnection::RecvData(Packet *pack)
{
	status = connection_reading;

	// Receive encoded json as raw bytes
	PBYTE encoded_json = new BYTE[PACKET_SIZE];

	// Attempt to read in encoded json
	if(!Read(encoded_json, PACKET_SIZE))
	{
		LOG_F(ERROR, "RecvData: Failed to receive encoded json!");
		return packet_accept_error;
	}

	// Get decoded json from bytes
	json decoded_json = json::parse(encoded_json);

	// Get command
	std::string cmd = decoded_json["cmd"].get<std::string>();

	// Copy command into packet
	strcpy_s(pack->command, cmd.length() + 1, cmd.c_str());

	// Copy body into packet
	//pack->data = json::parse(decoded_json["data"].dump());
	pack->data = decoded_json["data"];
	// Set status to ready
	status = connection_open;

	return packet_accept_success;
}

BOOL TcpConnection::SendData(Packet *pack)
{
	status = connection_writing;

	json json_response = json({});

	json data = pack->data;

	json_response["cmd"] = std::string(pack->command);
	json_response["data"] = data.dump();

	std::string jserial = json_response.dump();

	BYTE jpacket[PACKET_SIZE];
	ZeroMemory(&jpacket, PACKET_SIZE);

	memcpy(jpacket, jserial.data(), jserial.length());

	if (!Write(jpacket, PACKET_SIZE))
	{
		LOG_F(ERROR, "SendData: Failed to send data!");
		return false;
	}

	status = connection_open;

	return true;
}

size_t TcpConnection::GetReadBytes()
{
	return read_bytes_;
}

void TcpConnection::SetReadBytes(size_t len)
{
	read_bytes_ = len;
}

void TcpConnection::SetReadBufferSize(size_t size)
{
	read_buffer_size_ = size;
}

size_t TcpConnection::GetReadBufferSize()
{
	return read_buffer_size_;
}