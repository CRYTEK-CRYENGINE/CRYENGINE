// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Platform/platform.h>

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
// Profiling is enabled in every configuration except Release

// Brofiler
	#if ALLOW_BROFILER

		#include <Cry_Brofiler.h>

		#define BROFILER_THREADNAME(szName) BROFILER_THREAD(szName)
		#define BROFILER_FRAMESTART(szName) BROFILER_FRAME(szName)

		#if defined(PERFORMANCE_BUILD)
			#define BROFILER_REGION(szName)           BROFILER_EVENT(szName)
			#define BROFILER_REGION_WAITING(szName)   BROFILER_EVENT(szName)
			#define BROFILER_FUNCTION(szName)         BROFILER_EVENT(szName)
			#define BROFILER_FUNCTION_WAITING(szName) BROFILER_EVENT(szName)
			#define BROFILER_SECTION(szName)          BROFILER_EVENT(szName)
			#define BROFILER_SECTION_WAITING(szName)  BROFILER_EVENT(szName)
			#define BROFILER_MARKER(szName)           /*not implemented*/
			#define BROFILER_PUSH(szName)             /*not implemented*/
			#define BROFILER_POP()                    /*not implemented*/
		#else                                       // profile builds take another path to allow runtime switch
			#define BROFILER_REGION(szName)           /*do nothing*/
			#define BROFILER_REGION_WAITING(szName)   /*do nothing*/
			#define BROFILER_FUNCTION(szName)         /*do nothing*/
			#define BROFILER_FUNCTION_WAITING(szName) /*do nothing*/
			#define BROFILER_SECTION(szName)          /*do nothing*/
			#define BROFILER_SECTION_WAITING(szName)  /*do nothing*/
			#define BROFILER_MARKER(szName)           /*do nothing*/
			#define BROFILER_PUSH(szName)             /*do nothing*/
			#define BROFILER_POP()                    /*do nothing*/
		#endif

	#else

		#define BROFILER_THREADNAME(szName)       /*do nothing*/
		#define BROFILER_FRAMESTART(szName)       /*do nothing*/
		#define BROFILER_REGION(szName)           /*do nothing*/
		#define BROFILER_REGION_WAITING(szName)   /*do nothing*/
		#define BROFILER_FUNCTION(szName)         /*do nothing*/
		#define BROFILER_FUNCTION_WAITING(szName) /*do nothing*/
		#define BROFILER_SECTION(szName)          /*do nothing*/
		#define BROFILER_SECTION_WAITING(szName)  /*do nothing*/
		#define BROFILER_MARKER(szName)           /*do nothing*/
		#define BROFILER_PUSH(szName)             /*do nothing*/
		#define BROFILER_POP()                    /*do nothing*/

	#endif

#else

	#define BROFILER_THREADNAME(szName)       /*do nothing*/
	#define BROFILER_FRAMESTART(szName)       /*do nothing*/
	#define BROFILER_REGION(szName)           /*do nothing*/
	#define BROFILER_REGION_WAITING(szName)   /*do nothing*/
	#define BROFILER_FUNCTION(szName)         /*do nothing*/
	#define BROFILER_FUNCTION_WAITING(szName) /*do nothing*/
	#define BROFILER_SECTION(szName)          /*do nothing*/
	#define BROFILER_SECTION_WAITING(szName)  /*do nothing*/
	#define BROFILER_MARKER(szName)           /*do nothing*/
	#define BROFILER_PUSH(szName)             /*do nothing*/
	#define BROFILER_POP()                    /*do nothing*/

#endif
