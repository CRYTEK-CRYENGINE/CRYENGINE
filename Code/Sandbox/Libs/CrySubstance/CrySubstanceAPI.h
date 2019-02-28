// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#ifndef CRY_SUBSTANCE_STATIC
#if defined(CrySubstance_EXPORTS)
#define CRY_SUBSTANCE_API __declspec(dllexport)
#else
#define CRY_SUBSTANCE_API __declspec(dllimport)
#endif
#else
#define CRY_SUBSTANCE_API
#endif

