// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.
#include <CryCore/Platform/platform.h>
#include <CryCore/Platform/platform_impl.inl>
#include <CryCore/Platform/CryPlatformDefines.h>
#include "UnitTest.h"

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX

GTEST_API_ int main(int argc, char **argv) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

#endif // CRY_PLATFORM_WINDOWS
