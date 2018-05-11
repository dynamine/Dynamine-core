#include "TcpConnection.hpp"

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
	DeleteCriticalSection(&write_mutex_);
	Close(true);
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

	if ((!force && (status == connection_writing)) || (status == connection_reading))
	{
		status = connection_closing;
		LeaveCriticalSection(&write_mutex_);
		return;
	}
	closesocket(socket_fd_);
	status = connection_closed;
	LeaveCriticalSection(&write_mutex_);
}

packet_accept_result TcpConnection::RecvData(Packet *pack)
{
	status = connection_reading;

	//TODO: Implement in the future

	//PARTIAL_PACK_1 *partialPack1 = new PARTIAL_PACK_1;
	//PARTIAL_PACK_2 *partialPack2 = new PARTIAL_PACK_2;

	//size_t partialPack2Size = 0;
	//unsigned char newHash[0x14];

	///*----------------Receive Packets---------------------*/
	//// Receive first packet
	//if (!Read(partialPack1, PACKET_SIZE))
	//{
	//	LOG_F(ERROR, "RecvData: Failed to receive partialPack1!");
	//	return packet_accept_error;
	//}

	//// Receive second packet
	//if (!Read(partialPack2, PACKET_SIZE))
	//{
	//	LOG_F(ERROR, "SendData: Failed to receive partialPack2!");
	//	return packet_accept_error;
	//}
	///*----------------------------------------------------*/

	///*-----------------Parse Partial Pack 1---------------*/
	//// Check to make sure the headerKey matches up
	//for (int i = 0; i < 4; ++i)
	//{
	//	if (partialPack1->header[i] != headerKey[i])
	//	{
	//		LOG_F(ERROR, "RecvData: Failed to receive correct headerKey!");
	//		return packet_accept_partial;
	//	}
	//}
	//// Get the size of the second packet
	//partialPack2Size = (partialPack1->header[4] << 24) |
	//	(partialPack1->header[5] << 16) |
	//	(partialPack1->header[6] << 8) |
	//	partialPack1->header[7];

	//// Populate packet with the data from the first packet
	//memcpy(pack->header, partialPack1->header, 0x8);
	//memcpy(pack->dataHash, partialPack1->packet_hash, 0x14);
	//pack->dataSize = partialPack2Size - 0x8;
	///*----------------------------------------------------*/

	///*-----------------Parse Partial Pack 2---------------*/
	//// Get hash of the second packet
	//tools::DoSha1(partialPack2, partialPack2Size, newHash);

	//// Compare hashes to make sure the data isn't corrupt or tampered with
	//if (memcpy(pack->dataHash, newHash, 0x14) != 0)
	//{
	//	Output::GetInstance().ErrPrint("RecvData: DataHashes did not match!");
	//	return packet_accept_error;
	//}

	//// Decrypt data and store in packet
	//tools::DoRc4(partialPack2->data, partialPack2->data_size, pack->data);

	//// Store id and size of data from second packet in main packet
	//pack->id = partialPack2->id;
	//pack->dataSize = partialPack2->data_size;
	///*----------------------------------------------------*/

	//delete partialPack1;
	//delete partialPack2;

	status = connection_open;

	return packet_accept_success;
}

BOOL TcpConnection::SendData(Packet *pack)
{
	status = connection_writing;

	//PARTIAL_PACK_1 *partialPack1 = new PARTIAL_PACK_1;
	//PARTIAL_PACK_2 *partialPack2 = new PARTIAL_PACK_2;

	//size_t partialPack2Size = 0;

	///*----------------Setup Second Packet----------------*/
	//// Set our id
	//partialPack2->id = pack->id;

	//// Set our data_size
	//partialPack2->data_size = pack->dataSize;

	//// Encrypt the data
	//tools::DoRc4(pack->data, pack->dataSize, partialPack2->data);
	///*---------------------------------------------------*/


	///*---------------Setup First Packet------------------*/
	//// Setup the size of the second packet
	//partialPack2Size = pack->dataSize + 0x8;

	//// Populate first 4 bytes of header with auth Key
	//for (int i = 0; i < 4; ++i)
	//{
	//	partialPack1->header[i] = headerKey[i];
	//}

	//// Populate next 4 bytes of header with big endian size of data
	//partialPack1->header[4] = (partialPack2Size & 0xFF000000) >> 24;
	//partialPack1->header[5] = (partialPack2Size & 0xFF0000) >> 16;
	//partialPack1->header[6] = (partialPack2Size & 0xFF00) >> 8;
	//partialPack1->header[7] = (partialPack2Size & 0xFF);

	//// Populate packet_hash in packet
	//tools::DoSha1(partialPack2, partialPack2Size, partialPack1->packet_hash);
	///*----------------------------------------------------*/


	///*----------------Send Packets------------------------*/
	//// Send first packet
	//if (!Write(partialPack1, PACKET_SIZE))
	//{
	//	Output::GetInstance().ErrPrint("SendData: Failed to send partialPack1!");
	//	return false;
	//}

	//// Send second packet
	//if (!Write(partialPack2, PACKET_SIZE))
	//{
	//	Output::GetInstance().ErrPrint("SendData: Failed to send partialPack2!");
	//	return false;
	//}
	///*----------------------------------------------------*/

	//delete partialPack1;
	//delete partialPack2;

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