// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#pragma warning(push)
#pragma warning(disable: 28204)

#if DX11_WRAPPABLE_INTERFACE && CAPTURE_REPLAY_LOG && (defined(WIN32) || defined (WIN64))
	#define MEMREPLAY_WRAP_D3D11
	#define MEMREPLAY_WRAP_D3D11_CONTEXT
#endif

#if DX11_WRAPPABLE_INTERFACE && CAPTURE_REPLAY_LOG && CRY_PLATFORM_DURANGO
	#define MEMREPLAY_WRAP_D3D11
	#define MEMREPLAY_WRAP_D3D11_CONTEXT
	#define MEMREPLAY_WRAP_XBOX_PERFORMANCE_DEVICE
	#define MEMREPLAY_INSTRUMENT_TEXTUREPOOL

	#if defined(MEMREPLAY_WRAP_XBOX_PERFORMANCE_DEVICE)
		#define MEMREPLAY_HIDE_BANKALLOC() MEMREPLAY_SCOPE(EMemReplayAllocClass::C_UserPointer, EMemReplayUserPointerClass::C_CryMalloc)
	#else
		#define MEMREPLAY_HIDE_BANKALLOC() 
	#endif
#endif

#if !defined(MEMREPLAY_HIDE_BANKALLOC)
	#define MEMREPLAY_HIDE_BANKALLOC() 
#endif

//=================================================================

#ifdef MEMREPLAY_WRAP_D3D11

class MemReplayD3DAnnotation : public IUnknown
{
public:
	static const GUID s_guid;

public:
	MemReplayD3DAnnotation(ID3D11DeviceChild* pRes, size_t sz);
	~MemReplayD3DAnnotation();

	void                              AddMap(UINT nSubRes, void* pData, size_t sz);
	void                              RemoveMap(UINT nSubRes);

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
	virtual ULONG STDMETHODCALLTYPE   AddRef();
	virtual ULONG STDMETHODCALLTYPE   Release();

private:
	struct MapDesc
	{
		UINT  nSubResource;
		void* pData;
	};

private:
	MemReplayD3DAnnotation(const MemReplayD3DAnnotation&);
	MemReplayD3DAnnotation& operator=(const MemReplayD3DAnnotation&);

private:
	ULONG                m_nRefCount;
	ID3D11DeviceChild*   m_pRes;
	std::vector<MapDesc> m_maps;
};

inline void MemReplayAnnotateD3DResource(ID3D11DeviceChild* pResource, size_t resSz)
{
	if (pResource)
	{
		UINT sz = sizeof(MemReplayD3DAnnotation*);
		MemReplayD3DAnnotation* pAnnotation;
		if (FAILED(pResource->GetPrivateData(MemReplayD3DAnnotation::s_guid, &sz, &pAnnotation)))
		{
			pAnnotation = new MemReplayD3DAnnotation(pResource, resSz);
			pResource->SetPrivateDataInterface(MemReplayD3DAnnotation::s_guid, pAnnotation);
		}
	}
}

inline MemReplayD3DAnnotation* MemReplayGetD3DAnnotation(ID3D11DeviceChild* pResource)
{
	if (pResource)
	{
		UINT sz = sizeof(MemReplayD3DAnnotation*);
		MemReplayD3DAnnotation* pAnnotation;
		if (!FAILED(pResource->GetPrivateData(MemReplayD3DAnnotation::s_guid, &sz, &pAnnotation)))
		{
			return pAnnotation;
		}
	}
	return NULL;
}

#endif

//=================================================================

#if defined(MEMREPLAY_WRAP_D3D11)
class CCryDeviceMemReplayHook : public ICryDeviceWrapperHook
{
public:
	CCryDeviceMemReplayHook();
	virtual const char* Name() const;

	/*** D3D11 Device ***/
	virtual void CreateBuffer_PostCallHook(HRESULT hr, const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer);
	virtual void CreateTexture1D_PostCallHook(HRESULT hr, const D3D11_TEXTURE1D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture1D** ppTexture1D);
	virtual void CreateTexture2D_PostCallHook(HRESULT hr, const D3D11_TEXTURE2D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture2D** ppTexture2D);
	virtual void CreateTexture3D_PostCallHook(HRESULT hr, const D3D11_TEXTURE3D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture3D** ppTexture3D);

	#if defined(MEMREPLAY_WRAP_XBOX_PERFORMANCE_DEVICE)
	virtual void CreatePlacementBuffer_PostCallHook(HRESULT hr, const D3D11_BUFFER_DESC* pDesc, void* pCpuVirtualAddress, ID3D11Buffer** ppBuffer);
	virtual void CreatePlacementTexture1D_PostCallHook(HRESULT hr, const D3D11_TEXTURE1D_DESC* pDesc, UINT TileModeIndex, UINT Pitch, void* pCpuVirtualAddress, ID3D11Texture1D** ppTexture1D);
	virtual void CreatePlacementTexture2D_PostCallHook(HRESULT hr, const D3D11_TEXTURE2D_DESC* pDesc, UINT TileModeIndex, UINT Pitch, void* pCpuVirtualAddress, ID3D11Texture2D** ppTexture2D);
	virtual void CreatePlacementTexture3D_PostCallHook(HRESULT hr, const D3D11_TEXTURE3D_DESC* pDesc, UINT TileModeIndex, UINT Pitch, void* pCpuVirtualAddress, ID3D11Texture3D** ppTexture3D);
	#endif // MEMREPLAY_WRAP_XBOX_PERFORMANCE_DEVICE

	#if defined(MEMREPLAY_WRAP_D3D11_CONTEXT)
	virtual void Map_PostCallHook(HRESULT hr, ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource);
	virtual void Unmap_PreCallHook(ID3D11Resource* pResource, UINT Subresource);
	#endif // MEMREPLAY_WRAP_D3D11_CONTEXT
};

#endif // MEMREPLAY_WRAP_D3D11
