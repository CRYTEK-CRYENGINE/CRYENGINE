// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#define POOLALLOCTESTSUIT

#if !defined(CRY_PLATFORM)
	#error CRY_PLATFORM is not defined, probably #include "stdafx.h" is missing.
#endif

#include <CryCore/Assert/CryAssert.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <list>
#include <limits>
#include "stdint.h"

#include <CryMemory/CryPool/PoolAlloc.h>
#include "AllocTrace.hpp"
#include "TGASave.h"

#define TICKER timeGetTime

static const size_t TEST_AREA_SIZE = 160 * 1024 * 1024;
static const size_t TEST_NODE_COUNT = TEST_AREA_SIZE / 2048;
static const size_t TEST_STL_COUNT = 16 * 1024 * 1024;
uint8 g_TestArea[sizeof(NCryPoolAlloc::CFirstFit < NCryPoolAlloc::CReferenced < NCryPoolAlloc::CMemoryStatic < TEST_AREA_SIZE >, TEST_NODE_COUNT >, NCryPoolAlloc::CListItemReference >)];
uint8 g_pSTLData[TEST_STL_COUNT];

template<class TContainer, class TPtr>
bool Validate(TContainer& rMemory, TPtr Handle, uint32 ID, uint32 AllocIdx)
{
	return true;
	uint8* pMemory = rMemory.Resolve<uint8*>(Handle);
	const uint32 S = rMemory.Size(Handle);
	for (uint32 b = 0; b < S; b++)
		if (pMemory[b] != (ID & 0xff))
		{
			printf("Corrupted %d\n", AllocIdx);
#ifdef _DEBUG
			__debugbreak();
#endif
			return false;
		}
	return true;
}

template<class TContainer, class TPtr>
void Sign(TContainer& rMemory, TPtr Handle, uint32 ID)
{
	return;
	uint8* pMemory = rMemory.Resolve<uint8*>(Handle);
	const uint32 S = rMemory.Size(Handle);
	for (uint32 b = 0; b < S; b++)
		//if(pMemory[b] && ID)
		//{
		//	printf("Tried to sign already used block %d\n",ID);
		//	__debugbreak();
		//}
		//else
		pMemory[b] = ID;
}

typedef NCryPoolAlloc::CFirstFit<NCryPoolAlloc::CInPlace<NCryPoolAlloc::CMemoryDynamic>, NCryPoolAlloc::CListItemInPlace> tdMySTLAllocator;
typedef NCryPoolAlloc::CSTLPoolAllocWrapper<size_t, tdMySTLAllocator>                                                     tdMySTLAllocatorWrapped;
tdMySTLAllocator* tdMySTLAllocatorWrapped::m_pContainer = 0;
tdMySTLAllocator STLContainer;

template<class TContainer, class TPtr>
void TestRSXAllocDumb(TContainer& rMemory)
{
	using namespace NCryPoolAlloc;

	printf("  rsx memory trace;     ");

	const size_t BFB = (int)rMemory.BiggestFreeBlock();

	STLContainer.InitMem(TEST_STL_COUNT, g_pSTLData);

	const uint32 Size = CRY_ARRAY_COUNT(g_Trace) / 3;
	size_t MostFragments = 0;

	tdMySTLAllocatorWrapped::Container(&STLContainer);
	std::vector<size_t, tdMySTLAllocatorWrapped> Handles;

	Handles.resize(Size);
	for (uint32 a = 0; a < Size; a++)
		Handles[a] = 0;

	for (uint32 a = 0; a < Size; a++)
	{
		uint32* pData = &g_Trace[a * 3];
		if (pData[1] == 0) //release?
		{
			if (!Validate<TContainer, TPtr>(rMemory, (TPtr)Handles[pData[0]], pData[0], a))
				break;
			Sign<TContainer, TPtr>(rMemory, (TPtr)Handles[pData[0]], 0);
			if (!rMemory.Free((TPtr)Handles[pData[0]]))
			{
				printf("could not release memory\n");
				rMemory.Free((TPtr)Handles[pData[0]]);
			}
			Handles[pData[0]] = 0;
			rMemory.Beat(); //defragmentation
		}
		else
		{
			rMemory.Beat(); //defragmentation
			Handles[pData[0]] = (size_t)rMemory.Allocate<TPtr>(pData[1], pData[2]);
			if (!Handles[pData[0]])
			{
				while (rMemory.Beat())
					;                       //defragmentation
				Handles[pData[0]] = (size_t)rMemory.Allocate<TPtr>(pData[1], pData[2]);
			}
			if (!Handles[pData[0]])
			{
				if (rMemory.MemFree() < pData[1])
					printf("Out of memory\n");
				else
					printf("Out of memory due to fragmentation\n");
				break;
			}
			else
			{
				uint8* pMem = rMemory.Resolve<uint8*>((TPtr)Handles[pData[0]]);
				if (rMemory.FragmentCount() > MostFragments)
					MostFragments = rMemory.FragmentCount();
			}
			const uint32 S = rMemory.Size((TPtr)Handles[pData[0]]);
			if (S < pData[1])
			{
				printf("allocated block(%dbyte) smaller than requested(%dbytes) at request %d\n", S, pData[1], a);
			}
			Sign<TContainer, TPtr>(rMemory, (TPtr)Handles[pData[0]], pData[0]);
			if (!Validate<TContainer, TPtr>(rMemory, (TPtr)Handles[pData[0]], pData[0], a))
				break;
		}
	}
	printf("\n\nBiggestBlock:%d %d\n\n", (int)BFB, (int)rMemory.BiggestFreeBlock());

	for (uint32 a = 0; a < Size; a++)
		rMemory.Free((TPtr)Handles[a]);

	printf(" Fragments:%6" PRISIZE_T "#", MostFragments);

}

template<class TContainer, class TPtr>
void TestFragmentedAlloc(TContainer& rMemory)
{
	printf("  fragmented allocation;");

	std::list<TPtr> Handles;
	size_t Allocated = 0;
	size_t MostFragments = 0;

	TPtr P0 = rMemory.Allocate<TPtr>(1024, 1);
	TPtr P1 = rMemory.Allocate<TPtr>(4096, 1);
	rMemory.Free(P0);
	if (P1)
	{
		Handles.push_back(P1);
		Allocated += 4096;
	}

	while (P0 && P1)
	{
		rMemory.Beat();
		P0 = rMemory.Allocate<TPtr>(1024, 1);
		if (!P0)
		{
			while (rMemory.Beat())
				;
			P0 = rMemory.Allocate<TPtr>(1024, 1);
		}
		P1 = rMemory.Allocate<TPtr>(4096, 1);
		if (!P1)
		{
			while (rMemory.Beat())
				;
			P1 = rMemory.Allocate<TPtr>(4096, 1);
		}
		if (rMemory.FragmentCount() > MostFragments)
			MostFragments = rMemory.FragmentCount();
		if (P0)
			rMemory.Free(P0);
		if (P1)
		{
			Handles.push_back(P1);
			Allocated += 4096;
		}
	}
	printf(" Fragments:%6" PRISIZE_T "# maxmem:%" PRISIZE_T " %3.2f%%", MostFragments, Allocated, static_cast<float>(Allocated) / static_cast<float>(rMemory.MemSize()) * 100.f);
	for (std::list<TPtr>::iterator it = Handles.begin(); it != Handles.end(); ++it)
		rMemory.Free(*it);
}

template<class TContainer, class TPtr>
void TestRealloc(TContainer& rMemory)
{
	printf("  realloc;");

	std::vector<TPtr> Handles;
	Handles.resize(1000);
	for (size_t a = 0; a < Handles.size(); a++)
		Handles[a] = rMemory.Allocate<TPtr>(10, 1);

	for (size_t a = 0; a < 1000000; a++)
		rMemory.Reallocate<TPtr>(&Handles[a % Handles.size()], cry_random_uint32(), 1);

	for (size_t a = 0; a < Handles.size(); a++)
		rMemory.Free(Handles[a]);
}

template<class TContainer, class TPtr>
void TestDefrag(TContainer& rMemory)
{
	const DWORD T0 = TICKER();
	TestRSXAllocDumb<TContainer, TPtr>(rMemory);
	const DWORD T1 = TICKER();
	printf(" -  %dms\n", T1 - T0);
	TestFragmentedAlloc<TContainer, TPtr>(rMemory);
	const DWORD T2 = TICKER();
	printf(" - %dms\n", T2 - T1);
	TestRealloc<TContainer, TPtr>(rMemory);
	const DWORD T3 = TICKER();
	printf(" - %dms\n", T3 - T2);
}

static int TestCounter = 0;

template<class TContainer, class TPtr>
void TestStatic(const char* pName)
{
	printf("%s...\n", pName);
	using namespace NCryPoolAlloc;
	{
		memset(g_TestArea, 0, sizeof(g_TestArea));
		TContainer& Memory = *new(g_TestArea)TContainer;
		Memory.InitMem();
		TestDefrag<TContainer, TPtr>(Memory);
		{
			char FileName[1024];
			cry_sprintf(FileName, "%dLogStatic.txt", TestCounter++);
			Memory.SaveStats(FileName);
		}
	}
	if (TContainer::Defragmentable())
	{
		printf(" Defrag:\n");
		memset(g_TestArea, 0, sizeof(g_TestArea));
		CDefragStacked<TContainer>& Memory = *new(g_TestArea)CDefragStacked<TContainer>;
		Memory.InitMem();
		TestDefrag<CDefragStacked<TContainer>, TPtr>(Memory);
		{
			char FileName[1024];
			cry_sprintf(FileName, "%dLogStatic.txt", TestCounter++);
			Memory.SaveStats(FileName);
		}
	}
}

template<class TContainer, class TPtr>
void TestDyn(const char* pName)
{
	printf("%s...\n", pName);
	using namespace NCryPoolAlloc;
	{
		memset(g_TestArea, 0, sizeof(g_TestArea));
		TContainer Memory;
		Memory.InitMem(sizeof(g_TestArea), g_TestArea);
		TestDefrag<TContainer, TPtr>(Memory);
		{
			char FileName[1024];
			cry_sprintf(FileName, "%dLogDyn.txt", TestCounter++);
			Memory.SaveStats(FileName);
		}
	}
	if (TContainer::Defragmentable())
	{
		printf(" Defrag:\n");
		memset(g_TestArea, 0, sizeof(g_TestArea));
		CDefragStacked<TContainer> Memory;
		Memory.InitMem(sizeof(g_TestArea), g_TestArea);
		TestDefrag<CDefragStacked<TContainer>, TPtr>(Memory);
		{
			char FileName[1024];
			cry_sprintf(FileName, "%dLogDyn.txt", TestCounter++);
			Memory.SaveStats(FileName);
		}
	}
}

typedef NCryPoolAlloc::CFirstFit<NCryPoolAlloc::CInPlace<NCryPoolAlloc::CMemoryDynamic>, NCryPoolAlloc::CListItemInPlace> tdMySTLAllocator2;
typedef NCryPoolAlloc::CSTLPoolAllocWrapper<std::string, tdMySTLAllocator2>                                               tdMySTLAllocatorWrapped2;
tdMySTLAllocator2* tdMySTLAllocatorWrapped2::m_pContainer = 0;
tdMySTLAllocator2 STLContainer2;

void TestSTL()
{
	STLContainer2.InitMem(TEST_STL_COUNT, g_pSTLData);

	tdMySTLAllocatorWrapped2::Container(&STLContainer2);
	typedef std::vector<std::string, tdMySTLAllocatorWrapped2> tdTest;
	tdTest Handles;

	const size_t Size = 1024;
	Handles.resize(Size);
	for (uint32 a = 0; a < Size; a++)
	{
		char Text[1024];
		cry_sprintf(Text, "%d", a % (Size / 117));
		Handles[a] = Text;
	}

	for (tdTest::iterator it = Handles.begin(); it != Handles.end(); ++it)
		printf("%s\n", it->c_str());

}

void TestFallback()
{
	using namespace NCryPoolAlloc;
	typedef CFirstFit<CInPlace<CMemoryDynamic>, CListItemInPlace> tdFallbackContainer;
	CFallback<tdFallbackContainer> Memory;
	uint8 Test[1024];
	Memory.InitMem(sizeof(Test), Test);
	Memory.FallbackMode(EFM_ENABLED);
	for (uint32 a = 0; a < 1024 * 1024 * 1024; a++)
		if (!Memory.Allocate<void*>(1024, 1))
		{
			printf("Fallback mode failed for %dKB", a);
			return;
		}
}

int main(int argc, char* argv[])
{
	using namespace NCryPoolAlloc;

	//	TestSTL();
	TestFallback();

	TestDyn<CReallocator<CInspector<CFirstFit<CInPlace<CMemoryDynamic>, CListItemInPlace>>>, void*>("CFirstFit<CInPlace<CMemoryDynamic>,CListItemInPlace >");
	TestDyn<CReallocator<CInspector<CBestFit<CInPlace<CMemoryDynamic>, CListItemInPlace>>>, void*>("CBestFit<CInPlace<CMemoryDynamic>,CListItemInPlace >");
	TestDyn<CReallocator<CInspector<CWorstFit<CInPlace<CMemoryDynamic>, CListItemInPlace>>>, void*>("CWorstFit<CInPlace<CMemoryDynamic>,CListItemInPlace >");
	TestDyn<CReallocator<CInspector<CFirstFit<CReferenced<CMemoryDynamic, TEST_NODE_COUNT>, CListItemReference>>>, size_t>("CFirstFit<CReferenced<CMemoryDynamic,...>,CListItemReference >");
	TestDyn<CReallocator<CInspector<CBestFit<CReferenced<CMemoryDynamic, TEST_NODE_COUNT>, CListItemReference>>>, size_t>("CBestFit<CReferenced<CMemoryDynamic,...>,CListItemReference >");
	TestDyn<CReallocator<CInspector<CWorstFit<CReferenced<CMemoryDynamic, TEST_NODE_COUNT>, CListItemReference>>>, size_t>("CWorstFit<CReferenced<CMemoryDynamic,...>,CListItemReference >");

	TestStatic<CReallocator<CInspector<CFirstFit<CInPlace<CMemoryStatic<TEST_AREA_SIZE>>, CListItemInPlace>>>, void*>("CFirstFit<CInPlace<CMemoryStatic<...> >,CListItemInPlace >");
	TestStatic<CReallocator<CInspector<CBestFit<CInPlace<CMemoryStatic<TEST_AREA_SIZE>>, CListItemInPlace>>>, void*>("CBestFit<CInPlace<CMemoryStatic<...> >,CListItemInPlace >");
	TestStatic<CReallocator<CInspector<CWorstFit<CInPlace<CMemoryStatic<TEST_AREA_SIZE>>, CListItemInPlace>>>, void*>("CWorstFit<CInPlace<CMemoryStatic<...> >,CListItemInPlace >");
	TestStatic<CReallocator<CInspector<CFirstFit<CReferenced<CMemoryStatic<TEST_AREA_SIZE>, TEST_NODE_COUNT>, CListItemReference>>>, size_t>("CFirstFit<CReferenced<CMemoryStatic<...>,...>,CListItemReference >");
	TestStatic<CReallocator<CInspector<CBestFit<CReferenced<CMemoryStatic<TEST_AREA_SIZE>, TEST_NODE_COUNT>, CListItemReference>>>, size_t>("CBestFit<CReferenced<CMemoryStatic<...>,...>,CListItemReference >");
	TestStatic<CReallocator<CInspector<CWorstFit<CReferenced<CMemoryStatic<TEST_AREA_SIZE>, TEST_NODE_COUNT>, CListItemReference>>>, size_t>("CWorstFit<CReferenced<CMemoryStatic<...>,...>,CListItemReference >");

	return 0;
}
