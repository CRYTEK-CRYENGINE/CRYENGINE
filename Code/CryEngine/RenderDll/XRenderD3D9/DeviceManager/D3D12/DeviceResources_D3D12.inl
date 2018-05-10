// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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
	const uint nMipCount   = pView.m_Desc.nMipCount   == SResourceView(0ULL).m_Desc.nMipCount   || pView.m_Desc.nMipCount   == SResourceView(~0ULL).m_Desc.nMipCount   ? (uint)-1 : (uint)pView.m_Desc.nMipCount;
	const uint nSliceCount = pView.m_Desc.nSliceCount == SResourceView(0ULL).m_Desc.nSliceCount || pView.m_Desc.nSliceCount == SResourceView(~0ULL).m_Desc.nSliceCount ? (uint)-1 : (uint)pView.m_Desc.nSliceCount;

	const uint nElemsAvailable = m_resourceElements;
	const uint nMipAvailable   = m_subResources[eSubResource_Mips];
	const uint nSliceAvailable = m_subResources[eSubResource_Slices];

	if (pView.m_Desc.eViewType == SResourceView::eShaderResourceView)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroStruct(srvDesc);
		srvDesc.Format = DXGI_FORMAT(pView.m_Desc.nFormat);

		// NOTE formats needs to be set precisely in the view, otherwise multiple distinct view descs might produce multiple views which are in fact identical
		CRY_ASSERT(
			!DeviceFormats::IsDepthStencil(DXGI_FORMAT(pView.m_Desc.nFormat)) ||
			( (pView.m_Desc.nFlags & SResourceView::eSRV_StencilOnly) && (srvDesc.Format == DeviceFormats::ConvertToStencilOnly(DXGI_FORMAT(pView.m_Desc.nFormat)))) ||
			(!(pView.m_Desc.nFlags & SResourceView::eSRV_StencilOnly) && (srvDesc.Format == DeviceFormats::ConvertToDepthOnly  (DXGI_FORMAT(pView.m_Desc.nFormat))))
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

			if (nSliceAvailable > 1 || m_eTT == eTT_3D || m_eTT == eTT_Cube)
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
				dsvDesc.Flags |=  (D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL);
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
	if (!GetNativeResource()->SubstituteUsed())
	{
		return eSubResult_Kept;
	}

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

	Alignment.typeStride   = Layout.m_elementSize;
	Alignment.rowStride    = Layout.m_elementSize * Layout.m_elementCount;
	Alignment.planeStride  = Layout.m_elementSize * Layout.m_elementCount;
	Alignment.volumeStride = Layout.m_elementSize * Layout.m_elementCount;

	return Alignment;
}

SResourceDimension CDeviceBuffer::GetDimension() const
{
	SResourceDimension Dimension = { 0 };
	SBufferLayout Layout = GetLayout();

	Dimension.Width        = Layout.m_elementSize * Layout.m_elementCount;
	Dimension.Height       = 1;
	Dimension.Depth        = 1;
	Dimension.Subresources = 1;

	return Dimension;
}

////////////////////////////////////////////////////////////////////////////

STextureLayout CDeviceTexture::GetLayout() const
{
	STextureLayout Layout = { };

	Layout.m_eSrcFormat =
	Layout.m_eDstFormat = CRendererResources::s_hwTexFormatSupport.GetClosestFormatSupported(DeviceFormats::ConvertToTexFormat(m_eNativeFormat), Layout.m_pPixelFormat);
	Layout.m_eTT = m_eTT;
	Layout.m_eFlags = m_eFlags;
	Layout.m_bIsSRGB = m_bIsSrgb;
	
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
		eTF = DeviceFormats::ConvertToTexFormat(sDesc.Format);
		nFlags = ConvertFromDX11Flags(sDesc);
	}

	D3DUAV* pUAV = nullptr;
	D3DShaderResource* pSRV = nullptr;
	D3DDepthSurface* pDSV = nullptr;
	D3DSurface* pRSV = nullptr;

	pView->QueryInterface(__uuidof(ID3D11UnorderedAccessView), (void**)&pUAV);
	pView->QueryInterface(__uuidof(ID3D11ShaderResourceView), (void**)&pSRV);
	pView->QueryInterface(__uuidof(ID3D11DepthStencilView), (void**)&pDSV);
	pView->QueryInterface(__uuidof(ID3D11RenderTargetView), (void**)&pRSV);

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

	if (pRSV)
	{
		D3D11_RENDER_TARGET_VIEW_DESC sRSVDesc;
		pRSV->GetDesc(&sRSVDesc);
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

	SAFE_RELEASE(pUAV);
	SAFE_RELEASE(pSRV);
	SAFE_RELEASE(pDSV);
	SAFE_RELEASE(pRSV);

	nWidth = std::max(nWidth >> nFirstMip, 1U);
	nHeight = std::max(nHeight >> nFirstMip, 1U);
	nDepth = std::max(nDepth >> nFirstMip, 1U);

	STextureLayout Layout = {};

	Layout.m_eSrcFormat =
	Layout.m_eDstFormat = CRendererResources::s_hwTexFormatSupport.GetClosestFormatSupported(eTF, Layout.m_pPixelFormat);
	Layout.m_eTT = eTT;
	Layout.m_eFlags = nFlags;
	Layout.m_nWidth = nWidth;
	Layout.m_nHeight = nHeight;
	Layout.m_nDepth = nDepth;
	Layout.m_nMips = nMips;
	Layout.m_nArraySize = nSlices;

	return Layout;
}

SResourceMemoryAlignment CDeviceTexture::GetAlignment(uint8 mip /*= 0*/, uint8 slices /*= 0*/) const
{
	SResourceMemoryAlignment Alignment = { 0 };
	STextureLayout Layout = GetLayout();

	if (!(Layout.m_nWidth  = Layout.m_nWidth  >> mip)) Layout.m_nWidth  = 1;
	if (!(Layout.m_nHeight = Layout.m_nHeight >> mip)) Layout.m_nHeight = 1;
	if (!(Layout.m_nDepth  = Layout.m_nDepth  >> mip)) Layout.m_nDepth  = 1;

	Alignment.typeStride   = CTexture::TextureDataSize(1              , 1               , 1              , 1, 1, DeviceFormats::ConvertToTexFormat(m_eNativeFormat), eTM_None);
	Alignment.rowStride    = CTexture::TextureDataSize(Layout.m_nWidth, 1               , 1              , 1, 1, DeviceFormats::ConvertToTexFormat(m_eNativeFormat), eTM_None);
	Alignment.planeStride  = CTexture::TextureDataSize(Layout.m_nWidth, Layout.m_nHeight, 1              , 1, 1, DeviceFormats::ConvertToTexFormat(m_eNativeFormat), eTM_None);
	Alignment.volumeStride = CTexture::TextureDataSize(Layout.m_nWidth, Layout.m_nHeight, Layout.m_nDepth, 1, 1, DeviceFormats::ConvertToTexFormat(m_eNativeFormat), eTM_None);

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
	D3D11_RESOURCE_DIMENSION eResourceDimension;
	m_pNativeResource->GetType(&eResourceDimension);

	D3DResource* pStagingResource;
	void* pStagingMemory = m_pStagingMemory[0];
	if (!(pStagingResource = m_pStagingResource[0]))
	{
		pStagingResource = GetDeviceObjectFactory().AllocateStagingResource(m_pNativeResource, FALSE, pStagingMemory);
	}

	assert(pStagingResource);

	if (gcpRendD3D->GetDeviceContext_Unsynchronized().CopyStagingResource(pStagingResource, m_pNativeResource, nSubRes, FALSE) == S_OK)
	{
		// Resources on D3D12_HEAP_TYPE_READBACK heaps do not support persistent map.
		// Map and Unmap must be called between CPU and GPU accesses to the same memory
		// address on some system architectures, when the page caching behavior is write-back.
		// Map and Unmap invalidate and flush the last level CPU cache on some ARM systems,
		// to marshal data between the CPU and GPU through memory addresses with write-back behavior.
		gcpRendD3D->GetDeviceContext_Unsynchronized().WaitStagingResource(pStagingResource);
		gcpRendD3D->GetDeviceContext_Unsynchronized().MapStagingResource(pStagingResource, FALSE, &pStagingMemory);

		D3D12_RESOURCE_DESC resourceDesc = m_pNativeResource->GetD3D12Resource()->GetDesc();
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
		gcpRendD3D->GetDeviceContext_Unsynchronized().GetRealDeviceContext()->GetD3D12Device()->GetCopyableFootprints(&resourceDesc, nSubRes, 1, 0, &layout, nullptr, nullptr, nullptr);
		cbTransfer(pStagingMemory, layout.Footprint.RowPitch, layout.Footprint.RowPitch * layout.Footprint.Width);

		gcpRendD3D->GetDeviceContext_Unsynchronized().UnmapStagingResource(pStagingResource, FALSE);
	}

	if (!(m_pStagingResource[0]))
	{
		GetDeviceObjectFactory().ReleaseStagingResource(pStagingResource);
	}
}

void CDeviceTexture::DownloadToStagingResource(uint32 nSubRes)
{
	assert(m_pStagingResource[0]);

	gcpRendD3D->GetDeviceContext_Unsynchronized().CopyStagingResource(m_pStagingResource[0], m_pNativeResource, nSubRes, FALSE);
}

void CDeviceTexture::UploadFromStagingResource(uint32 nSubRes, StagingHook cbTransfer)
{
	D3D11_RESOURCE_DIMENSION eResourceDimension;
	m_pNativeResource->GetType(&eResourceDimension);

	D3DResource* pStagingResource;
	void* pStagingMemory = m_pStagingMemory[1];
	if (!(pStagingResource = m_pStagingResource[1]))
	{
		pStagingResource = GetDeviceObjectFactory().AllocateStagingResource(m_pNativeResource, TRUE, pStagingMemory);
	}

	assert(pStagingResource);

	// The first call to Map allocates a CPU virtual address range for the resource.
	// The last call to Unmap deallocates the CPU virtual address range.
	// Applications cannot rely on the address being consistent, unless Map is persistently nested.
	gcpRendD3D->GetDeviceContext_Unsynchronized().WaitStagingResource(pStagingResource);
	gcpRendD3D->GetDeviceContext_Unsynchronized().MapStagingResource(pStagingResource, TRUE, &pStagingMemory);

	D3D12_RESOURCE_DESC resourceDesc = m_pNativeResource->GetD3D12Resource()->GetDesc();
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
	gcpRendD3D->GetDeviceContext_Unsynchronized().GetRealDeviceContext()->GetD3D12Device()->GetCopyableFootprints(&resourceDesc, nSubRes, 1, 0, &layout, nullptr, nullptr, nullptr);
	if (cbTransfer(pStagingMemory, layout.Footprint.RowPitch, layout.Footprint.RowPitch * layout.Footprint.Width))
	{
		gcpRendD3D->GetDeviceContext_Unsynchronized().CopyStagingResource(pStagingResource, m_pNativeResource, nSubRes, TRUE);
	}

	// Unmap also flushes the CPU cache, when necessary, so that GPU reads to this
	// address reflect any modifications made by the CPU.
	gcpRendD3D->GetDeviceContext_Unsynchronized().UnmapStagingResource(pStagingResource, TRUE);

	if (!(m_pStagingResource[1]))
	{
		GetDeviceObjectFactory().ReleaseStagingResource(pStagingResource);
	}
}

void CDeviceTexture::UploadFromStagingResource(uint32 nSubRes)
{
	assert(m_pStagingResource[1]);

	gcpRendD3D->GetDeviceContext_Unsynchronized().CopyStagingResource(m_pStagingResource[1], m_pNativeResource, nSubRes, TRUE);
}

void CDeviceTexture::AccessCurrStagingResource(uint32 nSubRes, bool forUpload, StagingHook cbTransfer)
{
	// Resources on D3D12_HEAP_TYPE_READBACK heaps do not support persistent map.
	// Applications cannot rely on the address being consistent, unless Map is persistently nested.
	gcpRendD3D->GetDeviceContext_Unsynchronized().WaitStagingResource(m_pStagingResource[forUpload]);
	gcpRendD3D->GetDeviceContext_Unsynchronized().MapStagingResource(m_pStagingResource[forUpload], forUpload, &m_pStagingMemory[forUpload]);

	SResourceMemoryAlignment memoryAlignment = GetAlignment();
	cbTransfer(m_pStagingMemory[forUpload], memoryAlignment.rowStride, memoryAlignment.planeStride);

	gcpRendD3D->GetDeviceContext_Unsynchronized().UnmapStagingResource(m_pStagingResource[forUpload], forUpload);
}

bool CDeviceTexture::AccessCurrStagingResource(uint32 nSubRes, bool forUpload)
{
	return gcpRendD3D->GetDeviceContext_Unsynchronized().TestStagingResource(m_pStagingResource[forUpload]) == S_OK;
}

#endif

//=============================================================================

#if DEVRES_USE_PINNING
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
