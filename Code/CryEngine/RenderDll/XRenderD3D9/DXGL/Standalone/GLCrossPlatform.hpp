// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GLCrossPlatform.hpp
//  Version:     v1.00
//  Created:     27/03/2014 by Valerio Guagliumi.
//  Description: Cross platform DXGL helper types
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __GLCROSSPLATFORM__
#define __GLCROSSPLATFORM__

#include <algorithm>
#include <cfloat>

using namespace std;

#if defined(_MSC_VER)
	#define ILINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
	#define ILINE __attribute__((always_inline))
#else
	#define ILINE inline
#endif

#define IF_LIKELY   if
#define IF_UNLIKELY if

#if !defined(_MSC_VER)

inline int sprintf_s(char* buffer, size_t size, const char* format, ...)
{
	#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	va_list args;
	va_start(args, format);
	int err = vsnprintf(buffer, size, format, args);
	va_end(args);
	return err;
	#else
		#error "Not implemented on this platform"
	#endif
}

template<size_t size>
int sprintf_s(char (&buffer)[size], const char* format, ...)
{
	#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	va_list args;
	va_start(args, format);
	int err = vsnprintf(buffer, size, format, args);
	va_end(args);
	return err;
	#else
		#error "Not implemented on this platform"
	#endif
}

inline int strcpy_s(char* dst, size_t size, const char* src)
{
	#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	strncpy(dst, src, size);
	dst[size - 1] = 0;
	return 0;
	#else
		#error "Not implemented on this platform"
	#endif
}

template<size_t SIZE>
int strcpy_s(char (&dst)[SIZE], const char* src)
{
	return strcpy_s(dst, SIZE, src);
}

inline void ZeroMemory(void* pPtr, int nSize)
{
	memset(pPtr, 0, nSize);
}

#endif //!defined(_MSC_VER)

namespace NCryOpenGL
{

inline void* Malloc(size_t size)
{
	return malloc(size)
}

inline void* Calloc(size_t num, size_t size)
{
	return calloc(num, size)
}

inline void* Realloc(void* memblock, size_t size)
{
	return realloc(memblock, size)
}

inline void Free(void* memblock)
{
	free(memblock)
}

void* CreateTLS();
void  SetTLSValue(void* pTLSHandle, void* pValue);
void* GetTLSValue(void* pTLSHandle);
void  DestroyTLS(void* pTLSHandle);
bool  MakeDir(const char* szDirName);

namespace NCrossPlatformImpl
{

struct SAutoLog
{
	FILE* m_pFile;

	SAutoLog(const char* szFileName)
	{
		m_pFile = fopen("DXGL.log", "w");
	}

	~SAutoLog()
	{
		if (m_pFile != NULL)
			fclose(m_pFile);
	}
};

struct SAutoTLSSlot
{
	void* m_pTLSHandle;

	SAutoTLSSlot()
	{
		m_pTLSHandle = CreateTLS();
	}

	~SAutoTLSSlot()
	{
		DestroyTLS(m_pTLSHandle);
	}
};

inline uint32 CRC32Reflect(unsigned int ref, char ch)
{
	unsigned int value = 0;

	// Swap bit 0 for bit 7
	// bit 1 for bit 6, etc.
	for (int i = 1; i < (ch + 1); i++)
	{
		if (ref & 1)
			value |= 1 << (ch - i);
		ref >>= 1;
	}
	return value;
}

extern SAutoLog g_kLog;
extern SAutoTLSSlot g_kCRCTable;

}

#define PRINTF_PARAMS(a, b)

void        LogMessage(ELogSeverity eSeverity, const char* szFormat, ...) PRINTF_PARAMS(2, 3);
inline void LogMessage(ELogSeverity eSeverity, const char* szFormat, ...)
{
	if (NCrossPlatformImpl::g_kLog.m_pFile != NULL)
	{
		va_list kArgs;
		va_start(kArgs, szFormat);
		vfprintf(NCrossPlatformImpl::g_kLog.m_pFile, szFormat, kArgs);
		va_end(kArgs);
		fputc('\n', NCrossPlatformImpl::g_kLog.m_pFile);
		fflush(NCrossPlatformImpl::g_kLog.m_pFile);
	}

	if (eSeverity == eLS_Fatal)
		std::exit();
}

inline uint32 GetCRC32(const char* pData, size_t uSize, uint32 uCRC)
{
	uint32* pCRCTable(static_cast<uint32*>(GetTLSValue(NCrossPlatformImpl::g_kCRCTable.m_pTLSHandle)));
	if (pCRCTable == NULL)
	{
		pCRCTable = new uint32[256];
		SetTLSValue(NCrossPlatformImpl::g_kCRCTable.m_pTLSHandle, pCRCTable);

		// This is the official polynomial used by CRC-32
		// in PKZip, WinZip and Ethernet.
		unsigned int ulPolynomial = 0x04c11db7;

		// 256 values representing ASCII character codes.
		for (int i = 0; i <= 0xFF; i++)
		{
			pCRCTable[i] = NCrossPlatformImpl::CRC32Reflect(i, 8) << 24;
			for (int j = 0; j < 8; j++)
				pCRCTable[i] = (pCRCTable[i] << 1) ^ (pCRCTable[i] & (1U << 31) ? ulPolynomial : 0);
			pCRCTable[i] = NCrossPlatformImpl::CRC32Reflect(pCRCTable[i], 32);
		}
	}

	int len;
	unsigned char* buffer;

	// Get the length.
	len = uSize;

	// Save the text in the buffer.
	buffer = (unsigned char*)pData;
	// Perform the algorithm on each character in the string, using the lookup table values.

	while (len--)
		uCRC = (uCRC >> 8) ^ pCRCTable[(uCRC & 0xFF) ^ *buffer++];
	// Exclusive OR the result with the beginning value.
	return uCRC ^ 0xffffffff;
}

using std::string;

struct STraceFile
{
	STraceFile()
		: m_pFile(NULL)
	{
	}

	~STraceFile()
	{
		if (m_pFile != NULL)
			fclose(m_pFile);
	}

	bool Open(const char* szFileName, bool bBinary)
	{
		if (m_pFile != NULL)
			return false;

		const char* szDirName = "DXGLTrace";
		do
		{
			char acFullPath[256];
			sprintf_s(acFullPath, "%s/%s", szDirName, szFileName);
			const char* szMode(bBinary ? "wb" : "w");
			m_pFile = fopen(acFullPath, szMode);
			if (m_pFile != NULL)
				return true;
		}
		while (MakeDir(szDirName));

		return false;
	}

	void Write(const void* pvData, uint32 uSize)
	{
		fwrite(pvData, (size_t)uSize, 1, m_pFile);
	}

	void Printf(const char* szFormat, ...)
	{
		va_list kArgs;
		va_start(kArgs, szFormat);
		vfprintf(m_pFile, szFormat, kArgs);
		va_end(kArgs);
	}

	FILE* m_pFile;
};

inline void RegisterConfigVariable(const char*, int* piVariable, int iValue)
{
	*piVariable = iValue;
}

#define IL2VAL(mask, shift)       \
  c |= ((x & mask) != 0) * shift; \
  x >>= ((x & mask) != 0) * shift

inline uint32 IntegerLog2(uint32 x)
{
	uint32 c = 0;
	IL2VAL(0xffff0000u, 16);
	IL2VAL(0xff00, 8);
	IL2VAL(0xf0, 4);
	IL2VAL(0xc, 2);
	IL2VAL(0x2, 1);
	return c;
}

#undef IL2VAL

template<typename T>
inline size_t countTrailingZeroes(T v)
{
	size_t n = 0;

	v = ~v & (v - 1);
	while (v)
	{
		++n;
		v >>= 1;
	}

	return n;
}

template<typename DestinationType, typename SourceType>
inline DestinationType alias_cast(SourceType pPtr)
{
	union
	{
		SourceType      pSrc;
		DestinationType pDst;
	} conv_union;
	conv_union.pSrc = pPtr;
	return conv_union.pDst;
}

inline void Memcpy(void* pDst, const void* pSrc, size_t uLength)
{
	memcpy(pDst, pSrc, uLength);
}

inline void PushProfileLabel(const char* szName)
{
	DXGLProfileLabelPush(szName);
}

inline void PopProfileLabel(const char* szName)
{
	DXGLProfileLabelPop(szName);
}

}

#endif //__GLCROSSPLATFORM__
