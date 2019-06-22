// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// This file provides support for encryption with Microsoft CNG library
// See https://docs.microsoft.com/en-us/windows/desktop/seccng/cng-portal

#if CRYNETWORK_USE_CNG

//////////////////////////////////////////////////////////////////////////
// CNG integration support
//////////////////////////////////////////////////////////////////////////

//! Initializes global CNG state. Must be called before any other ciphers can be used.
void InitCng();

//! Shuts down global CNG state.
void ShutdownCng();

//! Helper to manage unique ownership of CNG handles
template <typename D>
using SCngHandle = std::unique_ptr<void, D>;

//////////////////////////////////////////////////////////////////////////
// AES support
//////////////////////////////////////////////////////////////////////////

struct SCngKeyHandleDeleter
{
	void operator()(void* pHandle);
};

using SCngKeyHandle = SCngHandle<SCngKeyHandleDeleter>;

//! Helper to manage CNG key handle and key object memory
class CCngKey : public _reference_target_no_vtable<CCngKey>
{
public:
	using TSharedPtr = _smart_ptr<CCngKey>;

	static TSharedPtr CreateAesKeyFromSecret(const void* pSecret, const size_t secretSize);
	static void operator delete(void* ptr);
#if CRY_PLATFORM_DURANGO && (_MSC_VER <= 1900)
	static void operator delete(void* ptr, void*) noexcept;
#endif
	void* Handle() { return m_handle.get(); }
	
private:
	CCngKey(void* pHandle);

private:
	SCngKeyHandle m_handle;
};

using CCngKeySharedPtr = CCngKey::TSharedPtr;

//! AES initialization vector
using TCngAesInitVec = std::array<uint8, 16>;

//! Initialization state for CNG AES cipher
struct SCngAesInitState
{
	CCngKeySharedPtr key;
	TCngAesInitVec initVec;
};

//! AES CBC cipher
class CCngAesCipher
{
public:
	void Init(const SCngAesInitState& initState);

	bool EncryptInPlace(uint8* pBuf, const size_t bufSize);
	bool DecryptInPlace(uint8* pBuf, const size_t bufSize);

	static int GetBlockSize() { return 16; }

private:
	CCngKeySharedPtr m_key;
	TCngAesInitVec m_initVec;
};


//////////////////////////////////////////////////////////////////////////
// SHA256-HMAC support
//////////////////////////////////////////////////////////////////////////

struct SCngHashHandleDeleter
{
	void operator()(void* pHandle);
};

using SCngHashHandle = SCngHandle<SCngHashHandleDeleter>;

//! SHA256-HMAC object, handles current hashing state.
class CCngSha256Hmac : public _reference_target_no_vtable<CCngSha256Hmac>
{
public:
	enum { HashSize = 32 };
	using HashResult = std::array<uint8, HashSize>;

	using TSharedPtr = _smart_ptr<CCngSha256Hmac>;
	static TSharedPtr CreateFromSecret(const void* pSecret, const size_t secretSize);
	static void operator delete(void* ptr);
#if CRY_PLATFORM_DURANGO && (_MSC_VER <= 1900)
	static void operator delete(void* ptr, void*) noexcept;
#endif
	bool Hash(const uint8* pBuf, const size_t bufSize);
	bool FinishAndRestartHash(HashResult& result);

private:
	explicit CCngSha256Hmac(SCngHashHandle&& handle, bool isReusable, size_t secretSize);
	bool FinishHash(HashResult& result);

private:
	SCngHashHandle m_handle;
	bool m_isReusable;
	size_t m_secretSize;
};

using CCngSha256HmacSharedPtr = CCngSha256Hmac::TSharedPtr;

#endif // CRYNETWORK_USE_CNG
