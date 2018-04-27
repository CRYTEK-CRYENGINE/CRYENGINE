// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GLPlatform.hpp
//  Version:     v1.00
//  Created:     27/03/2014 by Valerio Guagliumi.
//  Description: Platform specific DXGL requirements implementation
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __GLPLATFORM__
#define __GLPLATFORM__

namespace NCryOpenGL
{

enum ELogSeverity
{
	eLS_Info,
	eLS_Warning,
	eLS_Error,
	eLS_Fatal
};

}

#define CUSTOM_SCOPED_PROFILE(_Name)

#if 1 // CRY_RENDERER_OPENGL
	#include "GLCryPlatform.hpp"
#elif CRY_PLATFORM_WINDOWS
	#include "../Standalone/GLWinPlatform.hpp"
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	#include "../Standalone/GLLinuxPlatform.hpp"
#else
	#error "Platform is not supported"
#endif

#define DXGL_PROFILING 0

#if defined(_MSC_VER)
	#define _DXGL_LOG_STRINGIFY_(x)                   # x
	#define _DXGL_LOG_STRINGIFY(x)                    _DXGL_LOG_STRINGIFY_(x)
	#define _DXGL_LOG_MSG(_TEXT, _FILE, _LINE, _FUNC) "DXGL: " _TEXT " : [@" _FUNC "] " _FILE "(" _LINE ")"
	#define DXGL_LOG_MSG(_TEXT)                       _DXGL_LOG_MSG(_TEXT, __FILE__, _DXGL_LOG_STRINGIFY(__LINE__), __FUNCSIG__)
#else
	#define _DXGL_LOG_MSG(_TEXT, _FILE, _FUNC)        "DXGL: " _TEXT " : [@" _FUNC "] " _FILE "(%d)"
	#define DXGL_LOG_MSG(_TEXT)                       _DXGL_LOG_MSG(_TEXT, __FILE__, "?") // GCC can't resolve __FUNCTION__ in  the preprocessor
#endif

#if defined(_MSC_VER)
	#define DXGL_WARNING(_Format, ...) NCryOpenGL::LogMessage(NCryOpenGL::eLS_Warning, DXGL_LOG_MSG(_Format), __VA_ARGS__)
	#define DXGL_ERROR(_Format, ...)   NCryOpenGL::LogMessage(NCryOpenGL::eLS_Error, DXGL_LOG_MSG(_Format), __VA_ARGS__)
	#define DXGL_INFO(_Format, ...)    NCryOpenGL::LogMessage(NCryOpenGL::eLS_Info, DXGL_LOG_MSG(_Format), __VA_ARGS__)
	#define DXGL_FATAL(_Format, ...)   NCryOpenGL::LogMessage(NCryOpenGL::eLS_Fatal, DXGL_LOG_MSG(_Format), __VA_ARGS__)
#else
	#define DXGL_WARNING(_Format, ...) NCryOpenGL::LogMessage(NCryOpenGL::eLS_Warning, DXGL_LOG_MSG(_Format), ## __VA_ARGS__, __LINE__)
	#define DXGL_ERROR(_Format, ...)   NCryOpenGL::LogMessage(NCryOpenGL::eLS_Error, DXGL_LOG_MSG(_Format), ## __VA_ARGS__, __LINE__)
	#define DXGL_INFO(_Format, ...)    NCryOpenGL::LogMessage(NCryOpenGL::eLS_Info, DXGL_LOG_MSG(_Format), ## __VA_ARGS__, __LINE__)
	#define DXGL_FATAL(_Format, ...)   NCryOpenGL::LogMessage(NCryOpenGL::eLS_Fatal, DXGL_LOG_MSG(_Format), ## __VA_ARGS__, __LINE__)
#endif

#define DXGL_NOT_IMPLEMENTED NCryOpenGL::BreakUnique(__FILE__, __LINE__);
#define DXGL_TODO(_DESC)

#define DXGL_ARRAY_SIZE(_Array) (CRY_ARRAY_COUNT(_Array))
#define _DXGL_QUOTE(X)          # X
#define DXGL_QUOTE(X)           _DXGL_QUOTE(X)
#define DXGL_CAT(_Left, _Right) _Left ## _Right
#define _DXGL_IF_0(...)
#define _DXGL_IF_1(...)         __VA_ARGS__
#define DXGL_IF(_Condition)     DXGL_CAT(_DXGL_IF_, _Condition)

#define DXGL_DECLARE_PTR(_Qualifier, _TypeName) \
  _Qualifier _TypeName;                         \
  typedef _smart_ptr<_TypeName> _TypeName ## Ptr;
#define DXGL_DECLARE_REF_COUNTED(_Qualifier, _TypeName) \
  DXGL_DECLARE_PTR(_Qualifier, _TypeName)               \
  _Qualifier _TypeName: public NCryOpenGL::SRefCounted < _TypeName >

namespace NCryOpenGL
{

template<typename T>
struct SRefCounted
{
	uint32 m_uRefCnt;

	SRefCounted()
		: m_uRefCnt(0)
	{
	}

	void AddRef()
	{
		++m_uRefCnt;
	}

	void Release()
	{
		if (--m_uRefCnt == 0)
			delete static_cast<T*>(this);
	}
};

struct SSpinlockBitMaskWord
{
	typedef LONG Word;
	enum {BIT_SIZE = sizeof(Word) * 8};
	volatile Word m_aMask;

	void          Set(Word kNewValue)
	{
		Exchange(&m_aMask, kNewValue);
	}

	Word Get() const
	{
		return CompareExchange(const_cast<Word*>(&m_aMask), 0, 0);
	}

	bool Set(uint32 uIndex, bool bFlag)
	{
		while (true)
		{
			Word kOld(m_aMask);
			Word uFlagMask(1 << uIndex);
			Word kNew(bFlag ? (kOld | uFlagMask) : (kOld & ~uFlagMask));
			if (CompareExchange(&m_aMask, kNew, kOld) == kOld)
				return (kOld & uFlagMask) != 0;
		}
	}

	bool Get(uint32 uIndex) const
	{
		assert(uIndex <= BIT_SIZE);
		return ((m_aMask >> uIndex) & 1) != 0;
	}
};

struct SUnsafeBitMaskWord
{
	typedef uint32 Word;
	enum {BIT_SIZE = sizeof(Word) * 8};
	Word m_aMask;

	void Set(Word kNewValue)
	{
		m_aMask = kNewValue;
	}

	Word Get() const
	{
		return m_aMask;
	}

	bool Set(uint32 uIndex, bool bFlag)
	{
		assert(uIndex <= BIT_SIZE);
		Word uFlagMask(1 << uIndex);
		Word kOld(m_aMask);
		m_aMask = bFlag ? (kOld | uFlagMask) : (kOld & ~uFlagMask);
		return (kOld & uFlagMask) != 0;
	}

	bool Get(uint32 uIndex) const
	{
		assert(uIndex <= BIT_SIZE);
		return ((m_aMask >> uIndex) & 1) != 0;
	}
};

template<uint32 uNumWords, typename BitMaskWord>
struct SBitMaskMultiWord
{
	typedef BitMaskWord Word;
	Word m_akWords[uNumWords];

	bool Set(uint32 uIndex, bool bFlag)
	{
		return m_akWords[uIndex / Word::BIT_SIZE].Set(uIndex % Word::BIT_SIZE, bFlag);
	}

	bool Get(uint32 uIndex) const
	{
		return m_akWords[uIndex / Word::BIT_SIZE].Get(uIndex % Word::BIT_SIZE);
	}
};

template<uint32 uNumWords, typename BitMaskWord>
struct SBitMaskImpl
{
	typedef SBitMaskMultiWord<uNumWords, BitMaskWord> Type;
	typedef typename BitMaskWord::Word                Word;

	static void Init(Type* pMask, bool bValue, Word kUsedMask)
	{
		memset(pMask->m_akWords, bValue ? (Word) ~0 : (Word)0, sizeof(Word) * (uNumWords - 1));
		pMask->m_akWords[uNumWords - 1].m_aMask = bValue ? kUsedMask : (Word)0;
	}

	static void SetZero(Type* pMask)
	{
		for (uint32 uPosition = 0; uPosition < uNumWords; ++uPosition)
			pMask->m_akWords[uPosition].Set((Word)0);
	}

	static void SetOne(Type* pMask, Word kUsedMask)
	{
		for (uint32 uPosition = 0; uPosition < uNumWords - 1; ++uPosition)
			pMask->m_akWords[uPosition].Set(~(Word)0);
		pMask->m_akWords[uNumWords - 1].Set(kUsedMask);
	}

	static bool IsZero(Type* pMask, Word kUsedMask)
	{
		for (uint32 uPosition = 0; uPosition < uNumWords - 1; ++uPosition)
			if (pMask->m_akWords[uPosition].Get() != (Word)0)
				return false;
		return (pMask->m_akWords[uNumWords - 1].Get() & kUsedMask) == (Word)0;
	}

	static bool IsOne(Type* pMask, Word kUsedMask)
	{
		for (uint32 uPosition = 0; uPosition < uNumWords - 1; ++uPosition)
			if (pMask->m_akWords[uPosition].Get() != ~(Word)0)
				return false;
		return (pMask->m_akWords[uNumWords - 1].Get() & kUsedMask) == kUsedMask;
	}

	static void BitwiseOr(BitMaskWord* pResult, const BitMaskWord& kLeft, const BitMaskWord& kRight)
	{
		for (uint32 uIndex = 0; uIndex < uNumWords; ++uIndex)
			pResult->m_akWords[uIndex].Set(kLeft.m_akWords[uIndex].Get() | kRight.m_akWords[uIndex].Get());
	}

	static void BitwiseAnd(BitMaskWord* pResult, const BitMaskWord& kLeft, const BitMaskWord& kRight)
	{
		for (uint32 uIndex = 0; uIndex < uNumWords; ++uIndex)
			pResult->m_akWords[uIndex].Set(kLeft.m_akWords[uIndex].Get() & kRight.m_akWords[uIndex].Get());
	}

	static bool Equals(const BitMaskWord& kLeft, const BitMaskWord& kRight)
	{
		for (uint32 uIndex = 0; uIndex < uNumWords; ++uIndex)
			if (kLeft.m_akWords[uIndex].Get() != kRight.m_akWords[uIndex].Get())
				return false;
		return true;
	}

	static void Copy(BitMaskWord* pDestination, const BitMaskWord& kSource)
	{
		for (uint32 uIndex = 0; uIndex < uNumWords; ++uIndex)
			pDestination->m_akWords[uIndex].Set(kSource.m_akWords[uIndex].Get());
	}
};

template<typename BitMaskWord>
struct SBitMaskImpl<1, BitMaskWord>
{
	typedef BitMaskWord                Type;
	typedef typename BitMaskWord::Word Word;

	static void Init(Type* pMask, bool bValue, Word kUsedMask)
	{
		pMask->m_aMask = bValue ? kUsedMask : (Word)0;
	}

	static void SetZero(Type* pMask)
	{
		pMask->Set((Word)0);
	}

	static void SetOne(Type* pMask, Word kUsedMask)
	{
		pMask->Set(kUsedMask);
	}

	static bool IsZero(const Type* pMask, Word kUsedMask)
	{
		return (pMask->Get() & kUsedMask) == (Word)0;
	}

	static bool IsOne(const Type* pMask, Word kUsedMask)
	{
		return (pMask->Get() & kUsedMask) == kUsedMask;
	}

	static void BitwiseOr(Type* pResult, const Type& kLeft, const Type& kRight)
	{
		pResult->Set(kLeft.Get() | kRight.Get());
	}

	static void BitwiseAnd(Type* pResult, const Type& kLeft, const Type& kRight)
	{
		pResult->Set(kLeft.Get() & kRight.Get());
	}

	static bool Equals(const Type& kLeft, const Type& kRight)
	{
		return kLeft.Get() == kRight.Get();
	}

	static void Copy(Type* pDestination, const Type& kSource)
	{
		pDestination->Set(kSource.Get());
	}
};

template<uint32 uSize, typename BitMaskWord>
struct SBitMask : SBitMaskImpl<(uSize + BitMaskWord::BIT_SIZE - 1) / BitMaskWord::BIT_SIZE, BitMaskWord>::Type
{
	typedef SBitMaskImpl<(uSize + BitMaskWord::BIT_SIZE - 1) / BitMaskWord::BIT_SIZE, BitMaskWord> Impl;
	typedef typename BitMaskWord::Word                                                             Word;
	enum { USED_MASK = ~(~(Word)0 << uSize % BitMaskWord::BIT_SIZE) };

	// The following methods are not thread safe regardless of the BitMaskWord type

	SBitMask(bool bInitValue = false)
	{
		Impl::Init(this, bInitValue, (Word)USED_MASK);
	}

	SBitMask(const SBitMask& kOther)
	{
		Impl::Copy(this, kOther);
	}

	void SetZero()
	{
		return Impl::SetZero(this);
	}

	void SetOne()
	{
		return Impl::SetOne(this, (Word)USED_MASK);
	}

	bool IsZero() const
	{
		return Impl::IsZero(this, (Word)USED_MASK);
	}

	bool IsOne() const
	{
		return Impl::IsOne(this, (Word)USED_MASK);
	}

	SBitMask& operator|=(const SBitMask& kOther)
	{
		Impl::BitwiseOr(this, *this, kOther);
		return *this;
	}

	SBitMask& operator&=(const SBitMask& kOther)
	{
		Impl::BitwiseAnd(this, *this, kOther);
		return *this;
	}

	SBitMask operator|(const SBitMask& kOther) const
	{
		SBitMask kResult;
		Impl::BitwiseOr(&kResult, *this, kOther);
		return kResult;
	}

	SBitMask operator&(const SBitMask& kOther) const
	{
		SBitMask kResult;
		Impl::BitwiseAnd(&kResult, *this, kOther);
		return kResult;
	}

	bool operator==(const SBitMask& kOther) const
	{
		return Impl::Equals(*this, kOther);
	}

	bool operator!=(const SBitMask& kOther) const
	{
		return !Impl::Equals(*this, kOther);
	}

	SBitMask& operator=(const SBitMask& kOther)
	{
		Impl::Copy(this, kOther);
		return *this;
	}
};

inline void* CreateTLS()
{
#if CRY_PLATFORM_WINDOWS
	return alias_cast<void*>(TlsAlloc());
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	pthread_key_t* pKey(new pthread_key_t());
	pthread_key_create(pKey, NULL);
	return alias_cast<void*>(pKey);
#else
	#error "Not implemented on this platform"
#endif
}

inline void DestroyTLS(void* pTLSHandle)
{
#if CRY_PLATFORM_WINDOWS
	TlsFree(alias_cast<DWORD>(pTLSHandle));
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	pthread_key_t* pKey(alias_cast<pthread_key_t*>(pTLSHandle));
	pthread_key_delete(*pKey);
#else
	#error "Not implemented on this platform"
#endif
}

inline void* GetTLSValue(void* pTLSHandle)
{
#if CRY_PLATFORM_WINDOWS
	return TlsGetValue(alias_cast<DWORD>(pTLSHandle));
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	return pthread_getspecific(*alias_cast<pthread_key_t*>(pTLSHandle));
#else
	#error "Not implemented on this platform"
#endif
}

inline void SetTLSValue(void* pTLSHandle, void* pValue)
{
#if CRY_PLATFORM_WINDOWS
	TlsSetValue(alias_cast<DWORD>(pTLSHandle), pValue);
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	pthread_setspecific(*alias_cast<pthread_key_t*>(pTLSHandle), pValue);
#else
	#error "Not implemented on this platform"
#endif
}

struct SList
{
	typedef SLockFreeSingleLinkedListHeader TListHeader;
	typedef SLockFreeSingleLinkedListEntry  TListEntry;

	SList()
	{
		CryInitializeSListHead(m_kHeader);
	}

	void Push(TListEntry& kEntry)
	{
		CryInterlockedPushEntrySList(m_kHeader, kEntry);
	}

	TListEntry* Pop()
	{
		return alias_cast<TListEntry*>(CryInterlockedPopEntrySList(m_kHeader));
	}

	TListHeader m_kHeader;
};

struct SScopedProfileLabel
{
	const char* m_szName;

	SScopedProfileLabel(const char* szName)
		: m_szName(szName)
	{
		PushProfileLabel(m_szName);
	}

	~SScopedProfileLabel()
	{
		PopProfileLabel(m_szName);
	}
};

}

#if DXGL_PROFILING
	#define DXGL_SCOPED_PROFILE(_Name) NCryOpenGL::SScopedProfileLabel kDXGLScopedProfileLabel(_Name);
#else
	#define DXGL_SCOPED_PROFILE(_Name)
#endif

#endif //__GLPLATFORM__
