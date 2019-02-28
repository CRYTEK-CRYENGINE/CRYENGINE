// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <DriverD3D.h>
////////////////////////////////////////////////////////////////////////////
// ResourceView API

CDeviceResourceView* CDeviceResource::CreateResourceView(const SResourceView pView)
{
	HRESULT hr = E_FAIL;
	CDeviceResourceView* pResult = nullptr;
	D3DResource* pResource = GetNativeResource();

	// DX11 can not create NULL-views to NULL-resources (unlike DX12 fe.)
	if (!pResource)
		return pResult;

	// DX expects -1 for selecting all mip maps/slices. max count throws an exception
	const uint nMipCount = pView.m_Desc.nMipCount == SResourceView(0ULL).m_Desc.nMipCount || pView.m_Desc.nMipCount == SResourceView(~0ULL).m_Desc.nMipCount ? (uint) - 1 : (uint)pView.m_Desc.nMipCount;
	const uint nSliceCount = pView.m_Desc.nSliceCount == SResourceView(0ULL).m_Desc.nSliceCount || pView.m_Desc.nSliceCount == SResourceView(~0ULL).m_Desc.nSliceCount ? (uint) - 1 : (uint)pView.m_Desc.nSliceCount;

	const uint nElemsAvailable = m_resourceElements;
	const uint nMipAvailable = m_subResources[eSubResource_Mips];
	const uint nSliceAvailable = m_subResources[eSubResource_Slices];

	if (pView.m_Desc.eViewType == SResourceView::eShaderResourceView)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroStruct(srvDesc);
		srvDesc.Format = DXGI_FORMAT(pView.m_Desc.nFormat);

		// NOTE formats needs to be set precisely in the view, otherwise multiple distinct view descs might produce multiple views which are in fact identical
		CRY_ASSERT(
		  !DeviceFormats::IsDepthStencil(DXGI_FORMAT(pView.m_Desc.nFormat)) ||
		  ((pView.m_Desc.nFlags & SResourceView::eSRV_StencilOnly) && (srvDesc.Format == DeviceFormats::ConvertToStencilOnly(DXGI_FORMAT(pView.m_Desc.nFormat)))) ||
		  (!(pView.m_Desc.nFlags & SResourceView::eSRV_StencilOnly) && (srvDesc.Format == DeviceFormats::ConvertToDepthOnly(DXGI_FORMAT(pView.m_Desc.nFormat))))
		  );

		// TODO: implement same constraint as for depth/stencil
		if (DeviceFormats::IsSRGBReadable(DXGI_FORMAT(pView.m_Desc.nFormat)))
		{
			// sRGB SRV
			if (pView.m_Desc.bSrgbRead)
				srvDesc.Format = DeviceFormats::ConvertToSRGB(DXGI_FORMAT(pView.m_Desc.nFormat));
		}

		if (pView.m_Desc.bMultisample && m_eTT == eTT_2D)
		{
			srvDesc.ViewDimension = nSliceAvailable > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY : D3D11_SRV_DIMENSION_TEXTURE2DMS;

			if (nSliceAvailable > 1)
			{
				srvDesc.Texture2DMSArray.FirstArraySlice = pView.m_Desc.nFirstSlice;
				srvDesc.Texture2DMSArray.ArraySize = nSliceCount;

				CRY_ASSERT(srvDesc.Texture2DMSArray.ArraySize == ~0 || (srvDesc.Texture2DMSArray.ArraySize > 0 && srvDesc.Texture2DMSArray.ArraySize <= nSliceAvailable));
			}
		}
		else if (m_bFilterable)
		{
			// *INDENT-OFF*
			srvDesc.ViewDimension =
				 m_eTT == eTT_1D        ? (nSliceAvailable  > 1 ? D3D11_SRV_DIMENSION_TEXTURE1DARRAY   : D3D11_SRV_DIMENSION_TEXTURE1D)
			:	(m_eTT == eTT_2D        ? (nSliceAvailable  > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DARRAY   : D3D11_SRV_DIMENSION_TEXTURE2D)
			:	(m_eTT == eTT_Cube      ? (nSliceAvailable  > 6 ? D3D11_SRV_DIMENSION_TEXTURECUBEARRAY : D3D11_SRV_DIMENSION_TEXTURECUBE)
			:	(m_eTT == eTT_3D        ? (nSliceAvailable  > 1 ? D3D11_SRV_DIMENSION_UNKNOWN          : D3D11_SRV_DIMENSION_TEXTURE3D)
			:	(m_eTT == eTT_2DArray   ?                         D3D11_SRV_DIMENSION_TEXTURE2DARRAY
			:	(m_eTT == eTT_CubeArray ?                         D3D11_SRV_DIMENSION_TEXTURECUBEARRAY : D3D11_SRV_DIMENSION_UNKNOWN)))));
			// *INDENT-ON*

			// D3D11_TEX1D_SRV, D3D11_TEX2D_SRV, D3D11_TEX3D_SRV, D3D11_TEXCUBE_SRV and array versions can be aliased here
			srvDesc.Texture1D.MostDetailedMip = pView.m_Desc.nMostDetailedMip;
			srvDesc.Texture1D.MipLevels = nMipCount;

			CRY_ASSERT(srvDesc.Texture1D.MipLevels == ~0 || (srvDesc.Texture1D.MipLevels > 0 && srvDesc.Texture1D.MipLevels <= nMipAvailable));

			if (nSliceAvailable > 1)
			{
				srvDesc.Texture1DArray.FirstArraySlice = pView.m_Desc.nFirstSlice;
				srvDesc.Texture1DArray.ArraySize = nSliceCount;

				CRY_ASSERT(srvDesc.Texture1DArray.ArraySize == ~0 || (srvDesc.Texture1DArray.ArraySize > 0 && srvDesc.Texture1DArray.ArraySize <= nSliceAvailable));
			}
		}
		else
		{
			srvDesc.ViewDimension = pView.m_Desc.bRaw ? D3D11_SRV_DIMENSION_BUFFEREX : D3D11_SRV_DIMENSION_BUFFER;

			if (nElemsAvailable > 0)
			{
				srvDesc.Buffer.FirstElement = UINT(pView.m_Desc.nOffsetAndSize >> (46 - pView.m_Desc.nOffsetBits));
				srvDesc.Buffer.NumElements = UINT(pView.m_Desc.nOffsetAndSize & MASK64(46 - pView.m_Desc.nOffsetBits));

				CRY_ASSERT(srvDesc.Buffer.NumElements > 0 && srvDesc.Buffer.NumElements <= nElemsAvailable);
			}
		}

		D3DShaderResource* pSRV = NULL;
		hr = gcpRendD3D->GetDevice().CreateShaderResourceView(pResource, &srvDesc, &pSRV);
		pResult = pSRV;
	}
	else // SResourceView::eRenderTargetView || SResourceView::eDepthStencilView || SResourceView::eUnorderedAccessView)
	{
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
		ZeroStruct(rtvDesc);
		rtvDesc.Format = DXGI_FORMAT(pView.m_Desc.nFormat);

		if (pView.m_Desc.bMultisample && m_eTT == eTT_2D && pView.m_Desc.eViewType != SResourceView::eUnorderedAccessView)
		{
			rtvDesc.ViewDimension = nSliceAvailable > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY : D3D11_RTV_DIMENSION_TEXTURE2DMS;

			if (nSliceAvailable > 1)
			{
				rtvDesc.Texture2DMSArray.FirstArraySlice = pView.m_Desc.nFirstSlice;
				rtvDesc.Texture2DMSArray.ArraySize = nSliceCount;

				CRY_ASSERT(rtvDesc.Texture2DMSArray.ArraySize == ~0 || (rtvDesc.Texture2DMSArray.ArraySize > 0 && rtvDesc.Texture2DMSArray.ArraySize <= nSliceAvailable));
			}
		}
		else if (m_bFilterable)
		{
			// *INDENT-OFF*
			rtvDesc.ViewDimension =
				 m_eTT == eTT_1D        ? (nSliceAvailable  > 1 ? D3D11_RTV_DIMENSION_TEXTURE1DARRAY : D3D11_RTV_DIMENSION_TEXTURE1D)
			:	(m_eTT == eTT_2D        ? (nSliceAvailable  > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DARRAY : D3D11_RTV_DIMENSION_TEXTURE2D)
			:	(m_eTT == eTT_Cube      ?                         D3D11_RTV_DIMENSION_TEXTURE2DARRAY
			:	(m_eTT == eTT_3D        ? (nSliceAvailable  > 1 ? D3D11_RTV_DIMENSION_UNKNOWN        : D3D11_RTV_DIMENSION_TEXTURE3D)
			:	(m_eTT == eTT_2DArray   ?                         D3D11_RTV_DIMENSION_TEXTURE2DARRAY
			:	(m_eTT == eTT_CubeArray ?                         D3D11_RTV_DIMENSION_TEXTURE2DARRAY : D3D11_RTV_DIMENSION_UNKNOWN)))));
			// *INDENT-ON*

			rtvDesc.Texture1D.MipSlice = pView.m_Desc.nMostDetailedMip;

			if (nSliceAvailable > 1 || m_eTT == eTT_3D || m_eTT == eTT_Cube || m_eTT == eTT_CubeArray)
			{
				rtvDesc.Texture1DArray.FirstArraySlice = pView.m_Desc.nFirstSlice;
				rtvDesc.Texture1DArray.ArraySize = nSliceCount;

				CRY_ASSERT(rtvDesc.Texture1DArray.ArraySize == ~0 || (rtvDesc.Texture1DArray.ArraySize > 0 && rtvDesc.Texture1DArray.ArraySize <= nSliceAvailable));
			}
		}
		else
		{
			rtvDesc.ViewDimension = pView.m_Desc.bRaw ? D3D11_RTV_DIMENSION_BUFFER : D3D11_RTV_DIMENSION_BUFFER;

			if (nElemsAvailable > 0)
			{
				rtvDesc.Buffer.FirstElement = UINT(pView.m_Desc.nOffsetAndSize >> (46 - pView.m_Desc.nOffsetBits));
				rtvDesc.Buffer.NumElements = UINT(pView.m_Desc.nOffsetAndSize & MASK64(46 - pView.m_Desc.nOffsetBits));

				CRY_ASSERT(rtvDesc.Buffer.NumElements > 0 && rtvDesc.Buffer.NumElements <= nElemsAvailable);
			}
		}

		switch (pView.m_Desc.eViewType)
		{
		case SResourceView::eRenderTargetView:
			{
				D3DSurface* pRTV = NULL;
				hr = gcpRendD3D->GetDevice().CreateRenderTargetView(pResource, &rtvDesc, &pRTV);
				pResult = pRTV;
			}
			break;
		case SResourceView::eDepthStencilView:
			{
				D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
				ZeroStruct(dsvDesc);
				dsvDesc.Format = DXGI_FORMAT(pView.m_Desc.nFormat);

				// NOTE formats needs to be set precisely in the view, otherwise multiple distinct view descs might produce multiple views which are in fact identical
				CRY_ASSERT(
				  !DeviceFormats::IsDepthStencil(DXGI_FORMAT(pView.m_Desc.nFormat)) ||
				  (dsvDesc.Format == DeviceFormats::ConvertToDepthStencil(DXGI_FORMAT(pView.m_Desc.nFormat)))
				  );

				dsvDesc.Flags = pView.m_Desc.nFlags;
				memcpy(&dsvDesc.Texture1DArray, &rtvDesc.Texture1DArray, sizeof(dsvDesc.Texture1DArray));

				// Depth/Stencil read only DSV
				if (pView.m_Desc.nFlags & SResourceView::eDSV_ReadOnly)
					dsvDesc.Flags |= (D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL);
				// Depth/Stencil read/write DSV
				else
					dsvDesc.Flags &= ~(D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL);

				if (rtvDesc.ViewDimension != D3D11_RTV_DIMENSION_UNKNOWN &&
				    rtvDesc.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE3D)
					dsvDesc.ViewDimension = (D3D11_DSV_DIMENSION)(rtvDesc.ViewDimension - 1);

				D3DDepthSurface* pDSV = NULL;
				hr = gcpRendD3D->GetDevice().CreateDepthStencilView(pResource, &dsvDesc, &pDSV);
				pResult = pDSV;
			}
			break;
		case SResourceView::eUnorderedAccessView:
			{
				D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
				memcpy(&uavDesc, &rtvDesc, sizeof(uavDesc));

				// typed UAV loads are only allowed for single-component 32-bit element types
				if (pView.m_Desc.nFlags & SResourceView::eUAV_ReadWrite)
					uavDesc.Format = DXGI_FORMAT_R32_UINT;
				if (pView.m_Desc.bRaw)
					uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

				if (rtvDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMS ||
				    rtvDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY)
					uavDesc.ViewDimension = D3D11_UAV_DIMENSION_UNKNOWN;

				D3DUAV* pUAV = NULL;
				hr = gcpRendD3D->GetDevice().CreateUnorderedAccessView(pResource, &uavDesc, &pUAV);
				pResult = pUAV;
			}
			break;
		}
	}

	if (FAILED(hr))
	{
		assert(0);
		return NULL;
	}

	return pResult;
}

template<typename T>
static inline uint32 ConvertFromDX11Flags(const T& desc)
{
	// *INDENT-OFF*
	return
		((desc.BindFlags      & D3D11_BIND_VERTEX_BUFFER             ) ? CDeviceObjectFactory::BIND_VERTEX_BUFFER    : 0) |
		((desc.BindFlags      & D3D11_BIND_INDEX_BUFFER              ) ? CDeviceObjectFactory::BIND_INDEX_BUFFER     : 0) |
		((desc.BindFlags      & D3D11_BIND_CONSTANT_BUFFER           ) ? CDeviceObjectFactory::BIND_CONSTANT_BUFFER  : 0) |
		((desc.BindFlags      & D3D11_BIND_SHADER_RESOURCE           ) ? CDeviceObjectFactory::BIND_SHADER_RESOURCE  : 0) |
	//	((desc.BindFlags      & D3D11_BIND_STREAM_OUTPUT             ) ? CDeviceObjectFactory::BIND_STREAM_OUTPUT    : 0) |
		((desc.BindFlags      & D3D11_BIND_RENDER_TARGET             ) ? CDeviceObjectFactory::BIND_RENDER_TARGET    : 0) |
		((desc.BindFlags      & D3D11_BIND_DEPTH_STENCIL             ) ? CDeviceObjectFactory::BIND_DEPTH_STENCIL    : 0) |
		((desc.BindFlags      & D3D11_BIND_UNORDERED_ACCESS          ) ? CDeviceObjectFactory::BIND_UNORDERED_ACCESS : 0) |
		((desc.CPUAccessFlags & D3D11_CPU_ACCESS_WRITE               ) ? CDeviceObjectFactory::USAGE_CPU_WRITE       : 0) |
		((desc.CPUAccessFlags & D3D11_CPU_ACCESS_READ                ) ? CDeviceObjectFactory::USAGE_CPU_READ        : 0) |
		((desc.MiscFlags      & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) ? CDeviceObjectFactory::USAGE_STRUCTURED      : 0) |
		((desc.MiscFlags      & D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS) ? CDeviceObjectFactory::USAGE_INDIRECTARGS    : 0) |
		((desc.MiscFlags      & D3D11_RESOURCE_MISC_UAV_OVERLAP      ) ? CDeviceObjectFactory::USAGE_UAV_OVERLAP     : 0) |
		((desc.MiscFlags      & D3D11_RESOURCE_MISC_HIFREQ_HEAP      ) ? CDeviceObjectFactory::USAGE_HIFREQ_HEAP     : 0) |
		((desc.Usage         == D3D11_USAGE_DYNAMIC                  ) ? CDeviceObjectFactory::USAGE_CPU_WRITE       : 0) |
		((desc.Usage         == D3D11_USAGE_STAGING                  ) ? CDeviceObjectFactory::USAGE_CPU_READ        : 0);
	// *INDENT-ON*
}

SResourceLayout CDeviceResource::GetLayout() const
{
	SResourceLayout Layout = { 0 };

	if (!GetNativeResource())
		return Layout;

	D3D11_RESOURCE_DIMENSION dim;
	GetNativeResource()->GetType(&dim);
	switch (dim)
	{
	case D3D11_RESOURCE_DIMENSION_BUFFER:
		{
			D3D11_BUFFER_DESC desc;
			reinterpret_cast<D3DBuffer*>(GetNativeResource())->GetDesc(&desc);
			Layout.m_byteCount = desc.ByteWidth;
			Layout.m_elementCount = desc.ByteWidth / (m_eNativeFormat == DXGI_FORMAT_UNKNOWN ? std::max(1U, desc.StructureByteStride) : DeviceFormats::GetStride(m_eNativeFormat));
			Layout.m_eFlags = ConvertFromDX11Flags(desc);
		}
		break;
	case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
		{
			D3D11_TEXTURE1D_DESC desc;
			reinterpret_cast<D3DLookupTexture*>(GetNativeResource())->GetDesc(&desc);
			Layout.m_byteCount = desc.Width * desc.ArraySize * DeviceFormats::GetStride(desc.Format);
			Layout.m_elementCount = desc.Width * desc.ArraySize;
			Layout.m_eFlags = ConvertFromDX11Flags(desc);
		}
		break;
	case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
		{
			D3D11_TEXTURE2D_DESC desc;
			reinterpret_cast<D3DTexture*>(GetNativeResource())->GetDesc(&desc);
			Layout.m_byteCount = desc.Width * desc.Height * desc.ArraySize * DeviceFormats::GetStride(desc.Format);
			Layout.m_elementCount = desc.Width * desc.Height * desc.ArraySize;
			Layout.m_eFlags = ConvertFromDX11Flags(desc);
		}
		break;
	case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
		{
			D3D11_TEXTURE3D_DESC desc;
			reinterpret_cast<D3DVolumeTexture*>(GetNativeResource())->GetDesc(&desc);
			Layout.m_byteCount = desc.Width * desc.Height * desc.Depth * DeviceFormats::GetStride(desc.Format);
			Layout.m_elementCount = desc.Width * desc.Height * desc.Depth;
			Layout.m_eFlags = ConvertFromDX11Flags(desc);
		}
		break;
	}

	return Layout;
}

CDeviceResource::ESubstitutionResult CDeviceResource::SubstituteUsedResource()
{
	auto& rFactory = CDeviceObjectFactory::GetInstance();
	const auto& fenceManager = rFactory.GetDX11Scheduler()->GetFenceManager();

	// NOTE: Poor man's resource tracking (take current time as last-used moment)
	HRESULT hResult = rFactory.GetDX11Device()->SubstituteUsedCommittedResource(fenceManager.GetCurrentValues(), &m_pNativeResource);

	if (hResult == S_FALSE)
		return eSubResult_Kept;
	if (hResult == E_FAIL)
		return eSubResult_Failed;

	ReleaseResourceViews();
	AllocatePredefinedResourceViews();

	// TODO: also call this when it is moved to CDeviceResource
	// InvalidateDeviceResource(uint32 dirtyFlags);

	return eSubResult_Substituted;
}

////////////////////////////////////////////////////////////////////////////

SBufferLayout CDeviceBuffer::GetLayout() const
{
	SBufferLayout Layout = { m_eNativeFormat };

	if (!GetBaseBuffer())
		return Layout;

	D3D11_RESOURCE_DIMENSION dim;
	GetBaseBuffer()->GetType(&dim);
	switch (dim)
	{
	case D3D11_RESOURCE_DIMENSION_BUFFER:
		{
			D3D11_BUFFER_DESC desc;
			GetBuffer()->GetDesc(&desc);
			Layout.m_elementSize = (m_eNativeFormat == DXGI_FORMAT_UNKNOWN ? desc.StructureByteStride : DeviceFormats::GetStride(m_eNativeFormat));
			Layout.m_elementCount = desc.ByteWidth / Layout.m_elementSize;
			Layout.m_eFlags = ConvertFromDX11Flags(desc);
		}
		break;
	}

	return Layout;
}

SResourceMemoryAlignment CDeviceBuffer::GetAlignment() const
{
	SResourceMemoryAlignment Alignment = { 0 };
	SBufferLayout Layout = GetLayout();

	Alignment.typeStride = Layout.m_elementSize;
	Alignment.rowStride = Layout.m_elementSize * Layout.m_elementCount;
	Alignment.planeStride = Layout.m_elementSize * Layout.m_elementCount;
	Alignment.volumeStride = Layout.m_elementSize * Layout.m_elementCount;

	return Alignment;
}

SResourceDimension CDeviceBuffer::GetDimension() const
{
	SResourceDimension Dimension = { 0 };
	SBufferLayout Layout = GetLayout();

	Dimension.Width = Layout.m_elementSize * Layout.m_elementCount;
	Dimension.Height = 1;
	Dimension.Depth = 1;
	Dimension.Subresources = 1;

	return Dimension;
}

////////////////////////////////////////////////////////////////////////////

STextureLayout CDeviceTexture::GetLayout() const
{
	STextureLayout Layout = {};

	Layout.m_eSrcFormat =
	Layout.m_eDstFormat = CRendererResources::s_hwTexFormatSupport.GetClosestFormatSupported(DeviceFormats::ConvertToTexFormat(m_eNativeFormat), Layout.m_pPixelFormat);
	Layout.m_eTT = m_eTT;
	Layout.m_eFlags = m_eFlags;
	Layout.m_bIsSRGB = m_bIsSrgb;
#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
	Layout.m_nESRAMOffset = SKIP_ESRAM;
#endif

	if (!GetBaseTexture())
		return Layout;

	D3D11_RESOURCE_DIMENSION dim;
	GetBaseTexture()->GetType(&dim);
	switch (dim)
	{
	case D3D11_RESOURCE_DIMENSION_BUFFER:
		break;
	case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
		{
			D3D11_TEXTURE1D_DESC desc;
			GetLookupTexture()->GetDesc(&desc);
			Layout.m_nWidth = desc.Width;
			Layout.m_nHeight = 1;
			Layout.m_nDepth = 1;
			Layout.m_nArraySize = desc.ArraySize;
			Layout.m_nMips = desc.MipLevels;
			Layout.m_eTT = eTT_1D;
			Layout.m_eFlags |= ConvertFromDX11Flags(desc);
		}
		break;
	case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
		{
			D3D11_TEXTURE2D_DESC desc;
			Get2DTexture()->GetDesc(&desc);
			Layout.m_nWidth = desc.Width;
			Layout.m_nHeight = desc.Height;
			Layout.m_nDepth = 1;
			Layout.m_nArraySize = desc.ArraySize;
			Layout.m_nMips = desc.MipLevels;
			Layout.m_eTT = eTT_2D;
			Layout.m_eFlags |= ConvertFromDX11Flags(desc);
		}
		break;
	case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
		{
			D3D11_TEXTURE3D_DESC desc;
			GetVolumeTexture()->GetDesc(&desc);
			Layout.m_nWidth = desc.Width;
			Layout.m_nHeight = desc.Height;
			Layout.m_nDepth = desc.Depth;
			Layout.m_nArraySize = 1;
			Layout.m_nMips = desc.MipLevels;
			Layout.m_eTT = eTT_3D;
			Layout.m_eFlags |= ConvertFromDX11Flags(desc);
		}
		break;
	}

	return Layout;
}

STextureLayout CDeviceTexture::GetLayout(D3DBaseView* pView)
{
	D3DResource* pResource = nullptr;
	pView->GetResource(&pResource);

	uint32 nWidth = 0;
	uint32 nHeight = 0;
	uint32 nDepth = 0;
	uint32 nMips = 1;
	uint32 nSlices = 1;
	uint32 nFirstMip = 0;
	uint32 nFlags = 0;
	ETEX_Format eTF = eTF_Unknown;
	ETEX_Type eTT = eTT_User;

	D3D11_RESOURCE_DIMENSION eResourceDimension = D3D11_RESOURCE_DIMENSION_UNKNOWN;
	pResource->GetType(&eResourceDimension);
	pResource->Release();
	if (eResourceDimension == D3D11_RESOURCE_DIMENSION_BUFFER)
	{
		D3D11_BUFFER_DESC sDesc;
		((ID3D11Buffer*)pResource)->GetDesc(&sDesc);
		nWidth = sDesc.ByteWidth;
		nHeight = 1;
		nDepth = 1;
		nSlices = 1;
		eTF = eTF_R8;
		nFlags = ConvertFromDX11Flags(sDesc);
	}
	else if (eResourceDimension == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
	{
		D3D11_TEXTURE1D_DESC sDesc;
		((ID3D11Texture1D*)pResource)->GetDesc(&sDesc);
		nWidth = sDesc.Width;
		nHeight = 1;
		nDepth = 1;
		nSlices = sDesc.ArraySize;
		eTF = DeviceFormats::ConvertToTexFormat(sDesc.Format);
		nFlags = ConvertFromDX11Flags(sDesc);
	}
	else if (eResourceDimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
	{
		D3D11_TEXTURE2D_DESC sDesc;
		((ID3D11Texture2D*)pResource)->GetDesc(&sDesc);
		nWidth = sDesc.Width;
		nHeight = sDesc.Height;
		nDepth = 1;
		nSlices = sDesc.ArraySize;
		eTF = DeviceFormats::ConvertToTexFormat(sDesc.Format);
		nFlags = ConvertFromDX11Flags(sDesc);
	}
	else if (eResourceDimension == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
	{
		D3D11_TEXTURE3D_DESC sDesc;
		((ID3D11Texture3D*)pResource)->GetDesc(&sDesc);
		nWidth = sDesc.Width;
		nHeight = sDesc.Height;
		nDepth = sDesc.Depth;
		nSlices = 1;
		eTF = DeviceFormats::ConvertToTexFormat(sDesc.Format);
		nFlags = ConvertFromDX11Flags(sDesc);
	}

	D3DUAV* pUAV = nullptr;
	D3DShaderResource* pSRV = nullptr;
	D3DDepthSurface* pDSV = nullptr;
	D3DSurface* pRTV = nullptr;

#if CRY_PLATFORM_DURANGO
	/**/ if (pView->m_Type == 0) { pRTV = static_cast<D3DSurface*>(pView); }
	else if (pView->m_Type == 1) { pDSV = static_cast<D3DDepthSurface*>(pView); }
	else if (pView->m_Type == 2) { pSRV = static_cast<D3DShaderResource*>(pView); }
	else if (pView->m_Type == 3) { pUAV = static_cast<D3DUAV*>(pView); }
#elif !CRY_RENDERER_OPENGL && !CRY_RENDERER_OPENGLES
	// TODO for GL
	pView->QueryInterface(__uuidof(ID3D11UnorderedAccessView), (void**)&pUAV);
	pView->QueryInterface(__uuidof(ID3D11ShaderResourceView), (void**)&pSRV);
	pView->QueryInterface(__uuidof(ID3D11DepthStencilView), (void**)&pDSV);
	pView->QueryInterface(__uuidof(ID3D11RenderTargetView), (void**)&pRTV);
#endif

	if (pUAV)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC sUAVDesc;
		pUAV->GetDesc(&sUAVDesc);
		if (sUAVDesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1D)
		{
			nFirstMip = sUAVDesc.Texture1D.MipSlice;
			eTT = eTT_1D;
		}
		else if (sUAVDesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1DARRAY)
		{
			nFirstMip = sUAVDesc.Texture1DArray.MipSlice;
			nSlices = sUAVDesc.Texture1DArray.ArraySize;
			eTT = eTT_1D;   // No enum
		}
		else if (sUAVDesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2D)
		{
			nFirstMip = sUAVDesc.Texture2D.MipSlice;
			eTT = eTT_2D;
		}
		else if (sUAVDesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2DARRAY)
		{
			nFirstMip = sUAVDesc.Texture2DArray.MipSlice;
			nSlices = sUAVDesc.Texture2DArray.ArraySize;
			eTT = eTT_2DArray;
		}
		else if (sUAVDesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE3D)
		{
			nSlices = sUAVDesc.Texture3D.WSize;
			eTT = eTT_3D;
		}
	}

	// TODO: BUFFER doesn't have the sub-range calculated
	if (pSRV)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC sSRVDesc;
		pSRV->GetDesc(&sSRVDesc);
		if (sSRVDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE1D)
		{
			nMips = sSRVDesc.Texture1D.MipLevels;
			nFirstMip = sSRVDesc.Texture1D.MostDetailedMip;
			eTT = eTT_1D;
		}
		else if (sSRVDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE1DARRAY)
		{
			nMips = sSRVDesc.Texture1DArray.MipLevels;
			nFirstMip = sSRVDesc.Texture1DArray.MostDetailedMip;
			nSlices = sSRVDesc.Texture1DArray.ArraySize;
			eTT = eTT_1D;   // No enum
		}
		else if (sSRVDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D)
		{
			nMips = sSRVDesc.Texture2D.MipLevels;
			nFirstMip = sSRVDesc.Texture2D.MostDetailedMip;
			eTT = eTT_2D;
		}
		else if (sSRVDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DARRAY)
		{
			nMips = sSRVDesc.Texture2DArray.MipLevels;
			nFirstMip = sSRVDesc.Texture2DArray.MostDetailedMip;
			nSlices = sSRVDesc.Texture2DArray.ArraySize;
			eTT = eTT_2DArray;
		}
		else if (sSRVDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DMS)
		{
			eTT = eTT_2DMS;
		}
		else if (sSRVDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY)
		{
			nSlices = sSRVDesc.Texture2DMSArray.ArraySize;
			eTT = eTT_2DMS;   // No enum
		}
		else if (sSRVDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURECUBE)
		{
			nMips = sSRVDesc.TextureCube.MipLevels;
			nFirstMip = sSRVDesc.TextureCube.MostDetailedMip;
			nSlices = 6;
			eTT = eTT_Cube;
		}
		else if (sSRVDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURECUBEARRAY)
		{
			nMips = sSRVDesc.TextureCubeArray.MipLevels;
			nFirstMip = sSRVDesc.TextureCubeArray.MostDetailedMip;
			nSlices = sSRVDesc.TextureCubeArray.NumCubes * 6;
			eTT = eTT_CubeArray;
		}
		else if (sSRVDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE3D)
		{
			nMips = sSRVDesc.Texture3D.MipLevels;
			nFirstMip = sSRVDesc.Texture3D.MostDetailedMip;
			eTT = eTT_3D;
		}
	}

	if (pDSV)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC sDSVDesc;
		pDSV->GetDesc(&sDSVDesc);
		if (sDSVDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE1D)
		{
			nFirstMip = sDSVDesc.Texture1D.MipSlice;
			eTT = eTT_1D;
		}
		else if (sDSVDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE1DARRAY)
		{
			nSlices = sDSVDesc.Texture1DArray.ArraySize;
			nFirstMip = sDSVDesc.Texture1DArray.MipSlice;
			eTT = eTT_1D;   // No enum
		}
		else if (sDSVDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2D)
		{
			nFirstMip = sDSVDesc.Texture2D.MipSlice;
			eTT = eTT_2D;
		}
		else if (sDSVDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DARRAY)
		{
			nSlices = sDSVDesc.Texture2DArray.ArraySize;
			nFirstMip = sDSVDesc.Texture2DArray.MipSlice;
			eTT = eTT_2DArray;
		}
		else if (sDSVDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DMS)
		{
			eTT = eTT_2DMS;
		}
		else if (sDSVDesc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY)
		{
			nSlices = sDSVDesc.Texture2DMSArray.ArraySize;
			eTT = eTT_2DMS;   // No enum
		}
	}

	if (pRTV)
	{
		D3D11_RENDER_TARGET_VIEW_DESC sRSVDesc;
		pRTV->GetDesc(&sRSVDesc);
		if (sRSVDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE1D)
		{
			nFirstMip = sRSVDesc.Texture1D.MipSlice;
			eTT = eTT_1D;
		}
		else if (sRSVDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE1DARRAY)
		{
			nSlices = sRSVDesc.Texture1DArray.ArraySize;
			nFirstMip = sRSVDesc.Texture1DArray.MipSlice;
			eTT = eTT_1D;   // No enum
		}
		else if (sRSVDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D)
		{
			nFirstMip = sRSVDesc.Texture2D.MipSlice;
			eTT = eTT_2D;
		}
		else if (sRSVDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DARRAY)
		{
			nSlices = sRSVDesc.Texture2DArray.ArraySize;
			nFirstMip = sRSVDesc.Texture2DArray.MipSlice;
			eTT = eTT_2DArray;
		}
		else if (sRSVDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMS)
		{
			eTT = eTT_2DMS;
		}
		else if (sRSVDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY)
		{
			nSlices = sRSVDesc.Texture2DMSArray.ArraySize;
			eTT = eTT_2DMS;   // No enum
		}
		else if (sRSVDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE3D)
		{
			nDepth = sRSVDesc.Texture3D.WSize;
			eTT = eTT_3D;
		}
	}

#if !CRY_PLATFORM_DURANGO
	SAFE_RELEASE(pUAV);
	SAFE_RELEASE(pSRV);
	SAFE_RELEASE(pDSV);
	SAFE_RELEASE(pRTV);
#endif

	nWidth  = std::max(nWidth  >> nFirstMip, 1U);
	nHeight = std::max(nHeight >> nFirstMip, 1U);
	nDepth  = std::max(nDepth  >> nFirstMip, 1U);

	STextureLayout Layout = {};

	Layout.m_eSrcFormat   =
	Layout.m_eDstFormat   = CRendererResources::s_hwTexFormatSupport.GetClosestFormatSupported(eTF, Layout.m_pPixelFormat);
	Layout.m_eTT          = eTT;
	Layout.m_eFlags       = nFlags;
	Layout.m_nWidth       = nWidth;
	Layout.m_nHeight      = nHeight;
	Layout.m_nDepth       = nDepth;
	Layout.m_nMips        = nMips;
	Layout.m_nArraySize   = nSlices;
#if CRY_PLATFORM_DURANGO && DURANGO_USE_ESRAM
	Layout.m_nESRAMOffset = SKIP_ESRAM;
#endif

	return Layout;
}

SResourceMemoryAlignment CDeviceTexture::GetAlignment(uint8 mip /*= 0*/, uint8 slices /*= 0*/) const
{
	SResourceMemoryAlignment Alignment = { 0 };
	STextureLayout Layout = GetLayout();

	if (!(Layout.m_nWidth  = Layout.m_nWidth  >> mip)) Layout.m_nWidth  = 1;
	if (!(Layout.m_nHeight = Layout.m_nHeight >> mip)) Layout.m_nHeight = 1;
	if (!(Layout.m_nDepth  = Layout.m_nDepth  >> mip)) Layout.m_nDepth  = 1;

	Alignment.typeStride   = CTexture::TextureDataSize(1              , 1               , 1              , 1, 1, DeviceFormats::ConvertToTexFormat(m_eNativeFormat));
	Alignment.rowStride    = CTexture::TextureDataSize(Layout.m_nWidth, 1               , 1              , 1, 1, DeviceFormats::ConvertToTexFormat(m_eNativeFormat));
	Alignment.planeStride  = CTexture::TextureDataSize(Layout.m_nWidth, Layout.m_nHeight, 1              , 1, 1, DeviceFormats::ConvertToTexFormat(m_eNativeFormat));
	Alignment.volumeStride = CTexture::TextureDataSize(Layout.m_nWidth, Layout.m_nHeight, Layout.m_nDepth, 1, 1, DeviceFormats::ConvertToTexFormat(m_eNativeFormat));

	return Alignment;
}

SResourceDimension CDeviceTexture::GetDimension(uint8 mip /*= 0*/, uint8 slices /*= 0*/) const
{
	SResourceDimension Dimension = { 0 };
	STextureLayout Layout = GetLayout();

	if (!(Layout.m_nWidth  = Layout.m_nWidth  >> mip)) Layout.m_nWidth  = 1;
	if (!(Layout.m_nHeight = Layout.m_nHeight >> mip)) Layout.m_nHeight = 1;
	if (!(Layout.m_nDepth  = Layout.m_nDepth  >> mip)) Layout.m_nDepth  = 1;

	Dimension.Width        = Layout.m_nWidth;
	Dimension.Height       = Layout.m_nHeight;
	Dimension.Depth        = Layout.m_nDepth;
	Dimension.Subresources = (slices ? slices : Layout.m_nArraySize) * (Layout.m_nMips - mip);

	return Dimension;
}

#ifdef DEVRES_USE_STAGING_POOL

void CDeviceTexture::DownloadToStagingResource(uint32 nSubRes, StagingHook cbTransfer)
{
	D3DResource* pStagingResource;
	void* pStagingMemory = m_pStagingMemory[0];
	if (!(pStagingResource = m_pStagingResource[0]))
	{
		pStagingResource = GetDeviceObjectFactory().AllocateStagingResource(m_pNativeResource, FALSE, pStagingMemory);
	}

	assert(pStagingResource);

#if (CRY_RENDERER_DIRECT3D >= 111)
	gcpRendD3D->GetDeviceContext().CopySubresourceRegion1(pStagingResource, nSubRes, 0, 0, 0, m_pNativeResource, nSubRes, NULL, D3D11_COPY_NO_OVERWRITE);
#else
	gcpRendD3D->GetDeviceContext().CopySubresourceRegion(pStagingResource, nSubRes, 0, 0, 0, m_pNativeResource, nSubRes, NULL);
#endif

	// Although CopySubresourceRegion() is asynchronous, the following Map() will synchronize under D3D11
	// And without Map() there is no access to the contents, which means synchronization is automatic.

	D3D11_MAPPED_SUBRESOURCE lrct;
	HRESULT hr = gcpRendD3D->GetDeviceContext_ForMapAndUnmap().Map(pStagingResource, nSubRes, D3D11_MAP_READ, 0, &lrct);

	if (S_OK == hr)
	{
		const bool update = cbTransfer(lrct.pData, lrct.RowPitch, lrct.DepthPitch);
		gcpRendD3D->GetDeviceContext_ForMapAndUnmap().Unmap(pStagingResource, nSubRes);
	}

	if (!(m_pStagingResource[0]))
	{
		GetDeviceObjectFactory().ReleaseStagingResource(pStagingResource);
	}
}

void CDeviceTexture::DownloadToStagingResource(uint32 nSubRes)
{
	assert(m_pStagingResource[0]);

#if (CRY_RENDERER_DIRECT3D >= 111)
	gcpRendD3D->GetDeviceContext().CopySubresourceRegion1(m_pStagingResource[0], nSubRes, 0, 0, 0, m_pNativeResource, nSubRes, NULL, D3D11_COPY_NO_OVERWRITE);
#else
	gcpRendD3D->GetDeviceContext().CopySubresourceRegion(m_pStagingResource[0], nSubRes, 0, 0, 0, m_pNativeResource, nSubRes, NULL);
#endif

	GetDeviceObjectFactory().IssueFence(m_hStagingFence[0]);
}

void CDeviceTexture::UploadFromStagingResource(const uint32 nSubRes, StagingHook cbTransfer)
{
	D3DResource* pStagingResource;
	void* pStagingMemory = m_pStagingMemory[1];
	if (!(pStagingResource = m_pStagingResource[1]))
	{
		pStagingResource = GetDeviceObjectFactory().AllocateStagingResource(m_pNativeResource, TRUE, pStagingMemory);
	}
	else
	{
		// We have to wait for a previous UploadFromStaging/DownloadToStaging to have finished on the GPU, before we can access the staging resource again on CPU.
		GetDeviceObjectFactory().SyncFence(m_hStagingFence[1], true, true);
	}

	assert(pStagingResource);

	// ID3D11DeviceContext::Map : Map cannot be called with MAP_WRITE access, because the Resource was created as D3D11_USAGE_DYNAMIC.
	// D3D11_USAGE_DYNAMIC Resources must use either MAP_WRITE_DISCARD or MAP_WRITE_NO_OVERWRITE with Map.

	D3D11_MAPPED_SUBRESOURCE lrct;
	HRESULT hr = gcpRendD3D->GetDeviceContext_ForMapAndUnmap().Map(pStagingResource, nSubRes, D3D11_MAP_WRITE_NO_OVERWRITE_SR, 0, &lrct);
	if (S_OK != hr)
		hr = gcpRendD3D->GetDeviceContext_ForMapAndUnmap().Map(pStagingResource, nSubRes, D3D11_MAP_WRITE_DISCARD_SR, 0, &lrct);

	if (S_OK == hr)
	{
		const bool update = cbTransfer(lrct.pData, lrct.RowPitch, lrct.DepthPitch);
		gcpRendD3D->GetDeviceContext_ForMapAndUnmap().Unmap(pStagingResource, 0);
		if (update)
		{
#if (CRY_RENDERER_DIRECT3D >= 111)
			gcpRendD3D->GetDeviceContext().CopySubresourceRegion1(m_pNativeResource, nSubRes, 0, 0, 0, pStagingResource, nSubRes, NULL, D3D11_COPY_NO_OVERWRITE);
#else
			gcpRendD3D->GetDeviceContext().CopySubresourceRegion(m_pNativeResource, nSubRes, 0, 0, 0, pStagingResource, nSubRes, NULL);
#endif
		}
	}

	if (!(m_pStagingResource[1]))
	{
		GetDeviceObjectFactory().ReleaseStagingResource(pStagingResource);
	}
}

void CDeviceTexture::UploadFromStagingResource(const uint32 nSubRes)
{
	assert(m_pStagingResource[1]);

#if (CRY_RENDERER_DIRECT3D >= 111)
	gcpRendD3D->GetDeviceContext().CopySubresourceRegion1(m_pNativeResource, nSubRes, 0, 0, 0, m_pStagingResource[1], nSubRes, NULL, D3D11_COPY_NO_OVERWRITE);
#else
	gcpRendD3D->GetDeviceContext().CopySubresourceRegion(m_pNativeResource, nSubRes, 0, 0, 0, m_pStagingResource[1], nSubRes, NULL);
#endif

	GetDeviceObjectFactory().IssueFence(m_hStagingFence[1]);
}

void CDeviceTexture::AccessCurrStagingResource(uint32 nSubRes, bool forUpload, StagingHook cbTransfer)
{
	// We have to wait for a previous UploadFromStaging/DownloadToStaging to have finished on the GPU, before we can access the staging resource again on CPU.
	GetDeviceObjectFactory().SyncFence(m_hStagingFence[forUpload], true, true);

	D3D11_MAPPED_SUBRESOURCE lrct;
	HRESULT hr = gcpRendD3D->GetDeviceContext_ForMapAndUnmap().Map(m_pStagingResource[forUpload], nSubRes, forUpload ? D3D11_MAP_WRITE : D3D11_MAP_READ, 0, &lrct);

	if (S_OK == hr)
	{
		const bool update = cbTransfer(lrct.pData, lrct.RowPitch, lrct.DepthPitch);
		gcpRendD3D->GetDeviceContext_ForMapAndUnmap().Unmap(m_pStagingResource[forUpload], nSubRes);
	}
}

bool CDeviceTexture::AccessCurrStagingResource(uint32 nSubRes, bool forUpload)
{
	return GetDeviceObjectFactory().SyncFence(m_hStagingFence[forUpload], false, false) == S_OK;
}

#endif

//=============================================================================

#if DEVRES_USE_PINNING && !CRY_PLATFORM_DURANGO
void CDeviceTexture::WeakPin()
{
}

void CDeviceTexture::Pin()
{
}

void CDeviceTexture::Unpin()
{
}
#endif
