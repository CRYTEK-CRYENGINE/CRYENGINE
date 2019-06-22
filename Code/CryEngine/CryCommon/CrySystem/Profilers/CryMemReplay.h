// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <CryCore/AlignmentTools.h>

enum class EMemReplayAllocClass : uint16
{
	UserPointer = 0,
	D3DDefault,
	D3DManaged,
};

enum class EMemReplayUserPointerClass : uint16
{
	Unknown = 0,
	CrtNew,
	CrtNewArray,
	CryNew,
	CryNewArray,
	CrtMalloc,
	CryMalloc,
	STL,
	VirtualAlloc,
	HeapAlloc,
};

// Add new types at the end, do not modify existing values.
enum class EMemStatContextType : uint32
{
	MAX = 0,
	CGF = 1,
	MTL = 2,
	DBA = 3,
	CHR = 4,
	LMG = 5,
	AG = 6,
	Texture = 7,
	ParticleLibrary = 8,

	Physics = 9,
	Terrain = 10,
	Shader = 11,
	Other = 12,
	RenderMesh = 13,
	Entity = 14,
	Navigation = 15,
	ScriptCall = 16,

	CDF = 17,

	RenderMeshType = 18,

	ANM = 19,
	CGA = 20,
	CAF = 21,
	ArchetypeLib = 22,

	AudioSystem = 23,
	EntityArchetype = 24,

	LUA = 25,
	D3D = 26,
	ParticleEffect = 27,
	SoundBuffer = 28,
	AudioImpl = 29,

	AIObjects = 30,
	Animation = 31,
	Debug = 32,

	FSQ = 33,
	Mannequin = 34,

	GeomCache = 35
};

enum class EMemStatContainerType
{
	Vector,
	Tree,
};

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
	virtual bool EnterScope(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId) = 0;

	//! Records an event against the currently active scope and exits it.
	virtual void ExitScope_Alloc(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId, UINT_PTR id, UINT_PTR sz, UINT_PTR alignment = 0) = 0;
	virtual void ExitScope_Realloc(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId, UINT_PTR originalId, UINT_PTR newId, UINT_PTR sz, UINT_PTR alignment = 0) = 0;
	virtual void ExitScope_Free(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId, UINT_PTR id) = 0;
	virtual void ExitScope() = 0;

	virtual void AllocUsage(EMemReplayAllocClass allocClass, UINT_PTR id, UINT_PTR used) = 0;

	virtual void AddAllocReference(void* ptr, void* ref) = 0;
	virtual void RemoveAllocReference(void* ref) = 0;

	virtual void AddLabel(const char* label) = 0;
	virtual void AddLabelFmt(const char* labelFmt, ...) = 0;
	virtual void AddFrameStart() = 0;
	virtual void AddScreenshot() = 0;

	typedef uint32 FixedContextID;
	virtual FixedContextID AddFixedContext(EMemStatContextType type, const char* str) = 0;
	virtual void PushFixedContext(FixedContextID id) = 0;

	virtual void PushContext(EMemStatContextType type, const char* str) = 0;
	virtual void PushContextV(EMemStatContextType type, const char* format, va_list args) = 0;
	virtual void PopContext() = 0;

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
struct CDummyMemReplay final : IMemReplay
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
	bool EnterScope(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId) { return false; }

	//! Records an event against the currently active scope and exits it.
	void ExitScope_Alloc(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId, UINT_PTR id, UINT_PTR sz, UINT_PTR alignment = 0)                                {}
	void ExitScope_Realloc(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId, UINT_PTR originalId, UINT_PTR newId, UINT_PTR sz, UINT_PTR alignment = 0)      {}
	void ExitScope_Free(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId, UINT_PTR id)                                                                      {}
	void ExitScope()                                                                                      {}

	void AllocUsage(EMemReplayAllocClass allocClass, UINT_PTR id, UINT_PTR used)                          {}

	void AddAllocReference(void* ptr, void* ref)                                                          {}
	void RemoveAllocReference(void* ref)                                                                  {}

	void AddLabel(const char* label)                                                                      {}
	void AddLabelFmt(const char* labelFmt, ...)                                                           {}
	void AddFrameStart()                                                                                  {}
	void AddScreenshot()                                                                                  {}

	typedef uint32 FixedContextID;
	FixedContextID AddFixedContext(EMemStatContextType type, const char* str)                            { return 0; }
	void PushFixedContext(FixedContextID id)                                                              {}

	void PushContext(EMemStatContextType type, const char* str)                                          {}
	void PushContextV(EMemStatContextType type, const char* format, va_list args)                        {}
	void PopContext()                                                                                     {}

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
static IMemReplay* s_pMemReplay = nullptr;
static SUninitialized<CDummyMemReplay> s_dummyMemReplay;
inline IMemReplay* CryGetIMemReplay()
{
	if(!s_pMemReplay)
	{
		IMemoryManager* pMemMan = CryGetIMemoryManager();
		if (pMemMan)
			s_pMemReplay = pMemMan->GetIMemReplay();
		if (!s_pMemReplay)
		{
			s_dummyMemReplay.DefaultConstruct();
			return & s_dummyMemReplay.operator CDummyMemReplay&();
		}
	}
	return s_pMemReplay;
}
#else
inline IMemReplay* CryGetIMemReplay()
{
	return nullptr;
}
#endif

#if defined(__cplusplus) && CAPTURE_REPLAY_LOG
class CMemStatStaticContext
{
public:
	CMemStatStaticContext(EMemStatContextType type, const char* str)
	{
		m_contextId = CryGetIMemReplay()->AddFixedContext(type, str);
	}

	IMemReplay::FixedContextID GetId(EMemStatContextType type, const char* name)
	{
		if (m_contextId == s_invalidContextId)
			m_contextId = CryGetIMemReplay()->AddFixedContext(type, name);
		return m_contextId;
	}

	static constexpr IMemReplay::FixedContextID s_invalidContextId = ~0;

private:
	CMemStatStaticContext(const CMemStatStaticContext&);
	CMemStatStaticContext& operator=(const CMemStatStaticContext&);

	IMemReplay::FixedContextID m_contextId;
};

class CMemStatStaticContextInstance
{
public:
	CMemStatStaticContextInstance(CMemStatStaticContext& context, EMemStatContextType type, const char* name) //extra params just for interface compatibility
	{
		const IMemReplay::FixedContextID fixedId = context.GetId(type, name);
		if (fixedId != CMemStatStaticContext::s_invalidContextId)
			CryGetIMemReplay()->PushFixedContext(fixedId);
		wasPushed = (fixedId != CMemStatStaticContext::s_invalidContextId);
	}
	~CMemStatStaticContextInstance()
	{
		if (wasPushed)
			CryGetIMemReplay()->PopContext();
	}
private:
	CMemStatStaticContextInstance(const CMemStatStaticContextInstance&);
	CMemStatStaticContextInstance& operator=(const CMemStatStaticContextInstance&);

	bool wasPushed;
};

// no-op for compatibility with static context
class CMemStatContext
{
public:
	CMemStatContext(EMemStatContextType type, const char* str) {}
private:
	CMemStatContext(const CMemStatContext&);
	CMemStatContext& operator=(const CMemStatContext&);
};

class CMemStatContextInstance
{
public:
	CMemStatContextInstance(CMemStatContext&, EMemStatContextType type, const char* str) //first param just for interface compatibility
	{
		CryGetIMemReplay()->PushContext(type, str);
	}
	~CMemStatContextInstance()
	{
		CryGetIMemReplay()->PopContext();
	}
private:
	CMemStatContextInstance(const CMemStatContextInstance&);
	CMemStatContextInstance& operator=(const CMemStatContextInstance&);
};

class CMemStatContextFormat
{
public:
	CMemStatContextFormat(EMemStatContextType type, const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		CryGetIMemReplay()->PushContextV(type, format, args);
		va_end(args);
	}
	~CMemStatContextFormat()
	{
		CryGetIMemReplay()->PopContext();
	}
private:
	CMemStatContextFormat(const CMemStatContextFormat&);
	CMemStatContextFormat& operator=(const CMemStatContextFormat&);
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
	//! name can be a string literal or some const char* variable
	#define MEMSTAT_CONTEXT(ctxType, ctxName) \
		static std::conditional<CRY_IS_STRING_LITERAL(ctxName), CMemStatStaticContext, CMemStatContext>::type \
				MEMSTAT_CONCAT(_memctx, __LINE__) (ctxType, ctxName); \
		std::conditional<CRY_IS_STRING_LITERAL(ctxName), CMemStatStaticContextInstance, CMemStatContextInstance>::type \
				MEMSTAT_CONCAT(_memctxInst, __LINE__) (MEMSTAT_CONCAT(_memctx, __LINE__), ctxType, ctxName);

	//! context with the name of the current function
	#define MEMSTAT_FUNCTION_CONTEXT(ctxType) MEMSTAT_CONTEXT(ctxType, __FUNC__);

	//! context with a formatted name
	#define MEMSTAT_CONTEXT_FMT(ctxType, ctxFormat, ...) CMemStatContextFormat MEMSTAT_CONCAT(_memctx, __LINE__) (ctxType, ctxFormat, __VA_ARGS__);
#else
	#define MEMSTAT_CONTEXT(...)
	#define MEMSTAT_FUNCTION_CONTEXT(...)
	#define MEMSTAT_CONTEXT_FMT(...)
#endif

#if INCLUDE_MEMSTAT_CONTAINERS
template<typename T>
static void MemReplayRegisterContainerStub(const void* key, EMemStatContainerType type)
{
	CryGetIMemReplay()->RegisterContainer(key, int(type));
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
	#define MEMSTAT_USAGE(ptr, size) CryGetIMemReplay()->AllocUsage(EMemReplayAllocClass::UserPointer, (UINT_PTR)ptr, size)
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
	CMemReplayScope(EMemReplayAllocClass cls, EMemReplayUserPointerClass subCls, int moduleId)
		: m_needsExit(CryGetIMemReplay()->EnterScope(cls, subCls, moduleId)), cls(cls), subCls(subCls), moduleId(moduleId)
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
			CryGetIMemReplay()->ExitScope_Alloc(cls, subCls, moduleId, id, sz, alignment);
		}
	}

	void Realloc(UINT_PTR origId, UINT_PTR newId, size_t newSz, size_t newAlign)
	{
		if (m_needsExit)
		{
			m_needsExit = false;
			CryGetIMemReplay()->ExitScope_Realloc(cls, subCls, moduleId, origId, newId, newSz, newAlign);
		}
	}

	void Free(UINT_PTR id)
	{
		if (m_needsExit)
		{
			m_needsExit = false;
			CryGetIMemReplay()->ExitScope_Free(cls, subCls, moduleId, id);
		}
	}
private:
	CMemReplayScope(const CMemReplayScope&);
	CMemReplayScope& operator=(const CMemReplayScope&);

private:
	bool m_needsExit;
	EMemReplayAllocClass cls;
	EMemReplayUserPointerClass subCls;
	int moduleId;
};
#endif

#if CAPTURE_REPLAY_LOG
	#ifdef eCryModule
		#define MEMREPLAY_SCOPE(cls, subCls)             CMemReplayScope _mrCls((cls), (subCls), eCryModule)
	#else
		#define MEMREPLAY_SCOPE(cls, subCls)             CMemReplayScope _mrCls((cls), (subCls), eCryM_Launcher)
	#endif
	#define MEMREPLAY_SCOPE_ALLOC(id, sz, align)         _mrCls.Alloc((UINT_PTR)(id), (sz), (align))
	#define MEMREPLAY_SCOPE_REALLOC(oid, nid, sz, align) _mrCls.Realloc((UINT_PTR)(oid), (UINT_PTR)(nid), (sz), (align))
	#define MEMREPLAY_SCOPE_FREE(id)                     _mrCls.Free((UINT_PTR)(id))
#else
	#define MEMREPLAY_SCOPE(cls, subCls)
	#define MEMREPLAY_SCOPE_ALLOC(id, sz, align)
	#define MEMREPLAY_SCOPE_REALLOC(oid, nid, sz, align)
	#define MEMREPLAY_SCOPE_FREE(id)
#endif
