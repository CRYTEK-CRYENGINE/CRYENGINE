// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Project/CryModuleDefs.h>
#define eCryModule eCryM_Animation

#define CRYANIMATION_EXPORTS
//#define NOT_USE_CRY_MEMORY_MANAGER
//#define DEFINE_MODULE_NAME "CryAnimation"

//! Include standard headers.
#include <CryCore/Platform/platform.h>

#if defined(USER_ivof) || defined(USER_ivo) || defined(USER_sven)
	#define ASSERT_ON_ASSET_CHECKS
#endif

#ifdef ASSERT_ON_ASSET_CHECKS
	#define ANIM_ASSET_ASSERT_TRACE(condition, parenthese_message) CRY_ASSERT_TRACE(condition, parenthese_message)
	#define ANIM_ASSET_ASSERT(condition)                           CRY_ASSERT(condition)
#else
	#define ANIM_ASSET_ASSERT_TRACE(condition, parenthese_message)
	#define ANIM_ASSET_ASSERT(condition)
#endif

#define LOG_ASSET_PROBLEM(condition, parenthese_message)                \
  do                                                                    \
  {                                                                     \
    gEnv->pLog->LogWarning("Anim asset error: %s failed", # condition); \
    gEnv->pLog->LogWarning parenthese_message;                          \
  } while (0)

#define ANIM_ASSET_CHECK_TRACE(condition, parenthese_message)     \
  do                                                              \
  {                                                               \
    bool anim_asset_ok = (condition);                             \
    if (!anim_asset_ok)                                           \
    {                                                             \
      ANIM_ASSET_ASSERT_TRACE(anim_asset_ok, parenthese_message); \
      LOG_ASSET_PROBLEM(condition, parenthese_message);           \
    }                                                             \
  } while (0)

#define ANIM_ASSET_CHECK_MESSAGE(condition, message) ANIM_ASSET_CHECK_TRACE(condition, (message))
#define ANIM_ASSET_CHECK(condition)                  ANIM_ASSET_CHECK_MESSAGE(condition, NULL)

//! Include main interfaces.
#include <CryMath/Cry_Math.h>

#include <CrySystem/ISystem.h>
#include <CrySystem/ITimer.h>
#include <CrySystem/ILog.h>
#include <CrySystem/IConsole.h>
#include <CrySystem/File/ICryPak.h>
#include <CryCore/StlUtils.h>

#include <CryAnimation/ICryAnimation.h>
#include "AnimationBase.h"
#include <CrySystem/Profilers/FrameProfiler/FrameProfiler_JobSystem.h>  // to be removed
