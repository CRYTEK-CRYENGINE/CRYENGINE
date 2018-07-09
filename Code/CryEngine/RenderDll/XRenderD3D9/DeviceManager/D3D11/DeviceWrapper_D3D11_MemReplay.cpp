// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DriverD3D.h"

#include "DeviceWrapper_D3D11_MemReplay.h"

//=================================================================

#ifdef MEMREPLAY_WRAP_D3D11

const GUID MemReplayD3DAnnotation::s_guid = {
	{ 0xff23dad0 },{ 0xd47b },{ 0x4d80 },{ 0xaf, 0x63, 0x7e, 0xf5, 0xea, 0x2c, 0x4a, 0x9d }
};

MemReplayD3DAnnotation::MemReplayD3DAnnotation(ID3D11DeviceChild* pRes, size_t sz)
	: m_nRefCount(0)
	, m_pRes(pRes)
{
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_D3DManaged, 0);
	MEMREPLAY_SCOPE_ALLOC(pRes, sz, 0);
}

MemReplayD3DAnnotation::~MemReplayD3DAnnotation()
{
	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_D3DManaged, 0);
	MEMREPLAY_SCOPE_FREE(m_pRes);
}

void MemReplayD3DAnnotation::AddMap(UINT nSubRes, void* pData, size_t sz)
{
	MapDesc desc;
	desc.nSubResource = nSubRes;
	desc.pData = pData;
	m_maps.push_back(desc);

	MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
	MEMREPLAY_SCOPE_ALLOC(pData, sz, 0);
}

void MemReplayD3DAnnotation::RemoveMap(UINT nSubRes)
{
	for (std::vector<MapDesc>::reverse_iterator it = m_maps.rbegin(), itEnd = m_maps.rend(); it != itEnd; ++it)
	{
		if (it->nSubResource == nSubRes)
		{
			MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc);
			MEMREPLAY_SCOPE_FREE(it->pData);

			m_maps.erase(m_maps.begin() + std::distance(&*m_maps.begin(), &*it));
			if (m_maps.empty())
				stl::free_container(m_maps);
			break;
		}
	}
}

HRESULT MemReplayD3DAnnotation::QueryInterface(REFIID riid, void** ppvObject)
{
	if (riid == IID_IUnknown)
	{
		*reinterpret_cast<IUnknown**>(ppvObject) = this;
		return S_OK;
	}

	*ppvObject = NULL;
	return E_NOINTERFACE;
}

ULONG MemReplayD3DAnnotation::AddRef()
{
	return ++m_nRefCount;
}

ULONG MemReplayD3DAnnotation::Release()
{
	ULONG nr = --m_nRefCount;

	if (!nr)
		delete this;

	return nr;
}

#endif

//=================================================================

#ifdef MEMREPLAY_WRAP_D3D11
static size_t ComputeSurfaceSize(DWORD width, DWORD height, DWORD depth, DXGI_FORMAT fmt)
{
	switch (fmt)
	{
	default:
	case DXGI_FORMAT_UNKNOWN:
		return width * height * depth;

	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
		return width * height * depth * 16;

	case DXGI_FORMAT_R32G32B32_TYPELESS:
	case DXGI_FORMAT_R32G32B32_FLOAT:
	case DXGI_FORMAT_R32G32B32_UINT:
	case DXGI_FORMAT_R32G32B32_SINT:
	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R32G32_SINT:
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		return width * height * depth * 8;

	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UINT:
	case DXGI_FORMAT_R11G11B10_FLOAT:
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_R16G16_TYPELESS:
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_SINT:
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
	case DXGI_FORMAT_R8G8_B8G8_UNORM:
	case DXGI_FORMAT_G8R8_G8B8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
		return width * height * depth * 4;

	case DXGI_FORMAT_R8G8_TYPELESS:
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R8G8_SNORM:
	case DXGI_FORMAT_R8G8_SINT:
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SNORM:
	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_B5G6R5_UNORM:
	case DXGI_FORMAT_B5G5R5A1_UNORM:
		return width * height * depth * 2;

	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SNORM:
	case DXGI_FORMAT_R8_SINT:
	case DXGI_FORMAT_A8_UNORM:
		return width * height * depth;

	case DXGI_FORMAT_R1_UNORM:
		return width * height * depth / 8;

	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
		return width * height * depth / 2;

	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
	case DXGI_FORMAT_BC7_TYPELESS:
	case DXGI_FORMAT_BC7_UNORM:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		return width * height * depth;
	}
}

static size_t ComputeSurfaceSize(DWORD width, DWORD height, DWORD depth, DWORD mipLevels, DXGI_FORMAT fmt)
{
	size_t sz = 0;

	for (DWORD i = 0; i < mipLevels; ++i, width = std::max<DWORD>(width >> 1, 1), height = std::max<DWORD>(height >> 1, 1), depth = std::max<DWORD>(depth >> 1, 1))
		sz += ComputeSurfaceSize(width, height, depth, fmt);

	return sz;
}

///////////////////////////////////////////////////////////////////////////////
CCryDeviceMemReplayHook::CCryDeviceMemReplayHook()
{
}

///////////////////////////////////////////////////////////////////////////////
const char* CCryDeviceMemReplayHook::Name() const
{
	return "CCryDeviceMemReplayHook";
}

///////////////////////////////////////////////////////////////////////////////
void CCryDeviceMemReplayHook::CreateBuffer_PostCallHook(HRESULT hr, const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer)
{
	if (FAILED(hr))
		return;

	ID3D11Buffer* pBuffer = *ppBuffer;

	D3D11_BUFFER_DESC desc;
	pBuffer->GetDesc(&desc);

	MemReplayAnnotateD3DResource(pBuffer, desc.ByteWidth);
}

///////////////////////////////////////////////////////////////////////////////
void CCryDeviceMemReplayHook::CreateTexture1D_PostCallHook(HRESULT hr, const D3D11_TEXTURE1D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture1D** ppTexture1D)
{
	if (FAILED(hr))
		return;

	ID3D11Texture1D* pTexture = *ppTexture1D;

	D3D11_TEXTURE1D_DESC desc;
	pTexture->GetDesc(&desc);

	MemReplayAnnotateD3DResource(pTexture, desc.ArraySize * ComputeSurfaceSize(desc.Width, 1, 1, desc.MipLevels, desc.Format));
}

///////////////////////////////////////////////////////////////////////////////
void CCryDeviceMemReplayHook::CreateTexture2D_PostCallHook(HRESULT hr, const D3D11_TEXTURE2D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture2D** ppTexture2D)
{
	if (FAILED(hr))
		return;

	ID3D11Texture2D* pTexture = *ppTexture2D;

	D3D11_TEXTURE2D_DESC desc;
	pTexture->GetDesc(&desc);

	MemReplayAnnotateD3DResource(pTexture, desc.ArraySize * ComputeSurfaceSize(desc.Width, desc.Height, 1, desc.MipLevels, desc.Format));
}

///////////////////////////////////////////////////////////////////////////////
void CCryDeviceMemReplayHook::CreateTexture3D_PostCallHook(HRESULT hr, const D3D11_TEXTURE3D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture3D** ppTexture3D)
{
	if (FAILED(hr))
		return;

	ID3D11Texture3D* pTexture = *ppTexture3D;

	D3D11_TEXTURE3D_DESC desc;
	pTexture->GetDesc(&desc);

	MemReplayAnnotateD3DResource(pTexture, ComputeSurfaceSize(desc.Width, desc.Height, desc.Depth, desc.MipLevels, desc.Format));
}

///////////////////////////////////////////////////////////////////////////////
	#if defined(MEMREPLAY_WRAP_XBOX_PERFORMANCE_DEVICE)
void CCryDeviceMemReplayHook::CreatePlacementBuffer_PostCallHook(HRESULT hr, const D3D11_BUFFER_DESC* pDesc, void* pCpuVirtualAddress, ID3D11Buffer** ppBuffer)
{
	if (FAILED(hr))
		return;

	ID3D11Buffer* pBuffer = *ppBuffer;

	D3D11_BUFFER_DESC desc;
	pBuffer->GetDesc(&desc);

	MemReplayAnnotateD3DResource(pBuffer, desc.ByteWidth);
}

///////////////////////////////////////////////////////////////////////////////
void CCryDeviceMemReplayHook::CreatePlacementTexture1D_PostCallHook(HRESULT hr, const D3D11_TEXTURE1D_DESC* pDesc, UINT TileModeIndex, UINT Pitch, void* pCpuVirtualAddress, ID3D11Texture1D** ppTexture1D)
{
		#if !defined(MEMREPLAY_INSTRUMENT_TEXTUREPOOL)
	if (FAILED(hr))
		return;

	ID3D11Texture1D* pTexture = *ppTexture1D;

	D3D11_TEXTURE1D_DESC desc;
	pTexture->GetDesc(&desc);

	MemReplayAnnotateD3DResource(pTexture, ComputeSurfaceSize(desc.Width, 1, 1, desc.MipLevels, desc.ArraySize, desc.Format, (XG_TILE_MODE)TileModeIndex));
		#endif
}

///////////////////////////////////////////////////////////////////////////////
void CCryDeviceMemReplayHook::CreatePlacementTexture2D_PostCallHook(HRESULT hr, const D3D11_TEXTURE2D_DESC* pDesc, UINT TileModeIndex, UINT Pitch, void* pCpuVirtualAddress, ID3D11Texture2D** ppTexture2D)
{
		#if !defined(MEMREPLAY_INSTRUMENT_TEXTUREPOOL)
	if (FAILED(hr))
		return;

	ID3D11Texture2D* pTexture = *ppTexture2D;

	D3D11_TEXTURE2D_DESC desc;
	pTexture->GetDesc(&desc);

	MemReplayAnnotateD3DResource(pTexture, ComputeSurfaceSize(desc.Width, desc.Height, 1, desc.MipLevels, desc.ArraySize, desc.Format, (XG_TILE_MODE)TileModeIndex));
		#endif
}

///////////////////////////////////////////////////////////////////////////////
void CCryDeviceMemReplayHook::CreatePlacementTexture3D_PostCallHook(HRESULT hr, const D3D11_TEXTURE3D_DESC* pDesc, UINT TileModeIndex, UINT Pitch, void* pCpuVirtualAddress, ID3D11Texture3D** ppTexture3D)
{
		#if !defined(MEMREPLAY_INSTRUMENT_TEXTUREPOOL)
	if (FAILED(hr))
		return;

	ID3D11Texture3D* pTexture = *ppTexture3D;

	D3D11_TEXTURE3D_DESC desc;
	pTexture->GetDesc(&desc);

	MemReplayAnnotateD3DResource(pTexture, ComputeSurfaceSize(desc.Width, desc.Height, desc.Depth, desc.MipLevels, 1, desc.Format, (XG_TILE_MODE)TileModeIndex));
		#endif
}

	#endif // MEMREPLAY_WRAP_XBOX_PERFORMANCE_DEVICE

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
	#if defined(MEMREPLAY_WRAP_D3D11_CONTEXT)
///////////////////////////////////////////////////////////////////////////////
void CCryDeviceMemReplayHook::Map_PostCallHook(HRESULT hr, ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource)
{
	MemReplayD3DAnnotation* pAnnotation = MemReplayGetD3DAnnotation(pResource);
	if (pAnnotation)
	{
		size_t sz = 0;

		ID3D11Buffer* pBuffer;
		ID3D11Texture1D* pTex1D;
		ID3D11Texture2D* pTex2D;
		ID3D11Texture3D* pTex3D;

		if (!FAILED(pResource->QueryInterface(__uuidof(ID3D11Buffer), (void**)&pBuffer)))
		{
			D3D11_BUFFER_DESC desc;
			pBuffer->GetDesc(&desc);
			sz = desc.ByteWidth;

			pBuffer->Release();
		}
		else if (!FAILED(pResource->QueryInterface(__uuidof(ID3D11Texture1D), (void**)&pTex1D)))
		{
			D3D11_TEXTURE1D_DESC desc;
			pTex1D->GetDesc(&desc);

			UINT nMip = Subresource % desc.MipLevels;
			sz = ComputeSurfaceSize(max(1U, desc.Width >> nMip), 1, 1, 1, desc.Format);

			pTex1D->Release();
		}
		else if (!FAILED(pResource->QueryInterface(__uuidof(ID3D11Texture3D), (void**)&pTex2D)))
		{
			D3D11_TEXTURE2D_DESC desc;
			pTex2D->GetDesc(&desc);

			UINT nMip = Subresource % desc.MipLevels;
			sz = ComputeSurfaceSize(max(1U, desc.Width >> nMip), max(1U, desc.Height >> nMip), 1, 1, desc.Format);

			pTex2D->Release();
		}
		else if (!FAILED(pResource->QueryInterface(__uuidof(ID3D11Texture1D), (void**)&pTex3D)))
		{
			D3D11_TEXTURE3D_DESC desc;
			pTex3D->GetDesc(&desc);

			UINT nMip = Subresource % desc.MipLevels;
			sz = ComputeSurfaceSize(max(1U, desc.Width >> nMip), max(1U, desc.Height >> nMip), max(1U, desc.Depth >> nMip), 1, desc.Format);

			pTex3D->Release();
		}

		if (sz)
		{
			pAnnotation->AddMap(Subresource, pMappedResource->pData, sz);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
void CCryDeviceMemReplayHook::Unmap_PreCallHook(ID3D11Resource* pResource, UINT Subresource)
{
	MemReplayD3DAnnotation* pAnnotation = MemReplayGetD3DAnnotation(pResource);
	if (pAnnotation)
	{
		pAnnotation->RemoveMap(Subresource);
	}
}

	#endif // MEMREPLAY_WRAP_D3D11_CONTEXT
#endif   // MEMREPLAY_WRAP_D3D11

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::MemReplayWrapD3DDevice()
{
#if DX11_WRAPPABLE_INTERFACE && CAPTURE_REPLAY_LOG
	CryReplayInfo replayInfo;
	CryGetIMemReplay()->GetInfo(replayInfo);

	if (replayInfo.filename)
	{
	#if defined(MEMREPLAY_WRAP_D3D11)
		RegisterDeviceWrapperHook(new CCryDeviceMemReplayHook());
	#endif // MEMREPLAY_WRAP_D3D11
	}
#endif
}
