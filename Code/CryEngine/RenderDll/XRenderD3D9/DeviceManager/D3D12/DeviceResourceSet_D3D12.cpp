// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "DeviceResourceSet_D3D12.h"

using namespace NCryDX12;

void CDeviceResourceSet_DX12::ReleaseDescriptors()
{
	if (m_pDescriptors != nullptr)
	{
		gcpRendD3D->m_DevBufMan.ReleaseDescriptorBlock(m_pDescriptors);
		m_pDescriptors = nullptr;
	}

	m_DescriptorBlockDX12 = CDescriptorBlock();
}

// requires no command-list, just a device
bool CDeviceResourceSet_DX12::UpdateImpl(const CDeviceResourceSetDesc& desc, CDeviceResourceSetDesc::EDirtyFlags dirtyFlags)
{
	CRY_ASSERT(gRenDev->m_pRT->IsRenderThread());

	int descriptorCount = 0;
	for (auto it : desc.GetResources())
	{
		if (it.second.type != SResourceBinding::EResourceType::Sampler)
			++descriptorCount;
	}

	CryStackAllocatorWithSizeVector(D3D12_CPU_DESCRIPTOR_HANDLE, descriptorCount, descriptorMem, NoAlign);
	StackDescriptorVector descriptors(descriptorMem);
	descriptors.reserve(descriptorCount);

	if (m_pDescriptors != nullptr)
		gcpRendD3D->m_DevBufMan.ReleaseDescriptorBlock(m_pDescriptors);

	m_pDescriptors = gcpRendD3D->m_DevBufMan.CreateDescriptorBlock(descriptorCount);
	m_DescriptorBlockDX12 = *m_pDescriptors;

	// gather descriptors for all bound resources and copy to new block. NOTE: CBV_SRV_UAV heap, SMP heap not yet supported
	bool bSuccess = GatherDescriptors(desc,
		descriptors,
		GetDevice()->GetResourceDescriptorScratchSpace(),
		m_ConstantBuffersInUse,
		m_ShaderResourceViewsInUse,
		m_UnorderedAccessViewsInUse);

	if (bSuccess)
	{
		CRY_ASSERT(descriptors.size() == descriptorCount);
		FillDescriptorBlock(descriptors, m_DescriptorBlockDX12);

		return true;
	}

	return false;
}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CDeviceResourceLayout_DX12::Init(const SDeviceResourceLayoutDesc& desc)
{
	SResourceMappings resourceMappings;

	for (auto& itLayoutBinding : desc.m_resourceBindings)
	{
		const SDeviceResourceLayoutDesc::SLayoutBindPoint& layoutBindPoint = itLayoutBinding.first;

		switch (layoutBindPoint.slotType)
		{
		case SDeviceResourceLayoutDesc::ELayoutSlotType::InlineConstantBuffer:
		{
			const SResourceBindPoint& resourceBindPoint = itLayoutBinding.second.begin()->first;

			resourceMappings.m_RootParameters[layoutBindPoint.layoutSlot].InitAsConstantBufferView(
				resourceBindPoint.slotNumber, 0, GetShaderVisibility(resourceBindPoint.stages));

			++resourceMappings.m_NumRootParameters;
		}
		break;

		case SDeviceResourceLayoutDesc::ELayoutSlotType::ResourceSet:
		{
			int descriptorCount = 0;
			int descriptorRangeStart = resourceMappings.m_DescRangeCursor;

			::EShaderStage shaderStages = EShaderStage_None;

			for (auto itResource : itLayoutBinding.second)
			{
				const SResourceBindPoint& resourceBindPoint = itResource.first;
				const SResourceBinding&   resourceBinding = itResource.second;

				if (itResource.first.slotType == SResourceBindPoint::ESlotType::Sampler)
				{
					const NCryDX12::CSamplerState& samplerState = GET_DX12_SAMPLERSTATE(resourceBinding.samplerState)->GetDX12SamplerState();

					// copy parameters from sampler desc first
					auto samplerDesc = samplerState.GetSamplerDesc();
					memcpy(&resourceMappings.m_StaticSamplers[resourceMappings.m_NumStaticSamplers], &samplerDesc, sizeof(samplerDesc));

					// fill in remaining parameters
					auto& staticSamplerDesc = resourceMappings.m_StaticSamplers[resourceMappings.m_NumStaticSamplers];
					staticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
					staticSamplerDesc.MinLOD = samplerDesc.MinLOD;
					staticSamplerDesc.MaxLOD = samplerDesc.MaxLOD;
					staticSamplerDesc.ShaderRegister = resourceBindPoint.slotNumber;
					staticSamplerDesc.RegisterSpace = 0;
					staticSamplerDesc.ShaderVisibility = GetShaderVisibility(resourceBindPoint.stages);

					++resourceMappings.m_NumStaticSamplers;

					CRY_ASSERT(resourceMappings.m_NumStaticSamplers <= CRY_ARRAY_COUNT(resourceMappings.m_StaticSamplers));
				}
				else
				{
					shaderStages |= resourceBindPoint.stages;

					resourceMappings.m_DescRanges[resourceMappings.m_DescRangeCursor++] = GetDescriptorRange(resourceBindPoint, descriptorCount);
					CRY_ASSERT(resourceMappings.m_DescRangeCursor < CRY_ARRAY_COUNT(resourceMappings.m_DescRanges));

					++descriptorCount;
				}
			}

			if (descriptorCount > 0)
			{
				resourceMappings.m_RootParameters[layoutBindPoint.layoutSlot].InitAsDescriptorTable(
					resourceMappings.m_DescRangeCursor - descriptorRangeStart, &resourceMappings.m_DescRanges[descriptorRangeStart], GetShaderVisibility(shaderStages));

				++resourceMappings.m_NumRootParameters;

				CRY_ASSERT(resourceMappings.m_NumRootParameters <= CRY_ARRAY_COUNT(resourceMappings.m_RootParameters));
			}
		}
		break;

		default:
			CRY_ASSERT(false);
		}
	}

	m_pDevice->GetRootSignatureCache().GetOrCreateRootSignature(resourceMappings, true, m_pDevice->GetNodeMask(), m_pRootSignature);
	return m_pRootSignature != nullptr;
}


bool CDeviceResourceSet_DX12::GatherDescriptors(
	const CDeviceResourceSetDesc& resourceDesc,
	StackDescriptorVector& descriptors,
	CDescriptorBlock& descriptorScratchSpace,
	std::vector< std::pair<SResourceBindPoint, CCryDX12Buffer*> >& constantBuffersInUse,
	std::vector< std::pair<SResourceBindPoint, CCryDX12ShaderResourceView*> >& shaderResourceViewsInUse,
	std::vector< std::pair<SResourceBindPoint, CCryDX12UnorderedAccessView*> >& unorderedAccessViewsInUse) const
{
	const int descriptorCount = descriptors.size();

	constantBuffersInUse.resize(0);
	constantBuffersInUse.reserve(descriptorCount);

	shaderResourceViewsInUse.resize(0);
	shaderResourceViewsInUse.reserve(descriptorCount);

	unorderedAccessViewsInUse.resize(0);
	unorderedAccessViewsInUse.reserve(descriptorCount);

	for (const auto& it : resourceDesc.GetResources())
	{
		const SResourceBindPoint& bindPoint = it.first;
		const SResourceBinding&   resource = it.second;

		if (!resource.IsValid())
		{
			CRY_ASSERT_MESSAGE(false, "Invalid resource in resource set desc. Update failed");
			return false;
		}

		switch (bindPoint.slotType)
		{
		case SResourceBindPoint::ESlotType::ConstantBuffer:
		{
			buffer_size_t start, length;
			CCryDX12Buffer* constantBuffer = reinterpret_cast<CCryDX12Buffer*>(resource.pConstantBuffer->GetD3D(&start, &length));

			{
				D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;

				const NCryDX12::CView& pCBV = constantBuffer->GetDX12View();
				assert(pCBV.GetType() == EVT_ConstantBufferView);

				cbvDesc = pCBV.GetCBVDesc();
				cbvDesc.BufferLocation += start;
				cbvDesc.SizeInBytes = length > 0 ? length : cbvDesc.SizeInBytes - start;
				cbvDesc.SizeInBytes = min(cbvDesc.SizeInBytes, D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * NCryDX12::CONSTANT_BUFFER_ELEMENT_SIZE);

				GetDevice()->GetD3D12Device()->CreateConstantBufferView(&cbvDesc, descriptorScratchSpace.GetHandleOffsetCPU(0));
			}

			descriptors.emplace_back(descriptorScratchSpace.GetHandleOffsetCPU(0));
			descriptorScratchSpace.IncrementCursor();

			constantBuffersInUse.emplace_back(bindPoint, constantBuffer);
		}
		break;

		case SResourceBindPoint::ESlotType::TextureAndBuffer:
		{
			CDeviceResource* pResource = uint8(bindPoint.flags & SResourceBindPoint::EFlags::IsTexture)
				? (CDeviceResource*)resource.pTexture->GetDevTexture()
				: (CDeviceResource*)resource.pBuffer->GetDevBuffer();

			CCryDX12ShaderResourceView* pSrv = GET_DX12_SHADER_VIEW(pResource, resource.view);
			descriptors.emplace_back(pSrv->GetDX12View().GetDescriptorHandle());

			shaderResourceViewsInUse.emplace_back(bindPoint, pSrv);
		}
		break;

		case SResourceBindPoint::ESlotType::UnorderedAccessView:
		{
			CDeviceResource* pResource = uint8(bindPoint.flags & SResourceBindPoint::EFlags::IsTexture)
				? (CDeviceResource*)resource.pTexture->GetDevTexture()
				: (CDeviceResource*)resource.pBuffer->GetDevBuffer();

			CCryDX12UnorderedAccessView* pUav = GET_DX12_UNORDERED_VIEW(pResource, resource.view);
			descriptors.emplace_back(pUav->GetDX12View().GetDescriptorHandle());

			unorderedAccessViewsInUse.emplace_back(bindPoint, pUav);
		}
		break;

		case SResourceBindPoint::ESlotType::Sampler:
			break;
		}
	}

	return true;
}

void CDeviceResourceSet_DX12::FillDescriptorBlock(
	StackDescriptorVector& descriptors,
	CDescriptorBlock& descriptorBlock) const
{
	static constexpr UINT DescriptorRangesOfOnes[] = {
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	};
	// 64 descriptors
	static constexpr auto chunkSize = CRY_ARRAY_COUNT(DescriptorRangesOfOnes);

	CRY_ASSERT(descriptorBlock.GetCapacity() - descriptorBlock.GetCursor() == descriptors.size());
	auto pDevice = GetDevice()->GetD3D12Device();

	if (!descriptors.empty())
	{
		// Deal with unbound number of descriptors in chunks of chunkSize
		const auto srcSize = UINT(descriptors.size());
		for (UINT i = 0; i < srcSize; i += chunkSize)
		{
			const UINT dstRangeCount = 1;
			const UINT srcRangeCount = std::min<UINT>(srcSize - i, chunkSize);

			D3D12_CPU_DESCRIPTOR_HANDLE dstHandle = descriptorBlock.GetHandleOffsetCPU(i);

			pDevice->CopyDescriptors(
				dstRangeCount, &dstHandle, &srcRangeCount,
				srcRangeCount, &descriptors[i], &DescriptorRangesOfOnes[0],
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
	}
}

