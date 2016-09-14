// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Project/CryModuleDefs.h>
#define eCryModule eCryM_Render
#include <CryCore/Platform/platform.h>
#include <CryMemory/CrySizer.h>

#if !defined(_RELEASE) // DO_RENDERLOG cannot be compiled in release configs for consoles
	#define DO_RENDERLOG 1
#endif

//defined in DX9, but not DX10
#if CRY_PLATFORM_DURANGO || defined(OPENGL)
	#define D3D_OK S_OK
#endif

#if !defined(_RELEASE)
	#define RENDERER_ENABLE_BREAK_ON_ERROR 0
#endif
#if !defined(RENDERER_ENABLE_BREAK_ON_ERROR)
	#define RENDERER_ENABLE_BREAK_ON_ERROR 0
#endif
#if RENDERER_ENABLE_BREAK_ON_ERROR
	#include <winerror.h>
namespace detail
{
const char* ToString(long const hr);
bool        CheckHResult(long const hr, bool breakOnError, const char* file, const int line);
}
//# undef FAILED
//# define FAILED(x) (detail::CheckHResult((x), false, __FILE__, __LINE__))
	#define CHECK_HRESULT(x) (detail::CheckHResult((x), true, __FILE__, __LINE__))
#else
	#define CHECK_HRESULT(x) (!FAILED(x))
#endif

// BUFFER_ENABLE_DIRECT_ACCESS
// stores pointers to actual backing storage of vertex buffers. Can only be used on architectures
// that have a unified memory architecture and further guarantee that buffer storage does not change
// on repeated accesses.
// NOTE: DX12 doesn't support non-direct buffer access, because the otherwise used MAP_DISCARD
// implementation doesn't track if to-be-discarded resources have open views, this is a design decision
#if (CRY_PLATFORM_CONSOLE || defined(CRY_USE_DX12))
	#define BUFFER_ENABLE_DIRECT_ACCESS 1
#endif

// BUFFER_USE_STAGED_UPDATES
// On platforms that support staging buffers, special buffers are allocated that act as a staging area
// for updating buffer contents on the fly.
#if !(CRY_PLATFORM_CONSOLE)
	#define BUFFER_USE_STAGED_UPDATES 1
#else
	#define BUFFER_USE_STAGED_UPDATES 0
#endif

// BUFFER_SUPPORT_TRANSIENT_POOLS
// On d3d11 we want to separate the fire-and-forget allocations from the longer lived dynamic ones
#if CRY_PLATFORM_DESKTOP
	#define BUFFER_SUPPORT_TRANSIENT_POOLS 1
#else
	#define BUFFER_SUPPORT_TRANSIENT_POOLS 0
#endif

#ifndef BUFFER_ENABLE_DIRECT_ACCESS
	#define BUFFER_ENABLE_DIRECT_ACCESS 0
#endif

// CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS
// Enable if we have direct access to video memory and the device manager
// should manage constant buffers
#if BUFFER_ENABLE_DIRECT_ACCESS == 1
	#if CRY_PLATFORM_DURANGO || defined(CRY_USE_DX12) || defined(CRY_USE_GNM_RENDERER)
		#define CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS 1
	#else
		#define CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS 0
	#endif
#else
	#define CONSTANT_BUFFER_ENABLE_DIRECT_ACCESS 0
#endif

// enable support for D3D11.1 features if the platform supports it
#if CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS || defined(CRY_USE_DX12)
	#define DEVICE_SUPPORTS_D3D11_1
#endif

#if CRY_PLATFORM_DURANGO && !defined(CRY_USE_DX12)
	#define DEVICE_SUPPORTS_PERFORMANCE_DEVICE
#endif

#if CRY_PLATFORM_DURANGO
	#define DURANGO_USE_ESRAM
#endif

//#define DEFINE_MODULE_NAME "CryRender9"

#ifdef _DEBUG
	#define CRTDBG_MAP_ALLOC
#endif //_DEBUG

#undef USE_STATIC_NAME_TABLE
#define USE_STATIC_NAME_TABLE

#if !defined(_RELEASE)
	#define ENABLE_FRAME_PROFILER
#endif

#if !defined(_RELEASE) || defined(PERFORMANCE_BUILD)
	#define ENABLE_SIMPLE_GPU_TIMERS
	#define ENABLE_FRAME_PROFILER_LABELS
#endif

#ifdef ENABLE_FRAME_PROFILER
	#define PROFILE 1
#endif

#define FUNCTION_PROFILER_RENDERER FUNCTION_PROFILER(iSystem, PROFILE_RENDERER)

#define SCOPED_RENDERER_ALLOCATION_NAME_HINT(str)

#define SHADER_ASYNC_COMPILATION

#if !defined(_RELEASE)
//# define DETAILED_PROFILING_MARKERS
#endif
#if defined(DETAILED_PROFILING_MARKERS)
	#define DETAILED_PROFILE_MARKER(x) DETAILED_PROFILE_MARKER((x))
#else
	#define DETAILED_PROFILE_MARKER(x) (void)0
#endif

#define RAIN_OCC_MAP_SIZE 256

#if CRY_PLATFORM_ORBIS
	#define USE_SCUE
	#ifdef USE_SCUE
//#  define GNM_COMPATIBILITY_MODE						// Turn this on to use GNM validation / Razor GPU captures
		#ifndef GNM_COMPATIBILITY_MODE
			#define CUSTOM_FETCH_SHADERS // InputLayouts generate fetch shader code instead of being generated when vertex shader created
		#endif
//#  define ENABLE_SCUE_VALIDATION	        // Checks for NULL bindings + incorrect bindings
//#  define GPU_MEMORY_MAPPING_VALIDATION   // Checks that the objects being bound are mapped in GPU visible memory (Slow)
	#else
	#endif
	#define CUE_SUPPORTS_GEOMETRY_SHADERS // Define if you want to use geometry shaders

	#define ORBIS_RENDERER_SUPPORT_JPG
#endif

#ifdef ENABLE_SCUE_VALIDATION
enum EVerifyType
{
	eVerify_Normal,
	eVerify_ConstantBuffer,
	eVerify_VertexBuffer,
	eVerify_SRVTexture,
	eVerify_SRVBuffer,
	eVerify_UAVTexture,
	eVerify_UAVBuffer,
};
#endif

#if CRY_PLATFORM_SSE2 && CRY_COMPILER_MSVC
	#include <fvec.h>
	#include <CryCore/Assert/CryAssert.h> // to restore assert macro which was changed by <fvec.h>
	#define CONST_INT32_PS(N, V3, V2, V1, V0)                                   \
	  const _MM_ALIGN16 int _ ## N[] = { V0, V1, V2, V3 };   /*little endian!*/ \
	  const F32vec4 N = _mm_load_ps((float*)_ ## N);
#endif

// enable support for baked meshes and decals on PC
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE || CRY_PLATFORM_DURANGO
	#define TEXTURE_GET_SYSTEM_COPY_SUPPORT
#endif

#if BUFFER_ENABLE_DIRECT_ACCESS && CRY_PLATFORM_WINDOWS && !defined(CRY_USE_DX12)
	#error BUFFER_ENABLE_DIRECT_ACCESS is not supported on windows pre DX12 platforms
#endif

#define MAX_REND_RECURSION_LEVELS 2

#if defined(OPENGL)
	#define CRY_OPENGL_ADAPT_CLIP_SPACE   1
	#define CRY_OPENGL_FLIP_Y             1
	#define CRY_OPENGL_MODIFY_PROJECTIONS !CRY_OPENGL_ADAPT_CLIP_SPACE
	#define CRY_OPENGL_SINGLE_CONTEXT     1
#endif

#ifdef STRIP_RENDER_THREAD
	#define m_nCurThreadFill    0
	#define m_nCurThreadProcess 0
#endif

#ifdef STRIP_RENDER_THREAD
	#define ASSERT_IS_RENDER_THREAD(rt)
	#define ASSERT_IS_MAIN_THREAD(rt)
#else
	#define ASSERT_IS_RENDER_THREAD(rt)         assert((rt)->IsRenderThread());
	#define ASSERT_IS_MAIN_THREAD(rt)           assert((rt)->IsMainThread());
	#define ASSERT_IS_MAIN_OR_RENDER_THREAD(rt) assert((rt)->IsMainThread() || (rt)->IsRenderThread());
#endif

//#define ASSERT_IN_SHADER( expr ) assert( expr );
#define ASSERT_IN_SHADER(expr)

// TODO: linux clang doesn't behave like orbis clang and linux gcc doesn't behave like darwin gcc, all linux instanced can't manage squish-template overloading
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_ORBIS || CRY_PLATFORM_APPLE
	#define EXCLUDE_SQUISH_SDK
#endif

#define _USE_MATH_DEFINES
#include <math.h>

// nv API
#if CRY_PLATFORM_WINDOWS && !defined(EXCLUDE_NV_API) && !defined(OPENGL) && !defined(CRY_USE_DX12)
	#define USE_NV_API 1
#endif

// AMD EXT
#if CRY_PLATFORM_WINDOWS && !defined(EXCLUDE_AMD_API) && !defined(OPENGL) && !defined(CRY_USE_DX12)
	#define USE_AMD_EXT 1
#endif
 
// windows desktop API available for usage
#if CRY_PLATFORM_WINDOWS
	#define WINDOWS_DESKTOP_API
#endif

#if CRY_PLATFORM_WINDOWS && !defined(OPENGL)
	#define LEGACY_D3D9_INCLUDE
#endif

#if CRY_PLATFORM_DURANGO && (!defined(RELEASE) || defined(ENABLE_PROFILING_CODE))
	#define USE_PIX_DURANGO
#endif

#if defined(CRY_USE_DX12)
	#define CRY_USE_DX12_DEVIRTUALIZED
#endif

#if defined(DEVICE_SUPPORTS_D3D11_1) && !defined(CRY_PLATFORM_ORBIS)
	#include <CryCore/Platform/CryWindows.h>
	#ifdef CRY_PLATFORM_DURANGO
		#include <d3d11_x.h>
		#include "DX12\Includes\d3d11_empty.h"
	#else
		#include "D3D11_1.h"
	#endif

	#if defined(CRY_USE_DX12_DEVIRTUALIZED) //including system headers before redefining D3D interfaces
		#include "DX12\Includes\d3d11_empty.h"
	#endif
#endif

//////////////////////////////////////////////////////////////////////////
#if defined(CRY_USE_GNM_RENDERER)
	#include "XRenderD3D9/GNM/GnmBase.hpp"
#elif defined(CRY_USE_GNM)
	#include "XRenderD3D9/DXOrbis/CCryDXOrbisMisc.hpp"
	#include "XRenderD3D9/DXOrbis/CCryDXOrbisRenderer.hpp"
	#include "XRenderD3D9/DXOrbis/DXOrbisGI/CCryDXOrbisGI.hpp"
#elif defined(OPENGL)
	#include <CryCore/Platform/CryLibrary.h>
	#include <CryCore/Platform/CryWindows.h>
	#include "XRenderD3D9/DXGL/CryDXGL.hpp"
	#if CRY_PLATFORM_WINDOWS
		typedef uintptr_t SOCKET;   // ../Common/Shaders/RemoteCompiler.h
	#endif
#elif !defined(CRY_USE_DX12)
	#include <CryCore/Platform/CryWindows.h>
	#if CRY_PLATFORM_DURANGO
		#if defined(ENABLE_PROFILING_CODE)
			#define USE_INSTRUMENTED_LIBS
		#endif

		#define DURANGO_MONOD3D_DRIVER
		#if defined(DURANGO_MONOD3D_DRIVER)
			#include "d3d11_x.h" // includes <windows.h>
			LINK_SYSTEM_LIBRARY("d3d11_x.lib")
		#else
			#include "D3D11_1.h" // includes <windows.h>
			#if defined(USE_INSTRUMENTED_LIBS)
				LINK_SYSTEM_LIBRARY("d3d11i.lib")
			#else
				LINK_SYSTEM_LIBRARY("d3d11.lib")
			#endif
		#endif
	#else
		#include "d3d11.h" // includes <windows.h>
	#endif

	#if defined(LEGACY_D3D9_INCLUDE)
		#include <CryCore/Platform/CryWindows.h>
		#include "d3d9.h" // includes <windows.h>
	#endif
#endif

//////////////////////////////////////////////////////////////////////////
//#define Direct3D IDXGIAdapter

#if defined(CRY_USE_DX12)
    #if CRY_PLATFORM_DURANGO
        #define DXGIFactory               IDXGIFactory1
        #define DXGIDevice                IDXGIDevice2
        #define DXGIAdapter               IDXGIAdapter2
        #define DXGIOutput                IDXGIOutput
        #define DXGISwapChain             IDXGISwapChain1
        typedef char DXGI_COLOR_SPACE_TYPE;
        typedef char DXGI_MATRIX_3X2_F;
    #else
        #define DXGIFactory               IDXGIFactory4
        #define DXGIDevice                IDXGIDevice3
        #define DXGIAdapter               IDXGIAdapter3
        #define DXGIOutput                IDXGIOutput4
        #define DXGISwapChain             IDXGISwapChain3
        #define	IID_GRAPHICS_PPV_ARGS IID_PPV_ARGS
    #endif

    #if defined(DEVICE_SUPPORTS_D3D11_1)
        #define D3DDeviceContext          ID3D11DeviceContext1
        #define D3DDevice                 ID3D11Device1
    #else
        #define D3DDeviceContext          ID3D11DeviceContext
        #define D3DDevice                 ID3D11Device
    #endif

#elif defined(DEVICE_SUPPORTS_D3D11_1) && !CRY_PLATFORM_ORBIS
    #define     DXGIFactory               IDXGIFactory2
    #define     DXGIDevice                IDXGIDevice1
    #define     DXGIAdapter               IDXGIAdapter1
    #define     DXGIOutput                IDXGIOutput1
    #define     DXGISwapChain             IDXGISwapChain1

    #define     D3DDeviceContext          ID3D11DeviceContext1
    #define     D3DDevice                 ID3D11Device1
#else
    #define     DXGIFactory               IDXGIFactory1
    #define     DXGIDevice                IDXGIDevice1
    #define     DXGIAdapter               IDXGIAdapter1
    #define     DXGIOutput                IDXGIOutput
    #define     DXGISwapChain             IDXGISwapChain

    #define     D3DDeviceContext          ID3D11DeviceContext
    #define     D3DDevice                 ID3D11Device
#endif

#define         D3DVertexDeclaration      ID3D11InputLayout
#define         D3DVertexShader           ID3D11VertexShader
#define         D3DPixelShader            ID3D11PixelShader
#define         D3DResource               ID3D11Resource

#if defined(CRY_USE_GNM_RENDERER)
    #define     D3DBaseTexture            ID3D11BaseTexture
#else
    #define     D3DBaseTexture            ID3D11Resource
#endif

#define         D3DLookupTexture          ID3D11Texture1D
#define         D3DTexture                ID3D11Texture2D
#define         D3DVolumeTexture          ID3D11Texture3D
#define         D3DCubeTexture            ID3D11Texture2D
#define         D3DVertexBuffer           ID3D11Buffer
#define         D3DShaderResource         ID3D11ShaderResourceView
#define         D3DUAV                    ID3D11UnorderedAccessView
#define         D3DIndexBuffer            ID3D11Buffer
#define         D3DBuffer                 ID3D11Buffer
#define         D3DSurface                ID3D11RenderTargetView
#define         D3DDepthSurface           ID3D11DepthStencilView
#define         D3DBaseView               ID3D11View
#define         D3DQuery                  ID3D11Query
#define         D3DViewPort               D3D11_VIEWPORT
#define         D3DRectangle              D3D11_RECT
#define         D3DFormat                 DXGI_FORMAT
#define         D3DPrimitiveType          D3D11_PRIMITIVE_TOPOLOGY
#define         D3DBlob                   ID3D10Blob
#define         D3DSamplerState           ID3D11SamplerState

#if defined(CRY_USE_DX12_DEVIRTUALIZED)
    #define     ID3D11DeviceContext1      CCryDX12DeviceContext
    #define     ID3D11Device1             CCryDX12Device
    #define     ID3D11DeviceContext       CCryDX12DeviceContext
    #define     ID3D11Device              CCryDX12Device
    #define     ID3D11DeviceChild         CCryDX12DeviceChild<IEmptyDeviceChild>
    #define     ID3D11BlendState          CCryDX12BlendState
    #define     ID3D11DepthStencilState   CCryDX12DepthStencilState
    #define     ID3D11RasterizerState     CCryDX12RasterizerState
    #define     ID3D11SamplerState        CCryDX12SamplerState
    #define     ID3D11View                CCryDX12View<IEmptyView>
    #define     ID3D11DepthStencilView    CCryDX12DepthStencilView
    #define     ID3D11RenderTargetView    CCryDX12RenderTargetView
    #define     ID3D11ShaderResourceView  CCryDX12ShaderResourceView
    #define     ID3D11UnorderedAccessView CCryDX12UnorderedAccessView
    #define     ID3D11Resource            CCryDX12Resource<IEmptyResource>
    #define     ID3D11Buffer              CCryDX12Buffer
    #define     ID3D11Texture1D           CCryDX12Texture1D
    #define     ID3D11Texture2D           CCryDX12Texture2D
    #define     ID3D11Texture3D           CCryDX12Texture3D
    #define     ID3D11Asynchronous        CCryDX12Asynchronous<IEmptyAsynchronous>
    #define     ID3D11Query               CCryDX12Query
    #define     ID3D11InputLayout         CCryDX12InputLayout

    typedef     IEmptyDeviceContext1      ID3D11DeviceContext1ToImplement;
    typedef     IEmptyDevice1             ID3D11Device1ToImplement;
    typedef     IEmptyDeviceContext1      ID3D11DeviceContextToImplement;
    typedef     IEmptyDevice1             ID3D11DeviceToImplement;
    typedef     IEmptyDeviceChild         ID3D11DeviceChildToImplement;
    typedef     IEmptyState               ID3D11BlendStateToImplement;
    typedef     IEmptyState               ID3D11DepthStencilStateToImplement;
    typedef     IEmptyState               ID3D11RasterizerStateToImplement;
    typedef     IEmptyState               ID3D11SamplerStateToImplement;
    typedef     IEmptyView                ID3D11ViewToImplement;
    typedef     IEmptyView                ID3D11DepthStencilViewToImplement;
    typedef     IEmptyView                ID3D11RenderTargetViewToImplement;
    typedef     IEmptyView                ID3D11ShaderResourceViewToImplement;
    typedef     IEmptyView                ID3D11UnorderedAccessViewToImplement;
    typedef     IEmptyResource            ID3D11ResourceToImplement;
    typedef     IEmptyResource            ID3D11BufferToImplement;
    typedef     IEmptyResource            ID3D11Texture1DToImplement;
    typedef     IEmptyResource            ID3D11Texture2DToImplement;
    typedef     IEmptyResource            ID3D11Texture3DToImplement;
    typedef     IEmptyAsynchronous        ID3D11AsynchronousToImplement;
    typedef     IEmptyAsynchronous        ID3D11QueryToImplement;
    typedef     IEmptyInputLayout         ID3D11InputLayoutToImplement;
#elif defined(CRY_USE_DX12)
    #if defined(DEVICE_SUPPORTS_D3D11_1)
        typedef ID3D11DeviceContext1      ID3D11DeviceContext1ToImplement;
        typedef ID3D11Device1             ID3D11Device1ToImplement;
    #endif

    typedef     ID3D11DeviceContext       ID3D11DeviceContextToImplement;
    typedef     ID3D11Device              ID3D11DeviceToImplement;
    typedef     ID3D11DeviceChild         ID3D11DeviceChildToImplement;
    typedef     ID3D11BlendState          ID3D11BlendStateToImplement;
    typedef     ID3D11DepthStencilState   ID3D11DepthStencilStateToImplement;
    typedef     ID3D11RasterizerState     ID3D11RasterizerStateToImplement;
    typedef     ID3D11SamplerState        ID3D11SamplerStateToImplement;
    typedef     ID3D11DepthStencilView    ID3D11DepthStencilViewToImplement;
    typedef     ID3D11RenderTargetView    ID3D11RenderTargetViewToImplement;
    typedef     ID3D11ShaderResourceView  ID3D11ShaderResourceViewToImplement;
    typedef     ID3D11UnorderedAccessView ID3D11UnorderedAccessViewToImplement;
    typedef     ID3D11Resource            ID3D11ResourceToImplement;
    typedef     ID3D11Buffer              ID3D11BufferToImplement;
    typedef     ID3D11Texture1D           ID3D11Texture1DToImplement;
    typedef     ID3D11Texture2D           ID3D11Texture2DToImplement;
    typedef     ID3D11Texture3D           ID3D11Texture3DToImplement;
    typedef     ID3D11Asynchronous        ID3D11AsynchronousToImplement;
    typedef     ID3D11Query               ID3D11QueryToImplement;
    typedef     ID3D11InputLayout         ID3D11InputLayoutToImplement;
#endif

//////////////////////////////////////////////////////////////////////////
#define MAX_FRAME_LATENCY    1
#define MAX_FRAMES_IN_FLIGHT (MAX_FRAME_LATENCY + 1)    // Current and Last

#if defined(CRY_USE_DX12)
	#include <CryCore/Platform/CryLibrary.h>
	#include "XRenderD3D9/DX12/CryDX12.hpp"

	#include "XRenderD3D9/DX12/CryDX12Guid.hpp"
	#include "XRenderD3D9/DX12/Device/CCryDX12Device.hpp"
	#include "XRenderD3D9/DX12/Device/CCryDX12DeviceContext.hpp"
	#include "XRenderD3D9/DX12/Device/CCryDX12DeviceChild.hpp"
	#include "XRenderD3D9/DX12/Resource/State/CCryDX12SamplerState.hpp"
	#include "XRenderD3D9/DX12/Resource/CCryDX12View.hpp"
	#include "XRenderD3D9/DX12/Resource/View/CCryDX12DepthStencilView.hpp"
	#include "XRenderD3D9/DX12/Resource/View/CCryDX12RenderTargetView.hpp"
	#include "XRenderD3D9/DX12/Resource/View/CCryDX12ShaderResourceView.hpp"
	#include "XRenderD3D9/DX12/Resource/View/CCryDX12UnorderedAccessView.hpp"
	#include "XRenderD3D9/DX12/Resource/Misc/CCryDX12Buffer.hpp"
	#include "XRenderD3D9/DX12/Resource/Texture/CCryDX12Texture1D.hpp"
	#include "XRenderD3D9/DX12/Resource/Texture/CCryDX12Texture2D.hpp"
	#include "XRenderD3D9/DX12/Resource/Texture/CCryDX12Texture3D.hpp"
	#include "XRenderD3D9/DX12/Resource/CCryDX12Asynchronous.hpp"
	#include "XRenderD3D9/DX12/Resource/Misc/CCryDX12Query.hpp"
	#include "XRenderD3D9/DX12/Resource/Misc/CCryDX12InputLayout.hpp"
	typedef uintptr_t SOCKET;
#endif

#if CRY_PLATFORM_DURANGO
	#include <xg.h>
#endif

#if !defined(_RELEASE) || defined(ENABLE_STATOSCOPE_RELEASE)
	#define ENABLE_TEXTURE_STREAM_LISTENER
#endif

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_ORBIS || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	#define FEATURE_SILHOUETTE_POM
#endif

#if CRY_PLATFORM_ORBIS && !defined(CRY_USE_GNM_RENDERER)
	#define FEATURE_PER_SHADER_INPUT_LAYOUT_CACHE
#elif CRY_PLATFORM_WINDOWS && !defined(OPENGL)
	#define FEATURE_PER_SHADER_INPUT_LAYOUT_CACHE
#endif

#if !defined(_RELEASE) && (CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO) && !defined(OPENGL)
	#define SUPPORT_D3D_DEBUG_RUNTIME
#endif

#if !CRY_PLATFORM_DURANGO && !CRY_PLATFORM_ORBIS
	#define SUPPORT_DEVICE_INFO
	#if CRY_PLATFORM_WINDOWS
		#define SUPPORT_DEVICE_INFO_MSG_PROCESSING
		#define SUPPORT_DEVICE_INFO_USER_DISPLAY_OVERRIDES
	#endif
#endif

#include <Cry3DEngine/I3DEngine.h>
#include <CryGame/IGame.h>


#if defined(CRY_USE_DX12)
// ConstantBuffer/ShaderResource/UnorderedAccess need markers,
// VerbexBuffer/IndexBuffer are fine with discard
	#define D3D11_MAP_WRITE_DISCARD_VB      D3D11_MAP(D3D11_MAP_WRITE_DISCARD)
	#define D3D11_MAP_WRITE_DISCARD_IB      D3D11_MAP(D3D11_MAP_WRITE_DISCARD)
	#define D3D11_MAP_WRITE_DISCARD_CB      D3D11_MAP(D3D11_MAP_WRITE_DISCARD + DX12_MAP_DISCARD_MARKER)
	#define D3D11_MAP_WRITE_DISCARD_SR      D3D11_MAP(D3D11_MAP_WRITE_DISCARD + DX12_MAP_DISCARD_MARKER)
	#define D3D11_MAP_WRITE_DISCARD_UA      D3D11_MAP(D3D11_MAP_WRITE_DISCARD + DX12_MAP_DISCARD_MARKER)

	#define D3D11_MAP_WRITE_NO_OVERWRITE_VB (D3D11_MAP_WRITE_NO_OVERWRITE)
	#define D3D11_MAP_WRITE_NO_OVERWRITE_IB (D3D11_MAP_WRITE_NO_OVERWRITE)
	#define D3D11_MAP_WRITE_NO_OVERWRITE_CB (D3D11_MAP_WRITE_NO_OVERWRITE)
	#define D3D11_MAP_WRITE_NO_OVERWRITE_SR (D3D11_MAP_WRITE_NO_OVERWRITE)
	#define D3D11_MAP_WRITE_NO_OVERWRITE_UA (D3D11_MAP_WRITE_NO_OVERWRITE)

	#define D3D11_COPY_NO_OVERWRITE_REVERT  D3D11_COPY_FLAGS(D3D11_COPY_NO_OVERWRITE + DX12_COPY_REVERTSTATE_MARKER)
	#define D3D11_COPY_NO_OVERWRITE_PXLSRV  D3D11_COPY_FLAGS(D3D11_COPY_NO_OVERWRITE + DX12_COPY_PIXELSTATE_MARKER)
	#define D3D11_RESOURCE_MISC_UAV_OVERLAP D3D11_RESOURCE_MISC_FLAG(DX12_RESOURCE_FLAG_OVERLAP)
#else
	#define D3D11_MAP_WRITE_DISCARD_VB      (D3D11_MAP_WRITE_DISCARD)
	#define D3D11_MAP_WRITE_DISCARD_IB      (D3D11_MAP_WRITE_DISCARD)
	#define D3D11_MAP_WRITE_DISCARD_CB      (D3D11_MAP_WRITE_DISCARD)
	#define D3D11_MAP_WRITE_DISCARD_SR      (D3D11_MAP_WRITE_DISCARD)
	#define D3D11_MAP_WRITE_DISCARD_UA      (D3D11_MAP_WRITE_DISCARD)

// NO_OVERWRITE on CBs/SRs-UAs could actually work when we require 11.1
// and check the feature in D3D11_FEATURE_DATA_D3D11_OPTIONS
	#define D3D11_MAP_WRITE_NO_OVERWRITE_VB (D3D11_MAP_WRITE_NO_OVERWRITE)
	#define D3D11_MAP_WRITE_NO_OVERWRITE_IB (D3D11_MAP_WRITE_NO_OVERWRITE)
	#define D3D11_MAP_WRITE_NO_OVERWRITE_CB (D3D11_MAP_WRITE_DISCARD)
	#define D3D11_MAP_WRITE_NO_OVERWRITE_SR (D3D11_MAP_WRITE_DISCARD)
	#define D3D11_MAP_WRITE_NO_OVERWRITE_UA (D3D11_MAP_WRITE_DISCARD)

	#define D3D11_COPY_NO_OVERWRITE_REVERT  D3D11_COPY_NO_OVERWRITE
	#define D3D11_COPY_NO_OVERWRITE_PXLSRV  D3D11_COPY_NO_OVERWRITE
	#define D3D11_RESOURCE_MISC_UAV_OVERLAP D3D11_RESOURCE_MISC_FLAG(0)
#endif

#if !defined(USE_D3DX)

// this should be moved into seperate D3DX defintion file which should still be used
// for console builds, until everything in the engine has been cleaned up which still
// references this

typedef struct ID3DXConstTable  ID3DXConstTable;
typedef struct ID3DXConstTable* LPD3DXCONSTANTTABLE;

	#if !CRY_PLATFORM_ORBIS

		#if CRY_PLATFORM_DURANGO
// D3DPOOL define still used as function parameters, so defined to backwards compatible with D3D9
typedef enum _D3DPOOL
{
	D3DPOOL_DEFAULT     = 0,
	D3DPOOL_MANAGED     = 1,
	D3DPOOL_SYSTEMMEM   = 2,
	D3DPOOL_SCRATCH     = 3,
	D3DPOOL_FORCE_DWORD = 0x7fffffff
} D3DPOOL;
		#endif
	#endif

	#ifndef MAKEFOURCC
		#define MAKEFOURCC(ch0, ch1, ch2, ch3)                                              \
		  ((unsigned int)(unsigned char)(ch0) | ((unsigned int)(unsigned char)(ch1) << 8) | \
		   ((unsigned int)(unsigned char)(ch2) << 16) | ((unsigned int)(unsigned char)(ch3) << 24))
	#endif // defined(MAKEFOURCC)

#endif

const int32 g_nD3D10MaxSupportedSubres = (6 * 15);
//////////////////////////////////////////////////////////////////////////

#define USAGE_WRITEONLY 8

//////////////////////////////////////////////////////////////////////////
// Linux specific defines for Renderer.
//////////////////////////////////////////////////////////////////////////

#if defined(_AMD64_) && !CRY_PLATFORM_LINUX && !CRY_PLATFORM_ANDROID
	#include <io.h>
#endif

#include "Common/CryDeviceWrapper.h"

/////////////////////////////////////////////////////////////////////////////
// STL //////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

#include <vector>
#include <list>
#include <map>
#include <set>
#include <algorithm>
#include <memory>

//=======================================================================

#if CRY_COMPILER_MSVC && CRY_COMPILER_VERSION < 1800

	#include <memory> // brings in TEMPLATE macros.
	#define MAKE_UNIQUE(TEMPLATE_LIST, PADDING_LIST, LIST, COMMA, X1, X2, X3, X4) \
	  template<class T COMMA LIST(_CLASS_TYPE)>                                   \
	  inline std::unique_ptr<T> CryMakeUnique(LIST(_TYPE_REFREF_ARG))             \
	  {                                                                           \
	    return std::unique_ptr<T>(new T(LIST(_FORWARD_ARG)));                     \
	  }
_VARIADIC_EXPAND_0X(MAKE_UNIQUE, , , , )
	#undef MAKE_UNIQUE

#else

	#include <memory>  // std::unique_ptr
	#include <utility> // std::forward

template<typename T, typename ... TArgs>
inline std::unique_ptr<T> CryMakeUnique(TArgs&& ... args)
{
	return std::unique_ptr<T>(new T(std::forward<TArgs>(args) ...));
}

#endif

#ifdef DEBUGALLOC

	#include <crtdbg.h>
	#define DEBUG_CLIENTBLOCK new(_NORMAL_BLOCK, __FILE__, __LINE__)
	#define new               DEBUG_CLIENTBLOCK

// memman
	#define   calloc(s, t)  _calloc_dbg(s, t, _NORMAL_BLOCK, __FILE__, __LINE__)
	#define   malloc(s)     _malloc_dbg(s, _NORMAL_BLOCK, __FILE__, __LINE__)
	#define   realloc(p, s) _realloc_dbg(p, s, _NORMAL_BLOCK, __FILE__, __LINE__)

#endif

#include <CryString/CryName.h>
#include "Common/CryNameR.h"

#if defined(CRY_PLATFORM_ORBIS)
	#define MAX_TMU   32
#else
	#define MAX_TMU   64
#endif
#define MAX_STREAMS 16

//! Include main interfaces.
#include <CrySystem/File/ICryPak.h>
#include <CrySystem/IProcess.h>
#include <CrySystem/ITimer.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/ILog.h>
#include <CrySystem/IConsole.h>
#include <CryRenderer/IRenderer.h>
#include <CrySystem/IStreamEngine.h>
#include <CryMemory/CrySizer.h>
#include <CryCore/smartptr.h>
#include <CryCore/Containers/CryArray.h>
#include <CryMemory/PoolAllocator.h>

#include <CryCore/Containers/CryArray.h>

enum ERenderPrimitiveType
{
	eptUnknown                = -1,
	eptTriangleList           = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
	eptTriangleStrip          = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
	eptLineList               = D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
	eptLineStrip              = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,
	eptPointList              = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST,
	ept1ControlPointPatchList = D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST,
	ept2ControlPointPatchList = D3D11_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST,
	ept3ControlPointPatchList = D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST,
	ept4ControlPointPatchList = D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST,

	// non-real primitives, used for logical batching
	eptHWSkinGroups = 0x3f
};

inline ERenderPrimitiveType GetInternalPrimitiveType(PublicRenderPrimitiveType t)
{
	switch (t)
	{
	case prtTriangleList:
	default:
		return eptTriangleList;
	case prtTriangleStrip:
		return eptTriangleStrip;
	case prtLineList:
		return eptLineList;
	case prtLineStrip:
		return eptLineStrip;
	}
}

#define SUPPORT_FLEXIBLE_INDEXBUFFER // supports 16 as well as 32 bit indices AND index buffer bind offset

enum RenderIndexType
{
#if defined(SUPPORT_FLEXIBLE_INDEXBUFFER)
	Index16 = DXGI_FORMAT_R16_UINT,
	Index32 = DXGI_FORMAT_R32_UINT
#else
	Index16,
	Index32
#endif
};

// Interfaces from the Game
extern ILog* iLog;
extern IConsole* iConsole;
extern ITimer* iTimer;
extern ISystem* iSystem;

template<class Container>
unsigned sizeOfVP(Container& arr)
{
	int i;
	unsigned size = 0;
	for (i = 0; i < (int)arr.size(); i++)
	{
		typename Container::value_type& T = arr[i];
		size += T->Size();

	}
	size += (arr.capacity() - arr.size()) * sizeof(typename Container::value_type);
	return size;
}

template<class Container>
unsigned sizeOfV(Container& arr)
{
	int i;
	unsigned size = 0;
	for (i = 0; i < (int)arr.size(); i++)
	{
		typename Container::value_type& T = arr[i];
		size += T.Size();

	}
	size += (arr.capacity() - arr.size()) * sizeof(typename Container::value_type);
	return size;
}
template<class Container>
unsigned sizeOfA(Container& arr)
{
	int i;
	unsigned size = 0;
	for (i = 0; i < arr.size(); i++)
	{
		typename Container::value_type& T = arr[i];
		size += T.Size();

	}
	return size;
}
template<class Map>
unsigned sizeOfMap(Map& map)
{
	unsigned size = 0;
	for (typename Map::iterator it = map.begin(); it != map.end(); ++it)
	{
		typename Map::mapped_type& T = it->second;
		size += T.Size();
	}
	size += map.size() * sizeof(stl::MapLikeStruct);
	return size;
}
template<class Map>
unsigned sizeOfMapStr(Map& map)
{
	unsigned size = 0;
	for (typename Map::iterator it = map.begin(); it != map.end(); ++it)
	{
		typename Map::mapped_type& T = it->second;
		size += T.capacity();
	}
	size += map.size() * sizeof(stl::MapLikeStruct);
	return size;
}
template<class Map>
unsigned sizeOfMapP(Map& map)
{
	unsigned size = 0;
	for (typename Map::iterator it = map.begin(); it != map.end(); ++it)
	{
		typename Map::mapped_type& T = it->second;
		size += T->Size();
	}
	size += map.size() * sizeof(stl::MapLikeStruct);
	return size;
}
template<class Map>
unsigned sizeOfMapS(Map& map)
{
	unsigned size = 0;
	for (typename Map::iterator it = map.begin(); it != map.end(); ++it)
	{
		typename Map::mapped_type& T = it->second;
		size += sizeof(T);
	}
	size += map.size() * sizeof(stl::MapLikeStruct);
	return size;
}

#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	#define VOLUMETRIC_FOG_SHADOWS
#endif

#if CRY_PLATFORM_WINDOWS && !defined(CRY_USE_DX12) && !(defined(OPENGL) && defined(RELEASE))
	#define ENABLE_NULL_D3D11DEVICE
#endif

#if CRY_PLATFORM_DURANGO || CRY_PLATFORM_ORBIS
	#define TEXSTRM_BYTECENTRIC_MEMORY
#endif

// Enable to eliminate DevTextureDataSize calls during stream updates - costs 4 bytes per mip header
#define TEXSTRM_STORE_DEVSIZES

#ifndef TEXSTRM_BYTECENTRIC_MEMORY
	#define TEXSTRM_TEXTURECENTRIC_MEMORY
#endif

#if CRY_PLATFORM_DESKTOP && !defined(OPENGL) && !defined(CRY_USE_DX12)
	#define TEXSTRM_DEFERRED_UPLOAD
#endif

#if CRY_PLATFORM_DESKTOP
	#define TEXSTRM_COMMIT_COOLDOWN
#endif

#if 0 /*|| defined(CRY_USE_DX12)*/
	#define TEXSTRM_ASYNC_UPLOAD
#endif

// Multi-threaded texture uploading, requires UpdateSubresourceRegion() being thread-safe
#if CRY_PLATFORM_DURANGO /*|| defined(CRY_USE_DX12)*/
	#define TEXSTRM_ASYNC_TEXCOPY
#endif

// The below submits the device context state changes and draw commands
// asynchronously via a high priority packet queue.
// Note: please continously monitor ASYNC_DIP_SYNC profile marker for stalls
#if CRY_PLATFORM_DURANGO
	#define DURANGO_ENABLE_ASYNC_DIPS 1
#endif

#if CRY_PLATFORM_DURANGO
	#define TEXSTRM_CUBE_DMA_BROKEN
#endif

//#define TEXSTRM_SUPPORT_REACTIVE

#if defined(_RELEASE)
	#define EXCLUDE_RARELY_USED_R_STATS
#endif

#if !defined(_RELEASE)
	#define CD3D9RENDERER_DEBUG_CONSISTENCY_CHECK
#endif

#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Geo.h>
//#include "_Malloc.h"
//#include "math.h"
#include <CryCore/StlUtils.h>
#include "XRenderD3D9/DeviceManager/DeviceManager.h"
#include "Common/DevBuffer.h"

#include <CryRenderer/VertexFormats.h>

#include "Common/CommonRender.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include "Common/Shaders/ShaderComponents.h"
#include "Common/Shaders/Shader.h"
//#include "Common/XFile/File.h"
//#include "Common/Image.h"
#include "Common/Shaders/CShader.h"
#include "Common/RenderMesh.h"
#include "Common/RenderPipeline.h"
#include "Common/RenderThread.h"
#include "Common/Renderer.h"
#include "Common/Textures/Texture.h"
#include "Common/Shaders/Parser.h"
#include "Common/RenderFrameProfiler.h"
#include "Common/Shadow_Renderer.h"
#include "Common/DeferredRenderUtils.h"
#include "Common/ShadowUtils.h"
#include "Common/WaterUtils.h"

#include "Common/OcclQuery.h"

// All handled render elements (except common ones included in "RendElement.h")
#include "Common/RendElements/CREBeam.h"
#include "Common/RendElements/CRELensOptics.h"
#include "Common/RendElements/CREHDRProcess.h"
#include "Common/RendElements/CRECloud.h"
#include "Common/RendElements/CREDeferredShading.h"
#include "Common/RendElements/CREMeshImpl.h"

#include "Common/PostProcess/PostProcess.h"

#include "../XRenderD3D9/DeviceManager/DeviceObjects.h"

/*-----------------------------------------------------------------------------
   Vector transformations.
   -----------------------------------------------------------------------------*/

inline void TransformVector(Vec3& out, const Vec3& in, const Matrix44A& m)
{
	out.x = in.x * m(0, 0) + in.y * m(1, 0) + in.z * m(2, 0);
	out.y = in.x * m(0, 1) + in.y * m(1, 1) + in.z * m(2, 1);
	out.z = in.x * m(0, 2) + in.y * m(1, 2) + in.z * m(2, 2);
}

inline void TransformPosition(Vec3& out, const Vec3& in, const Matrix44A& m)
{
	TransformVector(out, in, m);
	out += m.GetRow(3);
}

inline Plane TransformPlaneByUsingAdjointT(const Matrix44A& M, const Matrix44A& TA, const Plane plSrc)
{
	Vec3 newNorm;
	TransformVector(newNorm, plSrc.n, TA);
	newNorm.Normalize();

	if (M.Determinant() < 0.f)
		newNorm *= -1;

	Plane plane;
	Vec3 p;
	TransformPosition(p, plSrc.n * plSrc.d, M);
	plane.Set(newNorm, p | newNorm);

	return plane;
}

inline Matrix44 TransposeAdjoint(const Matrix44A& M)
{
	Matrix44 ta;

	ta(0, 0) = M(1, 1) * M(2, 2) - M(2, 1) * M(1, 2);
	ta(1, 0) = M(2, 1) * M(0, 2) - M(0, 1) * M(2, 2);
	ta(2, 0) = M(0, 1) * M(1, 2) - M(1, 1) * M(0, 2);

	ta(0, 1) = M(1, 2) * M(2, 0) - M(2, 2) * M(1, 0);
	ta(1, 1) = M(2, 2) * M(0, 0) - M(0, 2) * M(2, 0);
	ta(2, 1) = M(0, 2) * M(1, 0) - M(1, 2) * M(0, 0);

	ta(0, 2) = M(1, 0) * M(2, 1) - M(2, 0) * M(1, 1);
	ta(1, 2) = M(2, 0) * M(0, 1) - M(0, 0) * M(2, 1);
	ta(2, 2) = M(0, 0) * M(1, 1) - M(1, 0) * M(0, 1);

	ta(0, 3) = 0.f;
	ta(1, 3) = 0.f;
	ta(2, 3) = 0.f;

	return ta;
}

inline Plane TransformPlane(const Matrix44A& M, const Plane& plSrc)
{
	Matrix44 tmpTA = TransposeAdjoint(M);
	return TransformPlaneByUsingAdjointT(M, tmpTA, plSrc);
}

// Homogeneous plane transform.
inline Plane TransformPlane2(const Matrix34A& m, const Plane& src)
{
	Plane plDst;

	float v0 = src.n.x, v1 = src.n.y, v2 = src.n.z, v3 = src.d;
	plDst.n.x = v0 * m(0, 0) + v1 * m(1, 0) + v2 * m(2, 0);
	plDst.n.y = v0 * m(0, 1) + v1 * m(1, 1) + v2 * m(2, 1);
	plDst.n.z = v0 * m(0, 2) + v1 * m(1, 2) + v2 * m(2, 2);

	plDst.d = v0 * m(0, 3) + v1 * m(1, 3) + v2 * m(2, 3) + v3;

	return plDst;
}

// Homogeneous plane transform.
inline Plane TransformPlane2(const Matrix44A& m, const Plane& src)
{
	Plane plDst;

	float v0 = src.n.x, v1 = src.n.y, v2 = src.n.z, v3 = src.d;
	plDst.n.x = v0 * m(0, 0) + v1 * m(0, 1) + v2 * m(0, 2) + v3 * m(0, 3);
	plDst.n.y = v0 * m(1, 0) + v1 * m(1, 1) + v2 * m(1, 2) + v3 * m(1, 3);
	plDst.n.z = v0 * m(2, 0) + v1 * m(2, 1) + v2 * m(2, 2) + v3 * m(2, 3);

	plDst.d = v0 * m(3, 0) + v1 * m(3, 1) + v2 * m(3, 2) + v3 * m(3, 3);

	return plDst;
}
inline Plane TransformPlane2_NoTrans(const Matrix44A& m, const Plane& src)
{
	Plane plDst;
	TransformVector(plDst.n, src.n, m);
	plDst.d = src.d;

	return plDst;
}

inline Plane TransformPlane2Transposed(const Matrix44A& m, const Plane& src)
{
	Plane plDst;

	float v0 = src.n.x, v1 = src.n.y, v2 = src.n.z, v3 = src.d;
	plDst.n.x = v0 * m(0, 0) + v1 * m(1, 0) + v2 * m(2, 0) + v3 * m(3, 0);
	plDst.n.y = v0 * m(0, 1) + v1 * m(1, 1) + v2 * m(2, 1) + v3 * m(3, 1);
	plDst.n.z = v0 * m(0, 2) + v1 * m(2, 1) + v2 * m(2, 2) + v3 * m(3, 2);

	plDst.d = v0 * m(0, 3) + v1 * m(1, 3) + v2 * m(2, 3) + v3 * m(3, 3);

	return plDst;
}

//===============================================================================================

#define MAX_PATH_LENGTH 512

//////////////////////////////////////////////////////////////////////////
// Report warning to validator.
//////////////////////////////////////////////////////////////////////////
inline void Warning(const char* format, ...) PRINTF_PARAMS(1, 2);
inline void Warning(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	if (iSystem)
		iSystem->WarningV(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING, 0, NULL, format, args);
	va_end(args);
}

//////////////////////////////////////////////////////////////////////////
// Report warning to validator.
//////////////////////////////////////////////////////////////////////////
inline void LogWarning(const char* format, ...) PRINTF_PARAMS(1, 2);
inline void LogWarning(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	if (iSystem)
		iSystem->WarningV(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING, 0, NULL, format, args);
	va_end(args);
}

inline void LogWarningEngineOnly(const char* format, ...) PRINTF_PARAMS(1, 2);
inline void LogWarningEngineOnly(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	if (iSystem)
		iSystem->WarningV(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING, VALIDATOR_FLAG_IGNORE_IN_EDITOR, NULL, format, args);
	va_end(args);
}

//////////////////////////////////////////////////////////////////////////
// Report warning to validator.
//////////////////////////////////////////////////////////////////////////
inline void FileWarning(const char* filename, const char* format, ...) PRINTF_PARAMS(2, 3);
inline void FileWarning(const char* filename, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	if (iSystem)
		iSystem->WarningV(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING, VALIDATOR_FLAG_FILE, filename, format, args);
	va_end(args);
}

//////////////////////////////////////////////////////////////////////////
// Report warning to validator.
//////////////////////////////////////////////////////////////////////////
inline void TextureWarning(const char* filename, const char* format, ...) PRINTF_PARAMS(2, 3);
inline void TextureWarning(const char* filename, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	if (iSystem)
		iSystem->WarningV(VALIDATOR_MODULE_RENDERER, VALIDATOR_WARNING, (VALIDATOR_FLAG_FILE | VALIDATOR_FLAG_TEXTURE), filename, format, args);
	va_end(args);
}

inline void TextureError(const char* filename, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	if (iSystem)
		iSystem->WarningV(VALIDATOR_MODULE_RENDERER, VALIDATOR_ERROR, (VALIDATOR_FLAG_FILE | VALIDATOR_FLAG_TEXTURE), filename, format, args);
	va_end(args);
}

inline void _SetVar(const char* szVarName, int nVal)
{
	ICVar* var = iConsole->GetCVar(szVarName);
	if (var)
		var->Set(nVal);
	else
	{
		assert(0);
	}
}

// Get the sub-string starting at the last . in the string, or NULL if the string contains no dot
// Note: The returned pointer refers to a location inside the provided string, no allocation is performed
const char* fpGetExtension(const char* in);

// Remove extension from string, including the .
// If the string has no extension, the whole string will be copied into the buffer
// Note: The out buffer must have space to store a copy of the in-string and a null-terminator
void fpStripExtension(const char* in, char* out, size_t bytes);
template<size_t bytes>
void fpStripExtension(const char* in, char (&out)[bytes]) { fpStripExtension(in, out, bytes); }

// Adds an extension to the path, if an extension is already present the function does nothing
// The extension should include the .
// Note: The path buffer must have enough unused space to store a copy of the extension string
void fpAddExtension(char* path, const char* extension, size_t bytes);
template<size_t bytes>
void fpAddExtension(char (&path)[bytes], const char* extension) { fpAddExtension(path, extension, bytes); }

// Converts DOS slashes to UNIX slashes
// Note: The dst buffer must have space to store a copy of src and a null-terminator
void fpConvertDOSToUnixName(char* dst, const char* src, size_t bytes);
template<size_t bytes>
void fpConvertDOSToUnixName(char (&dst)[bytes], const char* src) { fpConvertDOSToUnixName(dst, src, bytes); }

// Converts UNIX slashes to DOS slashes
// Note: the dst buffer must have space to store a copy of src and a null-terminator
void fpConvertUnixToDosName(char* dst, const char* src, size_t bytes);
template<size_t bytes>
void fpConvertUnixToDosName(char (&dst)[bytes], const char* src) { fpConvertUnixToDosName(dst, src, bytes); }

// Combines the path and name strings, inserting a UNIX slash as required, and stores the result into the dst buffer
// path may be NULL, in which case name will be copied into the dst buffer, and the UNIX slash is NOT inserted
// Note: the dst buffer must have space to store: a copy of name, a copy of path (if not null), a UNIX slash (if path doesn't end with one) and a null-terminator
void fpUsePath(const char* name, const char* path, char* dst, size_t bytes);
template<size_t bytes>
void fpUsePath(const char* name, const char* path, char (&dst)[bytes]) { fpUsePath(name, path, dst, bytes); }

//=========================================================================================
//
// Normal timing.
//
#define ticks(Timer)   { Timer -= CryGetTicks(); }
#define unticks(Timer) { Timer += CryGetTicks() + 34; }

//=============================================================================

// the int 3 call for 32-bit version for .l-generated files.
#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
	#define LEX_DBG_BREAK
#else
	#define LEX_DBG_BREAK __debugbreak()
#endif

#include "Common/Defs.h"

#include "XRenderD3D9/DeviceManager/DeviceManagerInline.h"
#include <CrySystem/Profilers/FrameProfiler/FrameProfiler_JobSystem.h>  // to be removed
#ifdef INCLUDE_SCALEFORM_SDK
	#include <CrySystem/Scaleform/ConfigScaleform.h>
#endif
