#pragma once

struct IThread {
	virtual void ThreadEntry() {};
};

struct IThreadManager {
	static HANDLE SpawnThread(IThread *pTask, const char*,...) { return CreateThread(0,0,TaskProc,pTask,0,0); }
	static unsigned long __stdcall TaskProc(void *pTask) { ((IThread*)pTask)->ThreadEntry(); return 0; }
};