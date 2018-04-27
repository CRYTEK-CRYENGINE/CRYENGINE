// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <UnitTest.h>
#include <CryMemory/STLAlignedAlloc.h>

TEST(CryAlignedVectorTest, All)
{
	stl::aligned_vector<int, 16> vec;

	vec.push_back(1);
	vec.push_back(2);
	vec.push_back(3);

	REQUIRE(vec.size() == 3);
	REQUIRE(((INT_PTR)(&vec[0]) % 16) == 0);
}
