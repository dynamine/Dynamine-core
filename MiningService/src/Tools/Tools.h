#pragma once
#include <tchar.h>
#include <windows.h>
#include <crtdbg.h>

VOID PDelete(Packet* mem)
{
	if (mem == NULL)
		return;

	delete mem;
	mem = NULL;
}

