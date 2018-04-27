// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <UnitTest.h>
#include <CryCore/Containers/CryArray.h>

TEST(CryDynArrayTest, DifferenceType)
{
	DynArray<string, uint>::difference_type dif = -1;
	REQUIRE(dif < 0);
}

TEST(CryDynArrayTest, Basics)
{
	const int ais[] = { 11, 7, 5, 3, 2, 1, 0 };
	DynArray<int> ai;
	REQUIRE(ai.size() == 0);
	DynArray<int> ai1(4);
	REQUIRE(ai1.size() == 4);
	DynArray<int> ai3(6, 0);
	REQUIRE(ai3.size() == 6);
	DynArray<int> ai4(ai3);
	REQUIRE(ai4.size() == 6);
	DynArray<int> ai6(ais);
	REQUIRE(ai6.size() == 7);
	DynArray<int> ai7(ais + 1, ais + 5);
	REQUIRE(ai7.size() == 4);

	ai.push_back(3);
	ai.insert(&ai[0], 1, 1);
	ai.insert(1, 1, 2);
	ai.insert(0, 0);

	for (int i = 0; i < 4; i++)
	{
		REQUIRE(ai[i] == i);
	}

	ai.push_back(5u);
	REQUIRE(ai.size() == 5);
	ai.push_back(ai);
	REQUIRE(ai.size() == 10);
	ai.push_back(ArrayT(ais));
	REQUIRE(ai.size() == 17);
}

TEST(CryDynArrayTest, InitializerList)
{
	DynArray<int> arr = { 1, 3, 5 };
	REQUIRE(arr.size() == 3);
	REQUIRE(arr[0] == 1);
	REQUIRE(arr[1] == 3);
	REQUIRE(arr[2] == 5);
}

TEST(CryDynArrayTest, InsertEraseAssign)
{
	std::basic_string<char> bstr;

	const int nStrs = 11;
	string Strs[nStrs] = { "nought", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten" };

	DynArray<string> s;
	for (int i = 0; i < nStrs; i += 2)
		s.push_back(Strs[i]);
	for (int i = 1; i < nStrs; i += 2)
		s.insert(i, Strs[i]);
	s.push_back(string("eleven"));
	s.push_back("twelve");
	s.push_back("thirteen");
	REQUIRE(s.size() == 14);
	for (int i = 0; i < nStrs; i++)
		REQUIRE(s[i] == Strs[i]);

	DynArray<string> s2 = s;
	s.erase(5, 2);
	REQUIRE(s.size() == 12);

	s.insert(3, &Strs[5], &Strs[8]);

	s2 = s2(3, 4);
	REQUIRE(s2.size() == 4);

	s2.assign("single");
	REQUIRE(s2.size() == 1);
	REQUIRE(s2[0] == "single");
}

TEST(CryDynArrayTest, GrowShrink)
{
	DynArray<CryStackStringT<char, 8>> sf;
	sf.push_back("little");
	sf.push_back("Bigger one");
	sf.insert(1, "medium");
	sf.erase(0);
	REQUIRE(sf[0] == "medium");
	REQUIRE(sf[1] == "Bigger one");
}

struct Counts
{
	int construct = 0, copy_init = 0, copy_assign = 0, move_init = 0, move_assign = 0, destruct = 0;

	Counts()
	{}

	Counts(int a, int b, int c, int d, int e, int f)
		: construct(a), copy_init(b), copy_assign(c), move_init(d), move_assign(e), destruct(f) {}

	bool operator==(const Counts& o) const { return !memcmp(this, &o, sizeof(*this)); }
};

static Counts s_counts;

static Counts delta_counts()
{
	Counts delta = s_counts;
	s_counts = Counts();
	return delta;
}

struct Tracker
{
	int res;

	Tracker(int r = 0)
	{
		s_counts.construct += r;
		res = r;
	}
	Tracker(const Tracker& in)
	{
		s_counts.copy_init += in.res;
		res = in.res;
	}
	Tracker(Tracker&& in)
	{
		s_counts.move_init += in.res;
		res = in.res;
		in.res = 0;
	}
	void operator=(const Tracker& in)
	{
		s_counts.copy_assign += res + in.res;
		res = in.res;
	}
	void operator=(Tracker&& in)
	{
		s_counts.move_assign += res + in.res;
		res = in.res;
		in.res = 0;
	}
	~Tracker()
	{
		s_counts.destruct += res;
		res = 0;
	}
};

TEST(CryDynArrayTest, LocalDynArray)
{
	{
		// construct, copy_init, copy_assign, move_init, move_assign, destruct;
		typedef LocalDynArray<Tracker, 4> TrackerArray;
		TrackerArray at(3, 1);                            // construct init argument, move-init first element, copy-init 2 elements
		REQUIRE(delta_counts() == Counts(1, 2, 0, 1, 0, 0));
		at.erase(1);                                      // destruct 1 element, move-init 1 element
		REQUIRE(delta_counts() == Counts(0, 0, 0, 1, 0, 1));

		// Move forwarding
		at.reserve(6);                                    // move_init 2
		REQUIRE(delta_counts() == Counts(0, 0, 0, 2, 0, 0));
		at.append(Tracker(1));                            // construct, move_init
		at.push_back(Tracker(1));                         // construct, move_init
		at += Tracker(1);                                 // construct, move_init
		at.insert(1, Tracker(1));                         // construct, move_init, move_init * 4
		REQUIRE(delta_counts() == Counts(4, 0, 0, 8, 0, 0));

		DynArray<TrackerArray> aas(3, at);                // copy init 18 elements
		REQUIRE(delta_counts() == Counts(0, 18, 0, 0, 0, 0));
		aas.erase(0);                                     // destruct 6 elements, move element arrays (no element construction or destruction)
		REQUIRE(delta_counts() == Counts(0, 0, 0, 0, 0, 6));
	}                                                   // destruct 12 elements from aas and 6 from at
	REQUIRE(delta_counts() == Counts(0, 0, 0, 0, 0, 18));
	int construct = 0, copy_init = 0, copy_assign = 0, move_init = 0, move_assign = 0, destruct = 0;
}