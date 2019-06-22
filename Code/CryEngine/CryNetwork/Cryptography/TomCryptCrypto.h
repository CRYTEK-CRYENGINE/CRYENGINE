// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// This file provides support for encryption with LibTomCrypt library.

#if CRYNETWORK_USE_TOMCRYPT

//////////////////////////////////////////////////////////////////////////
// LibTomCrypt integration support
//////////////////////////////////////////////////////////////////////////

//! Initializes global TomCrypt state. Must be called before any other ciphers can be used.
void InitTomCrypt();

//! Shuts down global TomCrypt state.
void ShutdownTomCrypt();


//////////////////////////////////////////////////////////////////////////
// AES support
//////////////////////////////////////////////////////////////////////////

//! Helper to manage TomCrypt AES CBC state
class CTomCryptAesState : public _reference_target_no_vtable<CTomCryptAesState>
{
public:
	using TSharedPtr = _smart_ptr<CTomCryptAesState>;

	static TSharedPtr CreateAesStateFromSecret(const void* pSecret, const size_t secretSize);
	static void operator delete(void* ptr);

	//! Returns symmetric_CBC*
	// NOTE: unable to forward-declare symmetric_CBC, as it's just a C-style struct typedef.
	void* Handle();

	~CTomCryptAesState();

private:
	CTomCryptAesState();
};

using CCTomCryptAesStateSharedPtr = CTomCryptAesState::TSharedPtr;

//! AES initialization vector
using TTomCryptAesInitVec = std::array<uint8, 16>;

//! Initialization state for TomCrypt AES cipher
struct STomCryptAesInitState
{
	CCTomCryptAesStateSharedPtr pState;
	TTomCryptAesInitVec initVec;
};

class CTomCryptAesCipher
{
public:
	void Init(const STomCryptAesInitState& initState);

	bool EncryptInPlace(uint8* pBuf, const size_t bufSize);
	bool DecryptInPlace(uint8* pBuf, const size_t bufSize);

	static int GetBlockSize() { return 16; }

private:
	CCTomCryptAesStateSharedPtr m_pState;
	TTomCryptAesInitVec m_initVec;
};

//////////////////////////////////////////////////////////////////////////
// SHA256-HMAC support
//////////////////////////////////////////////////////////////////////////

class CTomCryptSha256Hmac : public _reference_target_no_vtable<CTomCryptSha256Hmac>
{
public:
	enum { HashSize = 32 };
	using HashResult = std::array<uint8, HashSize>;

	using TSharedPtr = _smart_ptr<CTomCryptSha256Hmac>;
	static TSharedPtr CreateFromSecret(const void* pSecret, const size_t secretSize);
	static void operator delete(void* ptr);

	bool Hash(const uint8* pBuf, const size_t bufSize);
	bool FinishAndRestartHash(HashResult& result);

private:
	explicit CTomCryptSha256Hmac(int shaIdx, size_t secretSize);
	bool FinishHash(HashResult& result);

private:
	int m_shaIdx;
	size_t m_secretSize;
};

using CTomCryptSha256HmacSharedPtr = CTomCryptSha256Hmac::TSharedPtr;

#endif // CRYNETWORK_USE_TOMCRYPT

