// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CryDXGL.hpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Entry point header for the DXGL wrapper library
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYDXGL__
#define __CRYDXGL__

#if !defined(DXGL_FULL_EMULATION)
	#define DXGL_FULL_EMULATION 0
#endif //!defined(DXGL_FULL_EMULATION)

#include "Definitions/CryDXGLMisc.hpp"

#if DXGL_FULL_EMULATION
	#include "Definitions/DXGL_IDXGISwapChain.h"
	#include "Definitions/DXGL_IDXGIFactory1.h"
#endif

#if !DXGL_FULL_EMULATION
	#include "Interfaces/CCryDXGLBlob.hpp"
	#include "Interfaces/CCryDXGLSwapChain.hpp"
	#include "Interfaces/CCryDXGLQuery.hpp"
	#include "Interfaces/CCryDXGLSwitchToRef.hpp"

	#include "Interfaces/CCryDXGLBuffer.hpp"
	#include "Interfaces/CCryDXGLDepthStencilView.hpp"
	#include "Interfaces/CCryDXGLInputLayout.hpp"
	#include "Interfaces/CCryDXGLRenderTargetView.hpp"
	#include "Interfaces/CCryDXGLShader.hpp"
	#include "Interfaces/CCryDXGLShaderReflection.hpp"
	#include "Interfaces/CCryDXGLDepthStencilView.hpp"
	#include "Interfaces/CCryDXGLShaderResourceView.hpp"
	#include "Interfaces/CCryDXGLUnorderedAccessView.hpp"
	#include "Interfaces/CCryDXGLView.hpp"
	#include "Interfaces/CCryDXGLBlendState.hpp"
	#include "Interfaces/CCryDXGLDepthStencilState.hpp"
	#include "Interfaces/CCryDXGLRasterizerState.hpp"
	#include "Interfaces/CCryDXGLSamplerState.hpp"
	#include "Interfaces/CCryDXGLTexture1D.hpp"
	#include "Interfaces/CCryDXGLTexture2D.hpp"
	#include "Interfaces/CCryDXGLTexture3D.hpp"
	#include "Interfaces/CCryDXGLGIAdapter.hpp"
	#include "Interfaces/CCryDXGLGIFactory.hpp"
	#include "Interfaces/CCryDXGLGIOutput.hpp"
#endif //!DXGL_FULL_EMULATION

#if DXGL_VIRTUAL_DEVICE_AND_CONTEXT || DXGL_FULL_EMULATION
	#include "Definitions/DXGL_ID3D11Device.h"
	#include "Definitions/DXGL_ID3D11DeviceContext.h"
#else
	#include "Interfaces/CCryDXGLDevice.hpp"
	#include "Interfaces/CCryDXGLDeviceContext.hpp"
#endif

// Set to 0 to force runtime shader translation
#define DXGL_INPUT_GLSL !DXGL_FULL_EMULATION

// TODO: Investigate what prevents framebuffer completeness in some framebuffers with depth stencil bigger than color buffers (which is ok according to the standard)
#define OGL_DO_NOT_ALLOW_LARGER_RT

typedef ID3D10Blob* LPD3D10BLOB;
typedef ID3D10Blob  ID3DBlob;

#if DXGL_FULL_EMULATION
	#if CRY_RENDERER_OPENGL
		#define DXGL_API DXGL_IMPORT
	#else
		#define DXGL_API DXGL_EXPORT
	#endif
#else
	#define DXGL_API
#endif

#if DXGL_FULL_EMULATION && !CRY_PLATFORM_WINDOWS
	#define DXGL_EXTERN extern "C"
#else
	#define DXGL_EXTERN
#endif

////////////////////////////////////////////////////////////////////////////
//  Required global functions declared in D3D11.h and included headers
////////////////////////////////////////////////////////////////////////////

typedef HRESULT (WINAPI * PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)(IDXGIAdapter*,
                                                                  D3D_DRIVER_TYPE, HMODULE, UINT,
                                                                  CONST D3D_FEATURE_LEVEL*,
                                                                  UINT FeatureLevels, UINT, CONST DXGI_SWAP_CHAIN_DESC*,
                                                                  IDXGISwapChain**, ID3D11Device**,
                                                                  D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

DXGL_EXTERN DXGL_API HRESULT WINAPI D3D11CreateDeviceAndSwapChain(
  IDXGIAdapter* pAdapter,
  D3D_DRIVER_TYPE DriverType,
  HMODULE Software,
  UINT Flags,
  CONST D3D_FEATURE_LEVEL* pFeatureLevels,
  UINT FeatureLevels,
  UINT SDKVersion,
  CONST DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
  IDXGISwapChain** ppSwapChain,
  ID3D11Device** ppDevice,
  D3D_FEATURE_LEVEL* pFeatureLevel,
  ID3D11DeviceContext** ppImmediateContext);

DXGL_EXTERN DXGL_API HRESULT WINAPI D3DCreateBlob(SIZE_T NumBytes, LPD3D10BLOB* ppBuffer);

////////////////////////////////////////////////////////////////////////////
//  Required global functions declared in D3DCompiler.h and included headers
////////////////////////////////////////////////////////////////////////////

DXGL_EXTERN DXGL_API HRESULT WINAPI D3DReflect(
  LPCVOID pSrcData,
  SIZE_T SrcDataSize,
  REFIID pInterface,
  void** ppReflector);

DXGL_EXTERN DXGL_API HRESULT WINAPI D3DDisassemble(
  LPCVOID pSrcData,
  SIZE_T SrcDataSize,
  UINT Flags,
  LPCSTR szComments,
  ID3DBlob** ppDisassembly);

////////////////////////////////////////////////////////////////////////////
//  Required global functions declared in D3DX11.h and included headers
////////////////////////////////////////////////////////////////////////////

DXGL_EXTERN DXGL_API HRESULT WINAPI D3DX11CreateTextureFromMemory(
  ID3D11Device* pDevice,
  const void* pSrcData,
  size_t SrcDataSize,
  D3DX11_IMAGE_LOAD_INFO* pLoadInfo,
  ID3DX11ThreadPump* pPump,
  ID3D11Resource** ppTexture,
  long* pResult);

DXGL_EXTERN DXGL_API HRESULT WINAPI D3DX11SaveTextureToFile(ID3D11DeviceContext* pDevice,
                                                            ID3D11Resource* pSrcResource,
                                                            D3DX11_IMAGE_FILE_FORMAT fmt,
                                                            const char* pDestFile);

DXGL_EXTERN DXGL_API HRESULT WINAPI D3DX11CompileFromMemory(
  LPCSTR pSrcData,
  SIZE_T SrcDataLen,
  LPCSTR pFileName,
  CONST D3D10_SHADER_MACRO* pDefines,
  LPD3D10INCLUDE pInclude,
  LPCSTR pFunctionName,
  LPCSTR pProfile,
  UINT Flags1,
  UINT Flags2,
  ID3DX11ThreadPump* pPump,
  ID3D10Blob** ppShader,
  ID3D10Blob** ppErrorMsgs,
  HRESULT* pHResult);

////////////////////////////////////////////////////////////////////////////
//  Required global functions declared in dxgi.h and included headers
////////////////////////////////////////////////////////////////////////////

DXGL_EXTERN DXGL_API HRESULT WINAPI CreateDXGIFactory(REFIID riid, void** ppFactory);

DXGL_EXTERN DXGL_API HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void** ppFactory);

////////////////////////////////////////////////////////////////////////////
//  Frame debugging functions
////////////////////////////////////////////////////////////////////////////

DXGL_EXTERN DXGL_API void DXGLProfileLabel(const char* szName);

DXGL_EXTERN DXGL_API void DXGLProfileLabelPush(const char* szName);

DXGL_EXTERN DXGL_API void DXGLProfileLabelPop(const char* szName);

////////////////////////////////////////////////////////////////////////////
//  DXGL Extensions
////////////////////////////////////////////////////////////////////////////

#if !DXGL_FULL_EMULATION

// Statically initialize DXGL. Specify with uNumSharedContexts the maximum
// number of threads expected to call device methods at the same time.
void DXGLInitialize(uint32 uNumSharedContexts);

	#if OGL_SINGLE_CONTEXT

// Direct3D mandates that access to the device context by multiple threads
// has to be serialized. DXGL additionally requires that each thread taking
// control over the device context calls DXGLBindDeviceContext before
// calling any of its functions, and DXGLUnbindDeviceContext after.
void DXGLBindDeviceContext(ID3D11DeviceContext* pDeviceContext);
void DXGLUnbindDeviceContext(ID3D11DeviceContext* pDeviceContext);

	#else

// Any thread frequently using the device should keep a reserved context
// to avoid performance penalty of context switching.
// In order to do so, call DXGLReserveContext before using the device
// and DXGLReleaseContext after.
void DXGLReserveContext(ID3D11Device* pDevice);
void DXGLReleaseContext(ID3D11Device* pDevice);

// Direct3D mandates that access to the device context by multiple threads
// has to be serialized. DXGL additionally requires that each thread taking
// control over the device context calls DXGLBindDeviceContext before
// calling any of its functions, and DXGLUnbindDeviceContext after.
void DXGLBindDeviceContext(ID3D11DeviceContext* pDeviceContext, bool bReserved);
void DXGLUnbindDeviceContext(ID3D11DeviceContext* pDeviceContext, bool bReserved);

	#endif

HRESULT DXGLMapBufferRange(ID3D11DeviceContext* pDeviceContext, ID3D11Buffer* pBuffer, size_t uOffset, size_t uSize, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource);

void    DXGLSetDepthBoundsTest(bool bEnabled, float fMin, float fMax);

void    DXGLTogglePixelTracing(ID3D11DeviceContext* pDeviceContext, bool bEnable, uint32 uShaderHash, uint32 uPixelX = 0, uint32 uPixelY = 0);
void    DXGLToggleVertexTracing(ID3D11DeviceContext* pDeviceContext, bool bEnable, uint32 uShaderHash, uint32 uVertexID = 0);
void    DXGLToggleComputeTracing(
  ID3D11DeviceContext* pDeviceContext, bool bEnable, uint32 uShaderHash,
  uint32 uWorkGroupIDX = 0, uint32 uWorkGroupIDY = 0, uint32 uWorkGroupIDZ = 0,
  uint32 uLocalInvocationIDX = 0, uint32 uLocalInvocationIDY = 0, uint32 uLocalInvocationIDZ = 0);

void DXGLCSSetConstantBuffers(ID3D11DeviceContext* pDeviceContext, UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* pConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants);
void DXGLPSSetConstantBuffers(ID3D11DeviceContext* pDeviceContext, UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* pConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants);
void DXGLVSSetConstantBuffers(ID3D11DeviceContext* pDeviceContext, UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* pConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants);
void DXGLGSSetConstantBuffers(ID3D11DeviceContext* pDeviceContext, UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* pConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants);
void DXGLHSSetConstantBuffers(ID3D11DeviceContext* pDeviceContext, UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* pConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants);
void DXGLDSSetConstantBuffers(ID3D11DeviceContext* pDeviceContext, UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* pConstantBuffers, const UINT* pFirstConstant, const UINT* pNumConstants);

void DXGLIssueFrameFences(ID3D11Device* pDevice);

#endif //!DXGL_FULL_EMULATION

#if defined(USE_SDL2_VIDEO)
DXGL_EXTERN DXGL_API bool DXGLCreateSDLWindow(const char* szTitle, uint32 uWidth, uint32 uHeight, bool bFullScreen, HWND* pHandle);
DXGL_EXTERN DXGL_API void DXGLDestroySDLWindow(HWND kHandle);
#endif

////////////////////////////////////////////////////////////////////////////
//  DxErr Logging and error functions
////////////////////////////////////////////////////////////////////////////

DXGL_EXTERN DXGL_API const char* WINAPI  DXGetErrorStringA(HRESULT hr);
DXGL_EXTERN DXGL_API const WCHAR* WINAPI DXGetErrorStringW(HRESULT hr);

DXGL_EXTERN DXGL_API const char* WINAPI  DXGetErrorDescriptionA(HRESULT hr);
DXGL_EXTERN DXGL_API const WCHAR* WINAPI DXGetErrorDescriptionW(HRESULT hr);

DXGL_EXTERN DXGL_API HRESULT WINAPI      DXTraceA(const char* strFile, DWORD dwLine, HRESULT hr, const char* strMsg, BOOL bPopMsgBox);
DXGL_EXTERN DXGL_API HRESULT WINAPI      DXTraceW(const char* strFile, DWORD dwLine, HRESULT hr, const WCHAR* strMsg, BOOL bPopMsgBox);

#ifdef UNICODE
	#define DXGetErrorString      DXGetErrorStringW
	#define DXGetErrorDescription DXGetErrorDescriptionW
	#define DXTrace               DXTraceW
#else
	#define DXGetErrorString      DXGetErrorStringA
	#define DXGetErrorDescription DXGetErrorDescriptionA
	#define DXTrace               DXTraceA
#endif

////////////////////////////////////////////////////////////////////////////
//  Renderer helpers
////////////////////////////////////////////////////////////////////////////

#if !DXGL_FULL_EMULATION

	#if OGL_SINGLE_CONTEXT

struct SDXGLDeviceContextThreadLocalHandle
{
	void* m_pTLSHandle;

	SDXGLDeviceContextThreadLocalHandle();
	~SDXGLDeviceContextThreadLocalHandle();

	void Set(ID3D11DeviceContext* pDeviceContext);
};

	#else

struct SDXGLContextThreadLocalHandle
{
	void* m_pTLSHandle;

	SDXGLContextThreadLocalHandle();
	~SDXGLContextThreadLocalHandle();

	void Set(ID3D11Device* pDevice);
};

struct SDXGLDeviceContextThreadLocalHandle
{
	void* m_pTLSHandle;

	SDXGLDeviceContextThreadLocalHandle();
	~SDXGLDeviceContextThreadLocalHandle();

	void Set(ID3D11DeviceContext* pDeviceContext, bool bReserved);
};

	#endif

#endif //!DXGL_FULL_EMULATION

#endif //__CRYDXGL__
