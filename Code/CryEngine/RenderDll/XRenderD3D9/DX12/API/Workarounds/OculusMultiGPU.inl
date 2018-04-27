// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

void CD3DOculusRenderer::CopyMultiGPUFrameData()
{
	CDeviceCommandListUPtr pCLA = GetDeviceObjectFactory().AcquireCommandList(CDeviceObjectFactory::eQueue_Graphics);
	pCLA->LockToThread();

	ID3D12GraphicsCommandList* pCL0 = *(*((BroadcastableD3D12GraphicsCommandList<2>*)pCLA->GetDX12CommandList()->GetD3D12CommandList()))[0];
	//	ID3D12GraphicsCommandList* pCL1 = *(*((BroadcastableD3D12GraphicsCommandList<2>*)pCLA->GetCommandList()->GetD3D12CommandList()))[1];

	// Scene3D layer
	CCryDX12Texture2D* lRV = (CCryDX12Texture2D*)m_pStereoRenderer->GetEyeDisplayContext(CCamera::eEye_Left).first->GetCurrentBackBuffer()->GetDevTexture()->GetBaseTexture();
	CCryDX12Texture2D* rRV = (CCryDX12Texture2D*)m_pStereoRenderer->GetEyeDisplayContext(CCamera::eEye_Right).first->GetCurrentBackBuffer()->GetDevTexture()->GetBaseTexture();

	NCryDX12::CResource& lRVResource = lRV->GetDX12Resource(); lRVResource.VerifyBackBuffer();
	NCryDX12::CResource& rRVResource = rRV->GetDX12Resource(); rRVResource.VerifyBackBuffer();

	const int idx0 = m_pOculusDevice->GetCurrentSwapChainIndex(m_scene3DRenderData[0].vrTextureSet.pDeviceTextureSwapChain);
	const int idx1 = m_pOculusDevice->GetCurrentSwapChainIndex(m_scene3DRenderData[1].vrTextureSet.pDeviceTextureSwapChain);

	IUnknown* lRVN = m_scene3DRenderData[0].texturesNative[idx0];
	IUnknown* rRVN = m_scene3DRenderData[1].texturesNative[idx1];

	D3D12_RESOURCE_BARRIER barrierDesc0[8] = {};
	D3D12_RESOURCE_BARRIER barrierDesc2[8] = {};
	int n = 0;

	for (uint32 i = 0; i < 8; ++i)
	{
		barrierDesc0[i].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrierDesc0[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	}

	barrierDesc0[n].Transition.pResource = *(*((BroadcastableD3D12Resource<2>*)lRVResource.GetD3D12Resource()))[0];
	barrierDesc0[n].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrierDesc0[n].Transition.StateBefore = lRVResource.GetState();
	barrierDesc0[n].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

	if (barrierDesc0[n].Transition.StateBefore != barrierDesc0[n].Transition.StateAfter)
		n++;

	barrierDesc0[n].Transition.pResource = *(*((BroadcastableD3D12Resource<2>*)rRVResource.GetD3D12Resource()))[0];
	barrierDesc0[n].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrierDesc0[n].Transition.StateBefore = rRVResource.GetState();
	barrierDesc0[n].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

	if (barrierDesc0[n].Transition.StateBefore != barrierDesc0[n].Transition.StateAfter)
		n++;

	barrierDesc0[n].Transition.pResource = (ID3D12Resource*)lRVN;
	barrierDesc0[n].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrierDesc0[n].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barrierDesc0[n].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

	if (barrierDesc0[n].Transition.StateBefore != barrierDesc0[n].Transition.StateAfter)
		n++;

	barrierDesc0[n].Transition.pResource = (ID3D12Resource*)rRVN;
	barrierDesc0[n].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrierDesc0[n].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barrierDesc0[n].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

	if (barrierDesc0[n].Transition.StateBefore != barrierDesc0[n].Transition.StateAfter)
		n++;

	// Quad layers
	for (uint32 i = 0; i < RenderLayer::eQuadLayers_Total; ++i)
	{
		CCryDX12Texture2D* qRV = (CCryDX12Texture2D*)m_pStereoRenderer->GetVrQuadLayerDisplayContext(static_cast<RenderLayer::EQuadLayers>(i)).first->GetCurrentBackBuffer()->GetDevTexture()->GetBaseTexture();

		NCryDX12::CResource& qRVResource = qRV->GetDX12Resource(); qRVResource.VerifyBackBuffer();

		const int idx = m_pOculusDevice->GetCurrentSwapChainIndex(m_quadLayerRenderData[i].vrTextureSet.pDeviceTextureSwapChain);

		IUnknown* qRVN = m_quadLayerRenderData[i].texturesNative[idx];

		barrierDesc0[n].Transition.pResource = *(*((BroadcastableD3D12Resource<2>*)qRVResource.GetD3D12Resource()))[0];
		barrierDesc0[n].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrierDesc0[n].Transition.StateBefore = qRVResource.GetState();
		barrierDesc0[n].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

		if (barrierDesc0[n].Transition.StateBefore != barrierDesc0[n].Transition.StateAfter)
			n++;

		barrierDesc0[n].Transition.pResource = (ID3D12Resource*)qRVN;
		barrierDesc0[n].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrierDesc0[n].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
		barrierDesc0[n].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

		if (barrierDesc0[n].Transition.StateBefore != barrierDesc0[n].Transition.StateAfter)
			n++;
	}

	for (uint32 i = 0; i < 8; ++i)
	{
		barrierDesc2[i] = barrierDesc0[i];

		std::swap(barrierDesc2[i].Transition.StateBefore, barrierDesc2[i].Transition.StateAfter);
	}

	pCL0->ResourceBarrier(n, barrierDesc0);

	pCL0->CopyResource((ID3D12Resource*)lRVN, *(*((BroadcastableD3D12Resource<2>*)lRVResource.GetD3D12Resource()))[0]);
	pCL0->CopyResource((ID3D12Resource*)rRVN, *(*((BroadcastableD3D12Resource<2>*)rRVResource.GetD3D12Resource()))[0]);

	for (uint32 i = 0; i < RenderLayer::eQuadLayers_Total; ++i)
	{
		CCryDX12Texture2D* qRV = (CCryDX12Texture2D*)m_pStereoRenderer->GetVrQuadLayerDisplayContext(static_cast<RenderLayer::EQuadLayers>(i)).first->GetCurrentBackBuffer()->GetDevTexture()->GetBaseTexture();

		NCryDX12::CResource& qRVResource = qRV->GetDX12Resource(); qRVResource.VerifyBackBuffer();

		const int idx = m_pOculusDevice->GetCurrentSwapChainIndex(m_quadLayerRenderData[i].vrTextureSet.pDeviceTextureSwapChain);

		IUnknown* qRVN = m_quadLayerRenderData[i].texturesNative[idx];

		pCL0->CopyResource((ID3D12Resource*)qRVN, *(*((BroadcastableD3D12Resource<2>*)qRVResource.GetD3D12Resource()))[0]);
	}

	pCL0->ResourceBarrier(n, barrierDesc2);

	pCLA->GetDX12CommandList()->m_nCommands = 2 + RenderLayer::eQuadLayers_Total;
	pCLA->Close();
	GetDeviceObjectFactory().ForfeitCommandList(std::move(pCLA), CDeviceObjectFactory::eQueue_Graphics);
}

void CD3DOculusRenderer::CopyMultiGPUMirrorData(CTexture* pBackbufferTexture)
{
	CDeviceCommandListUPtr pCLA = GetDeviceObjectFactory().AcquireCommandList(CDeviceObjectFactory::eQueue_Graphics);
	pCLA->LockToThread();

	ID3D12GraphicsCommandList* pCL0 = *(*((BroadcastableD3D12GraphicsCommandList<2>*)pCLA->GetDX12CommandList()->GetD3D12CommandList()))[0];
	//	ID3D12GraphicsCommandList* pCL1 = *(*((BroadcastableD3D12GraphicsCommandList<2>*)pCLA->GetCommandList()->GetD3D12CommandList()))[1];

	CCryDX12Texture2D* bRV = (CCryDX12Texture2D*)pBackbufferTexture->GetDevTexture()->Get2DTexture();

	NCryDX12::CResource& bRVResource = bRV->GetDX12Resource(); bRVResource.VerifyBackBuffer();

	IUnknown* bRVN = m_mirrorData.pMirrorTextureNative;

	D3D12_RESOURCE_BARRIER barrierDesc0[2] = {};
	D3D12_RESOURCE_BARRIER barrierDesc2[2] = {};
	int n = 0;

	for (uint32 i = 0; i < 2; ++i)
	{
		barrierDesc0[i].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrierDesc0[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	}

	barrierDesc0[n].Transition.pResource = *(*((BroadcastableD3D12Resource<2>*)bRVResource.GetD3D12Resource()))[0];
	barrierDesc0[n].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrierDesc0[n].Transition.StateBefore = bRVResource.GetState();
	barrierDesc0[n].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

	if (barrierDesc0[n].Transition.StateBefore != barrierDesc0[n].Transition.StateAfter)
		n++;

	barrierDesc0[n].Transition.pResource = (ID3D12Resource*)bRVN;
	barrierDesc0[n].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrierDesc0[n].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barrierDesc0[n].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

	if (barrierDesc0[n].Transition.StateBefore != barrierDesc0[n].Transition.StateAfter)
		n++;

	for (uint32 i = 0; i < 2; ++i)
	{
		barrierDesc2[i] = barrierDesc0[i];

		std::swap(barrierDesc2[i].Transition.StateBefore, barrierDesc2[i].Transition.StateAfter);
	}

	pCL0->ResourceBarrier(n, barrierDesc0);
	pCL0->CopyResource(*(*((BroadcastableD3D12Resource<2>*)bRVResource.GetD3D12Resource()))[0], (ID3D12Resource*)bRVN);
	pCL0->ResourceBarrier(n, barrierDesc2);

	pCLA->GetDX12CommandList()->m_nCommands = 1;
	pCLA->Close();
	GetDeviceObjectFactory().ForfeitCommandList(std::move(pCLA), CDeviceObjectFactory::eQueue_Graphics);
}
