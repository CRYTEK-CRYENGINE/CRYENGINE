// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <UnitTest.h>
#include <CryCore/CryVariant.h>
#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Math_SSE.h>

TEST(CryVariantTest, DefaultInitialization)
{
	CryVariant<string, Vec3, short, int> v;
	REQUIRE(stl::holds_alternative<string>(v));
	REQUIRE(v.index() == 0);
	REQUIRE(stl::get<string>(v) == string());
}

TEST(CryVariantTest, EmplaceAndGet)
{
	CryVariant<int, string> v;

	v.emplace<int>(5);
	REQUIRE(stl::get<int>(v) == 5);
	REQUIRE(stl::get<0>(v) == 5);
	REQUIRE(stl::get_if<int>(&v) != nullptr);
	REQUIRE((*stl::get_if<int>(&v)) == 5);
	REQUIRE(stl::get_if<string>(&v) == nullptr);
	REQUIRE(stl::get_if<0>(&v) != nullptr);
	REQUIRE((*stl::get_if<0>(&v)) == 5);
	REQUIRE(stl::get_if<1>(&v) == nullptr);

	v.emplace<string>("Hello World");
	REQUIRE(stl::get<string>(v) == "Hello World");
	REQUIRE(stl::get<1>(v) == "Hello World");
	REQUIRE(stl::get_if<int>(&v) == nullptr);
	REQUIRE(stl::get_if<string>(&v) != nullptr);
	REQUIRE((*stl::get_if<string>(&v)) == "Hello World");
	REQUIRE(stl::get_if<0>(&v) == nullptr);
	REQUIRE(stl::get_if<1>(&v) != nullptr);
	REQUIRE((*stl::get_if<1>(&v)) == "Hello World");

	v.emplace<0>(5);
	REQUIRE(stl::get<int>(v) == 5);
	REQUIRE(stl::get<0>(v) == 5);
	REQUIRE(stl::get_if<int>(&v) != nullptr);
	REQUIRE((*stl::get_if<int>(&v)) == 5);
	REQUIRE(stl::get_if<string>(&v) == nullptr);
	REQUIRE(stl::get_if<0>(&v) != nullptr);
	REQUIRE((*stl::get_if<0>(&v)) == 5);
	REQUIRE(stl::get_if<1>(&v) == nullptr);

	v.emplace<1>("Hello World");
	REQUIRE(stl::get<string>(v) == "Hello World");
	REQUIRE(stl::get<1>(v) == "Hello World");
	REQUIRE(stl::get_if<int>(&v) == nullptr);
	REQUIRE(stl::get_if<string>(&v) != nullptr);
	REQUIRE((*stl::get_if<string>(&v)) == "Hello World");
	REQUIRE(stl::get_if<0>(&v) == nullptr);
	REQUIRE(stl::get_if<1>(&v) != nullptr);
	REQUIRE((*stl::get_if<1>(&v)) == "Hello World");
}

TEST(CryVariantTest, Equals)
{
	CryVariant<bool, int> v1, v2;

	v1.emplace<bool>(true);
	v2.emplace<bool>(true);
	REQUIRE(v1 == v2);
	REQUIRE(!(v1 != v2));

	v1.emplace<bool>(true);
	v2.emplace<bool>(false);
	REQUIRE(!(v1 == v2));
	REQUIRE(v1 != v2);

	v1.emplace<bool>(true);
	v2.emplace<int>(1);
	REQUIRE(!(v1 == v2));
	REQUIRE(v1 != v2);
}

TEST(CryVariantTest, Less) // used for sorting in stl containers
{
	CryVariant<bool, int> v1, v2;

	v1.emplace<int>(0);
	v2.emplace<int>(5);
	REQUIRE(v1 < v2);
	REQUIRE(!(v2 < v1));

	v1.emplace<int>(0);
	v2.emplace<int>(0);
	REQUIRE(!(v1 < v2));
	REQUIRE(!(v2 < v1));

	v1.emplace<bool>(true);
	v2.emplace<int>(0);
	REQUIRE(v1 < v2);
	REQUIRE(!(v2 < v1));
}

TEST(CryVariantTest, CopyAndMove)
{
	CryVariant<int, string> v1, v2;

	v1.emplace<int>(0);
	v2.emplace<int>(1);
	v1 = v2;
	REQUIRE(stl::get<int>(v1) == 1);

	v1.emplace<string>("Hello");
	v2.emplace<string>("World");
	v1 = v2;
	REQUIRE(stl::get<string>(v1) == "World");

	v1.emplace<int>(0);
	v2.emplace<string>("Hello World");
	v1 = v2;
	REQUIRE(stl::holds_alternative<string>(v1));
	REQUIRE(stl::get<string>(v1) == "Hello World");

	v1.emplace<int>(0);
	v2.emplace<int>(1);
	v1 = std::move(v2);
	REQUIRE(stl::get<int>(v1) == 1);

	v1.emplace<string>("Hello");
	v2.emplace<string>("World");
	v1 = std::move(v2);
	REQUIRE(stl::get<string>(v1) == "World");

	v1.emplace<int>(0);
	v2.emplace<string>("Hello World");
	v1 = std::move(v2);
	REQUIRE(stl::holds_alternative<string>(v1));
	REQUIRE(stl::get<string>(v1) == "Hello World");

	v1.emplace<string>("Hello World");
	CryVariant<int, string> v3(v1);
	REQUIRE(stl::holds_alternative<string>(v3));
	REQUIRE(stl::get<string>(v3) == "Hello World");

	v1.emplace<string>("Hello World");
	CryVariant<int, string> v4(std::move(v1));
	REQUIRE(stl::holds_alternative<string>(v4));
	REQUIRE(stl::get<string>(v4) == "Hello World");
}

TEST(CryVariantTest, Swap)
{
	CryVariant<int, string> v1, v2;

	v1.emplace<int>(0);
	v2.emplace<int>(1);
	v1.swap(v2);
	REQUIRE(stl::holds_alternative<int>(v1));
	REQUIRE(stl::get<int>(v1) == 1);
	REQUIRE(stl::holds_alternative<int>(v2));
	REQUIRE(stl::get<int>(v2) == 0);

	v1.emplace<int>(0);
	v2.emplace<string>("Hello World");
	v1.swap(v2);
	REQUIRE(stl::holds_alternative<string>(v1));
	REQUIRE(stl::get<string>(v1) == "Hello World");
	REQUIRE(stl::holds_alternative<int>(v2));
	REQUIRE(stl::get<int>(v2) == 0);
}

TEST(CryVariantTest, Visit)
{
	auto visitor = [](CryVariant<int, string>& v)
	{
		if (stl::holds_alternative<int>(v))
			v.emplace<int>(stl::get<int>(v) * 2);
		else
			v.emplace<string>(stl::get<string>(v) + " " + stl::get<string>(v));
	};

	CryVariant<int, string> v1, v2;
	v1.emplace<int>(5);
	v2.emplace<string>("Hello World");

	stl::visit(visitor, v1);
	stl::visit(visitor, v2);
	REQUIRE(stl::get<int>(v1) == 10);
	REQUIRE(stl::get<string>(v2) == "Hello World Hello World");
}
