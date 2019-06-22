#pragma once

enum ESystemEvent { ESYSTEM_EVENT_LEVEL_LOAD_END, ESYSTEM_EVENT_3D_POST_RENDERING_END };
struct ISystemEventListener {};

#include <CrySystem/ILog.h>

struct ITimer	{	enum ETimer	{	ETIMER_GAME, ETIMER_UI }; };

#define CPUF_SSE 0x10
struct ISystem : ILog {
	int GetCPUFlags() { return CPUF_SSE; }
	struct ILog *GetILog() { return this; }
	ISystem *GetISystemEventDispatcher() { return this; }
	void RegisterListener(void*, const char*) {}
	virtual struct IPhysRenderer *GetIPhysRenderer() const { return nullptr; }
	virtual float GetCurrTime(ITimer::ETimer = ITimer::ETIMER_GAME) const { return 0; }
	virtual float GetFrameTime() const { return 0.01f; }
	ISystem* GetMaterialManager() { return this; }
	static struct ISurfaceType* GetSurfaceTypeByName(const char*) { return nullptr; }
};

inline void ModuleInitISystem(ISystem*, const char*) {}