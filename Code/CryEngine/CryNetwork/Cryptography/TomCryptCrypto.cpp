// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TomCryptCrypto.h"

#if CRYNETWORK_USE_TOMCRYPT

#include <tomcrypt.h>
#include <CryCore/Assert/CryAssert.h> // tomcrypt will re-define assert

//////////////////////////////////////////////////////////////////////////
// Global algorithms
//////////////////////////////////////////////////////////////////////////

void InitTomCrypt()
{
	int idx = register_cipher(&aes_desc);
	if (idx == -1)
	{
		CryFatalError("Failed to register TomCrypt AES algorithm");
	}

	idx = register_hash(&sha256_desc);
	if (idx == -1)
	{
		CryFatalError("Failed to register TomCrypt SHA256 algorithm");
	}
}

void ShutdownTomCrypt()
{
	int err = unregister_cipher(&aes_desc);
	if (err != CRYPT_OK)
	{
		NetWarning("Failed to unregister TomCrypt AES algorithm");
	}

	err = unregister_hash(&sha256_desc);
	if (err != CRYPT_OK)
	{
		NetWarning("Failed to unregister TomCrypt SHA256 algorithm");
	}
}


//////////////////////////////////////////////////////////////////////////
// CTomCryptAesState
//////////////////////////////////////////////////////////////////////////


/*static*/ CTomCryptAesState::TSharedPtr CTomCryptAesState::CreateAesStateFromSecret(const void* pSecret, const size_t secretSize)
{
	const int aesIdx = find_cipher("aes");
	if (aesIdx == -1)
	{
		NetWarning("TomCrypt AES algorithm is not initialized");
		return nullptr;
	}

	// Allocate buffer for TomCrypt's symmetric_CBC object and for CTomCryptAesState in a single allocation
	const size_t objSize = sizeof(CTomCryptAesState) + sizeof(symmetric_CBC);
	std::unique_ptr<uint8[]> p(new uint8[objSize]);

	symmetric_CBC* pCbc = reinterpret_cast<symmetric_CBC*>(p.get() + sizeof(CTomCryptAesState));

	uint8 iv[16] = {};

	int err = cbc_start(aesIdx, iv, reinterpret_cast<const unsigned char*>(pSecret), static_cast<int>(secretSize), 0, pCbc);
	if (err != CRYPT_OK)
	{
		return nullptr;
	}

	CTomCryptAesState* pKey = new (reinterpret_cast<void*>(p.get())) CTomCryptAesState();
	p.release();
	return TSharedPtr(pKey);
}

/*static*/ void CTomCryptAesState::operator delete(void* ptr)
{
	// See buffer allocation in CTomCryptAesState::CreateAesStateFromSecret
	delete[] reinterpret_cast<uint8*>(ptr);
}

CTomCryptAesState::CTomCryptAesState()
{}

CTomCryptAesState::~CTomCryptAesState()
{
	symmetric_CBC* pCbc = reinterpret_cast<symmetric_CBC*>(Handle());
	int err = cbc_done(pCbc);
	if (err != CRYPT_OK)
	{
		NetWarning("Failed to uninitialize TomCrypt AES CBC state");
	}
}

void* CTomCryptAesState::Handle()
{
	uint8* p = reinterpret_cast<uint8*>(this);
	return reinterpret_cast<symmetric_CBC*>(p + sizeof(CTomCryptAesState));
}

//////////////////////////////////////////////////////////////////////////
// CTomCryptAesCipher
//////////////////////////////////////////////////////////////////////////

void CTomCryptAesCipher::Init(const STomCryptAesInitState& initState)
{
	m_initVec = initState.initVec;
	m_pState = initState.pState;
}

bool CTomCryptAesCipher::EncryptInPlace(uint8* pBuf, const size_t bufSize)
{
	CRY_ASSERT_MESSAGE(m_pState, "CTomCryptAesCipher is not initialized");
	CRY_ASSERT(pBuf != nullptr);
	CRY_ASSERT_MESSAGE(bufSize % 16 == 0, "Encryption buffer has wrong size");

	if (!m_pState || !pBuf)
		return false;

	int err;

	symmetric_CBC* pCbc = reinterpret_cast<symmetric_CBC*>(m_pState->Handle());
	
	err = cbc_setiv(m_initVec.data(), m_initVec.size(), pCbc);
	CRY_ASSERT(err == CRYPT_OK);
	if (err != CRYPT_OK)
	{
		return false;
	}

	err = cbc_encrypt(pBuf, pBuf, bufSize, pCbc);
	if (err != CRYPT_OK)
	{
		return false;
	}

	unsigned long ivSize = m_initVec.size();
	err = cbc_getiv(m_initVec.data(), &ivSize, pCbc);
	if (err != CRYPT_OK)
	{
		return false;
	}

	return true;
}

bool CTomCryptAesCipher::DecryptInPlace(uint8* pBuf, const size_t bufSize)
{
	CRY_ASSERT_MESSAGE(m_pState, "CTomCryptAesCipher is not initialized");
	CRY_ASSERT(pBuf != nullptr);
	CRY_ASSERT_MESSAGE(bufSize % 16 == 0, "Encryption buffer has wrong size");

	if (!m_pState || !pBuf)
		return false;

	int err;

	symmetric_CBC* pCbc = reinterpret_cast<symmetric_CBC*>(m_pState->Handle());

	err = cbc_setiv(m_initVec.data(), m_initVec.size(), pCbc);
	CRY_ASSERT(err == CRYPT_OK);
	if (err != CRYPT_OK)
	{
		return false;
	}

	err = cbc_decrypt(pBuf, pBuf, bufSize, pCbc);
	if (err != CRYPT_OK)
	{
		return false;
	}

	unsigned long ivSize = m_initVec.size();
	err = cbc_getiv(m_initVec.data(), &ivSize, pCbc);
	if (err != CRYPT_OK)
	{
		return false;
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
// CTomCryptSha256Hmac
//////////////////////////////////////////////////////////////////////////

static hmac_state* GetTomCryptHmacObjectPtr(void* pHmac) { return reinterpret_cast<hmac_state*>(reinterpret_cast<uint8*>(pHmac) + sizeof(CTomCryptSha256Hmac)); }
static void* GetTomCryptHmacSecretPtr(void* pHmac) { return reinterpret_cast<uint8*>(GetTomCryptHmacObjectPtr(pHmac)) + sizeof(hmac_state); }

static bool InitTomCryptHmac(hmac_state* pHmacState, int shaIdx, const void* pSecret, const size_t secretSize)
{
	int err = hmac_init(pHmacState, shaIdx, reinterpret_cast<const uint8*>(pSecret), static_cast<unsigned long>(secretSize));
	if (err != CRYPT_OK)
	{
		NetWarning("Failed to create SHA256-HMAC object");
		return false;
	}
	return true;
}

CTomCryptSha256Hmac::CTomCryptSha256Hmac(int shaIdx, size_t secretSize)
	: m_shaIdx(shaIdx)
	, m_secretSize(secretSize)
{}


/*static*/ CTomCryptSha256Hmac::TSharedPtr CTomCryptSha256Hmac::CreateFromSecret(const void* pSecret, const size_t secretSize)
{
	const int shaIdx = find_hash("sha256");
	if (shaIdx == -1)
	{
		NetWarning("TomCrypt SHA256 algorithm is not initialized");
		return nullptr;
	}

	if (pSecret == nullptr || secretSize == 0)
	{
		NetWarning("Wrong SHA256-HMAC secret");
		return nullptr;
	}

	// Allocate buffer for TomCrypt's hmac_state object, secret and for CTomCryptSha256Hmac in a single allocation
	// We have to keep holding secret, as we potentially have to recreate hash object after every use.
	const size_t objSize = sizeof(CTomCryptSha256Hmac) + sizeof(hmac_state) + secretSize;
	std::unique_ptr<uint8[]> p(new uint8[objSize]);

	hmac_state* pHmacState = GetTomCryptHmacObjectPtr(p.get());
	void* pSecretBuffer = GetTomCryptHmacSecretPtr(p.get());

	::memcpy(pSecretBuffer, pSecret, secretSize);

	if (!InitTomCryptHmac(pHmacState, shaIdx, pSecretBuffer, secretSize))
	{
		return nullptr;
	}

	CTomCryptSha256Hmac* pKey = new (reinterpret_cast<void*>(p.get())) CTomCryptSha256Hmac(shaIdx, secretSize);
	p.release();
	return TSharedPtr(pKey);
}

/*static*/ void CTomCryptSha256Hmac::operator delete(void* ptr)
{
	// See buffer allocation in CTomCryptSha256Hmac::CreateFromSecret
	delete[] reinterpret_cast<uint8*>(ptr);
}

bool CTomCryptSha256Hmac::Hash(const uint8* pBuf, const size_t bufSize)
{
	hmac_state* pHmacState = GetTomCryptHmacObjectPtr(this);
	int err = hmac_process(pHmacState, pBuf, static_cast<unsigned long>(bufSize));
	CRY_ASSERT(err == CRYPT_OK);
	return (err == CRYPT_OK);
}

bool CTomCryptSha256Hmac::FinishAndRestartHash(HashResult& result)
{
	if (!FinishHash(result))
	{
		return false;
	}

	hmac_state* pHmacState = GetTomCryptHmacObjectPtr(this);
	const void* pSecret = GetTomCryptHmacSecretPtr(this);

	return InitTomCryptHmac(pHmacState, m_shaIdx, pSecret, m_secretSize);
}

bool CTomCryptSha256Hmac::FinishHash(HashResult& result)
{
	hmac_state* pHmacState = GetTomCryptHmacObjectPtr(this);

	unsigned long hashSize = result.size();
	int err = hmac_done(pHmacState, result.data(), &hashSize);
	if (err != CRYPT_OK)
	{
		return false;
	}
	CRY_ASSERT(hashSize == HashSize);
	return true;
}

#endif // CRYNETWORK_USE_TOMCRYPT

