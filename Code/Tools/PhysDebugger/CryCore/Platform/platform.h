#pragma once

#define STANDALONE_PHYSICS
#define NOT_USE_CRY_MEMORY_MANAGER

#include "CryCore/Platform/CryPlatformDefines.h"
#include <CryCore/Platform/Win64Specific.h>
#include <CryCore/Platform/CryWindows.h>

#pragma warning (disable : 4390 4996)
#define ILINE inline
#define assert(X)
#define CRY_ASSERT_MESSAGE(X,M)
#define CRY_ASSERT_H_INCLUDED
#define AUTO_STRUCT_INFO
#define AUTO_STRUCT_INFO_LOCAL
#define AUTO_TYPE_INFO(T)
#define BIT(x) (1u << (x))
typedef UINT_PTR TRUNCATE_PTR,EXPAND_PTR;
#define DLL_EXPORT __declspec(dllexport)
#define DLL_IMPORT 
#define CRY_ARRAY_COUNT(A) (sizeof(A)/sizeof((A)[0]))
template<class T>	inline void ZeroArray(T& t)	{	memset(&t, 0, sizeof(t[0]) * CRY_ARRAY_COUNT(t));	}
template<class T> inline void ZeroStruct(T& t) { memset(&t, 0, sizeof(T)); }
template<class T> struct Array {
	struct iter { 
		iter(T& val) : val(val) {}
		T& val;
		bool operator!=(const iter&) { return false; }
		iter& operator++() { return *this; }
		T& operator*() { return val; }
	};
	T val;
	Array() {}
	iter begin() { return iter(val); }
	iter end() { return iter(val); }
	void fill(const T& f) { val=f; }
};
#define ColorF Vec4

#ifdef _DEBUG
	#define IF_DEBUG(expr) (expr)
#else
	#define IF_DEBUG(expr)
#endif
#define COMPOUND_MEMBER_OP(T, op, B)                                     \
  ILINE T& operator op ## = (const B& b) { return *this = *this op b; }  \
  ILINE T operator op(const B& b) const                                  
#define COMPOUND_MEMBER_OP_EQ(T, op, B)                                      \
  ILINE T operator op(const B& b) const { T t = *this; return t op ## = b; } \
  ILINE T& operator op ## = (const B& b)                                     
struct CTypeInfo {};
template<class T> inline CTypeInfo TypeInfo(const T&) { return CTypeInfo(); }

typedef const char* cstr;
#define NOT_USE_CRY_STRING
#include <string>
#include <functional>
typedef std::string string;
#define cry_sprintf sprintf_s
inline void CryFatalError(const char*,...) {}
inline void CryLog(const char*,...) {}
inline void CryLogAlways(const char*,...) {}

template<typename DstType, typename SrcType> inline DstType alias_cast(SrcType pPtr) { return *(DstType*)&pPtr; }
template<class T>	ILINE T& non_const(const T& t) { return const_cast<T&>(t); }

#include <CryThreading/CryAtomics.h>
extern "C" {
	__declspec(dllimport) unsigned long __stdcall TlsAlloc();
	__declspec(dllimport) void* __stdcall TlsGetValue(unsigned long dwTlsIndex);
	__declspec(dllimport) int __stdcall TlsSetValue(unsigned long dwTlsIndex, void* lpTlsValue);
}
#define TLS_DECLARE(type,var) extern int var##idx;
#define TLS_DEFINE(type,var) \
int var##idx; \
struct Init##var { \
	Init##var() { var##idx = TlsAlloc(); } \
}; \
Init##var g_init##var;
#define TLS_GET(type,var) (type)TlsGetValue(var##idx)
#define TLS_SET(var,val) TlsSetValue(var##idx,(void*)(val))
inline threadID CryGetCurrentThreadId() { return GetCurrentThreadId();}
inline uint64 CryGetTicks() { return __rdtsc(); }

struct IGeneralMemoryHeap {
	static char* Malloc(size_t sz, const char*) { return new char[sz]; }
	static void Free(void* ptr) { delete (char*)ptr; }
};
inline void* CryModuleMalloc(size_t sz) { return malloc(sz); }
inline void CryModuleFree(void* p) { free(p); }

extern uint cry_srand(uint seed);
extern float cry_random(float start, float end);
extern uint cry_random_uint32();
struct SScopedRandomSeedChange {
	uint seedPrev;
	SScopedRandomSeedChange() {}
	SScopedRandomSeedChange(uint seed) { Seed(seed); }
	void Seed(uint seed) { seedPrev = cry_srand(seed); }
	~SScopedRandomSeedChange() { cry_srand(seedPrev); }
};

inline void CrySleep(uint msec) { Sleep(msec); }
inline void *GetISystem() { return nullptr; }

#define DONT_USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION

struct SEnv {
	struct IThreadManager *pThreadManager;
	struct IPhysicalWorld *pPhysicalWorld;
	struct ISystem *pSystem,*pTimer,*p3DEngine;
	int bMultiplayer = 0;
};
extern SEnv *gEnv;

extern int g_iLastProfilerId;
struct CFrameProfiler {
	CFrameProfiler(char *name) {
		m_name = name;
		m_id = ++g_iLastProfilerId;
	}
	char *m_name;
	int m_id;
};

struct CFrameProfilerTimeSample {
	int iTotalTime;
	int iSelfTime;
	int iCount;
	CFrameProfiler *pProfiler;
	unsigned __int64 iCode;
	int iChild,iNext;

	void Reset() {
		iChild = iNext = -1;
		iTotalTime = iSelfTime = iCount = 0;
	}
};

struct CFrameProfilerSectionBase {
	__int64 m_iStartTime;
	unsigned __int64 m_iCurCode;
	int m_iCurSlot;
	int m_iCaller;
};

struct ProfilerData {
	int iLevel,iLastSampleSlot,iLastTimeSample;
	CFrameProfilerSectionBase sec0;
	CFrameProfilerSectionBase *pCurSection[16];
	CFrameProfilerTimeSample TimeSamples[256];
};
extern ProfilerData g_pd[];

struct CFrameProfilerSection : CFrameProfilerSectionBase {
	CFrameProfilerSection(CFrameProfiler *pProfiler, int iCaller) {
		ProfilerData &pd = g_pd[m_iCaller=iCaller];
		m_iCurCode = pd.pCurSection[pd.iLevel]->m_iCurCode | (unsigned __int64)pProfiler->m_id<<(7-pd.iLevel)*8;
		if (pd.TimeSamples[pd.iLastSampleSlot].iCode == m_iCurCode)
			m_iCurSlot = pd.iLastSampleSlot;
		else {
			if ((m_iCurSlot = pd.TimeSamples[pd.pCurSection[pd.iLevel]->m_iCurSlot].iChild) < 0) {
				pd.TimeSamples[pd.pCurSection[pd.iLevel]->m_iCurSlot].iChild = m_iCurSlot = ++pd.iLastTimeSample;
				pd.TimeSamples[m_iCurSlot].Reset();
			} else {
				for(; pd.TimeSamples[m_iCurSlot].iNext>=0 && pd.TimeSamples[m_iCurSlot].iCode!=m_iCurCode; m_iCurSlot=pd.TimeSamples[m_iCurSlot].iNext);
				if (pd.TimeSamples[m_iCurSlot].iCode!=m_iCurCode) {
					m_iCurSlot = (pd.TimeSamples[m_iCurSlot].iNext = ++pd.iLastTimeSample);
					pd.TimeSamples[m_iCurSlot].Reset();
				}
			}
			pd.TimeSamples[pd.iLastSampleSlot = m_iCurSlot].iCode = m_iCurCode;
		}
		pd.TimeSamples[m_iCurSlot].pProfiler = pProfiler;
		m_iStartTime = CryGetTicks();
		pd.TimeSamples[m_iCurSlot].iCount++;
		pd.pCurSection[++pd.iLevel] = this;
	}

	~CFrameProfilerSection() 
	{
		int iTime = (int)(CryGetTicks()-m_iStartTime);
		ProfilerData &pd = g_pd[m_iCaller];
		pd.TimeSamples[m_iCurSlot].iSelfTime += iTime;
		pd.TimeSamples[m_iCurSlot].iTotalTime += iTime;
		pd.TimeSamples[pd.pCurSection[--pd.iLevel]->m_iCurSlot].iSelfTime -= iTime;
	}
};

#define PROFILE_PHYSICS "Physics"
#define CRY_PROFILE_FUNCTION( subsystem ) \
	static CFrameProfiler staticFrameProfiler( __FUNCTION__ ); \
	CFrameProfilerSection frameProfilerSection( &staticFrameProfiler,get_iCaller() );	
#define CRY_PROFILE_REGION( subsystem, szName ) \
	static CFrameProfiler staticFrameProfiler( szName ); \
	CFrameProfilerSection frameProfilerSection( &staticFrameProfiler,get_iCaller() );

