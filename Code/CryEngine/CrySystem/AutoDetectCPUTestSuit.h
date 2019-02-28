// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _AUTODETECT_CPU_TESTSUITE_
#define _AUTODETECT_CPU_TESTSUITE_

#pragma once

#if CRY_PLATFORM_WINDOWS

class CCPUTestSuite
{
public:
	int RunTest();
};

#endif // #if CRY_PLATFORM_WINDOWS

#endif // #ifndef _AUTODETECT_CPU_TESTSUITE_
