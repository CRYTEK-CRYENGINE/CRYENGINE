// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if !defined(_MSC_VER)
	#define __in
	#define __in_opt
	
	// NOTE: This definition is breaking spirv-cross linux trybuilds since <locale_conv.h> header is defining a variable called __out.
	//#define __out
	#define __out_opt
	#define __inout
	#define __inout_opt
	#define __out_ecount_opt(X)
	#define __stdcall
	#define __noop ((void)0)
	#define STDMETHODCALLTYPE
	#define __in_range(Start, Count)
	#define __in_ecount(Count)
	#define __out_ecount(Count)
	#define __in_ecount_opt(Count)
	#define __in_bcount_opt(Count)
	#define __out_bcount(Count)
	#define __out_bcount_opt(Count)
	#define __in_xcount_opt(Count)
	#define __RPC__deref_out
	#define _In_
	#define _In_opt_
	#define _Out_
	#define _Out_opt_
	#define _Inout_
	#define _Inout_opt_
	#define _In_reads_opt_(inexpr)
	#define _In_reads_bytes_(DataSize)
	#define _In_reads_bytes_opt_(DataSize)
	#define _Out_writes_bytes_(DataSize)
	#define _Out_writes_bytes_opt_(DataSize)
	#define _Outptr_
	#define  _Always_(ptr)
	#define _COM_Outptr_
	#define _Out_writes_opt_(len)
	#define _In_range_(from,to)
	#define _In_reads_(NumElements)
	#define _Out_writes_to_opt_(a,b)
#endif  // !defined(_MSC_VER)

enum
{
	E_OUTOFMEMORY = 0x8007000E,
	E_FAIL        = 0x80004005,
	E_ABORT       = 0x80004004,
	E_INVALIDARG  = 0x80070057,
	E_NOINTERFACE = 0x80004002,
	E_NOTIMPL     = 0x80004001,
	E_UNEXPECTED  = 0x8000FFFF
};

typedef struct _LUID
{
	uint32 LowPart;
	long   HighPart;
} LUID, *PLUID;

typedef struct _GUID
{
	uint32 Data1;
	ushort Data2;
	ushort Data3;
	uchar Data4[8];
} GUID;

typedef GUID IID;

inline bool operator==(const GUID& kLeft, const GUID& kRight)
{
	return memcmp(&kLeft, &kRight, sizeof(GUID)) == 0;
}

typedef const GUID& REFGUID;
typedef const GUID& REFIID;
