// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _COMMON_H_
#define _COMMON_H_

#pragma once

#if defined(_MSC_VER)
#	if !defined(_WIN32_WINNT)
#		define _WIN32_WINNT 0x0501
#	endif
#include <Windows.h>
#endif

#endif // #ifndef _COMMON_H_