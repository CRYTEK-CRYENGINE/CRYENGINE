// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _SHARED_RESOURCES_H_
#define _SHARED_RESOURCES_H_

#pragma once

#ifdef INCLUDE_SCALEFORM_SDK

#pragma warning(push)
#pragma warning(disable : 6326)   // Potential comparison of a constant with another constant
#pragma warning(disable : 6011)   // Dereferencing NULL pointer
#include <CryCore/Platform/CryWindows.h>
#include <GFxLoader.h>            // includes <windows.h>
#include <CryRenderer/IScaleform.h>
#pragma warning(pop)
#include <CrySystem/Scaleform/ConfigScaleform.h>
#include "GAllocatorCryMem.h"
#include <CryMemory/CrySizer.h>
#include <CryThreading/IThreadManager.h>

class GFxLoader2;
class ICrySizer;
class GSystemInitWrapper;
class MeshCacheResetThread;
class GImeHelper;

class CSharedFlashPlayerResources
{
public:
	static CSharedFlashPlayerResources& GetAccess();

	static void Init();
	static void Shutdown();

public:
	GFxLoader2*       GetLoader(bool getRawInterface = false);
	IScaleformRecording* GetRenderer(bool getRawInterface = false);

	CryGFxMemInterface::Stats GetSysAllocStats() const;
	void                      GetMemoryUsage(ICrySizer* pSizer) const;

	int  CreateMemoryArena(unsigned int arenaID, bool resetCache) const;
	void DestoryMemoryArena(unsigned int arenaID) const;
	bool AreCustomMemoryArenasActive() const;

	void ResetMeshCache() const;
	bool IsFlashVideoIOStarving() const;

	float GetFlashHeapFragmentation() const;
	void  SetImeFocus(GFxMovieView* pMovie, bool bSet);

private:
	CSharedFlashPlayerResources();
	~CSharedFlashPlayerResources();

private:
	static CSharedFlashPlayerResources* ms_pSharedFlashPlayerResources;

private:
	GSystemInitWrapper* m_pGSystemInit;
	GFxLoader2* m_pLoader;
	IScaleformRecording* m_pRecorder;
	MeshCacheResetThread* m_pMeshCacheResetThread;
#if defined(USE_GFX_IME)
	GImeHelper* m_pImeHelper;
#endif
};

class GFxLoader2:public GFxLoader
{
	friend class CSharedFlashPlayerResources;

public:
	// cannot configure GFC's RefCountBase via public inheritance so implement own ref counting
	void AddRef();
	void Release();
	int  GetRefCount() const { return m_refCount; }

	void UpdateParserVerbosity();

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->Add(this, sizeof(*this));  // just add object, internals are counted implicitly by provided GFxCryMemInterface / GSysAlloc
	}

private:
	GFxLoader2();
	virtual ~GFxLoader2();
	void SetupDynamicFontCache();

private:
	volatile int m_refCount;
	GFxParseControl m_parserVerbosity;
};

class MeshCacheResetThread:public IThread
{
public:
	// IThread
	// Start accepting work on thread
	virtual void ThreadEntry();

	// Signals the thread that it should not accept anymore work and exit
	void SignalStopWork();

public:
	MeshCacheResetThread();
	~MeshCacheResetThread();

	void IssueReset();

private:
	static const char* ms_pThreadName;

private:
	volatile bool m_cancelRequestSent;
	CryEvent m_awakeThread;
};

#endif // #ifdef INCLUDE_SCALEFORM_SDK

#endif // #ifndef _SHARED_RESOURCES_H_
