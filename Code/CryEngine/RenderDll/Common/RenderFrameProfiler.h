// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef _SIMPLE_FRAME_PROFILER_
#define _SIMPLE_FRAME_PROFILER_

#include <CrySystem/Profilers/FrameProfiler/FrameProfiler.h>

#define PP_CONCAT2(A, B) A ## B
#define PP_CONCAT(A, B)  PP_CONCAT2(A, B)

// set #if 0 here if you don't want profiling to be compiled in the code
#ifdef ENABLE_FRAME_PROFILER
	#define PROFILE_FRAME(id) FRAME_PROFILER("Renderer:" # id, iSystem, PROFILE_RENDERER)
#else
	#define PROFILE_FRAME(id)
#endif

#if defined(ENABLE_FRAME_PROFILER_LABELS)

	// macros to implement the platform differences for pushing GPU Markers
	#if CRY_RENDERER_GNM
		#define PROFILE_LABEL_GPU(_NAME)      do { GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->SetProfilerMarker (_NAME); } while (0)
		#define PROFILE_LABEL_PUSH_GPU(_NAME) do { GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->BeginProfilerEvent(_NAME); } while (0)
		#define PROFILE_LABEL_POP_GPU(_NAME)  do { GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->EndProfilerEvent  (_NAME); } while (0)
	#elif CRY_PLATFORM_ORBIS
		#define PROFILE_LABEL_GPU(X)          do { DXOrbis::Device()->SetMarker(X); } while (0)
		#define PROFILE_LABEL_PUSH_GPU(X)     do { DXOrbis::Device()->PushMarker(X); } while (0)
		#define PROFILE_LABEL_POP_GPU(X)      do { DXOrbis::Device()->PopMarker(X); } while (0)
	#elif CRY_RENDERER_OPENGL || CRY_RENDERER_OPENGLES
		#define PROFILE_LABEL_GPU(_NAME)      DXGLProfileLabel(_NAME);
		#define PROFILE_LABEL_PUSH_GPU(_NAME) DXGLProfileLabelPush(_NAME);
		#define PROFILE_LABEL_POP_GPU(_NAME)  DXGLProfileLabelPop(_NAME);
	#elif(CRY_RENDERER_VULKAN >= 10)
		#define PROFILE_LABEL_GPU(_NAME)      do { GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->SetProfilerMarker (_NAME); } while (0)
		#define PROFILE_LABEL_PUSH_GPU(_NAME) do { GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->BeginProfilerEvent(_NAME); } while (0)
		#define PROFILE_LABEL_POP_GPU(_NAME)  do { GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->EndProfilerEvent  (_NAME); } while (0)
	#elif (CRY_RENDERER_DIRECT3D >= 120)
		#define PROFILE_LABEL_GPU(     _NAME) do { GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->SetProfilerMarker (_NAME); } while (0)
		#define PROFILE_LABEL_PUSH_GPU(_NAME) do { GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->BeginProfilerEvent(_NAME); } while (0)
		#define PROFILE_LABEL_POP_GPU( _NAME) do { GetDeviceObjectFactory().GetCoreCommandList().GetGraphicsInterface()->EndProfilerEvent  (_NAME); } while (0)
	#elif (CRY_RENDERER_DIRECT3D >= 110) && (CRY_RENDERER_DIRECT3D < 120) && CRY_PLATFORM_DURANGO
		#ifdef USE_PIX_DURANGO
			#include <CryString/UnicodeFunctions.h>
			#define PROFILE_LABEL_GPU(X)      do { if (gcpRendD3D->GetPixProfiler() && gcpRendD3D->CV_r_durango_async_dips == 0) { wchar_t buf[256]; Unicode::Convert(buf, X); gcpRendD3D->GetPixProfiler()->SetMarker(buf); } } while (0)
			#define PROFILE_LABEL_PUSH_GPU(X) do { if (gcpRendD3D->GetPixProfiler() && gcpRendD3D->CV_r_durango_async_dips == 0) { wchar_t buf[256]; Unicode::Convert(buf, X); gcpRendD3D->GetPixProfiler()->BeginEvent(buf); } } while (0)
			#define PROFILE_LABEL_POP_GPU(X)  do { if (gcpRendD3D->GetPixProfiler() && gcpRendD3D->CV_r_durango_async_dips == 0) gcpRendD3D->GetPixProfiler()->EndEvent(); } while (0)
		#else
			#define PROFILE_LABEL_GPU(X)      do { /*PIX not enabled*/ } while (0)
			#define PROFILE_LABEL_PUSH_GPU(X) do { /*PIX not enabled*/ } while (0)
			#define PROFILE_LABEL_POP_GPU(X)  do { /*PIX not enabled*/ } while (0)
		#endif
	#elif  CRY_PLATFORM_WINDOWS
		// TODO: replace by ID3DUserDefinedAnnotation https://msdn.microsoft.com/en-us/library/hh446881.aspx
		#define PROFILE_LABEL_GPU(X)      do { wchar_t buf[256]; Unicode::Convert(buf, X); D3DPERF_SetMarker(0xffffffff, buf); } while (0)
		#define PROFILE_LABEL_PUSH_GPU(X) do { wchar_t buf[128]; Unicode::Convert(buf, X); D3DPERF_BeginEvent(0xff00ff00, buf); } while (0)
		#define PROFILE_LABEL_POP_GPU(X)  do { D3DPERF_EndEvent(); } while (0)
	#else
		#error "Profiling labels not supported on this api/platform"
	#endif

	// real push/pop marker for GPU, also add to the internal profiler and CPU Markers
	#define PROFILE_LABEL(X)      do { PLATFORM_PROFILER_MARKER(X); PROFILE_LABEL_GPU(X); } while (0)
	#define PROFILE_LABEL_PUSH(X) do { CryProfile::PushProfilingMarker(EProfileDescription::PUSHPOP, X); PLATFORM_PROFILER_PUSH(X); PROFILE_LABEL_PUSH_GPU(X); if (gcpRendD3D->m_pPipelineProfiler) gcpRendD3D->m_pPipelineProfiler->BeginSection(X); } while (0)
	#define PROFILE_LABEL_POP(X)  do { CryProfile::PopProfilingMarker(); PLATFORM_PROFILER_POP(); PROFILE_LABEL_POP_GPU(X); if (gcpRendD3D->m_pPipelineProfiler) gcpRendD3D->m_pPipelineProfiler->EndSection(X); } while (0)

	// scope util class for GPU profiling Marker
	#define PROFILE_LABEL_SCOPE(X)                             \
	  class CProfileLabelScope                                 \
	  {                                                        \
	    const char* m_label;                                   \
	  public:                                                  \
	    CProfileLabelScope(const char* label) : m_label(label) \
	    { PROFILE_LABEL_PUSH(label); }                         \
	    ~CProfileLabelScope()                                  \
	    { PROFILE_LABEL_POP(m_label); }                        \
	  } PP_CONCAT(profileLabelScope, __LINE__)(X);             \
	  LOADING_TIME_PROFILE_SECTION_NAMED(X);

#else
// NULL implementation
	#define PROFILE_LABEL(X)
	#define PROFILE_LABEL_PUSH(X)
	#define PROFILE_LABEL_POP(X)
	#define PROFILE_LABEL_SCOPE(X)
#endif

#define PROFILE_LABEL_SHADER(X) PROFILE_LABEL(X)

#if defined(ENABLE_FRAME_PROFILER) && !defined(_RELEASE)
	#define FUNCTION_PROFILER_RENDER_FLAT FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER)
#else
	#define FUNCTION_PROFILER_RENDER_FLAT
#endif

#endif
