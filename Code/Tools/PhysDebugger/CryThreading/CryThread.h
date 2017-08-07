#pragma once

struct CryEvent {
	CryEvent() { hEvent = CreateEvent(0,FALSE,FALSE,0); }
	~CryEvent() { CloseHandle(hEvent); }
	void Set() { SetEvent(hEvent); }
	int Wait(int interval=INFINITE) { return WaitForSingleObject(hEvent,interval); }
	HANDLE hEvent;
};