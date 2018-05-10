#pragma once
#include "NetworkStructs.h"
#include <WinSock2.h>

class TcpConnection
{
public:
	TcpConnection(SOCKET socket);

	~TcpConnection();

	/* This will send the packet via tcp to the socketfd that is specified. It breaks it up
	* into two packets, the first with the header and hash of the second packet which contains
	* all the actual data, id of packet, and datasize. This way we can make sure that the important
	* data is successfully received.
	* Params: The packet needs to have the ID, data, and data_size vars all populated
	* returns: BOOLean of true if compled successfully or false if an error occurred
	*/
	BOOL SendData(Packet *pack);

	packet_accept_result RecvData(Packet *pack);

	VOID Close(BOOL force);

	VOID SetReadBufferSize(size_t size);
	VOID SetReadBytes(size_t len);

	size_t GetReadBytes();
	size_t GetReadBufferSize();

	// TODO: This should be private, deal with it later
	PWORD read_buffer;

protected:
	// TODO: NOT USED YET
	/* This is the first part of the packet that is sent and received.
	* It contains the header which has the size of the second packet and
	* the headerKey. It also contains the dataHash for the second packet.
	*/
	struct PARTIAL_PACK_1
	{
		unsigned char header[8];        // 8 Bytes
		unsigned char packet_hash[0x14]; // 20 bytes
	};

	struct PARTIAL_PACK_2
	{
		int id;             // 4 bytes
		size_t data_size;    // 4 bytes
		PVOID data;         // < 1016 bytes
	};

private:
	SOCKET socket_fd_;
	size_t read_buffer_size_;
	size_t read_bytes_;


	CRITICAL_SECTION write_mutex_;

	connection_status status;

	/* Read data from a socket file descriptor into the data
	* Params: data pointer of where to read in the data to
	* returns: boolean of true if completed successfully or false
	* if an error occurred*/
	BOOL Read(PVOID data, size_t size);
	/* Write data to a socket file descriptor from the data
	* Params: data pointer of the data to write to the socket
	* returns: boolean of true if completed successfully or false
	* if an error occurred*/
	BOOL Write(PVOID data, size_t size);
};
