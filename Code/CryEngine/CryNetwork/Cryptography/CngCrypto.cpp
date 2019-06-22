// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CngCrypto.h"

#if CRYNETWORK_USE_CNG

#include <bcrypt.h>

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

//////////////////////////////////////////////////////////////////////////
// Helpers
//////////////////////////////////////////////////////////////////////////

static_assert(std::is_same<void*, BCRYPT_ALG_HANDLE>::value, "BCRYPT_ALG_HANDLE is not void*");

struct SCngAlgorithmHandleDeleter
{
	void operator()(BCRYPT_ALG_HANDLE h)
	{
		const NTSTATUS res = BCryptCloseAlgorithmProvider(h, 0);
		CRY_ASSERT(res == STATUS_SUCCESS);
	}
};

using SCngAlgoHandle = SCngHandle<SCngAlgorithmHandleDeleter>;

static SCngAlgoHandle CreateCngAlgorithmHandle(LPCWSTR pszAlgId, LPCWSTR pszImplementation, ULONG dwFlags)
{
	BCRYPT_ALG_HANDLE hAlg;
	const NTSTATUS res = BCryptOpenAlgorithmProvider(&hAlg, pszAlgId, pszImplementation, dwFlags);
	return (res == STATUS_SUCCESS)
		? SCngAlgoHandle(hAlg)
		: SCngAlgoHandle();
}

//////////////////////////////////////////////////////////////////////////
// Global algorithms
//////////////////////////////////////////////////////////////////////////

static SCngAlgoHandle s_aesCbcAlgoHandle;
static size_t s_aesKeyObjectSize = 0;

static SCngAlgoHandle s_sha256HmacAlgoHandle;
static size_t s_sha256HmacObjectSize = 0;

static bool InitCngAesCbc()
{
	s_aesCbcAlgoHandle = CreateCngAlgorithmHandle(BCRYPT_AES_ALGORITHM, nullptr, 0);
	if (!s_aesCbcAlgoHandle)
	{
		return false;
	}

	NTSTATUS res = BCryptSetProperty(s_aesCbcAlgoHandle.get(), BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
	if (res != STATUS_SUCCESS)
	{
		return false;
	}

	DWORD keyObjectLength = 0;
	ULONG cbData = 0;
	res = BCryptGetProperty(s_aesCbcAlgoHandle.get(), BCRYPT_OBJECT_LENGTH, (PUCHAR)&keyObjectLength, sizeof(keyObjectLength), &cbData, 0);
	CRY_ASSERT(cbData == sizeof(keyObjectLength));
	if (res != STATUS_SUCCESS)
	{
		return false;
	}

	s_aesKeyObjectSize = keyObjectLength;

	return true;
}

static bool InitCngSha256Hmac()
{
	s_sha256HmacAlgoHandle = CreateCngAlgorithmHandle(BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
	if (!s_sha256HmacAlgoHandle)
	{
		return false;
	}

	DWORD hashObjectLength = 0;
	ULONG cbData = 0;
	NTSTATUS res = BCryptGetProperty(s_sha256HmacAlgoHandle.get(), BCRYPT_OBJECT_LENGTH, (PUCHAR)&hashObjectLength, sizeof(hashObjectLength), &cbData, 0);
	CRY_ASSERT(cbData == sizeof(hashObjectLength));
	if (res != STATUS_SUCCESS)
	{
		return false;
	}

	DWORD hashLength = 0;
	res = BCryptGetProperty(s_sha256HmacAlgoHandle.get(), BCRYPT_HASH_LENGTH, (PUCHAR)&hashLength, sizeof(hashLength), &cbData, 0);
	if (res != STATUS_SUCCESS)
	{
		return false;
	}
	if (hashLength != CCngSha256Hmac::HashSize)
	{
		CryFatalError("Unexpected hash size");
	}

	s_sha256HmacObjectSize = hashObjectLength;

	return true;
}

void InitCng()
{
	if (!InitCngAesCbc())
	{
		CryFatalError("Unable to initialize CNG AES CBC algorithm");
	}
	if (!InitCngSha256Hmac())
	{
		CryFatalError("Unable to initialize CNG SHA256-HMAC algorithm");
	}
}

void ShutdownCng()
{
	s_sha256HmacAlgoHandle.reset();
	s_aesCbcAlgoHandle.reset();
}

//////////////////////////////////////////////////////////////////////////
// CCngKey
//////////////////////////////////////////////////////////////////////////

static_assert(std::is_same<void*, BCRYPT_KEY_HANDLE>::value, "BCRYPT_KEY_HANDLE is not void*");

void SCngKeyHandleDeleter::operator()(void* h)
{
	const NTSTATUS res = BCryptDestroyKey(h);
	CRY_ASSERT(res == STATUS_SUCCESS);
}

/*static*/ CCngKey::TSharedPtr CCngKey::CreateAesKeyFromSecret(const void* pSecret, const size_t secretSize)
{
	if (!s_aesCbcAlgoHandle)
	{
		NetWarning("CNG AES CBC algorithm is not initialized");
		return nullptr;
	}

	// Allocate buffer for BCrypt key object and for CCngKey in a single allocation
	const size_t objSize = sizeof(CCngKey) + s_aesKeyObjectSize;
	std::unique_ptr<uint8[]> p(new uint8[objSize]);

	void* pKeyObjBuffer = p.get() + sizeof(CCngKey);

	BCRYPT_KEY_HANDLE hKey;
	const NTSTATUS res = BCryptGenerateSymmetricKey(s_aesCbcAlgoHandle.get(), &hKey, (PUCHAR)pKeyObjBuffer, s_aesKeyObjectSize, (PUCHAR)pSecret, secretSize, 0);
	if (res != STATUS_SUCCESS)
	{
		return nullptr;
	}

	CCngKey* pKey = new (reinterpret_cast<void*>(p.get())) CCngKey(hKey);
	p.release();
	return TSharedPtr(pKey);
}

/*static*/ void CCngKey::operator delete(void* ptr)
{
	// See buffer allocation in CCngKey::CreateAesKeyFromSecret
	delete[] reinterpret_cast<uint8*>(ptr);
}

#if CRY_PLATFORM_DURANGO && (_MSC_VER <= 1900)
/*static*/ void CCngKey::operator delete(void* ptr, void*) noexcept
{
	CCngKey::operator delete(ptr);
}
#endif

CCngKey::CCngKey(void* pHandle)
	: m_handle(pHandle)
{}

//////////////////////////////////////////////////////////////////////////
// CCngAesCipher
//////////////////////////////////////////////////////////////////////////


void CCngAesCipher::Init(const SCngAesInitState& initState)
{
	m_initVec = initState.initVec;
	m_key = initState.key;
}


bool CCngAesCipher::EncryptInPlace(uint8* pBuf, const size_t bufSize)
{
	CRY_ASSERT_MESSAGE(m_key, "CCngAesCipher is not initialized");
	CRY_ASSERT(pBuf != nullptr);
	CRY_ASSERT_MESSAGE(bufSize % 16 == 0, "Encryption buffer has wrong size");

	if (!m_key || !pBuf)
		return false;

	ULONG encodedSize = 0;
	const NTSTATUS res = BCryptEncrypt(m_key->Handle(), pBuf, bufSize, nullptr, &m_initVec.front(), m_initVec.size(), pBuf, bufSize, &encodedSize, 0);
	CRY_ASSERT(res == STATUS_SUCCESS);
	if (res != STATUS_SUCCESS)
	{
		return false;
	}
	CRY_ASSERT(encodedSize == bufSize);
	return true;
}

bool CCngAesCipher::DecryptInPlace(uint8* pBuf, const size_t bufSize)
{
	CRY_ASSERT_MESSAGE(m_key, "CCngAesCipher is not initialized");
	CRY_ASSERT(pBuf != nullptr);
	CRY_ASSERT_MESSAGE(bufSize % 16 == 0, "Decryption buffer has wrong size");

	if (!m_key || !pBuf)
	{
		return false;
	}

	ULONG decodedSize = 0;
	const NTSTATUS res = BCryptDecrypt(m_key->Handle(), pBuf, bufSize, nullptr, &m_initVec.front(), m_initVec.size(), pBuf, bufSize, &decodedSize, 0);
	CRY_ASSERT(res == STATUS_SUCCESS);
	if (res != STATUS_SUCCESS)
	{
		return false;
	}
	CRY_ASSERT(decodedSize == bufSize);
	return true;
}


//////////////////////////////////////////////////////////////////////////
// CCngSha256Hmac
//////////////////////////////////////////////////////////////////////////

static_assert(std::is_same<void*, BCRYPT_HASH_HANDLE>::value, "BCRYPT_HASH_HANDLE is not void*");

void SCngHashHandleDeleter::operator()(void* h)
{
	const NTSTATUS res = BCryptDestroyHash(h);
	CRY_ASSERT(res == STATUS_SUCCESS);
}

static SCngHashHandle CreateHash(void* pObjectMem, size_t objectSize, void* pSecret, size_t secretSize, bool& outIsReusable)
{
	BCRYPT_HASH_HANDLE hHash;
	outIsReusable = true;
	NTSTATUS res = BCryptCreateHash(s_sha256HmacAlgoHandle.get(), &hHash, (PUCHAR)pObjectMem, objectSize, (PUCHAR)pSecret, secretSize, BCRYPT_HASH_REUSABLE_FLAG);
	if (res != STATUS_SUCCESS)
	{
		// The provider does not support reusable hash implementation
		outIsReusable = false;
		res = BCryptCreateHash(s_sha256HmacAlgoHandle.get(), &hHash, (PUCHAR)pObjectMem, objectSize, (PUCHAR)pSecret, secretSize, 0);
	}

	if (res == STATUS_SUCCESS)
	{
		return SCngHashHandle(hHash);
	}
	return SCngHashHandle();
}

static void* GetCngHmacObjectPtr(void* pHmac) { return reinterpret_cast<uint8*>(pHmac) + sizeof(CCngSha256Hmac); }
static void* GetCngHmacSecretPtr(void* pHmac) { return reinterpret_cast<uint8*>(GetCngHmacObjectPtr(pHmac)) + s_sha256HmacObjectSize; }

/*static*/ CCngSha256Hmac::TSharedPtr CCngSha256Hmac::CreateFromSecret(const void* pSecret, const size_t secretSize)
{
	if (!s_sha256HmacAlgoHandle)
	{
		NetWarning("CNG SHA256-HMAC algorithm is not initialized");
		return nullptr;
	}

	if (pSecret == nullptr || secretSize == 0)
	{
		NetWarning("Wrong SHA256-HMAC secret");
		return nullptr;
	}

	// Allocate buffer for CNG hash object, secret and for CCngSha256Hmac in a single allocation
	// We have to keep holding secret, as we potentially have to recreate hash object after every use.
	const size_t objSize = sizeof(CCngSha256Hmac) + s_sha256HmacObjectSize + secretSize;
	std::unique_ptr<uint8[]> p(new uint8[objSize]);

	void* pHashObjBuffer = GetCngHmacObjectPtr(p.get());
	void* pSecretBuffer = GetCngHmacSecretPtr(p.get());

	::memcpy(pSecretBuffer, pSecret, secretSize);

	bool isReusable;
	SCngHashHandle hashHandle = CreateHash(pHashObjBuffer, s_sha256HmacObjectSize, pSecretBuffer, secretSize, *&isReusable);
	if (!hashHandle)
	{
		NetWarning("Failed to create SHA256-HMAC object");
		return nullptr;
	}

	CCngSha256Hmac* pKey = new (reinterpret_cast<void*>(p.get())) CCngSha256Hmac(std::move(hashHandle), isReusable, secretSize);
	p.release();
	return TSharedPtr(pKey);
}

/*static*/ void CCngSha256Hmac::operator delete(void* ptr)
{
	// See buffer allocation in CCngSha256Hmac::CreateFromSecret
	delete[] reinterpret_cast<uint8*>(ptr);
}

#if CRY_PLATFORM_DURANGO && (_MSC_VER <= 1900)
/*static*/ void CCngSha256Hmac::operator delete(void* ptr, void*) noexcept
{
	CCngSha256Hmac::operator delete(ptr);
}
#endif

CCngSha256Hmac::CCngSha256Hmac(SCngHashHandle&& handle, bool isReusable, size_t secretSize)
	: m_handle(std::move(handle))
	, m_isReusable(isReusable)
	, m_secretSize(secretSize)
{}

bool CCngSha256Hmac::Hash(const uint8* pBuf, const size_t bufSize)
{
	CRY_ASSERT_MESSAGE(m_handle, "CCngSha256Hmac is not initialized");
	if (!m_handle)
	{
		return false;
	}

	// API documentation: "This function does not modify the contents of this buffer."
	const NTSTATUS res = BCryptHashData(m_handle.get(), const_cast<uint8*>(pBuf), bufSize, 0);
	CRY_ASSERT(res == STATUS_SUCCESS);
	return res == STATUS_SUCCESS;
}

bool CCngSha256Hmac::FinishAndRestartHash(HashResult& result)
{
	if (!FinishHash(result))
	{
		return false;
	}
	if (!m_isReusable)
	{
		// Recreate hash object as it's not reusable
		m_handle.reset();
		void* pHashObjBuffer = GetCngHmacObjectPtr(this);
		void* pSecretBuffer = GetCngHmacSecretPtr(this);
		m_handle = CreateHash(pHashObjBuffer, s_sha256HmacObjectSize, pSecretBuffer, m_secretSize, *&m_isReusable);
		if (!m_handle)
		{
			return false;
		}
	}
	return true;
}

bool CCngSha256Hmac::FinishHash(HashResult& result)
{
	CRY_ASSERT_MESSAGE(m_handle, "CCngSha256Hmac is not initialized");
	if (!m_handle)
	{
		return false;
	}

	const NTSTATUS res = BCryptFinishHash(m_handle.get(), &result[0], result.size(), 0);
	CRY_ASSERT(res == STATUS_SUCCESS);
	return res == STATUS_SUCCESS;
}

#endif // CRYNETWORK_USE_CNG

