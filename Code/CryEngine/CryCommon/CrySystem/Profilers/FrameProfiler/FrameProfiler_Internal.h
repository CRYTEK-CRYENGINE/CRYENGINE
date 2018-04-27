// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Platform/platform.h>

#include "FrameProfiler_Shared.h"

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
// Profiling is enabled in every configuration except Release

// Internal Frame Profiler - currently only available in Profile builds
	#if ALLOW_FRAME_PROFILER

		#define INTERNAL_PROFILER_THREADNAME(szName) /*not implemented*/

		#define INTERNAL_PROFILER_FRAMESTART(szName) \
		  CBootProfileFrameScope bootProfilerFrameScope;

		#define INTERNAL_PROFILER_REGION(subsystem, szName)                                                              \
		  static CFrameProfiler staticFrameProfilerRegion(subsystem, EProfileDescription::REGION, szName, __FILE__, __LINE__); \
		  CFrameProfilerSection frameProfilerSectionRegion(&staticFrameProfilerRegion, szName, nullptr, EProfileDescription::REGION);

		#define INTERNAL_PROFILER_REGION_ARG(subsystem, szName, szArgument)                                              \
		  static CFrameProfiler staticFrameProfilerRegion(subsystem, EProfileDescription::REGION, szName, __FILE__, __LINE__); \
		  CFrameProfilerSection frameProfilerSectionRegion(&staticFrameProfilerRegion, szName, szArgument, EProfileDescription::REGION);

		#define INTERNAL_PROFILER_REGION_WAITING(subsystem, szName)                                                                                                            \
		  static CFrameProfiler staticFrameProfilerRegion(subsystem, (EProfileDescription)(EProfileDescription::REGION | EProfileDescription::WAITING), szName, __FILE__, __LINE__); \
		  CFrameProfilerSection frameProfilerSectionRegion(&staticFrameProfilerRegion, szName, nullptr, EProfileDescription::REGION);

		#define INTERNAL_PROFILER_FUNCTION(subsystem, szName)                                                                   \
		  static CFrameProfiler staticFrameProfiler(subsystem, EProfileDescription::FUNCTIONENTRY, szName, __FILE__, __LINE__); \
		  CFrameProfilerSection frameProfilerSection(&staticFrameProfiler, szName, nullptr, EProfileDescription::FUNCTIONENTRY);

		#define INTERNAL_PROFILER_FUNCTION_ARG(subsystem, szName, szArgument)                                                     \
		  static CFrameProfiler staticFrameProfiler(subsystem, EProfileDescription::FUNCTIONENTRY, __FUNC__, __FILE__, __LINE__); \
		  CFrameProfilerSection frameProfilerSection(&staticFrameProfiler, szName, szArgument, EProfileDescription::FUNCTIONENTRY);

		#define INTERNAL_PROFILER_FUNCTION_WAITING(subsystem, szName)                                                                                                                 \
		  static CFrameProfiler staticFrameProfiler(subsystem, (EProfileDescription)(EProfileDescription::FUNCTIONENTRY | EProfileDescription::WAITING), szName, __FILE__, __LINE__); \
		  CFrameProfilerSection frameProfilerSection(&staticFrameProfiler, szName, nullptr, EProfileDescription::FUNCTIONENTRY);

		#define INTERNAL_PROFILER_SECTION(subsystem, szName)                                                              \
		  static CFrameProfiler staticFrameProfiler(subsystem, EProfileDescription::SECTION, szName, __FILE__, __LINE__); \
		  CFrameProfilerSection frameProfilerSection(&staticFrameProfiler, szName, nullptr, EProfileDescription::SECTION);

		#define INTERNAL_PROFILER_SECTION_WAITING(subsystem, szName)                                                                                                            \
		  static CFrameProfiler staticFrameProfiler(subsystem, (EProfileDescription)(EProfileDescription::SECTION | EProfileDescription::WAITING), szName, __FILE__, __LINE__); \
		  CFrameProfilerSection frameProfilerSection(&staticFrameProfiler, szName, nullptr);

		#define INTERNAL_PROFILER_MARKER(szLabel) \
			{ if (gEnv && gEnv->bBootProfilerEnabledFrames) gEnv->pSystem->StartBootSectionProfiler(szLabel,nullptr,EProfileDescription::MARKER); };

		#define INTERNAL_PROFILER_PUSH(szLabel) \
			{ if (gEnv && gEnv->bBootProfilerEnabledFrames) gEnv->pSystem->StartBootSectionProfiler(szLabel,nullptr,EProfileDescription::PUSH_MARKER); };

		#define INTERNAL_PROFILER_POP(szLabel) \
			{ if (gEnv && gEnv->bBootProfilerEnabledFrames) gEnv->pSystem->StartBootSectionProfiler(szLabel,nullptr,EProfileDescription::POP_MARKER); };

	#else

		#define INTERNAL_PROFILER_THREADNAME(szName)                        /*do nothing*/
		#define INTERNAL_PROFILER_FRAMESTART(szName)                        /*do nothing*/
		#define INTERNAL_PROFILER_REGION(subsystem, szName)                 /*do nothing*/
		#define INTERNAL_PROFILER_REGION_ARG(subsystem, szName, szArgument) /*do nothing*/
		#define INTERNAL_PROFILER_REGION_WAITING(subsystem, szName)         /*do nothing*/
		#define INTERNAL_PROFILER_FUNCTION(subsystem, szName)               /*do nothing*/
		#define INTERNAL_PROFILER_FUNCTION_ARG(subsystem, szName, szArg)
		#define INTERNAL_PROFILER_FUNCTION_WAITING(subsystem, szName)       /*do nothing*/
		#define INTERNAL_PROFILER_SECTION(subsystem, szName)                /*do nothing*/
		#define INTERNAL_PROFILER_SECTION_WAITING(subsystem, szName)        /*do nothing*/
		#define INTERNAL_PROFILER_MARKER(szLabel)                           /*do nothing*/
		#define INTERNAL_PROFILER_PUSH(szLabel)                             /*do nothing*/
		#define INTERNAL_PROFILER_POP(szLabel)                              /*do nothing*/

	#endif

#else

	#define INTERNAL_PROFILER_THREADNAME(szName)                        /*do nothing*/
	#define INTERNAL_PROFILER_FRAMESTART(szName)                        /*do nothing*/
	#define INTERNAL_PROFILER_REGION(subsystem, szName)                 /*do nothing*/
	#define INTERNAL_PROFILER_REGION_ARG(subsystem, szName, szArgument) /*do nothing*/
	#define INTERNAL_PROFILER_REGION_WAITING(subsystem, szName)         /*do nothing*/
	#define INTERNAL_PROFILER_FUNCTION(subsystem, szName)               /*do nothing*/
	#define INTERNAL_PROFILER_FUNCTION_WAITING(subsystem, szName)       /*do nothing*/
	#define INTERNAL_PROFILER_SECTION(subsystem, szName)                /*do nothing*/
	#define INTERNAL_PROFILER_SECTION_WAITING(subsystem, szName)        /*do nothing*/
	#define INTERNAL_PROFILER_MARKER(szLabel)                           /*do nothing*/
	#define INTERNAL_PROFILER_PUSH(szLabel)                             /*do nothing*/
	#define INTERNAL_PROFILER_POP(szLabel)                              /*do nothing*/

#endif
