// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include <UnitTest.h>

#include <CryMemory/CrySizer.h>
#include <XML/xml.h>

TEST(CryCXmlNodeTest, Construction)
{
	CXmlNode n;
	REQUIRE(n.GetRefCount() == 0);
}

TEST(CryCXmlNodeTest, Construction2)
{
	CXmlNode n{"???", true};
	REQUIRE(n.GetRefCount() == 0);
}

