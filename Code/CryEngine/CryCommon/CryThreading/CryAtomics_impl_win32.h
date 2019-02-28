// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <CryCore/Platform/CryWindows.h>
#include <CryCore/Assert/CryAssert.h>

#if !(defined(NTDDI_VERSION) && defined(NTDDI_WIN8) && (NTDDI_VERSION >= NTDDI_WIN8))
// This declaration is missing from older Windows SDKs
// However, it's documented on MSDN as available starting with WinVista/Server2008
extern "C" WINBASEAPI PSLIST_ENTRY __fastcall InterlockedPushListSList(_Inout_ PSLIST_HEADER ListHead, _Inout_ PSLIST_ENTRY List, _Inout_ PSLIST_ENTRY ListEnd, _In_ ULONG Count);
#endif

//////////////////////////////////////////////////////////////////////////
void CryInterlockedPushEntrySList(SLockFreeSingleLinkedListHeader& list, SLockFreeSingleLinkedListEntry& element)
{
	static_assert(sizeof(SLockFreeSingleLinkedListHeader) == sizeof(SLIST_HEADER), "CRY_INTERLOCKED_SLIST_HEADER_HAS_WRONG_SIZE");
	static_assert(sizeof(SLockFreeSingleLinkedListEntry) >= sizeof(SLIST_ENTRY), "CRY_INTERLOCKED_SLIST_ENTRY_HAS_WRONG_SIZE");

	CRY_ASSERT_MESSAGE(IsAligned(&list, MEMORY_ALLOCATION_ALIGNMENT), "LockFree SingleLink List Header has wrong Alignment");
	CRY_ASSERT_MESSAGE(IsAligned(&element, MEMORY_ALLOCATION_ALIGNMENT), "LockFree SingleLink List Entry has wrong Alignment");
	InterlockedPushEntrySList(alias_cast<PSLIST_HEADER>(&list), alias_cast<PSLIST_ENTRY>(&element));
}

//////////////////////////////////////////////////////////////////////////
void CryInterlockedPushListSList(SLockFreeSingleLinkedListHeader& list, SLockFreeSingleLinkedListEntry& first, SLockFreeSingleLinkedListEntry& last, uint32 count)
{
	static_assert(sizeof(SLockFreeSingleLinkedListHeader) == sizeof(SLIST_HEADER), "CRY_INTERLOCKED_SLIST_HEADER_HAS_WRONG_SIZE");
	static_assert(sizeof(SLockFreeSingleLinkedListEntry) >= sizeof(SLIST_ENTRY), "CRY_INTERLOCKED_SLIST_ENTRY_HAS_WRONG_SIZE");

	CRY_ASSERT_MESSAGE(IsAligned(&list, MEMORY_ALLOCATION_ALIGNMENT), "LockFree SingleLink List Header has wrong Alignment");
	CRY_ASSERT_MESSAGE(IsAligned(&first, MEMORY_ALLOCATION_ALIGNMENT), "LockFree SingleLink List Entry has wrong Alignment");
	CRY_ASSERT_MESSAGE(IsAligned(&last, MEMORY_ALLOCATION_ALIGNMENT), "LockFree SingleLink List Entry has wrong Alignment");
	InterlockedPushListSList(alias_cast<PSLIST_HEADER>(&list), alias_cast<PSLIST_ENTRY>(&first), alias_cast<PSLIST_ENTRY>(&last), (ULONG)count);
}

//////////////////////////////////////////////////////////////////////////
void* CryInterlockedPopEntrySList(SLockFreeSingleLinkedListHeader& list)
{
	static_assert(sizeof(SLockFreeSingleLinkedListHeader) == sizeof(SLIST_HEADER), "CRY_INTERLOCKED_SLIST_HEADER_HAS_WRONG_SIZE");

	CRY_ASSERT_MESSAGE(IsAligned(&list, MEMORY_ALLOCATION_ALIGNMENT), "LockFree SingleLink List Header has wrong Alignment");
	return reinterpret_cast<void*>(InterlockedPopEntrySList(alias_cast<PSLIST_HEADER>(&list)));
}

//////////////////////////////////////////////////////////////////////////
void* CryRtlFirstEntrySList(SLockFreeSingleLinkedListHeader& list)
{
	static_assert(sizeof(SLockFreeSingleLinkedListHeader) == sizeof(SLIST_HEADER), "CRY_INTERLOCKED_SLIST_HEADER_HAS_WRONG_SIZE");
	static_assert(sizeof(SLockFreeSingleLinkedListEntry) >= sizeof(SLIST_ENTRY), "CRY_INTERLOCKED_SLIST_ENTRY_HAS_WRONG_SIZE");

	CRY_ASSERT_MESSAGE(IsAligned(&list, MEMORY_ALLOCATION_ALIGNMENT), "LockFree SingleLink List Header has wrong Alignment");
#if CRY_PLATFORM_DURANGO
	// This is normally implemented in NTDLL, but that can't be linked on Durango
	// However, we know that the X64 version of the header is used, so just access it directly
	return (void*)(((PSLIST_HEADER)&list)->HeaderX64.NextEntry << 4);
#else
	return reinterpret_cast<void*>(RtlFirstEntrySList(alias_cast<PSLIST_HEADER>(&list)));
#endif
}

//////////////////////////////////////////////////////////////////////////
void CryInitializeSListHead(SLockFreeSingleLinkedListHeader& list)
{
	CRY_ASSERT_MESSAGE(IsAligned(&list, MEMORY_ALLOCATION_ALIGNMENT), "LockFree SingleLink List Header has wrong Alignment");

	static_assert(sizeof(SLockFreeSingleLinkedListHeader) == sizeof(SLIST_HEADER), "CRY_INTERLOCKED_SLIST_HEADER_HAS_WRONG_SIZE");
	InitializeSListHead(alias_cast<PSLIST_HEADER>(&list));
}

//////////////////////////////////////////////////////////////////////////
void* CryInterlockedFlushSList(SLockFreeSingleLinkedListHeader& list)
{
	CRY_ASSERT_MESSAGE(IsAligned(&list, MEMORY_ALLOCATION_ALIGNMENT), "LockFree SingleLink List Header has wrong Alignment");

	static_assert(sizeof(SLockFreeSingleLinkedListHeader) == sizeof(SLIST_HEADER), "CRY_INTERLOCKED_SLIST_HEADER_HAS_WRONG_SIZE");
	return InterlockedFlushSList(alias_cast<PSLIST_HEADER>(&list));
}
