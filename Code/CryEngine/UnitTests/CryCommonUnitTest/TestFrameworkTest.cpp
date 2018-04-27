// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <UnitTest.h>
#include <memory>

struct SInlineStaticExample
{
	static const int var = 0; 

	enum { var2 = 3 };
};

struct SEqualUnequalOnlyStruct
{
	int x;
};
bool operator==(SEqualUnequalOnlyStruct const& lhs, SEqualUnequalOnlyStruct const& rhs) { return lhs.x == rhs.x; }
bool operator!=(SEqualUnequalOnlyStruct const& lhs, SEqualUnequalOnlyStruct const& rhs) { return lhs.x != rhs.x; }

struct SLessOnlyStruct
{
	int x;
};
bool operator<(SLessOnlyStruct const& lhs, SLessOnlyStruct const& rhs) { return lhs.x < rhs.x; }

enum EBitFlagEnum
{
	EBitFlagEnumA = 0,
	EBitFlagEnumB = BIT(0),
	EBitFlagEnumC = BIT(1),
};

void PlaceholderFunction() {};
template<typename T> void TemplatePlaceholderFunction() {};

//Ensure these basic evaluations compile and work as intended
//This is important when we touch the test framework itself
TEST(CryGTestFrameworkTest, RequireMacroEvaluation)
{
	//bool expression
	REQUIRE(true);

	//expression convertible to bool
	REQUIRE(100);

	//immediate value comparison
	REQUIRE(1 == 1);
	REQUIRE(1 != 2);

	//variable comparison
	int i = 1, j = 1;
	REQUIRE(i == j);
	REQUIRE(i != j - 1);
	REQUIRE(j == 1);
	REQUIRE(1 == j);

	//function comparison
	REQUIRE(PlaceholderFunction == PlaceholderFunction);
	REQUIRE(PlaceholderFunction != TemplatePlaceholderFunction<void>);
	REQUIRE(TemplatePlaceholderFunction<int> == TemplatePlaceholderFunction<int>);
	REQUIRE(TemplatePlaceholderFunction<float> != TemplatePlaceholderFunction<int>);

	//expressions using +, -, *, /, %, !, ~ are more associated than the macro decomposer
	//these must compile
	REQUIRE(1 + 1);
	REQUIRE(2 - 1);
	REQUIRE(2 * 2);
	REQUIRE(2 / 2);
	REQUIRE(3 % 2);
	REQUIRE(!0);
	REQUIRE(~0);

	//logical && ||
	REQUIRE(true && true);
	REQUIRE(true || false);
	REQUIRE(true && 1 > 0);

	//expressions using bitwise operators has to be wrapped in parentheses but this needs to compile
	int flag = EBitFlagEnumB | EBitFlagEnumC;
	REQUIRE((flag & EBitFlagEnumB));
	REQUIRE((1 << 3));

	//assignment needs to be wrapped in parentheses
	REQUIRE((i = 2));

	//comma separated expression has to be wrapped in parentheses but this needs to compile
	REQUIRE((0, 1));

	//non-trivial structures: must correctly preserve the state in the expression template
	{
		int off = 0;
		auto getLargeResult = [off] (bool incOffset) mutable
		{
			std::vector<int> vec;
			for (int i = 0; i < 1000; i++)
			{
				vec.push_back(i + off);
			}
			if (incOffset) ++off;
			return vec;
		};

		REQUIRE(getLargeResult(false) == getLargeResult(false));
		REQUIRE(getLargeResult(true) != getLargeResult(true));
	}

	//If a structure only defines equal and unequal (but not < <= > >=) it still needs to be equality comparable with REQUIRE()
	{
		REQUIRE(SEqualUnequalOnlyStruct{ 42 } == SEqualUnequalOnlyStruct{ 42 });
		REQUIRE(SEqualUnequalOnlyStruct{ 42 } != SEqualUnequalOnlyStruct{ -42 });
		SEqualUnequalOnlyStruct s1 = { 33 }, s2 = { 33 }, s3 = { 0 };
		REQUIRE(s1 == s2);
		REQUIRE(s1 != s3);
	}

	//If a structure only defines less than it still needs to be less-than comparable with REQUIRE()
	REQUIRE(SLessOnlyStruct{ 1 } < SLessOnlyStruct{ 2 });
}

TEST(CryGTestFrameworkTest, RequireMacroRobustness)
{
	//very long expression for testing the stability of test framework
	//must not crash / hang
	REQUIRE([] {
		int x = 3;
		++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--
			++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--
			++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--x;
		x += 24;
		return x;
	}() == [] {
		int x = 3;
		++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--
			++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--
			++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--++--x;
		x += 24;
		return x;
	}());
}

class CCopyMoveInspector
{
public:
	static int copyCount;
	static int moveCount;

	CCopyMoveInspector() = default;

	CCopyMoveInspector(const CCopyMoveInspector&)
	{
		++copyCount;
	}

	CCopyMoveInspector(CCopyMoveInspector&&)
	{
		++moveCount;
	}

	friend bool operator==(const CCopyMoveInspector&, const CCopyMoveInspector&) { return true;  }
	friend bool operator!=(const CCopyMoveInspector&, const CCopyMoveInspector&) { return false; }
	friend bool operator<=(const CCopyMoveInspector&, const CCopyMoveInspector&) { return true;  }
	friend bool operator>=(const CCopyMoveInspector&, const CCopyMoveInspector&) { return true;  }
	friend bool operator< (const CCopyMoveInspector&, const CCopyMoveInspector&) { return false; }
	friend bool operator> (const CCopyMoveInspector&, const CCopyMoveInspector&) { return false; }
};

int CCopyMoveInspector::copyCount = 0;
int CCopyMoveInspector::moveCount = 0;

struct SNonDefaultConstructible
{
	explicit SNonDefaultConstructible(int i) : i(i) {}
	friend bool operator== (const SNonDefaultConstructible& lhs, const SNonDefaultConstructible& rhs) { return lhs.i == rhs.i; }
	friend bool operator!= (const SNonDefaultConstructible& lhs, const SNonDefaultConstructible& rhs) { return lhs.i != rhs.i; }
	friend bool operator<= (const SNonDefaultConstructible& lhs, const SNonDefaultConstructible& rhs) { return lhs.i <= rhs.i; }
	friend bool operator>= (const SNonDefaultConstructible& lhs, const SNonDefaultConstructible& rhs) { return lhs.i >= rhs.i; }
	friend bool operator<  (const SNonDefaultConstructible& lhs, const SNonDefaultConstructible& rhs) { return lhs.i < rhs.i;  }
	friend bool operator>  (const SNonDefaultConstructible& lhs, const SNonDefaultConstructible& rhs) { return lhs.i > rhs.i;  }
	int i;
};

TEST(CryGTestFrameworkTest, RequireMacroSideEffect)
{
	//static const without definition
	//this must compile: no address is taken
	REQUIRE(SInlineStaticExample::var == SInlineStaticExample::var);
	REQUIRE(SInlineStaticExample::var == 0);

	REQUIRE(SInlineStaticExample::var2 == 3);

	//non-copyable types
	std::unique_ptr<int> p1{ new int(42) };
	std::unique_ptr<int> p2{ new int(42) };
	//this must compile: no copy is made while evaluating
	REQUIRE(p1 != p2);

	//this must compile: no instantiation of the type is made while evaluating
	REQUIRE(SNonDefaultConstructible(3) != SNonDefaultConstructible(4));

	CCopyMoveInspector a, b;
	CCopyMoveInspector::copyCount = CCopyMoveInspector::moveCount = 0;
	REQUIRE(a == b);//makes use of REQUIRE() macro and prove in the following tests that neither a nor b is copied or moved
	
	//we use EXPECT_EQ here because we don't want to side-effect the result of REQUIRE() that we are testing
	EXPECT_EQ(CCopyMoveInspector::copyCount, 0);
	EXPECT_EQ(CCopyMoveInspector::moveCount, 0);
}
