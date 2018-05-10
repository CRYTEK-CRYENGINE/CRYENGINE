// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#ifndef QTWM_DLL
#define QTWM_DLL
#endif // ! QTWM_DLL
#if defined(_WIN32) && defined(QTWM_DLL)
#ifdef CryQt_EXPORTS
#define CRYQT_API __declspec(dllexport)
#else
#define CRYQT_API __declspec(dllimport)
#endif
#else
#define CRYQT_API
#endif

