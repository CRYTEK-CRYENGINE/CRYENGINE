// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#define eCryModule eCryM_Schematyc2

#include <CryCore/Project/CryModuleDefs.h>
#include <CryCore/Platform/platform.h>

#include <CrySystem/IConsole.h>
#include <CrySystem/ISystem.h>
#include <CryString/CryString.h>

#include <CrySchematyc2/Prerequisites.h>

#define CRY_STRING

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#ifdef GetObject
#undef GetObject
#endif
