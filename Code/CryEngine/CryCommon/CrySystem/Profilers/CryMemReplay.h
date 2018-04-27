// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CryMemReplay.h
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CryMemReplay_h__
#define __CryMemReplay_h__
#pragma once

namespace EMemReplayAllocClass
{
enum Class
{
	C_UserPointer = 0,
	C_D3DDefault,
	C_D3DManaged,
};
}

namespace EMemReplayUserPointerClass
{
enum Class
{
	C_Unknown = 0,
	C_CrtNew,
	C_CrtNewArray,
	C_CryNew,
	C_CryNewArray,
	C_CrtMalloc,
	C_CryMalloc,
	C_STL,
};
}

#if CAPTURE_REPLAY_LOG
//! Memory replay interface, access it with CryGetMemReplay call.
struct IMemReplay
{
	virtual ~IMemReplay(){}
	virtual void DumpStats() = 0;
	virtual void DumpSymbols() = 0;

	virtual void StartOnCommandLine(const char* cmdLine) = 0;
	virtual void Start(bool bPaused = false, const char* openString = NULL) = 0;
	virtual void Stop() = 0;
	virtual void Flush() = 0;

	virtual void GetInfo(CryReplayInfo& infoOut) = 0;

	//! Call to begin a new allocation scope.
	virtual bool EnterScope(EMemReplayAllocClass::Class cls, uint16 subCls, int moduleId) = 0;

	//! Records an event against the currently active scope and exits it.
	virtual void ExitScope_Alloc(UINT_PTR id, UINT_PTR sz, UINT_PTR alignment = 0) = 0;
	virtual void ExitScope_Realloc(UINT_PTR originalId, UINT_PTR newId, UINT_PTR sz, UINT_PTR alignment = 0) = 0;
	virtual void ExitScope_Free(UINT_PTR id) = 0;
	virtual void ExitScope() = 0;

	virtual bool EnterLockScope() = 0;
	virtual void LeaveLockScope() = 0;

	virtual void AllocUsage(EMemReplayAllocClass::Class allocClass, UINT_PTR id, UINT_PTR used) = 0;

	virtual void AddAllocReference(void* ptr, void* ref) = 0;
	virtual void RemoveAllocReference(void* ref) = 0;

	virtual void AddLabel(const char* label) = 0;
	virtual void AddLabelFmt(const char* labelFmt, ...) = 0;
	virtual void AddFrameStart() = 0;
	virtual void AddScreenshot() = 0;

	virtual void AddContext(int type, uint32 flags, const char* str) = 0;
	virtual void AddContextV(int type, uint32 flags, const char* format, va_list args) = 0;
	virtual void RemoveContext() = 0;

	virtual void MapPage(void* base, size_t size) = 0;
	virtual void UnMapPage(void* base, size_t size) = 0;

	virtual void RegisterFixedAddressRange(void* base, size_t size, const char* name) = 0;
	virtual void MarkBucket(int bucket, size_t alignment, void* base, size_t length) = 0;
	virtual void UnMarkBucket(int bucket, void* base) = 0;
	virtual void BucketEnableCleanups(void* allocatorBase, bool enabled) = 0;

	virtual void MarkPool(int pool, size_t alignment, void* base, size_t length, const char* name) = 0;
	virtual void UnMarkPool(int pool, void* base) = 0;
	virtual void AddTexturePoolContext(void* ptr, int mip, int width, int height, const char* name, uint32 flags) = 0;

	virtual void AddSizerTree(const char* name) = 0;

	virtual void RegisterContainer(const void* key, int type) = 0;
	virtual void UnregisterContainer(const void* key) = 0;
	virtual void BindToContainer(const void* key, const void* alloc) = 0;
	virtual void UnbindFromContainer(const void* key, const void* alloc) = 0;
	virtual void SwapContainers(const void* keyA, const void* keyB) = 0;

	virtual bool EnableAsynchMode() = 0;
};
#endif

#if CAPTURE_REPLAY_LOG
struct CDummyMemReplay : IMemReplay
#else //CAPTURE_REPLAY_LOG
struct IMemReplay
#endif
{
	void DumpStats()                                                {}
	void DumpSymbols()                                              {}

	void StartOnCommandLine(const char* cmdLine)                    {}
	void Start(bool bPaused = false, const char* openString = NULL) {}
	void Stop()                                                     {}
	void Flush()                                                    {}

	void GetInfo(CryReplayInfo& infoOut)                            {}

	//! Call to begin a new allocation scope.
	bool EnterScope(EMemReplayAllocClass::Class cls, uint16 subCls, int moduleId) { return false; }

	//! Records an event against the currently active scope and exits it.
	void ExitScope_Alloc(UINT_PTR id, UINT_PTR sz, UINT_PTR alignment = 0)                                {}
	void ExitScope_Realloc(UINT_PTR originalId, UINT_PTR newId, UINT_PTR sz, UINT_PTR alignment = 0)      {}
	void ExitScope_Free(UINT_PTR id)                                                                      {}
	void ExitScope()                                                                                      {}

	bool EnterLockScope()                                                                                 { return false; }
	void LeaveLockScope()                                                                                 {}

	void AllocUsage(EMemReplayAllocClass::Class allocClass, UINT_PTR id, UINT_PTR used)                   {}

	void AddAllocReference(void* ptr, void* ref)                                                          {}
	void RemoveAllocReference(void* ref)                                                                  {}

	void AddLabel(const char* label)                                                                      {}
	void AddLabelFmt(const char* labelFmt, ...)                                                           {}
	void AddFrameStart()                                                                                  {}
	void AddScreenshot()                                                                                  {}

	void AddContext(int type, uint32 flags, const char* str)                                              {}
	void AddContextV(int type, uint32 flags, const char* format, va_list args)                            {}
	void RemoveContext()                                                                                  {}

	void MapPage(void* base, size_t size)                                                                 {}
	void UnMapPage(void* base, size_t size)                                                               {}

	void RegisterFixedAddressRange(void* base, size_t size, const char* name)                             {}
	void MarkBucket(int bucket, size_t alignment, void* base, size_t length)                              {}
	void UnMarkBucket(int bucket, void* base)                                                             {}
	void BucketEnableCleanups(void* allocatorBase, bool enabled)                                          {}

	void MarkPool(int pool, size_t alignment, void* base, size_t length, const char* name)                {}
	void UnMarkPool(int pool, void* base)                                                                 {}
	void AddTexturePoolContext(void* ptr, int mip, int width, int height, const char* name, uint32 flags) {}

	void AddSizerTree(const char* name)                                                                   {}

	void RegisterContainer(const void* key, int type)                                                     {}
	void UnregisterContainer(const void* key)                                                             {}
	void BindToContainer(const void* key, const void* alloc)                                              {}
	void UnbindFromContainer(const void* key, const void* alloc)                                          {}
	void SwapContainers(const void* keyA, const void* keyB)                                               {}

	bool EnableAsynchMode()                                                                               { return false; }
};

#if CAPTURE_REPLAY_LOG
inline IMemReplay* CryGetIMemReplay()
{
	static CDummyMemReplay s_dummyMemReplay;
	static IMemReplay* s_pMemReplay = 0;
	if (!s_pMemReplay)
	{
		// get it from System
		IMemoryManager* pMemMan = CryGetIMemoryManager();
		if (pMemMan)
			s_pMemReplay = pMemMan->GetIMemReplay();
		if (!s_pMemReplay)
			return &s_dummyMemReplay;
	}
	return s_pMemReplay;
}
#else
inline IMemReplay* CryGetIMemReplay()
{
	return NULL;
}
#endif

#if defined(__cplusplus) && CAPTURE_REPLAY_LOG
namespace EMemStatContextTypes
{
// Add new types at the end, do not modify existing values.
enum Type
{
	MSC_MAX             = 0,
	MSC_CGF             = 1,
	MSC_MTL             = 2,
	MSC_DBA             = 3,
	MSC_CHR             = 4,
	MSC_LMG             = 5,
	MSC_AG              = 6,
	MSC_Texture         = 7,
	MSC_ParticleLibrary = 8,

	MSC_Physics         = 9,
	MSC_Terrain         = 10,
	MSC_Shader          = 11,
	MSC_Other           = 12,
	MSC_RenderMesh      = 13,
	MSC_Entity          = 14,
	MSC_Navigation      = 15,
	MSC_ScriptCall      = 16,

	MSC_CDF             = 17,

	MSC_RenderMeshType  = 18,

	MSC_ANM             = 19,
	MSC_CGA             = 20,
	MSC_CAF             = 21,
	MSC_ArchetypeLib    = 22,

	MSC_SoundProject    = 23,
	MSC_EntityArchetype = 24,

	MSC_LUA             = 25,
	MSC_D3D             = 26,
	MSC_ParticleEffect  = 27,
	MSC_SoundBuffer     = 28,
	MSC_FSB             = 29, //!< Sound bank data

	MSC_AIObjects       = 30,
	MSC_Animation       = 31,
	MSC_Debug           = 32,

	MSC_FSQ             = 33,
	MSC_Mannequin       = 34,

	MSC_GeomCache       = 35
};
}

namespace EMemStatContextFlags
{
enum Flags
{
	MSF_None     = 0,
	MSF_Instance = 1,
};
}

namespace EMemStatContainerType
{
enum Type
{
	MSC_Vector,
	MSC_Tree,
};
}

class CMemStatContext
{
public:
	CMemStatContext(EMemStatContextTypes::Type type, uint32 flags, const char* str)
	{
		CryGetIMemReplay()->AddContext(type, flags, str);
	}
	~CMemStatContext()
	{
		CryGetIMemReplay()->RemoveContext();
	}
private:
	CMemStatContext(const CMemStatContext&);
	CMemStatContext& operator=(const CMemStatContext&);
};

class CMemStatContextFormat
{
public:
	CMemStatContextFormat(EMemStatContextTypes::Type type, uint32 flags, const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		CryGetIMemReplay()->AddContextV(type, flags, format, args);
		va_end(args);
	}
	~CMemStatContextFormat()
	{
		CryGetIMemReplay()->RemoveContext();
	}
private:
	CMemStatContextFormat(const CMemStatContextFormat&);
	CMemStatContextFormat& operator=(const CMemStatContextFormat&);
};

class CCondMemStatContext
{
public:
	CCondMemStatContext(bool cond, EMemStatContextTypes::Type type, uint32 flags, const char* str)
		: m_cond(cond)
	{
		if (cond)
			CryGetIMemReplay()->AddContext(type, flags, str);
	}
	~CCondMemStatContext()
	{
		if (m_cond)
			CryGetIMemReplay()->RemoveContext();
	}
private:
	CCondMemStatContext(const CCondMemStatContext&);
	CCondMemStatContext& operator=(const CCondMemStatContext&);
private:
	const bool m_cond;
};

class CCondMemStatContextFormat
{
public:
	CCondMemStatContextFormat(bool cond, EMemStatContextTypes::Type type, uint32 flags, const char* format, ...)
		: m_cond(cond)
	{
		if (cond)
		{
			va_list args;
			va_start(args, format);
			CryGetIMemReplay()->AddContextV(type, flags, format, args);
			va_end(args);
		}
	}
	~CCondMemStatContextFormat()
	{
		if (m_cond)
			CryGetIMemReplay()->RemoveContext();
	}
private:
	CCondMemStatContextFormat(const CCondMemStatContextFormat&);
	CCondMemStatContextFormat& operator=(const CCondMemStatContextFormat&);
private:
	const bool m_cond;
};
#endif // CAPTURE_REPLAY_LOG

#if CAPTURE_REPLAY_LOG
	#define INCLUDE_MEMSTAT_CONTEXTS     1
	#define INCLUDE_MEMSTAT_ALLOC_USAGES 1
	#define INCLUDE_MEMSTAT_CONTAINERS   1
#else
	#define INCLUDE_MEMSTAT_CONTEXTS     0
	#define INCLUDE_MEMSTAT_ALLOC_USAGES 0
	#define INCLUDE_MEMSTAT_CONTAINERS   0
#endif

#if CAPTURE_REPLAY_LOG
	#define MEMSTAT_CONCAT_(a, b) a ## b
	#define MEMSTAT_CONCAT(a, b)  MEMSTAT_CONCAT_(a, b)
#endif

#if INCLUDE_MEMSTAT_CONTEXTS
	#define MEMSTAT_CONTEXT(type, id, str)                        CMemStatContext MEMSTAT_CONCAT(_memctx, __LINE__) (type, id, str);
	#define MEMSTAT_CONTEXT_FMT(type, id, format, ...)            CMemStatContextFormat MEMSTAT_CONCAT(_memctx, __LINE__) (type, id, format, __VA_ARGS__);
	#define MEMSTAT_COND_CONTEXT(cond, type, id, str)             CCondMemStatContext MEMSTAT_CONCAT(_memctx, __LINE__) (cond, type, id, str);
	#define MEMSTAT_COND_CONTEXT_FMT(cond, type, id, format, ...) CCondMemStatContextFormat MEMSTAT_CONCAT(_memctx, __LINE__) (cond, type, id, format, __VA_ARGS__);
#else
	#define MEMSTAT_CONTEXT(...)
	#define MEMSTAT_CONTEXT_FMT(...)
	#define MEMSTAT_COND_CONTEXT(...)
	#define MEMSTAT_COND_CONTEXT_FMT(...)
#endif

#if INCLUDE_MEMSTAT_CONTAINERS
template<typename T>
static void MemReplayRegisterContainerStub(const void* key, int type)
{
	CryGetIMemReplay()->RegisterContainer(key, type);
}
	#define MEMSTAT_REGISTER_CONTAINER(key, type, T)         MemReplayRegisterContainerStub<T>(key, type)
	#define MEMSTAT_UNREGISTER_CONTAINER(key)                CryGetIMemReplay()->UnregisterContainer(key)
	#define MEMSTAT_BIND_TO_CONTAINER(key, ptr)              CryGetIMemReplay()->BindToContainer(key, ptr)
	#define MEMSTAT_UNBIND_FROM_CONTAINER(key, ptr)          CryGetIMemReplay()->UnbindFromContainer(key, ptr)
	#define MEMSTAT_SWAP_CONTAINERS(keyA, keyB)              CryGetIMemReplay()->SwapContainers(keyA, keyB)
	#define MEMSTAT_REBIND_TO_CONTAINER(key, oldPtr, newPtr) CryGetIMemReplay()->UnbindFromContainer(key, oldPtr); CryGetIMemReplay()->BindToContainer(key, newPtr)
#else
	#define MEMSTAT_REGISTER_CONTAINER(key, type, T)
	#define MEMSTAT_UNREGISTER_CONTAINER(key)
	#define MEMSTAT_BIND_TO_CONTAINER(key, ptr)
	#define MEMSTAT_UNBIND_FROM_CONTAINER(key, ptr)
	#define MEMSTAT_SWAP_CONTAINERS(keyA, keyB)
	#define MEMSTAT_REBIND_TO_CONTAINER(key, oldPtr, newPtr)
#endif

#if INCLUDE_MEMSTAT_ALLOC_USAGES
	#define MEMSTAT_USAGE(ptr, size) CryGetIMemReplay()->AllocUsage(EMemReplayAllocClass::C_UserPointer, (UINT_PTR)ptr, size)
#else
	#define MEMSTAT_USAGE(ptr, size)
#endif

#if CAPTURE_REPLAY_LOG

class CMemStatScopedLabel
{
public:
	explicit CMemStatScopedLabel(const char* name)
		: m_name(name)
	{
		CryGetIMemReplay()->AddLabelFmt("%s Begin", name);
	}

	~CMemStatScopedLabel()
	{
		CryGetIMemReplay()->AddLabelFmt("%s End", m_name);
	}

private:
	CMemStatScopedLabel(const CMemStatScopedLabel&);
	CMemStatScopedLabel& operator=(const CMemStatScopedLabel&);

private:
	const char* m_name;
};

	#define MEMSTAT_LABEL(a)           CryGetIMemReplay()->AddLabel(a)
	#define MEMSTAT_LABEL_FMT(a, ...)  CryGetIMemReplay()->AddLabelFmt(a, __VA_ARGS__)
	#define MEMSTAT_LABEL_SCOPED(name) CMemStatScopedLabel MEMSTAT_CONCAT(_memctxlabel, __LINE__) (name);

#else

	#define MEMSTAT_LABEL(a)
	#define MEMSTAT_LABEL_FMT(a, ...)
	#define MEMSTAT_LABEL_SCOPED(...)

#endif

#if CAPTURE_REPLAY_LOG

class CMemReplayScope
{
public:
	CMemReplayScope(EMemReplayAllocClass::Class cls, uint16 subCls, int moduleId)
		: m_needsExit(CryGetIMemReplay()->EnterScope(cls, subCls, moduleId))
	{
	}

	~CMemReplayScope()
	{
		if (m_needsExit)
			CryGetIMemReplay()->ExitScope();
	}

	void Alloc(UINT_PTR id, size_t sz, size_t alignment)
	{
		if (m_needsExit)
		{
			m_needsExit = false;
			CryGetIMemReplay()->ExitScope_Alloc(id, sz, alignment);
		}
	}

	void Realloc(UINT_PTR origId, UINT_PTR newId, size_t newSz, size_t newAlign)
	{
		if (m_needsExit)
		{
			m_needsExit = false;
			CryGetIMemReplay()->ExitScope_Realloc(origId, newId, newSz, newAlign);
		}
	}

	void Free(UINT_PTR id)
	{
		if (m_needsExit)
		{
			m_needsExit = false;
			CryGetIMemReplay()->ExitScope_Free(id);
		}
	}
private:
	CMemReplayScope(const CMemReplayScope&);
	CMemReplayScope& operator=(const CMemReplayScope&);

private:
	bool m_needsExit;
};

class CMemReplayLockScope
{
public:
	CMemReplayLockScope()
	{
		m_bNeedsExit = CryGetIMemReplay()->EnterLockScope();
	}

	~CMemReplayLockScope()
	{
		if (m_bNeedsExit)
			CryGetIMemReplay()->LeaveLockScope();
	}

private:
	CMemReplayLockScope(const CMemReplayLockScope&);
	CMemReplayLockScope& operator=(const CMemReplayLockScope&);

private:
	bool m_bNeedsExit;
};

	#ifdef eCryModule
		#define MEMREPLAY_SCOPE(cls, subCls)               CMemReplayScope _mrCls((cls), (subCls), eCryModule)
	#else
		#define MEMREPLAY_SCOPE(cls, subCls)               CMemReplayScope _mrCls((cls), (subCls), eCryM_Launcher)
	#endif
	#define MEMREPLAY_SCOPE_ALLOC(id, sz, align)         _mrCls.Alloc((UINT_PTR)(id), (sz), (align))
	#define MEMREPLAY_SCOPE_REALLOC(oid, nid, sz, align) _mrCls.Realloc((UINT_PTR)(oid), (UINT_PTR)nid, (sz), (align))
	#define MEMREPLAY_SCOPE_FREE(id)                     _mrCls.Free((UINT_PTR)(id))

	#define MEMREPLAY_LOCK_SCOPE()                       CMemReplayLockScope _mrCls

#else

	#define MEMREPLAY_SCOPE(cls, subCls)
	#define MEMREPLAY_SCOPE_ALLOC(id, sz, align)
	#define MEMREPLAY_SCOPE_REALLOC(oid, nid, sz, align)
	#define MEMREPLAY_SCOPE_FREE(id)

	#define MEMREPLAY_LOCK_SCOPE()

#endif

#endif  //__CryMemReplay_h__
