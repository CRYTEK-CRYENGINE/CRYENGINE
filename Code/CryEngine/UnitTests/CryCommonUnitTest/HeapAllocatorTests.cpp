// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <UnitTest.h>
#include <CryMemory/HeapAllocator.h>

TEST(CryHeapAllocatorTest, All)
{
	typedef stl::HeapAllocator<stl::PSyncNone> THeap;
	THeap beep;

	for (int i = 0; i < 8; ++i)
	{
		THeap::Array<float> af(beep, i + 77);
		af.push_back(3.14f);
		THeap::Array<bool, uint, 16> ab(beep, i * 5);
		THeap::Array<int> ai(beep);
		ai.reserve(9111);
		THeap::Array<string> as(beep, 8);
		ai.push_back(-7);
		ai.push_back(-6);
		af.erase(30, 40);
		if (i == 2)
			ai.resize(0);
		as[0] = "nil";
		as[7] = "sevven";
		if (i == 4)
			as.clear();
	}
	REQUIRE(beep.GetTotalMemory().nUsed == 0);
}
