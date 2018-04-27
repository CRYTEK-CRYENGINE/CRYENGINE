// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <UnitTest.h>
#include <CryCore/RingBuffer.h>

TEST(CryRingBufferTest, All)
{
	{
		CRingBuffer<int, 256> buffer;
		REQUIRE(buffer.empty() && buffer.size() == 0 && buffer.full() == false);

		// 0, 1, 2, 3
		buffer.push_back(2);
		buffer.push_front(1);
		buffer.push_front_overwrite(0);
		buffer.push_back_overwrite(3);

		REQUIRE(buffer.front() == 0);
		REQUIRE(buffer.back() == 3);

		buffer.pop_front();
		buffer.pop_back();

		REQUIRE(buffer.size() == 2);
		REQUIRE(buffer.front() == 1);
		REQUIRE(buffer.back() == 2);
	}

	{
		CRingBuffer<float, 3> buffer;

		// 0, 1, 2
		buffer.push_back(0.0f);
		buffer.push_back(1.0f);
		buffer.push_back(2.0f);
		REQUIRE(buffer.full());

		// 3, 1, 2
		buffer.pop_front();
		buffer.push_front(3.0f);
		bool bPushed = buffer.push_front(4.0f);
		REQUIRE(!bPushed && buffer.front() != 4.0f);

		// 4, 3, 1
		buffer.push_front_overwrite(4.0f);
		REQUIRE(buffer.front() == 4.0f);
		REQUIRE(buffer.back() == 1.0f);

		// nop
		bPushed = buffer.push_back(5.0f);
		REQUIRE(!bPushed && buffer.back() != 5.0f);

		// 3, 1, 5
		buffer.push_back_overwrite(5.0f);
		REQUIRE(buffer.front() == 3.0f);
		REQUIRE(buffer.back() == 5.0f);
	}
}