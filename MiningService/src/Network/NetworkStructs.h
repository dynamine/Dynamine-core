#pragma once
#include <json.hpp>

using namespace nlohmann;

const size_t PACKET_SIZE = 1024;

static unsigned char SECURITY_KEY[16] =
{
	0x09, 0x35, 0x4D, 0xC0,
	0x77, 0x9E, 0xED, 0x78,
	0xD4, 0xDA, 0x87, 0xB4,
	0xA0, 0x47, 0xBC, 0x31
};

const unsigned char headerKey[4] = { 'U', '3', 'R', '0' };

enum packet_accept_result
{
	packet_accept_success = 0x0,
	packet_accept_error = 0x1,
	packet_accept_partial = 0x2
};

enum packet_send_result
{
	packet_send_sucess = 0x0,
	packet_send_error = 0x1,
	packet_send_partial = 0x2
};

enum connection_status
{
	connection_closed = 0x0,
	connection_open = 0x1,
	connection_closing = 0x2,
	connection_reading = 0x3,
	connection_writing = 0x4
};

/* This is how the packet is setup.
* However, we want to receive each part of this separate to prevent DDOSing.
* The way this works is if the header is incorrect, then we reject the rest.
* That way we won't be receiving huge amounts of fake data if it is sent. We will only
* receive the large chunk of data if the id and header are correct. */
struct Packet
{
	Packet() { this->command = new char[CHAR_MAX];  }
	Packet(char* command, char* data)
	{
		this->command = new char[CHAR_MAX];
		memcpy(this->command, command, strlen(command)+1);
		this->command[strlen(this->command)] = '\0';

		this->data = json::parse(data);
	}

	Packet(char* command, json j)
	{
		this->command = new char[CHAR_MAX];
		memcpy(this->command, command, strlen(command) + 1);
		this->command[strlen(this->command)] = '\0';

		this->data = j;
	}

	char* command;
	json data;
};