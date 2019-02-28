// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   DXGL_DXGI.h
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Contains portable definition of structs and enums to match
//               those in DXGI.h in the DirectX SDK
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __DXGL_DXGI_h__
#define __DXGL_DXGI_h__

#include "DXGL_dxgitype.h"

////////////////////////////////////////////////////////////////////////////
//  Defines
////////////////////////////////////////////////////////////////////////////

#define DXGI_MAX_SWAP_CHAIN_BUFFERS     (16)
#define DXGI_PRESENT_TEST               0x00000001UL
#define DXGI_PRESENT_DO_NOT_SEQUENCE    0x00000002UL
#define DXGI_PRESENT_RESTART            0x00000004UL

#define DXGI_USAGE_SHADER_INPUT         (1UL << (0 + 4))
#define DXGI_USAGE_RENDER_TARGET_OUTPUT (1UL << (1 + 4))
#define DXGI_USAGE_BACK_BUFFER          (1UL << (2 + 4))
#define DXGI_USAGE_SHARED               (1UL << (3 + 4))
#define DXGI_USAGE_READ_ONLY            (1UL << (4 + 4))
#define DXGI_USAGE_DISCARD_ON_PRESENT   (1UL << (5 + 4))
#define DXGI_USAGE_UNORDERED_ACCESS     (1UL << (6 + 4))

#define DXGI_ENUM_MODES_INTERLACED      (1UL)

#define DXGI_ENUM_MODES_SCALING         (2UL)

#define DXGI_MWA_NO_WINDOW_CHANGES      (1 << 0)
#define DXGI_MWA_NO_ALT_ENTER           (1 << 1)
#define DXGI_MWA_NO_PRINT_SCREEN        (1 << 2)
#define DXGI_MWA_VALID                  (0x7)

////////////////////////////////////////////////////////////////////////////
//  Enums
////////////////////////////////////////////////////////////////////////////

typedef
  enum DXGI_RESIDENCY
{
	DXGI_RESIDENCY_FULLY_RESIDENT            = 1,
	DXGI_RESIDENCY_RESIDENT_IN_SHARED_MEMORY = 2,
	DXGI_RESIDENCY_EVICTED_TO_DISK           = 3
}   DXGI_RESIDENCY;

typedef
  enum DXGI_SWAP_EFFECT
{
	DXGI_SWAP_EFFECT_DISCARD    = 0,
	DXGI_SWAP_EFFECT_SEQUENTIAL = 1
}   DXGI_SWAP_EFFECT;

typedef
  enum DXGI_SWAP_CHAIN_FLAG
{
	DXGI_SWAP_CHAIN_FLAG_NONPREROTATED     = 1,
	DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH = 2,
	DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE    = 4
}   DXGI_SWAP_CHAIN_FLAG;

typedef
  enum DXGI_ADAPTER_FLAG
{
	DXGI_ADAPTER_FLAG_NONE        = 0,
	DXGI_ADAPTER_FLAG_REMOTE      = 1,
	DXGI_ADAPTER_FLAG_FORCE_DWORD = 0xffffffff
}   DXGI_ADAPTER_FLAG;

typedef UINT DXGI_USAGE;

////////////////////////////////////////////////////////////////////////////
//  Structs
////////////////////////////////////////////////////////////////////////////

typedef struct DXGI_FRAME_STATISTICS
{
	UINT          PresentCount;
	UINT          PresentRefreshCount;
	UINT          SyncRefreshCount;
	LARGE_INTEGER SyncQPCTime;
	LARGE_INTEGER SyncGPUTime;
}   DXGI_FRAME_STATISTICS;

typedef struct DXGI_MAPPED_RECT
{
	INT   Pitch;
	BYTE* pBits;
}   DXGI_MAPPED_RECT;

typedef struct DXGI_ADAPTER_DESC
{
	WCHAR  Description[128];
	UINT   VendorId;
	UINT   DeviceId;
	UINT   SubSysId;
	UINT   Revision;
	SIZE_T DedicatedVideoMemory;
	SIZE_T DedicatedSystemMemory;
	SIZE_T SharedSystemMemory;
	LUID   AdapterLuid;
}   DXGI_ADAPTER_DESC;

typedef struct DXGI_OUTPUT_DESC
{
	WCHAR              DeviceName[32];
	RECT               DesktopCoordinates;
	BOOL               AttachedToDesktop;
	DXGI_MODE_ROTATION Rotation;
	HMONITOR           Monitor;
}   DXGI_OUTPUT_DESC;

typedef struct DXGI_SHARED_RESOURCE
{
	HANDLE Handle;
}   DXGI_SHARED_RESOURCE;

typedef struct DXGI_SURFACE_DESC
{
	UINT             Width;
	UINT             Height;
	DXGI_FORMAT      Format;
	DXGI_SAMPLE_DESC SampleDesc;
}   DXGI_SURFACE_DESC;

typedef struct DXGI_SWAP_CHAIN_DESC
{
	DXGI_MODE_DESC   BufferDesc;
	DXGI_SAMPLE_DESC SampleDesc;
	DXGI_USAGE       BufferUsage;
	UINT             BufferCount;
	HWND             OutputWindow;
	BOOL             Windowed;
	DXGI_SWAP_EFFECT SwapEffect;
	UINT             Flags;
}   DXGI_SWAP_CHAIN_DESC;

typedef struct DXGI_ADAPTER_DESC1
{
	WCHAR  Description[128];
	UINT   VendorId;
	UINT   DeviceId;
	UINT   SubSysId;
	UINT   Revision;
	SIZE_T DedicatedVideoMemory;
	SIZE_T DedicatedSystemMemory;
	SIZE_T SharedSystemMemory;
	LUID   AdapterLuid;
	UINT   Flags;
}   DXGI_ADAPTER_DESC1;

typedef struct DXGI_DISPLAY_COLOR_SPACE
{
	FLOAT PrimaryCoordinates[8][2];
	FLOAT WhitePoints[16][2];
}   DXGI_DISPLAY_COLOR_SPACE;

typedef UINT DXGI_USAGE;

////////////////////////////////////////////////////////////////////////////
//  Forward declaration of unused interfaces
////////////////////////////////////////////////////////////////////////////

//struct IDXGIObject;            // Typedef as CCryDXGLGIObject
struct IDXGIDeviceSubObject;
struct IDXGIResource;
struct IDXGIKeyedMutex;
struct IDXGISurface;
struct IDXGISurface1;
//struct IDXGIAdapter;           // Typedef as CCryDXGLGIAdapter
//struct IDXGIOutput;            // Typedef as CCryDXGLGIOutput
//struct IDXGISwapChain;         // Typedef as CCryDXGLSwapChain
//struct IDXGIFactory;           // Typedef as CCryDXGLGIFactory
//struct IDXGIDevice;            // Typedef as CCryDXGLDevice
//struct IDXGIFactory1;          // Typedef as CCryDXGLGIFactory
//struct IDXGIAdapter1;          // Typedef as CCryDXGLGIAdapter
//struct IDXGIDevice1;           // Typedef as CCryDXGLDevice

////////////////////////////////////////////////////////////////////////////
//  Interfaces for full DX emulation
////////////////////////////////////////////////////////////////////////////

#if DXGL_FULL_EMULATION

struct IDXGIObject : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void* pData) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown* pUnknown) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT* pDataSize, void* pData) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void** ppParent) = 0;
};

struct IDXGIOutput : IDXGIObject
{
	virtual HRESULT STDMETHODCALLTYPE GetDesc(DXGI_OUTPUT_DESC* pDesc) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDisplayModeList(DXGI_FORMAT EnumFormat, UINT Flags, UINT* pNumModes, DXGI_MODE_DESC* pDesc) = 0;
	virtual HRESULT STDMETHODCALLTYPE FindClosestMatchingMode(const DXGI_MODE_DESC* pModeToMatch, DXGI_MODE_DESC* pClosestMatch, IUnknown* pConcernedDevice) = 0;
	virtual HRESULT STDMETHODCALLTYPE WaitForVBlank() = 0;
	virtual HRESULT STDMETHODCALLTYPE TakeOwnership(IUnknown* pDevice, BOOL Exclusive) = 0;
	virtual void STDMETHODCALLTYPE    ReleaseOwnership() = 0;
	virtual HRESULT STDMETHODCALLTYPE GetGammaControlCapabilities(DXGI_GAMMA_CONTROL_CAPABILITIES* pGammaCaps) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetGammaControl(const DXGI_GAMMA_CONTROL* pArray) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetGammaControl(DXGI_GAMMA_CONTROL* pArray) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetDisplaySurface(IDXGISurface* pScanoutSurface) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDisplaySurfaceData(IDXGISurface* pDestination) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats) = 0;
};

struct IDXGIAdapter : IDXGIObject
{
	virtual HRESULT STDMETHODCALLTYPE EnumOutputs(UINT Output, IDXGIOutput** ppOutput) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDesc(DXGI_ADAPTER_DESC* pDesc) = 0;
	virtual HRESULT STDMETHODCALLTYPE CheckInterfaceSupport(REFGUID InterfaceName, LARGE_INTEGER* pUMDVersion) = 0;
};

struct IDXGISwapChain;

struct IDXGIFactory : IDXGIObject
{
	virtual HRESULT STDMETHODCALLTYPE EnumAdapters(UINT Adapter, IDXGIAdapter** ppAdapter) = 0;
	virtual HRESULT STDMETHODCALLTYPE MakeWindowAssociation(HWND WindowHandle, UINT Flags) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetWindowAssociation(HWND* pWindowHandle) = 0;
	virtual HRESULT STDMETHODCALLTYPE CreateSwapChain(IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain) = 0;
	virtual HRESULT STDMETHODCALLTYPE CreateSoftwareAdapter(HMODULE Module, IDXGIAdapter** ppAdapter) = 0;
};

struct IDXGIDevice : IDXGIObject
{
	virtual HRESULT STDMETHODCALLTYPE GetAdapter(IDXGIAdapter** pAdapter) = 0;
	virtual HRESULT STDMETHODCALLTYPE CreateSurface(const DXGI_SURFACE_DESC* pDesc, UINT NumSurfaces, DXGI_USAGE Usage, const DXGI_SHARED_RESOURCE* pSharedResource, IDXGISurface** ppSurface) = 0;
	virtual HRESULT STDMETHODCALLTYPE QueryResourceResidency(IUnknown* const* ppResources, DXGI_RESIDENCY* pResidencyStatus, UINT NumResources) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetGPUThreadPriority(INT Priority) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetGPUThreadPriority(INT* pPriority) = 0;
};

struct IDXGIDeviceSubObject : IDXGIObject
{
	virtual HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void** ppDevice) = 0;
};

struct IDXGIAdapter1 : IDXGIAdapter
{
	virtual HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_ADAPTER_DESC1* pDesc) = 0;
};

struct IDXGIDevice1 : IDXGIDevice
{
	virtual HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT* pMaxLatency) = 0;
};

#endif //DXGL_FULL_EMULATION

#endif //__DXGL_DXGI_h__
