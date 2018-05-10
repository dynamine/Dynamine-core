#pragma once
#include <minwindef.h>
#include <json.hpp>
#include <iostream>
using namespace nlohmann;

class TCPServer
{
public:
	TCPServer(WORD port);
	~TCPServer();

	VOID Start();

	
};
