// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <UnitTest.h>
#include "gtest/gtest-spi.h"
#include <stdio.h>

#include <CryThreading/CryThreadSafePushContainer.h>
#include <CryString/CryStringUtils.h>
#include <array>
#include <thread>

void InsertElements(CryMT::CThreadSafePushContainer<int64>* container, uint64 begin, uint64 end)
{
	for (uint64 i = begin; i < end; ++i)
	{
		container->push_back(i);
	}
}

void ValidateElements(CryMT::CThreadSafePushContainer<int64>* container, bool* testResults, uint64 begin, uint64 end)
{
	// Go through container and mark all numbers that are in container as true
	for (uint64 value : *container) // test iterators
	{
		testResults[value] = true;
	}

	// Check that all number in range are true
	for (uint64 i = begin; i < end; ++i)
	{
		EXPECT_TRUE(testResults[i]);
	}
}

// Check the alignment requirements of the first element
TEST(CryThreadSafePushContainer, FirstElementAlignment)
{
	// 16 Alignment
	struct CRY_ALIGN(16) SAlignmentTest16
	{
		void* pData;
	};
	CryMT::CThreadSafePushContainer<SAlignmentTest16> container_16;
	container_16.push_back({ nullptr });
	EXPECT_EQ(((UINT_PTR)&*container_16.begin()) % alignof(SAlignmentTest16), 0);

	// 64 Alignment
	struct CRY_ALIGN(64) SAlignmentTest64
	{
		void* pData;
	};
	CryMT::CThreadSafePushContainer<SAlignmentTest64> container_64;
	container_64.push_back({ nullptr });
	EXPECT_EQ(((UINT_PTR)&*container_64.begin()) % alignof(SAlignmentTest64), 0);

	// 128 Alignment
	struct CRY_ALIGN(128) SAlignmentTest128
	{
		void* pData;
	};
	CryMT::CThreadSafePushContainer<SAlignmentTest128> container_128;
	container_128.push_back({ nullptr });
	EXPECT_EQ(((UINT_PTR)&*container_128.begin()) % alignof(SAlignmentTest128), 0);
}

TEST(CryThreadSafePushContainer, ST_PushAndReset)
{
	CryMT::CThreadSafePushContainer<int64> container;
	std::array<bool, 2000> testResults = { false };

	// Re-use same container multiple times
	for (int i = 0; i < 2; ++i)
	{
		// Push 1000 elements
		InsertElements(&container,0, 1000);
		ValidateElements(&container, &testResults[0], 0, 1000);

		// Push another 1000 elements
		InsertElements(&container, 1000, 2000);

		// Check that we did not loose any elements from the previous coalesce
		ValidateElements(&container, &testResults[0], 0, 2000); 
		
		// Clear container
		EXPECT_FALSE(container.empty());
		container.reset_container(); // test reset_container()
		EXPECT_TRUE(container.empty());
	}
}

TEST(CryThreadSafePushContainer, MT_PushAndClear)
{
	const int numElements = 100000;
	const int numThreads = 10;
	const int numElementsPerThread = (numElements / numThreads) / 2;
	CryMT::CThreadSafePushContainer<int64> container(123);
	std::array<bool, numElements> testResults = { false };
	std::thread threads[numThreads];

	// Re-use same container multiple times
	for (int i = 0; i < 2; ++i)
	{
		// Fill half
		for (int k = 0; k < numThreads; ++k)
		{
			int64 begin = numElementsPerThread * k;
			threads[k] = std::thread(InsertElements, &container, begin, begin + numElementsPerThread);
		}

		for (int k = 0; k < numThreads; ++k)
		{
			threads[k].join();
		}

		ValidateElements(&container, &testResults[0], 0, numElementsPerThread * numThreads);

		// Fill the other half
		for (int k = 0; k < numThreads; ++k)
		{
			int64 begin = numElementsPerThread * (numThreads + k);
			threads[k] = std::thread(InsertElements, &container, begin, begin + numElementsPerThread);
		}

		for (int k = 0; k < numThreads; ++k)
		{
			threads[k].join();
		}

		// Check that we did not loose any elements from the previous coalace
		ValidateElements(&container, &testResults[0], 0, numElementsPerThread * numThreads * 2);

		// Clear container
		EXPECT_FALSE(container.empty());
		container.clear(); // test clear()
		EXPECT_TRUE(container.empty());
	}
}
void InsertAndStoreIndices(CryMT::CThreadSafePushContainer<int>* container, uint32 count, std::vector<std::pair<uint32, int>>* indexValueMap)
{
	for(uint32 i = 0; i < count; ++i)
	{
		uint32 index;
		const int value = rand();
		container->push_back(value, index);
		indexValueMap->emplace_back(index, value);
	}
}

TEST(CryThreadSafePushContainer, IndicesRemainValid)
{
	CryMT::CThreadSafePushContainer<int> container;
	std::vector<std::pair<uint32, int>> indexValueMap;

	for (int i = 0; i < 10; ++i)
	{
		std::thread thread(InsertAndStoreIndices, &container, 250, &indexValueMap);
		thread.join();
	}

	for (std::pair<uint32, int>& indexValuePair : indexValueMap)
	{
		EXPECT_EQ(container[indexValuePair.first], indexValuePair.second);
	}
}

TEST(CryThreadSafePushContainer, Iterators)
{
	CryMT::CThreadSafePushContainer<int64> container;

	EXPECT_TRUE(container.empty());
	EXPECT_EQ(container.begin(), container.end());
	
	std::thread(InsertElements, &container, 0, 32).join();
	std::thread(InsertElements, &container, 32, 64).join();

	EXPECT_GE(container.capacity(), 64);
	EXPECT_FALSE(container.empty());
	EXPECT_GE(container.capacity(), 64);
	EXPECT_FALSE(container.empty());

	EXPECT_NO_FATAL_FAILURE(container.begin());
	EXPECT_NO_FATAL_FAILURE(container.end());
	EXPECT_NE(container.begin(), container.end());

	int i = 0;
	for (auto it = container.begin(); it != container.end(); ++it, ++i)
	{
		EXPECT_EQ(*it, i);
	}

	EXPECT_NO_FATAL_FAILURE(++container.end());
	EXPECT_NO_FATAL_FAILURE(container.end()++);
	EXPECT_NO_FATAL_FAILURE(--container.begin());
	EXPECT_NO_FATAL_FAILURE(container.begin()--);

	EXPECT_NE(container.begin(), ++container.begin());
	EXPECT_NE(container.begin(), --container.begin());
	EXPECT_NE(container.end(), ++container.end());
	EXPECT_NE(container.end(), --container.end());

	container.clear();
	EXPECT_TRUE(container.empty());
}

TEST(CryThreadSafePushContainer, Iterators2)
{
	CryMT::CThreadSafePushContainer<int32> container;
	container.push_back(rand());
	container.push_back(rand());
	container.push_back(rand());
	container.push_back(rand());

	size_t count = 0;
	auto it = container.end();
	for (; it != container.begin();)
	{
		--it;
		++count;
	}

	EXPECT_EQ(count, 4);
	EXPECT_NO_FATAL_FAILURE(--it);
}

// TEST(CryThreadSafePushContainer, Tomb)
// {
// 	CryMT::CThreadSafePushContainer<int32> container;
// 	container.push_back(0xfbfbfbfb);
// 	container.push_back(0xfbfbfbfb);
// 	
// 	EXPECT_FALSE(container.empty());
// 	
// 	EXPECT_NE(container.begin(), container.end());
// 	EXPECT_NE(++container.begin(), container.end());
// 	EXPECT_NE(container.begin(), --container.end());
// 	EXPECT_EQ(container.begin(), --(--container.end()));
// 
// 	EXPECT_EQ(*container.begin(), 0xfbfbfbfb);
// 	EXPECT_EQ(*++container.begin(), 0xfbfbfbfb);
// 	EXPECT_EQ(*--container.end(), 0xfbfbfbfb);
// 	EXPECT_EQ(*--(--container.end()), 0xfbfbfbfb);
// }

TEST(CryThreadSafePushContainer, Tomb)
{
	CryMT::CThreadSafePushContainer<int32> container;
	
	EXPECT_ASSERT_FAILURE(container.push_back(0xfbfbfbfb); , "tombstone");
}

TEST(CryThreadSafePushContainer, OutofBounds)
{
	CryMT::CThreadSafePushContainer<int64> container(1);

	uint32 index;
	container.push_back(432, index);
	EXPECT_EQ(index, 0);
	container.push_back(56);

	EXPECT_EQ(container[0], 432);
	EXPECT_EQ(container[1], 56);
	
	EXPECT_ASSERT_FAILURE(container[2], "out of bound");
	EXPECT_ASSERT_FAILURE(container[3], "out of bound");
}

TEST(CryThreadSafePushContainer, MultiContainer)
{
	constexpr uint NumContainers = 5;
	constexpr uint ThreadsPerContainer = 15;
	constexpr uint ElementsPerThread = 1000;

	CryMT::CThreadSafePushContainer<int64> containers[NumContainers];
	std::vector<std::thread> threads;


	for (int t = 0; t < ThreadsPerContainer; ++t)
	{
		for (int c = 0; c < NumContainers; ++c)
			threads.emplace_back(InsertElements, &containers[c], ElementsPerThread*t, ElementsPerThread*(t + 1));
	}

	for (std::thread& thread : threads)
	{
		thread.join();
	}

	std::unique_ptr<bool[]> testResults(new bool[ThreadsPerContainer * ElementsPerThread]);
	for (int c = 0; c < NumContainers; ++c)
	{
		ValidateElements(containers + c, testResults.get(), 0, ThreadsPerContainer * ElementsPerThread);
	}
}

// we expect a protection fault when accessing an empty container, as no memory is mapped yet
TEST(CryThreadSafePushContainerDeathTest, NoMemoryConsumed)
{
	CryMT::CThreadSafePushContainer<int64> container;
	volatile int64 x; // without this the access can be optimized away in release
	ASSERT_DEATH_IF_SUPPORTED(x = container[0], "");
	container.push_back(42);
	EXPECT_NO_FATAL_FAILURE(x = container[0]);
	container.reset_container();
	ASSERT_DEATH_IF_SUPPORTED(x = container[0], "");
}

TEST(CryThreadSafePushContainerDeathTest, MaximumCapacity)
{
	CryMT::CThreadSafePushContainer<int64> container(64, 4 << 10);
	ASSERT_DEATH_IF_SUPPORTED(InsertElements(&container, 0, 1024), "");
}

TEST(CryThreadSafePushContainerDeathTest, TooManyThreads)
{
	CryMT::CThreadSafePushContainer<int64> container;
	std::vector<std::thread> threads;
	for (int i = 0; i < 30; ++i)
	{
		threads.emplace_back([&] { container.push_back(1); });
	}
	for(auto& thread : threads)
	{
		thread.join();
	}

	EXPECT_FALSE(HasFatalFailure());
	ASSERT_DEATH_IF_SUPPORTED(container.push_back(1), "");
}
