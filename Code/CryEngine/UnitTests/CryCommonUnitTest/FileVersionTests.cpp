// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <UnitTest.h>
#include <CrySystem/CryVersion.h>

TEST(CryFileVersionTest, Comparison)
{
	{
		SFileVersion v1;
		v1.Set("1.2.3.4");

		SFileVersion v2;
		v2.Set("1.2.3.5");

		SFileVersion v3;
		v3.Set("1.2.3.4");

		REQUIRE(v1 < v2);
		REQUIRE(!(v1 > v2));
		REQUIRE(v2 > v1);
		REQUIRE(!(v2 < v1));

		REQUIRE(v1 <= v2);
		REQUIRE(!(v1 >= v2));
		REQUIRE(v2 >= v1);
		REQUIRE(!(v2 <= v1));

		REQUIRE(!(v1 == v2));
		REQUIRE(v1 == v3);
		REQUIRE(!(v1 < v3));
		REQUIRE(!(v1 > v3));
	}

	{
		SFileVersion v1;
		v1.Set("1.2.3.4");

		SFileVersion v2;
		v2.Set("1.2.4.4");

		SFileVersion v3;
		v3.Set("1.2.3.4");

		REQUIRE(!(v1 == v2));
		REQUIRE(v1 == v3);

		REQUIRE(v1 < v2);
		REQUIRE(!(v1 > v2));
		REQUIRE(v2 > v1);
		REQUIRE(!(v2 < v1));

		REQUIRE(v1 <= v2);
		REQUIRE(!(v1 >= v2));
		REQUIRE(v2 >= v1);
		REQUIRE(!(v2 <= v1));
	}

	{
		SFileVersion v1;
		v1.Set("1.2.3.4");

		SFileVersion v2;
		v2.Set("1.3.3.4");

		SFileVersion v3;
		v3.Set("1.2.3.4");

		REQUIRE(!(v1 == v2));
		REQUIRE(v1 == v3);

		REQUIRE(v1 < v2);
		REQUIRE(!(v1 > v2));
		REQUIRE(v2 > v1);
		REQUIRE(!(v2 < v1));

		REQUIRE(v1 <= v2);
		REQUIRE(!(v1 >= v2));
		REQUIRE(v2 >= v1);
		REQUIRE(!(v2 <= v1));
	}

	{
		SFileVersion v1;
		v1.Set("1.2.3.4");

		SFileVersion v2;
		v2.Set("2.2.3.4");

		SFileVersion v3;
		v3.Set("1.2.3.4");

		REQUIRE(!(v1 == v2));
		REQUIRE(v1 == v3);

		REQUIRE(v1 < v2);
		REQUIRE(!(v1 > v2));
		REQUIRE(v2 > v1);
		REQUIRE(!(v2 < v1));

		REQUIRE(v1 <= v2);
		REQUIRE(!(v1 >= v2));
		REQUIRE(v2 >= v1);
		REQUIRE(!(v2 <= v1));
	}
}

TEST(CryFileVersionTest, Set)
{
	{
		SFileVersion v1;
		v1.Set("1.2.3.4");

		REQUIRE(v1[0] == 4);
		REQUIRE(v1[1] == 3);
		REQUIRE(v1[2] == 2);
		REQUIRE(v1[3] == 1);
	}

	{
		SFileVersion v2(1, 2, 3, 4);

		REQUIRE(v2[0] == 4);
		REQUIRE(v2[1] == 3);
		REQUIRE(v2[2] == 2);
		REQUIRE(v2[3] == 1);
	}

	{
		SFileVersion v1;
		v1.Set("Lorem ipsum dolor sit amet");

		REQUIRE(v1[0] == 0);
		REQUIRE(v1[1] == 0);
		REQUIRE(v1[2] == 0);
		REQUIRE(v1[3] == 0);
	}
}

TEST(CryFileVersionTest, ToString)
{
	const char* szVersion = "1.2.3.4";
	const char* szShortVersion = "2.3.4";

	SFileVersion v1;
	v1.Set(szVersion);

	char buff[32];
	memset(buff, 0, sizeof(buff));

	v1.ToString(buff);
	REQUIRE(strcmp(buff, szVersion) == 0);

	memset(buff, 0, sizeof(buff));

	v1.ToShortString(buff);
	REQUIRE(strcmp(buff, szShortVersion) == 0);

	auto str = v1.ToString();
	REQUIRE(str == szVersion);
}
