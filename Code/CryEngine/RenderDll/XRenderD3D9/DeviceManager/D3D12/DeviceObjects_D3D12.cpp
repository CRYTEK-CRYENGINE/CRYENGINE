// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include <CryCore/CryCustomTypes.h>
#include "../../DeviceManager/DeviceObjects.h"
#include "DriverD3D.h"

#ifndef DEVRES_USE_STAGING_POOL
#error StagingPool is a requirement for DX12
#endif

#ifdef DX12_LINKEDADAPTER
	#include "DX12/API/Redirections/D3D12Device.inl"
#endif

using namespace NCryDX12;
D3D12_SHADER_BYTECODE g_EmptyShader;

#define GET_DX12_SHADER_VIEW(uniformBufferOrTexture, rView)    reinterpret_cast<CCryDX12ShaderResourceView*>((uniformBufferOrTexture)->LookupSRV(rView))
#define GET_DX12_UNORDERED_VIEW(uniformBufferOrTexture, rView) reinterpret_cast<CCryDX12UnorderedAccessView*>((uniformBufferOrTexture)->LookupUAV(rView))
#define GET_DX12_DEPTHSTENCIL_VIEW(dsTarget, dView)            reinterpret_cast<CCryDX12DepthStencilView*>((dsTarget)->LookupDSV(dView));
#define GET_DX12_RENDERTARGET_VIEW(rTarget, rView)             reinterpret_cast<CCryDX12RenderTargetView*>((rTarget)->LookupRTV(rView))
#define GET_DX12_SAMPLERSTATE(uniformSamplerState)             reinterpret_cast<CCryDX12SamplerState*>(CDeviceObjectFactory::LookupSamplerState(uniformSamplerState).second)

#define GET_DX12_TEXTURE_RESOURCE(uniformTexture)              reinterpret_cast<CCryDX12Resource<ID3D11Resource>*>((uniformTexture)->GetDevTexture()->GetBaseTexture())
#define GET_DX12_BUFFER_RESOURCE(uniformBuffer)                reinterpret_cast<CCryDX12Resource<ID3D11Resource>*>((uniformBuffer).GetDevBuffer()->GetBaseBuffer())
#define GET_DX12_CONSTANTBUFFER_RESOURCE(constantBuffer)       reinterpret_cast<CCryDX12Buffer*>((constantBuffer)->GetD3D())

static NCryDX12::CDevice* GetDevice()
{
	return GetDeviceObjectFactory().GetDX12Device();
}

static NCryDX12::CCommandScheduler* GetScheduler()
{
	return GetDeviceObjectFactory().GetDX12Scheduler();
}

D3D12_SHADER_RESOURCE_VIEW_DESC GetNullSrvDesc(const CTexture* pTexture)
{
	D3D12_SRV_DIMENSION viewDimension[eTT_MaxTexType] =
	{
		D3D12_SRV_DIMENSION_TEXTURE1D,                      // eTT_1D
		D3D12_SRV_DIMENSION_TEXTURE2D,                      // eTT_2D
		D3D12_SRV_DIMENSION_TEXTURE3D,                      // eTT_3D
		D3D12_SRV_DIMENSION_TEXTURECUBE,                    // eTT_Cube
		D3D12_SRV_DIMENSION_TEXTURECUBEARRAY,               // eTT_CubeArray
		D3D12_SRV_DIMENSION_TEXTURE2D,                      // eTT_Dyn2D
		D3D12_SRV_DIMENSION_TEXTURE2D,                      // eTT_User
		D3D12_SRV_DIMENSION_TEXTURECUBE,                    // eTT_NearestCube
		D3D12_SRV_DIMENSION_TEXTURE2DARRAY,                 // eTT_2DArray
		D3D12_SRV_DIMENSION_TEXTURE2DMS,                    // eTT_2DMS
		D3D12_SRV_DIMENSION_TEXTURE2D,                      // eTT_Auto2D
	};

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroStruct(srvDesc);

	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = pTexture ? viewDimension[pTexture->GetTexType()] : D3D12_SRV_DIMENSION_TEXTURE2D;

	return srvDesc;
}

D3D12_UNORDERED_ACCESS_VIEW_DESC GetNullUavDesc(const CTexture* pTexture)
{
	D3D12_UAV_DIMENSION viewDimension[eTT_MaxTexType] =
	{
		D3D12_UAV_DIMENSION_TEXTURE1D,                      // eTT_1D
		D3D12_UAV_DIMENSION_TEXTURE2D,                      // eTT_2D
		D3D12_UAV_DIMENSION_TEXTURE3D,                      // eTT_3D
		D3D12_UAV_DIMENSION_UNKNOWN,                        // eTT_Cube
		D3D12_UAV_DIMENSION_UNKNOWN,                        // eTT_CubeArray
		D3D12_UAV_DIMENSION_TEXTURE2D,                      // eTT_Dyn2D
		D3D12_UAV_DIMENSION_TEXTURE2D,                      // eTT_User
		D3D12_UAV_DIMENSION_UNKNOWN,                        // eTT_NearestCube
		D3D12_UAV_DIMENSION_TEXTURE2DARRAY,                 // eTT_2DArray
		D3D12_UAV_DIMENSION_UNKNOWN,                        // eTT_2DMS
		D3D12_UAV_DIMENSION_TEXTURE2D,                      // eTT_Auto2D
	};

	D3D12_UNORDERED_ACCESS_VIEW_DESC result;
	ZeroStruct(result);
	result.ViewDimension = pTexture ? viewDimension[pTexture->GetTexType()] : D3D12_UAV_DIMENSION_TEXTURE2D;
	result.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	return result;
}

D3D12_SHADER_RESOURCE_VIEW_DESC GetNullSrvDesc(const CGpuBuffer& buffer)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroStruct(srvDesc);
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32_UINT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;

	return srvDesc;
}

D3D12_UNORDERED_ACCESS_VIEW_DESC GetNullUavDesc(const CGpuBuffer& buffer)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	ZeroStruct(uavDesc);
	uavDesc.Format = DXGI_FORMAT_R32_UINT;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

	return uavDesc;
}

D3D12_SHADER_VISIBILITY GetShaderVisibility(::EShaderStage shaderStages)
{
	static D3D12_SHADER_VISIBILITY shaderVisibility[eHWSC_Num + 1] =
	{
		D3D12_SHADER_VISIBILITY_VERTEX,     // eHWSC_Vertex
		D3D12_SHADER_VISIBILITY_PIXEL,      // eHWSC_Pixel
		D3D12_SHADER_VISIBILITY_GEOMETRY,   // eHWSC_Geometry
		D3D12_SHADER_VISIBILITY_ALL,        // eHWSC_Compute
		D3D12_SHADER_VISIBILITY_DOMAIN,     // eHWSC_Domain
		D3D12_SHADER_VISIBILITY_HULL,       // eHWSC_Hull
		D3D12_SHADER_VISIBILITY_ALL,        // eHWSC_Num
	};

	EHWShaderClass shaderClass = eHWSC_Num;

	if ((int(shaderStages) & (int(shaderStages) - 1)) == 0) // only bound to a single shader stage?
	{
		for (shaderClass = eHWSC_Vertex; shaderClass != eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
		{
			if (shaderStages & SHADERSTAGE_FROM_SHADERCLASS(shaderClass))
				break;
		}
	}

	return shaderVisibility[shaderClass];
};

CD3DX12_DESCRIPTOR_RANGE GetDescriptorRange(SResourceBindPoint bindPoint, int descriptorIndex)
{
	CD3DX12_DESCRIPTOR_RANGE result;
	result.BaseShaderRegister = bindPoint.slotNumber;
	result.NumDescriptors = 1;
	result.OffsetInDescriptorsFromTableStart = descriptorIndex;
	result.RegisterSpace = 0;

	switch (bindPoint.slotType)
	{
		case SResourceBindPoint::ESlotType::ConstantBuffer:      result.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;     break;
		case SResourceBindPoint::ESlotType::TextureAndBuffer:    result.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;     break;
		case SResourceBindPoint::ESlotType::UnorderedAccessView: result.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;     break;
		case SResourceBindPoint::ESlotType::Sampler:             result.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER; break;
	};

	return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CDeviceResourceSet_DX12 : public CDeviceResourceSet
{
public:
	friend CDeviceGraphicsCommandInterfaceImpl;
	friend CDeviceComputeCommandInterfaceImpl;

	CDeviceResourceSet_DX12(CDeviceResourceSet::EFlags flags)
		: CDeviceResourceSet(flags)
		, m_pDescriptors(nullptr)
	{}

	~CDeviceResourceSet_DX12()
	{
		ReleaseDescriptors();
	}

	virtual bool UpdateImpl(const CDeviceResourceSetDesc& desc, CDeviceResourceSetDesc::EDirtyFlags dirtyFlags);

	const CDescriptorBlock& GetDescriptorBlock() const { return m_DescriptorBlockDX12; }

protected:
	// Workaround for: "template<template<typename, class> class C, typename T> using CContainer = C<T, CSingleBlockAllocator<T>>;"
	typedef std::vector<D3D12_CPU_DESCRIPTOR_HANDLE, CryStack::CSingleBlockAllocator<D3D12_CPU_DESCRIPTOR_HANDLE>> StackDescriptorVector;
	const inline size_t NoAlign(size_t nSize) { return nSize; }

	void ReleaseDescriptors();

	bool GatherDescriptors(
	  const CDeviceResourceSetDesc& resourceDesc,
	  StackDescriptorVector& descriptors,
	  CDescriptorBlock& descriptorScratchSpace,
	  std::vector< std::pair<SResourceBindPoint, CCryDX12Buffer*> >& constantBuffersInUse,
	  std::vector< std::pair<SResourceBindPoint, CCryDX12ShaderResourceView*> >& shaderResourceViewsInUse,
	  std::vector< std::pair<SResourceBindPoint, CCryDX12UnorderedAccessView*> >& unorderedAccessViewsInUse) const;

	void FillDescriptorBlock(
		StackDescriptorVector& descriptors,
		CDescriptorBlock& descriptorBlock) const;

	CDescriptorBlock  m_DescriptorBlockDX12;
	SDescriptorBlock* m_pDescriptors;

	std::vector< std::pair<SResourceBindPoint, CCryDX12Buffer*> >              m_ConstantBuffersInUse;
	std::vector< std::pair<SResourceBindPoint, CCryDX12ShaderResourceView*> >  m_ShaderResourceViewsInUse;
	std::vector< std::pair<SResourceBindPoint, CCryDX12UnorderedAccessView*> > m_UnorderedAccessViewsInUse;
};

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
		const SResourceBinding&   resource  = it.second;

		if (!resource.IsValid())
		{
			CRY_ASSERT_MESSAGE(false, "Invalid resource in resource set desc. Update failed");
			return false;
		}

		switch(bindPoint.slotType)
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

static const UINT DescriptorRangesOfOnes[256] = {
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
};

void CDeviceResourceSet_DX12::FillDescriptorBlock(
	StackDescriptorVector& descriptors,
	CDescriptorBlock& descriptorBlock) const
{
	CRY_ASSERT(descriptorBlock.GetCapacity() - descriptorBlock.GetCursor() == descriptors.size());
	auto pDevice = GetDevice()->GetD3D12Device();

	if (!descriptors.empty())
	{
		D3D12_CPU_DESCRIPTOR_HANDLE dstHandle = descriptorBlock.GetHandleOffsetCPU(0);

		// Deal with unbound number of descriptors in chunks of 256
		for (UINT i = 0, srcSize = UINT(descriptors.size()), chunkSize = CRY_ARRAY_COUNT(DescriptorRangesOfOnes); i < srcSize; i += chunkSize)
		{
			const UINT dstRangeCount = 1;
			const UINT srcRangeCount = std::min(srcSize - i, chunkSize);

			pDevice->CopyDescriptors(
				dstRangeCount, &dstHandle, &srcRangeCount,
				srcRangeCount, &descriptors[i], &DescriptorRangesOfOnes[0],
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CDeviceResourceLayout_DX12 : public CDeviceResourceLayout
{
public:
	CDeviceResourceLayout_DX12(CDevice* pDevice, UsedBindSlotSet requiredResourceBindings)
		: CDeviceResourceLayout(requiredResourceBindings)
		, m_pDevice(pDevice)
	{}

	bool            Init(const SDeviceResourceLayoutDesc& desc);
	CRootSignature* GetRootSignature() const { return m_pRootSignature.get(); }

protected:
	DX12_PTR(CDevice) m_pDevice;
	DX12_PTR(CRootSignature) m_pRootSignature;
};

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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CDeviceGraphicsPSO_DX12 : public CDeviceGraphicsPSO
{
public:

	CDeviceGraphicsPSO_DX12(CDevice* pDevice)
		: m_pDevice(pDevice)
	{}

	virtual EInitResult      Init(const CDeviceGraphicsPSODesc& desc) final;

	CGraphicsPSO*            GetGraphicsPSO() const       { return m_pGraphicsPSO.get(); }
	D3D12_PRIMITIVE_TOPOLOGY GetPrimitiveTopology() const { return m_PrimitiveTopology; }

protected:
	const SInputLayout*      m_pInputLayout;
	D3D12_PRIMITIVE_TOPOLOGY m_PrimitiveTopology;

	DX12_PTR(CDevice) m_pDevice;
	DX12_PTR(CGraphicsPSO) m_pGraphicsPSO;
};

CDeviceGraphicsPSO::EInitResult CDeviceGraphicsPSO_DX12::Init(const CDeviceGraphicsPSODesc& psoDesc)
{
	m_bValid = false;
	m_nUpdateCount++;

	if (psoDesc.m_pResourceLayout == NULL)
		return EInitResult::Failure;

	const CDeviceRenderPassDesc* pRenderPassDesc = GetDeviceObjectFactory().GetRenderPassDesc(psoDesc.m_pRenderPass.get());
	if (!pRenderPassDesc)
		return EInitResult::Failure;

	auto hwShaders = SDeviceObjectHelpers::GetShaderInstanceInfo(psoDesc.m_pShader, psoDesc.m_technique, psoDesc.m_ShaderFlags_RT, psoDesc.m_ShaderFlags_MD, psoDesc.m_ShaderFlags_MDV, nullptr, psoDesc.m_bAllowTesselation);
	// validate shaders first
	for (EHWShaderClass shaderClass = eHWSC_Vertex; shaderClass < eHWSC_Num; shaderClass = EHWShaderClass(shaderClass + 1))
	{
		if (hwShaders[shaderClass].pHwShader && hwShaders[shaderClass].pHwShaderInstance == NULL)
			return EInitResult::Failure;

		m_pHwShaderInstances[shaderClass] = hwShaders[shaderClass].pHwShaderInstance;
	}

	D3D11_RASTERIZER_DESC rasterizerDesc;
	D3D11_BLEND_DESC blendDesc;
	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	psoDesc.FillDescs(rasterizerDesc, blendDesc, depthStencilDesc);

	// prepare pso init params
	CGraphicsPSO::SInitParams psoInitParams;
	ZeroStruct(psoInitParams);

	// root signature
	psoInitParams.m_Desc.pRootSignature = reinterpret_cast<CDeviceResourceLayout_DX12*>(psoDesc.m_pResourceLayout.get())->GetRootSignature()->GetD3D12RootSignature();

	// rendertarget formats
	{
		
		const auto& renderTargets = pRenderPassDesc->GetRenderTargets();

		for (int i = 0; i < renderTargets.size(); ++i)
		{
			if (!renderTargets[i].pTexture)
				break;

			psoInitParams.m_Desc.RTVFormats[psoInitParams.m_Desc.NumRenderTargets++] = renderTargets[i].GetResourceFormat();
		}

		if (pRenderPassDesc->GetDepthTarget().pTexture)
			psoInitParams.m_Desc.DSVFormat = pRenderPassDesc->GetDepthTarget().GetResourceFormat();
	}

	// blend state
	{
		psoInitParams.m_Desc.BlendState.AlphaToCoverageEnable = blendDesc.AlphaToCoverageEnable;
		psoInitParams.m_Desc.BlendState.IndependentBlendEnable = blendDesc.IndependentBlendEnable;

		for (int i = 0; i < CRY_ARRAY_COUNT(blendDesc.RenderTarget); ++i)
		{
			uint32 RenderTargetWriteMask = i < psoInitParams.m_Desc.NumRenderTargets ? DeviceFormats::GetWriteMask(psoInitParams.m_Desc.RTVFormats[i]) : 0;

			psoInitParams.m_Desc.BlendState.RenderTarget[i].BlendEnable = blendDesc.RenderTarget[i].BlendEnable;
			psoInitParams.m_Desc.BlendState.RenderTarget[i].LogicOpEnable = false;
			psoInitParams.m_Desc.BlendState.RenderTarget[i].SrcBlend = (D3D12_BLEND)blendDesc.RenderTarget[i].SrcBlend;
			psoInitParams.m_Desc.BlendState.RenderTarget[i].DestBlend = (D3D12_BLEND)blendDesc.RenderTarget[i].DestBlend;
			psoInitParams.m_Desc.BlendState.RenderTarget[i].BlendOp = (D3D12_BLEND_OP)blendDesc.RenderTarget[i].BlendOp;
			psoInitParams.m_Desc.BlendState.RenderTarget[i].SrcBlendAlpha = (D3D12_BLEND)blendDesc.RenderTarget[i].SrcBlendAlpha;
			psoInitParams.m_Desc.BlendState.RenderTarget[i].DestBlendAlpha = (D3D12_BLEND)blendDesc.RenderTarget[i].DestBlendAlpha;
			psoInitParams.m_Desc.BlendState.RenderTarget[i].BlendOpAlpha = (D3D12_BLEND_OP)blendDesc.RenderTarget[i].BlendOpAlpha;
			psoInitParams.m_Desc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_CLEAR;
			psoInitParams.m_Desc.BlendState.RenderTarget[i].RenderTargetWriteMask = blendDesc.RenderTarget[i].RenderTargetWriteMask & RenderTargetWriteMask;
		}
	}

	// depth stencil and rasterizer state
	memcpy(&psoInitParams.m_Desc.DepthStencilState, &depthStencilDesc, sizeof(depthStencilDesc));
	memcpy(&psoInitParams.m_Desc.RasterizerState, &rasterizerDesc, sizeof(rasterizerDesc));

	auto extractShaderBytecode = [&](EHWShaderClass shaderClass)
	{
		return hwShaders[shaderClass].pHwShader
		       ? reinterpret_cast<CCryDX12Shader*>(hwShaders[shaderClass].pDeviceShader)->GetD3D12ShaderBytecode()
		       : g_EmptyShader;
	};

	psoInitParams.m_Desc.NodeMask = m_pDevice->GetNodeMask();
	psoInitParams.m_Desc.VS = extractShaderBytecode(eHWSC_Vertex);
	psoInitParams.m_Desc.DS = extractShaderBytecode(eHWSC_Domain);
	psoInitParams.m_Desc.HS = extractShaderBytecode(eHWSC_Hull);
	psoInitParams.m_Desc.GS = extractShaderBytecode(eHWSC_Geometry);
	psoInitParams.m_Desc.PS = extractShaderBytecode(eHWSC_Pixel);

	psoInitParams.m_Desc.SampleMask = UINT_MAX;
	psoInitParams.m_Desc.SampleDesc.Count = 1;
	// primitive topology
	{
		if (!ValidateShadersAndTopologyCombination(psoDesc, m_pHwShaderInstances))
			return EInitResult::ErrorShadersAndTopologyCombination;

		m_PrimitiveTopology = gcpRendD3D->FX_ConvertPrimitiveType(psoDesc.m_PrimitiveType);

		struct
		{
			ERenderPrimitiveType          primitiveType;
			D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopology;
		}
		topologyTypes[] =
		{
			{ eptUnknown,                D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED },
			{ eptTriangleList,           D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE  },
			{ eptTriangleStrip,          D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE  },
			{ eptLineList,               D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE      },
			{ eptLineStrip,              D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE      },
			{ eptPointList,              D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT     },
			{ ept1ControlPointPatchList, D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH     },
			{ ept2ControlPointPatchList, D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH     },
			{ ept3ControlPointPatchList, D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH     },
			{ ept4ControlPointPatchList, D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH     },
		};

		psoInitParams.m_Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
		for (int i = 0; i < CRY_ARRAY_COUNT(topologyTypes); ++i)
		{
			if (topologyTypes[i].primitiveType == psoDesc.m_PrimitiveType)
			{
				psoInitParams.m_Desc.PrimitiveTopologyType = topologyTypes[i].primitiveTopology;
				break;
			}
		}
	}

	// input layout
	if (psoDesc.m_VertexFormat != EDefaultInputLayouts::Empty)
	{
		if (psoDesc.m_VertexFormat == EDefaultInputLayouts::Unspecified)
			return EInitResult::Failure;

		if (CHWShader_D3D::SHWSInstance* pVsInstance = reinterpret_cast<CHWShader_D3D::SHWSInstance*>(hwShaders[eHWSC_Vertex].pHwShaderInstance))
		{
			uint32 streamMask = psoDesc.CombineVertexStreamMasks(uint8(pVsInstance->m_VStreamMask_Decl), psoDesc.m_ObjectStreamMask);

			m_pInputLayout = &CDeviceObjectFactory::LookupInputLayout(CDeviceObjectFactory::GetOrCreateInputLayoutHandle(&pVsInstance->m_Shader, streamMask, 0, 0, nullptr, psoDesc.m_VertexFormat)).first;

			// D3D12_INPUT_ELEMENT_DESC := D3D11_INPUT_ELEMENT_DESC
			psoInitParams.m_Desc.InputLayout.pInputElementDescs = reinterpret_cast<const D3D12_INPUT_ELEMENT_DESC*>(&m_pInputLayout->m_Declaration[0]);
			psoInitParams.m_Desc.InputLayout.NumElements = m_pInputLayout->m_Declaration.size();
		}
	}

#if defined(ENABLE_PROFILING_CODE)
	m_PrimitiveTypeForProfiling = psoDesc.m_PrimitiveType;
#endif

	m_pDevice->GetPSOCache().GetOrCreatePSO(psoInitParams, m_pGraphicsPSO);

	m_bValid = (m_pGraphicsPSO != nullptr);
	return (m_bValid ? EInitResult::Success : EInitResult::Failure);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CDeviceComputePSO_DX12 : public CDeviceComputePSO
{
public:

	CDeviceComputePSO_DX12(CDevice* pDevice)
		: m_pDevice(pDevice)
	{}

	virtual bool Init(const CDeviceComputePSODesc& desc) final;

	CComputePSO* GetComputePSO() const { return m_pComputePSO.get(); }

protected:

	DX12_PTR(CDevice) m_pDevice;
	DX12_PTR(CComputePSO) m_pComputePSO;
};

bool CDeviceComputePSO_DX12::Init(const CDeviceComputePSODesc& psoDesc)
{
	m_bValid = false;
	m_nUpdateCount++;

	if (psoDesc.m_pResourceLayout == NULL)
		return false;

	auto hwShaders = SDeviceObjectHelpers::GetShaderInstanceInfo(psoDesc.m_pShader, psoDesc.m_technique, psoDesc.m_ShaderFlags_RT, psoDesc.m_ShaderFlags_MD, psoDesc.m_ShaderFlags_MDV, nullptr, false);

	// validate shaders first
	if (hwShaders[eHWSC_Compute].pHwShader && hwShaders[eHWSC_Compute].pHwShaderInstance == NULL)
		return false;

	m_pHwShaderInstance = hwShaders[eHWSC_Compute].pHwShaderInstance;

	// prepare pso init params
	CComputePSO::SInitParams psoInitParams;
	ZeroStruct(psoInitParams);

	// root signature
	psoInitParams.m_Desc.pRootSignature = reinterpret_cast<CDeviceResourceLayout_DX12*>(psoDesc.m_pResourceLayout.get())->GetRootSignature()->GetD3D12RootSignature();

	// *INDENT-OFF*
	auto extractShaderBytecode = [&](EHWShaderClass shaderClass)
	{
		return hwShaders[shaderClass].pHwShader
			? reinterpret_cast<CCryDX12Shader*>(hwShaders[shaderClass].pDeviceShader)->GetD3D12ShaderBytecode()
			: g_EmptyShader;
	};
	// *INDENT-ON*

	psoInitParams.m_Desc.NodeMask = m_pDevice->GetNodeMask();
	psoInitParams.m_Desc.CS = extractShaderBytecode(eHWSC_Compute);

	m_pDevice->GetPSOCache().GetOrCreatePSO(psoInitParams, m_pComputePSO);

	return (m_bValid = (m_pComputePSO != nullptr));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CDeviceTimestampGroup::s_reservedGroups[4] = { false, false, false, false };

CDeviceTimestampGroup::CDeviceTimestampGroup()
	: m_numTimestamps(0)
	, m_groupIndex(0xFFFFFFFF)
	, m_fence(0)
	, m_frequency(0)
{
	m_timeValues.fill(0);
}

CDeviceTimestampGroup::~CDeviceTimestampGroup()
{
	s_reservedGroups[m_groupIndex] = false;
}

void CDeviceTimestampGroup::Init()
{
	GetDeviceObjectFactory().CreateFence(m_fence);
	m_numTimestamps = 0;

	m_groupIndex = 0xFFFFFFFF;
	for (uint32 i = 0; i < CRY_ARRAY_COUNT(s_reservedGroups); i++)
	{
		if (s_reservedGroups[i] == false)
		{
			s_reservedGroups[i] = true;
			m_groupIndex = i;
			break;
		}
	}

	assert(m_groupIndex < 0xFFFFFFFF);
}

void CDeviceTimestampGroup::BeginMeasurement()
{
	m_numTimestamps = 0;
}

void CDeviceTimestampGroup::EndMeasurement()
{
	GetDeviceObjectFactory().IssueFence(m_fence);

	CCryDX12DeviceContext* m_pDeviceContext = (CCryDX12DeviceContext*)gcpRendD3D->GetDeviceContext().GetRealDeviceContext();
	m_frequency = m_pDeviceContext->GetTimestampFrequency();
}

uint32 CDeviceTimestampGroup::IssueTimestamp(void* pCommandList)
{
	assert(m_numTimestamps < kMaxTimestamps);

	uint32 timestampIndex = m_groupIndex * kMaxTimestamps + m_numTimestamps;

	CCryDX12DeviceContext* m_pDeviceContext = (CCryDX12DeviceContext*)gcpRendD3D->GetDeviceContext().GetRealDeviceContext();
	m_pDeviceContext->InsertTimestamp(timestampIndex, 0, pCommandList ? reinterpret_cast<CDeviceCommandList*>(pCommandList)->GetDX12CommandList() : nullptr);

	++m_numTimestamps;
	return timestampIndex;
}

bool CDeviceTimestampGroup::ResolveTimestamps()
{
	if (m_numTimestamps == 0)
		return true;

	if (GetDeviceObjectFactory().SyncFence(m_fence, false, true) != S_OK)
		return false;

	CCryDX12DeviceContext* m_pDeviceContext = (CCryDX12DeviceContext*)gcpRendD3D->GetDeviceContext().GetRealDeviceContext();
	m_pDeviceContext->ResolveTimestamps();
	m_pDeviceContext->QueryTimestamps(m_groupIndex * kMaxTimestamps, m_numTimestamps, &m_timeValues[0]);

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CDeviceRenderPass::UpdateImpl(const CDeviceRenderPassDesc& passDesc)
{
	if (!passDesc.GetDeviceRendertargetViews(m_RenderTargetViews, m_RenderTargetCount))
		return false;

	if (!passDesc.GetDeviceDepthstencilView(m_pDepthStencilView))
		return false;

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeviceCommandListImpl::SetProfilerMarker(const char* label)
{
#if defined(ENABLE_FRAME_PROFILER_LABELS)
	PIXSetMarker(GetDX12CommandList()->GetD3D12CommandList(), 0, label);
#endif
}

void CDeviceCommandListImpl::BeginProfilerEvent(const char* label)
{
#if defined(ENABLE_FRAME_PROFILER_LABELS)
	m_profilerEventStack.push_back(label);
	PIXBeginEvent(GetDX12CommandList()->GetD3D12CommandList(), 0, label);
#endif
}

void CDeviceCommandListImpl::EndProfilerEvent(const char* label)
{
#if defined(ENABLE_FRAME_PROFILER_LABELS)
	PIXEndEvent(GetDX12CommandList()->GetD3D12CommandList());
	m_profilerEventStack.pop_back();
#endif
}

void CDeviceCommandListImpl::ClearStateImpl(bool bOutputMergerOnly) const
{
	// TODO: remove when r_GraphicsPipeline=0 algorithms don't exist anymore (and no emulated device-context is used)
	CD3D9Renderer* const __restrict rd = gcpRendD3D;

	D3DDepthSurface* pDSV = 0;
	D3DSurface*      pRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = { 0 };
	D3DUAV*          pUAVs[D3D11_PS_CS_UAV_REGISTER_COUNT] = { 0 };

	if (bOutputMergerOnly)
		rd->GetDeviceContext().OMSetRenderTargets/*AndUnorderedAccessViews*/(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, pRTVs, pDSV/*, 0, D3D11_PS_CS_UAV_REGISTER_COUNT, pUAVs, nullptr*/);
	else
		rd->GetDeviceContext().ClearState();
}

static auto lambdaCeaseCallback = [](void* cmd, uint nPoolId)
{
	reinterpret_cast<CDeviceCommandListImpl*>(cmd)->CeaseCommandListEvent(nPoolId);
};

static auto lambdaResumeCallback = [](void* cmd, uint nPoolId)
{
	reinterpret_cast<CDeviceCommandListImpl*>(cmd)->ResumeCommandListEvent(nPoolId);
};

void CDeviceCommandListImpl::CeaseCommandListEvent(int nPoolId)
{
	if (nPoolId != CMDQUEUE_GRAPHICS)
		return;

	auto* pCommandList = GetScheduler()->GetCommandList(nPoolId);
	CRY_ASSERT(m_sharedState.pCommandList == pCommandList || m_sharedState.pCommandList == nullptr);
	m_sharedState.pCommandList = nullptr;

#if defined(ENABLE_FRAME_PROFILER_LABELS)
	for (auto pEventLabel : m_profilerEventStack)
	{
		PIXEndEvent(pCommandList->GetD3D12CommandList());
	}
#endif

	reinterpret_cast<CDeviceCommandList*>(this)->Reset();
}

void CDeviceCommandListImpl::ResumeCommandListEvent(int nPoolId)
{
	if (nPoolId != CMDQUEUE_GRAPHICS)
		return;

	auto* pCommandList = GetScheduler()->GetCommandList(nPoolId);
	CRY_ASSERT(m_sharedState.pCommandList == nullptr);
	m_sharedState.pCommandList = pCommandList;

#if defined(ENABLE_FRAME_PROFILER_LABELS)
	for (auto pEventLabel : m_profilerEventStack)
	{
		PIXBeginEvent(pCommandList->GetD3D12CommandList(), 0, pEventLabel);
	}
#endif
}

void CDeviceCommandListImpl::ResetImpl()
{
	m_graphicsState.custom.primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
}

void CDeviceCommandListImpl::LockToThreadImpl()
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	pCommandListDX12->Begin();
	pCommandListDX12->SetResourceAndSamplerStateHeaps();
}

void CDeviceCommandListImpl::CloseImpl()
{
	FUNCTION_PROFILER_RENDERER
	GetDX12CommandList()->End();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeviceGraphicsCommandInterfaceImpl::PrepareUAVsForUseImpl(uint32 viewCount, CGpuBuffer** pViews, bool bCompute) const
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	for (int v = 0; v < viewCount; ++v)
	{
		if (CDeviceBuffer* pDevBuf = pViews[v]->GetDevBuffer())
		{
			// TODO: ResourceViewHandles[]
			const NCryDX12::CView& View = GET_DX12_UNORDERED_VIEW(pDevBuf, EDefaultResourceViews::UnorderedAccess)->GetDX12View();
			NCryDX12::CResource& Resource = View.GetDX12Resource();
			const D3D12_RESOURCE_STATES desiredState =
				bCompute 
				? D3D12_RESOURCE_STATE_UNORDERED_ACCESS 
				: D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

			if (Resource.InitHasBeenDeferred())
			{
				Resource.InitDeferred(pCommandListDX12, desiredState | D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			}

			assert(!((pCommandListDX12->GetD3D12ListType() != D3D12_COMMAND_LIST_TYPE_DIRECT) && (Resource.GetTargetState() & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) && (!Resource.IsOffCard())));

			pCommandListDX12->PrepareResourceUAVUsage(Resource, View, desiredState);
		}
	}
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareRenderPassForUseImpl(CDeviceRenderPass& renderPass) const
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	if (renderPass.m_pDepthStencilView)
	{
		const NCryDX12::CView& View   = renderPass.m_pDepthStencilView->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();

		D3D11_DEPTH_STENCIL_VIEW_DESC Desc; renderPass.m_pDepthStencilView->GetDesc(&Desc);
		const D3D12_RESOURCE_STATES desiredState = Desc.Flags & D3D11_DSV_READ_ONLY_DEPTH ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_DEPTH_WRITE;

		pCommandListDX12->PrepareResourceDSVUsage(Resource, View, desiredState);
	}

	// Get current render target views
	for (int i = 0; i < renderPass.m_RenderTargetCount; ++i)
	{
		const NCryDX12::CView& View   = renderPass.m_RenderTargetViews[i]->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource(); Resource.VerifyBackBuffer();

		const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_RENDER_TARGET;

		pCommandListDX12->PrepareResourceRTVUsage(Resource, View, desiredState);
	}
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareResourceForUseImpl(uint32 bindSlot, CTexture* pTexture, const ResourceViewHandle TextureView, ::EShaderStage srvUsage) const
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);

	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	CCryDX12ShaderResourceView* it = GET_DX12_SHADER_VIEW(pTexture->GetDevTexture(), TextureView);
	{
		const NCryDX12::CView& View = it->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();
		const D3D12_RESOURCE_STATES desiredState =
			(srvUsage &  EShaderStage_Pixel ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE     : D3D12_RESOURCE_STATE_COMMON) |
			(srvUsage & ~EShaderStage_Pixel ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON);

		if (Resource.InitHasBeenDeferred())
		{
			Resource.InitDeferred(pCommandListDX12, desiredState | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		}

		assert(!((pCommandListDX12->GetD3D12ListType() != D3D12_COMMAND_LIST_TYPE_DIRECT) && (Resource.GetTargetState() & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) && !(Resource.GetTargetState() & D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) && (!Resource.IsOffCard())));

		pCommandListDX12->PrepareResourceSRVUsage(Resource, View, desiredState);
	}
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareResourcesForUseImpl(uint32 bindSlot, CDeviceResourceSet* pResources) const
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);

	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();
	CDeviceResourceSet_DX12* pResourcesDX12 = reinterpret_cast<CDeviceResourceSet_DX12*>(pResources);

	for (auto& it : pResourcesDX12->m_ConstantBuffersInUse)
	{
		NCryDX12::CResource& Resource = it.second->GetDX12Resource();
		const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

		if (Resource.InitHasBeenDeferred())
		{
			Resource.InitDeferred(pCommandListDX12, desiredState | D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		}

		pCommandListDX12->PrepareResourceCBVUsage(Resource, desiredState);
	}

	for (auto& it : pResourcesDX12->m_ShaderResourceViewsInUse)
	{
		const NCryDX12::CView& View = it.second->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();

		const D3D12_RESOURCE_STATES desiredState =
			(it.first.stages &  EShaderStage_Pixel ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE     : D3D12_RESOURCE_STATE_COMMON) |
			(it.first.stages & ~EShaderStage_Pixel ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON);

		if (Resource.InitHasBeenDeferred())
		{
			Resource.InitDeferred(pCommandListDX12, desiredState | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		}

		assert(!((pCommandListDX12->GetD3D12ListType() != D3D12_COMMAND_LIST_TYPE_DIRECT) && (Resource.GetTargetState() & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) && !(Resource.GetTargetState() & D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) && (!Resource.IsOffCard())));

		pCommandListDX12->PrepareResourceSRVUsage(Resource, View, desiredState);
	}

	for (auto& it : pResourcesDX12->m_UnorderedAccessViewsInUse)
	{
		const NCryDX12::CView& View = it.second->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();
		const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

		if (Resource.InitHasBeenDeferred())
		{
			Resource.InitDeferred(pCommandListDX12, desiredState | D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}

		assert(!((pCommandListDX12->GetD3D12ListType() != D3D12_COMMAND_LIST_TYPE_DIRECT) && (Resource.GetTargetState() & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) && (!Resource.IsOffCard())));

		pCommandListDX12->PrepareResourceUAVUsage(Resource, View, desiredState);
	}
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pConstantBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass) const
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	NCryDX12::CResource& Resource = GET_DX12_CONSTANTBUFFER_RESOURCE(pConstantBuffer)->GetDX12Resource();
	const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

	if (Resource.InitHasBeenDeferred())
	{
		Resource.InitDeferred(pCommandListDX12, desiredState);
	}

	pCommandListDX12->PrepareResourceCBVUsage(Resource, desiredState);
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, ::EShaderStage shaderStages) const
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);
	PrepareInlineConstantBufferForUseImpl(bindSlot, pBuffer, shaderSlot, eHWSC_Num);
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareVertexBuffersForUseImpl(uint32 numStreams, uint32 lastStreamSlot, const CDeviceInputStream* vertexStreams) const
{
	if (m_graphicsState.vertexStreams != vertexStreams)
	{
		NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

		for (uint32 s = 0; s < numStreams; ++s)
		{
			const CDeviceInputStream& vertexStream = vertexStreams[s];

			CRY_ASSERT(vertexStream.hStream != ~0u);
			{
				// TODO: try not to call GetD3D() here, overhead: 1 call (and no inlinening) + 1 look-up + 2 accesses + branch
				buffer_size_t offset;
				auto pBuffer = reinterpret_cast<CCryDX12Buffer*>(gcpRendD3D.m_DevBufMan.GetD3D(vertexStream.hStream, &offset));
				NCryDX12::CResource& Resource = pBuffer->GetDX12Resource();
				const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

				if (Resource.InitHasBeenDeferred())
				{
					Resource.InitDeferred(pCommandListDX12, desiredState);
				}

				pCommandListDX12->PrepareResourceVBVUsage(Resource, desiredState);
			}
		}
	}
}

void CDeviceGraphicsCommandInterfaceImpl::PrepareIndexBufferForUseImpl(const CDeviceInputStream* indexStream) const
{
	if (m_graphicsState.indexStream != indexStream)
	{
		NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

		CRY_ASSERT(indexStream->hStream != ~0u);
		{
			// TODO: try not to call GetD3D() here, overhead: 1 call (and no inlinening) + 1 look-up + 2 accesses + branch
			buffer_size_t offset;
			auto pBuffer = reinterpret_cast<CCryDX12Buffer*>(gcpRendD3D.m_DevBufMan.GetD3D(indexStream->hStream, &offset));
			NCryDX12::CResource& Resource = pBuffer->GetDX12Resource();
			const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_INDEX_BUFFER;

			if (Resource.InitHasBeenDeferred())
			{
				Resource.InitDeferred(pCommandListDX12, desiredState);
			}

			pCommandListDX12->PrepareResourceIBVUsage(Resource, desiredState);
		}
	}
}

void CDeviceGraphicsCommandInterfaceImpl::BeginResourceTransitionsImpl(uint32 numTextures, CTexture** pTextures, EResourceTransitionType type)
{
	if (!CRenderer::CV_r_D3D12EarlyResourceBarriers)
		return;

	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	uint32 numBarriers = 0;
	for (uint32 i = 0; i < numTextures; i++)
	{
		CCryDX12Resource<ID3D11ResourceToImplement>* pResource = nullptr;
		if (pTextures[i] && pTextures[i]->GetDevTexture())
		{
			pResource = DX12_EXTRACT_RESOURCE(pTextures[i]->GetDevTexture()->GetBaseTexture());
		}

		if (pResource != nullptr)
		{
			NCryDX12::CResource& resource = pResource->GetDX12Resource();
			const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

			if (resource.NeedsTransitionBarrier(pCommandListDX12, desiredState))
			{
				pCommandListDX12->MaxResourceFenceValue(resource, CMDTYPE_WRITE);
				pCommandListDX12->BeginResourceStateTransition(resource, desiredState);
				pCommandListDX12->SetResourceFenceValue(resource, CMDTYPE_READ);
				numBarriers += 1;
			}
		}
	}

	if (numBarriers > 0)
	{
		pCommandListDX12->PendingResourceBarriers();

		if (CRenderer::CV_r_D3D12EarlyResourceBarriers > 1)
			gcpRendD3D->GetDeviceContext_Unsynchronized().Flush();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeviceGraphicsCommandInterfaceImpl::BeginRenderPassImpl(const CDeviceRenderPass& renderPass, const D3DRectangle& renderArea)
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	const NCryDX12::CView* pDSV = nullptr;
	const NCryDX12::CView* pRTV[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT] = { nullptr };

	// Get current depth stencil views
	if (renderPass.m_pDepthStencilView)
	{
		const NCryDX12::CView& View   = renderPass.m_pDepthStencilView->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();

		pDSV = &View;

		D3D11_DEPTH_STENCIL_VIEW_DESC Desc; renderPass.m_pDepthStencilView->GetDesc(&Desc);
		const D3D12_RESOURCE_STATES desiredState = Desc.Flags & D3D11_DSV_READ_ONLY_DEPTH ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_DEPTH_WRITE;

		assert(!Resource.NeedsTransitionBarrier(pCommandListDX12, View, desiredState));
	}

	// Get current render target views
	for (int i = 0; i < renderPass.m_RenderTargetCount; ++i)
	{
		const NCryDX12::CView& View   = renderPass.m_RenderTargetViews[i]->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource(); Resource.VerifyBackBuffer();

		pRTV[i] = &View;

		assert(!Resource.NeedsTransitionBarrier(pCommandListDX12, *pRTV[i], D3D12_RESOURCE_STATE_RENDER_TARGET));
	}

	pCommandListDX12->BindAndSetOutputViews(renderPass.m_RenderTargetCount, pRTV, pDSV);
}

void CDeviceGraphicsCommandInterfaceImpl::SetViewportsImpl(uint32 vpCount, const D3DViewPort* pViewports)
{
	// D3D11_VIEWPORT := D3D12_VIEWPORT
	GetDX12CommandList()->SetViewports(vpCount, (D3D12_VIEWPORT*)pViewports);
}

void CDeviceGraphicsCommandInterfaceImpl::SetScissorRectsImpl(uint32 rcCount, const D3DRectangle* pRects)
{
	// D3D11_RECT := D3D12_RECT
	GetDX12CommandList()->SetScissorRects(rcCount, (D3D12_RECT*)pRects);
}

void CDeviceGraphicsCommandInterfaceImpl::SetPipelineStateImpl(const CDeviceGraphicsPSO* devicePSO)
{
	auto pCommandListDX12 = GetDX12CommandList();
	const CDeviceGraphicsPSO_DX12* pDevicePSO = reinterpret_cast<const CDeviceGraphicsPSO_DX12*>(devicePSO);

	pCommandListDX12->SetPSO(pDevicePSO->GetGraphicsPSO());
	m_computeState.pPipelineState = nullptr; // on dx12 pipeline state is shared between graphics and compute

	D3D12_PRIMITIVE_TOPOLOGY psoPrimitiveTopology = pDevicePSO->GetPrimitiveTopology();
	if (m_graphicsState.custom.primitiveTopology.Set(psoPrimitiveTopology))
	{
	#if defined(ENABLE_PROFILING_CODE)
		CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nNumTopologySets);
	#endif

		pCommandListDX12->SetPrimitiveTopology(psoPrimitiveTopology);
	}
}

void CDeviceGraphicsCommandInterfaceImpl::SetResourceLayoutImpl(const CDeviceResourceLayout* resourceLayout)
{
	const CDeviceResourceLayout_DX12* pResourceLayoutDX12 = reinterpret_cast<const CDeviceResourceLayout_DX12*>(resourceLayout);
	GetDX12CommandList()->SetGraphicsRootSignature(pResourceLayoutDX12->GetRootSignature());
}

void CDeviceGraphicsCommandInterfaceImpl::SetVertexBuffersImpl(uint32 numStreams, uint32 lastStreamSlot, const CDeviceInputStream* vertexStreams)
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();
	pCommandListDX12->ClearVertexBufferHeap(lastStreamSlot + 1);

	for (uint32 s = 0; s < numStreams; ++s)
	{
		const CDeviceInputStream& vertexStream = vertexStreams[s];

		CRY_ASSERT(vertexStream.hStream != ~0u);
		{
			// TODO: try not to call GetD3D() here, overhead: 1 call (and no inlinening) + 1 look-up + 2 accesses + branch
			buffer_size_t offset;
			auto* pBuffer = reinterpret_cast<CCryDX12Buffer*>(gcpRendD3D.m_DevBufMan.GetD3D(vertexStream.hStream, &offset));

			assert(!pBuffer->GetDX12Resource().InitHasBeenDeferred());
			assert(!pBuffer->GetDX12Resource().NeedsTransitionBarrier(pCommandListDX12, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	#if defined(ENABLE_PROFILING_CODE)
			CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nNumBoundVertexBuffers[pBuffer->GetDX12Resource().IsOffCard()]);
	#endif

			pCommandListDX12->BindVertexBufferView(pBuffer->GetDX12View(), vertexStream.nSlot, TRange<uint32>(offset, offset), vertexStream.nStride);
		}
	}

	// TODO: if we know early that the resource(s) will be GENERIC_READ we can begin the barrier early and end it here
	pCommandListDX12->SetVertexBufferHeap(lastStreamSlot + 1);
}

void CDeviceGraphicsCommandInterfaceImpl::SetIndexBufferImpl(const CDeviceInputStream* indexStream)
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	CRY_ASSERT(indexStream->hStream != ~0u);
	{
		// TODO: try not to call GetD3D() here, overhead: 1 call (and no inlinening) + 1 look-up + 2 accesses + branch
		buffer_size_t offset;
		auto* pBuffer = reinterpret_cast<CCryDX12Buffer*>(gcpRendD3D.m_DevBufMan.GetD3D(indexStream->hStream, &offset));

		assert(!pBuffer->GetDX12Resource().InitHasBeenDeferred());
		assert(!pBuffer->GetDX12Resource().NeedsTransitionBarrier(pCommandListDX12, D3D12_RESOURCE_STATE_INDEX_BUFFER));

	#if defined(ENABLE_PROFILING_CODE)
		CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nNumBoundIndexBuffers[pBuffer->GetDX12Resource().IsOffCard()]);
	#endif

		// TODO: if we know early that the resource(s) will be GENERIC_READ we can begin the barrier early and end it here
	#if !defined(SUPPORT_FLEXIBLE_INDEXBUFFER)
		pCommandListDX12->BindAndSetIndexBufferView(pBuffer->GetDX12View(), DXGI_FORMAT_R16_UINT, offset);
	#else
		pCommandListDX12->BindAndSetIndexBufferView(pBuffer->GetDX12View(), (DXGI_FORMAT)indexStream->nStride, UINT(offset));
	#endif
	}
}

void CDeviceGraphicsCommandInterfaceImpl::SetResourcesImpl(uint32 bindSlot, const CDeviceResourceSet* pResources)
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);

	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();
	const CDeviceResourceSet_DX12* pResourcesDX12 = reinterpret_cast<const CDeviceResourceSet_DX12*>(pResources);

	for (auto& it : pResourcesDX12->m_ConstantBuffersInUse)
	{
		NCryDX12::CResource& Resource = it.second->GetDX12Resource();

		assert(!Resource.InitHasBeenDeferred());
		assert(!Resource.NeedsTransitionBarrier(pCommandListDX12, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	#if defined(ENABLE_PROFILING_CODE)
		CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nNumBoundConstBuffers[Resource.IsOffCard()]);
	#endif

		// TODO: if we know early that the resource(s) will be GENERIC_READ we can begin the barrier early and end it here
		pCommandListDX12->TrackResourceCBVUsage(Resource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	}

	for (auto& it : pResourcesDX12->m_ShaderResourceViewsInUse)
	{
		const NCryDX12::CView& View = it.second->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();
		const D3D12_RESOURCE_STATES desiredState =
			(it.first.stages &  EShaderStage_Pixel ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE     : D3D12_RESOURCE_STATE_COMMON) |
			(it.first.stages & ~EShaderStage_Pixel ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON);

		assert(!Resource.InitHasBeenDeferred());
		assert(!Resource.NeedsTransitionBarrier(pCommandListDX12, View, desiredState));

	#if defined(ENABLE_PROFILING_CODE)
		const bool bIsBuffer = Resource.GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER;
		if (bIsBuffer) CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nNumBoundUniformBuffers [Resource.IsOffCard()]);
		else           CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nNumBoundUniformTextures[Resource.IsOffCard()]);
	#endif

		// TODO: if we know early that the resource(s) will be GENERIC_READ we can begin the barrier early and end it here
		pCommandListDX12->TrackResourcePRVUsage(Resource, View, desiredState);
	}

	for (auto& it : pResourcesDX12->m_UnorderedAccessViewsInUse)
	{
		const NCryDX12::CView& View = it.second->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();

		assert(!Resource.InitHasBeenDeferred());
		assert(!Resource.NeedsTransitionBarrier(pCommandListDX12, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	#if defined(ENABLE_PROFILING_CODE)
		const bool bIsBuffer = Resource.GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER;
		if (bIsBuffer) CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nNumBoundUniformBuffers [Resource.IsOffCard()]);
		else           CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nNumBoundUniformTextures[Resource.IsOffCard()]);
	#endif

		// TODO: if we know early that the resource(s) will be GENERIC_READ we can begin the barrier early and end it here
		pCommandListDX12->TrackResourceUAVUsage(Resource, View, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	const CDescriptorBlock& descriptorBlock = pResourcesDX12->GetDescriptorBlock();
	pCommandListDX12->SetGraphicsDescriptorTable(bindSlot, descriptorBlock.GetHandleOffsetGPU(0));
}

void CDeviceGraphicsCommandInterfaceImpl::SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pBuffer, EConstantBufferShaderSlot shaderSlot, ::EShaderStage shaderStages)
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);
	SetInlineConstantBufferImpl(bindSlot, pBuffer, shaderSlot, eHWSC_Num);
}

void CDeviceGraphicsCommandInterfaceImpl::SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pConstantBuffer, EConstantBufferShaderSlot shaderSlot, EHWShaderClass shaderClass)
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	const NCryDX12::CView& View = GET_DX12_CONSTANTBUFFER_RESOURCE(pConstantBuffer)->GetDX12View();
	NCryDX12::CResource& Resource = View.GetDX12Resource();

	assert(!Resource.InitHasBeenDeferred());
	assert(!Resource.NeedsTransitionBarrier(pCommandListDX12, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	#if defined(ENABLE_PROFILING_CODE)
	CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nNumBoundInlineBuffers[Resource.IsOffCard()]);
	#endif

	// TODO: if we know early that the resource(s) will be GENERIC_READ we can begin the barrier early and end it here
	pCommandListDX12->TrackResourceCBVUsage(Resource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = View.GetCBVDesc().BufferLocation + pConstantBuffer->m_offset;
	pCommandListDX12->SetGraphicsConstantBufferView(bindSlot, gpuAddress);
}

void CDeviceGraphicsCommandInterfaceImpl::SetInlineConstantsImpl(uint32 bindSlot, uint32 constantCount, float* pConstants)
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);
	GetDX12CommandList()->SetGraphics32BitConstants(bindSlot, constantCount, pConstants, 0);
}

void CDeviceGraphicsCommandInterfaceImpl::SetStencilRefImpl(uint8 stencilRefValue)
{
	GetDX12CommandList()->SetStencilRef(stencilRefValue);
}

void CDeviceGraphicsCommandInterfaceImpl::SetDepthBiasImpl(float constBias, float slopeBias, float biasClamp)
{
	CRY_ASSERT_MESSAGE(false, "Depth bias can only be set via PSO on DirectX 12");
}

void CDeviceGraphicsCommandInterfaceImpl::DrawImpl(uint32 VertexCountPerInstance, uint32 InstanceCount, uint32 StartVertexLocation, uint32 StartInstanceLocation)
{
	GetDX12CommandList()->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}

void CDeviceGraphicsCommandInterfaceImpl::DrawIndexedImpl(uint32 IndexCountPerInstance, uint32 InstanceCount, uint32 StartIndexLocation, int BaseVertexLocation, uint32 StartInstanceLocation)
{
	GetDX12CommandList()->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}

void CDeviceGraphicsCommandInterfaceImpl::ClearSurfaceImpl(D3DSurface* pView, const FLOAT Color[4], UINT NumRects, const D3D11_RECT* pRects)
{
	const NCryDX12::CView& View = reinterpret_cast<CCryDX12RenderTargetView*>(pView)->GetDX12View();
	GetDX12CommandList()->ClearRenderTargetView(View, Color, NumRects, pRects);
}

void CDeviceGraphicsCommandInterfaceImpl::ClearSurfaceImpl(D3DDepthSurface* pView, int clearFlags, float depth, uint8 stencil, uint32 numRects, const D3D11_RECT* pRects)
{
	const NCryDX12::CView& View = reinterpret_cast<CCryDX12DepthStencilView*>(pView)->GetDX12View();
	GetDX12CommandList()->ClearDepthStencilView(View, D3D12_CLEAR_FLAGS(clearFlags), depth, stencil, numRects, pRects);
}

void CDeviceGraphicsCommandInterfaceImpl::BeginOcclusionQueryImpl(D3DOcclusionQuery* pQuery)
{
	CRY_ASSERT(GetDX12CommandList() == GetDeviceObjectFactory().GetCoreCommandList().GetDX12CommandList());
	gcpRendD3D->GetDeviceContext().GetRealDeviceContext()->Begin(pQuery);
}

void CDeviceGraphicsCommandInterfaceImpl::EndOcclusionQueryImpl(D3DOcclusionQuery* pQuery)
{
	CRY_ASSERT(GetDX12CommandList() == GetDeviceObjectFactory().GetCoreCommandList().GetDX12CommandList());
	gcpRendD3D->GetDeviceContext().GetRealDeviceContext()->End(pQuery);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeviceComputeCommandInterfaceImpl::PrepareUAVsForUseImpl(uint32 viewCount, CGpuBuffer** pViews) const
{
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	for (int v = 0; v < viewCount; ++v)
	{
		if (CDeviceBuffer* pDevBuf = pViews[v]->GetDevBuffer())
		{
			// TODO: ResourceViewHandles[]
			const NCryDX12::CView& View = GET_DX12_UNORDERED_VIEW(pDevBuf, EDefaultResourceViews::UnorderedAccess)->GetDX12View();
			NCryDX12::CResource& Resource = View.GetDX12Resource();
			const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

			if (Resource.InitHasBeenDeferred())
			{
				Resource.InitDeferred(pCommandListDX12, desiredState | D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			}

			assert(!((pCommandListDX12->GetD3D12ListType() != D3D12_COMMAND_LIST_TYPE_DIRECT) && (Resource.GetTargetState() & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) && (!Resource.IsOffCard())));

			pCommandListDX12->PrepareResourceUAVUsage(Resource, View, desiredState);
		}
	}
}

void CDeviceComputeCommandInterfaceImpl::PrepareResourcesForUseImpl(uint32 bindSlot, CDeviceResourceSet* pResources) const
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);

	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();
	CDeviceResourceSet_DX12* pResourcesDX12 = reinterpret_cast<CDeviceResourceSet_DX12*>(pResources);

	for (auto& it : pResourcesDX12->m_ConstantBuffersInUse)
	{
		NCryDX12::CResource& Resource = it.second->GetDX12Resource();
		const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

		if (Resource.InitHasBeenDeferred())
		{
			Resource.InitDeferred(pCommandListDX12, desiredState | D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		}

		pCommandListDX12->PrepareResourceCBVUsage(Resource, desiredState);
	}

	for (auto& it : pResourcesDX12->m_ShaderResourceViewsInUse)
	{
		const NCryDX12::CView& View = it.second->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();

		const D3D12_RESOURCE_STATES desiredState =
			(it.first.stages &  EShaderStage_Pixel ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE     : D3D12_RESOURCE_STATE_COMMON) |
			(it.first.stages & ~EShaderStage_Pixel ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON);

		if (Resource.InitHasBeenDeferred())
		{
			Resource.InitDeferred(pCommandListDX12, desiredState | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		}

		assert(!((pCommandListDX12->GetD3D12ListType() != D3D12_COMMAND_LIST_TYPE_DIRECT) && (Resource.GetTargetState() & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) && !(Resource.GetTargetState() & D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) && (!Resource.IsOffCard())));

		pCommandListDX12->PrepareResourceCRVUsage(Resource, View, desiredState);
	}

	for (auto& it : pResourcesDX12->m_UnorderedAccessViewsInUse)
	{
		const NCryDX12::CView& View = it.second->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();
		const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

		if (Resource.InitHasBeenDeferred())
		{
			Resource.InitDeferred(pCommandListDX12, desiredState | D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}

		assert(!((pCommandListDX12->GetD3D12ListType() != D3D12_COMMAND_LIST_TYPE_DIRECT) && (Resource.GetTargetState() & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) && !(Resource.GetTargetState() & D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) && (!Resource.IsOffCard())));

		pCommandListDX12->PrepareResourceUAVUsage(Resource, View, desiredState);
	}
}

void CDeviceComputeCommandInterfaceImpl::PrepareInlineConstantBufferForUseImpl(uint32 bindSlot, CConstantBuffer* pConstantBuffer, EConstantBufferShaderSlot shaderSlot, ::EShaderStage shaderStages) const
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	NCryDX12::CResource& Resource = GET_DX12_CONSTANTBUFFER_RESOURCE(pConstantBuffer)->GetDX12Resource();
	const D3D12_RESOURCE_STATES desiredState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

	pCommandListDX12->PrepareResourceCBVUsage(Resource, desiredState);
}

void CDeviceComputeCommandInterfaceImpl::SetPipelineStateImpl(const CDeviceComputePSO* pDevicePSO)
{
	const CDeviceComputePSO_DX12* pDevicePsoDX12 = reinterpret_cast<const CDeviceComputePSO_DX12*>(pDevicePSO);
	GetDX12CommandList()->SetPSO(pDevicePsoDX12->GetComputePSO());
	m_graphicsState.pPipelineState = nullptr; // on dx12 pipeline state is shared between graphics and compute
}

void CDeviceComputeCommandInterfaceImpl::SetResourceLayoutImpl(const CDeviceResourceLayout* pResourceLayout)
{
	const CDeviceResourceLayout_DX12* pResourceLayoutDX12 = reinterpret_cast<const CDeviceResourceLayout_DX12*>(pResourceLayout);
	GetDX12CommandList()->SetComputeRootSignature(pResourceLayoutDX12->GetRootSignature());
}

void CDeviceComputeCommandInterfaceImpl::SetResourcesImpl(uint32 bindSlot, const CDeviceResourceSet* pResources)
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);

	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();
	const CDeviceResourceSet_DX12* pResourcesDX12 = reinterpret_cast<const CDeviceResourceSet_DX12*>(pResources);

	for (auto& it : pResourcesDX12->m_ConstantBuffersInUse)
	{
		NCryDX12::CResource& Resource = it.second->GetDX12Resource();

		assert(!Resource.NeedsTransitionBarrier(pCommandListDX12, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	#if defined(ENABLE_PROFILING_CODE)
		CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nNumBoundConstBuffers[Resource.IsOffCard()]);
	#endif

		// TODO: if we know early that the resource(s) will be GENERIC_READ we can begin the barrier early and end it here
		pCommandListDX12->TrackResourceCBVUsage(Resource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	}

	for (auto& it : pResourcesDX12->m_ShaderResourceViewsInUse)
	{
		const NCryDX12::CView& View = it.second->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();
		const D3D12_RESOURCE_STATES desiredState =
			(it.first.stages &  EShaderStage_Pixel ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE     : D3D12_RESOURCE_STATE_COMMON) |
			(it.first.stages & ~EShaderStage_Pixel ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COMMON);

		assert(!Resource.NeedsTransitionBarrier(pCommandListDX12, View, desiredState));

	#if defined(ENABLE_PROFILING_CODE)
		CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nNumBoundUniformTextures[Resource.IsOffCard()]);
	#endif

		// TODO: if we know early that the resource(s) will be GENERIC_READ we can begin the barrier early and end it here
		pCommandListDX12->TrackResourceCRVUsage(Resource, View, desiredState);
	}

	for (auto& it : pResourcesDX12->m_UnorderedAccessViewsInUse)
	{
		const NCryDX12::CView& View = it.second->GetDX12View();
		NCryDX12::CResource& Resource = View.GetDX12Resource();

		assert(!Resource.NeedsTransitionBarrier(pCommandListDX12, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	#if defined(ENABLE_PROFILING_CODE)
		CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nNumBoundUniformBuffers[Resource.IsOffCard()]);
	#endif

		// TODO: if we know early that the resource(s) will be GENERIC_READ we can begin the barrier early and end it here
		pCommandListDX12->TrackResourceUAVUsage(Resource, View, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	const CDescriptorBlock& descriptorBlock = pResourcesDX12->GetDescriptorBlock();
	pCommandListDX12->SetComputeDescriptorTable(bindSlot, descriptorBlock.GetHandleOffsetGPU(0));
}

void CDeviceComputeCommandInterfaceImpl::SetInlineConstantBufferImpl(uint32 bindSlot, const CConstantBuffer* pConstantBuffer, EConstantBufferShaderSlot shaderSlot)
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);
	NCryDX12::CCommandList* pCommandListDX12 = GetDX12CommandList();

	const NCryDX12::CView& View = GET_DX12_CONSTANTBUFFER_RESOURCE(pConstantBuffer)->GetDX12View();
	NCryDX12::CResource& Resource = View.GetDX12Resource();

	assert(!Resource.NeedsTransitionBarrier(pCommandListDX12, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	#if defined(ENABLE_PROFILING_CODE)
	CryInterlockedIncrement(&gcpRendD3D->m_RP.m_PS[gcpRendD3D->m_RP.m_nProcessThreadID].m_nNumBoundInlineBuffers[Resource.IsOffCard()]);
	#endif

	// TODO: if we know early that the resource(s) will be GENERIC_READ we can begin the barrier early and end it here
	pCommandListDX12->TrackResourceCBVUsage(Resource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = View.GetCBVDesc().BufferLocation + pConstantBuffer->m_offset;
	pCommandListDX12->SetComputeConstantBufferView(bindSlot, gpuAddress);
}

void CDeviceComputeCommandInterfaceImpl::SetInlineConstantsImpl(uint32 bindSlot, uint32 constantCount, float* pConstants)
{
	CRY_ASSERT(bindSlot <= EResourceLayoutSlot_Max);
	GetDX12CommandList()->SetCompute32BitConstants(bindSlot, constantCount, pConstants, 0);
}

void CDeviceComputeCommandInterfaceImpl::DispatchImpl(uint32 X, uint32 Y, uint32 Z)
{
	GetDX12CommandList()->Dispatch(X, Y, Z);
}

void CDeviceComputeCommandInterfaceImpl::ClearUAVImpl(D3DUAV* pView, const FLOAT Values[4], UINT NumRects, const D3D11_RECT* pRects)
{
	const NCryDX12::CView& View = reinterpret_cast<CCryDX12UnorderedAccessView*>(pView)->GetDX12View();
	GetDX12CommandList()->ClearUnorderedAccessView(View, Values, NumRects, pRects);
}

void CDeviceComputeCommandInterfaceImpl::ClearUAVImpl(D3DUAV* pView, const UINT Values[4], UINT NumRects, const D3D11_RECT* pRects)
{
	const NCryDX12::CView& View = reinterpret_cast<CCryDX12UnorderedAccessView*>(pView)->GetDX12View();
	GetDX12CommandList()->ClearUnorderedAccessView(View, Values, NumRects, pRects);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceBuffer* pSrc, CDeviceBuffer* pDst)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().CopyResource(pDst->GetBaseBuffer(), pSrc->GetBaseBuffer());
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(D3DBuffer* pSrc, D3DBuffer* pDst)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().CopyResource(pDst, pSrc);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceTexture* pSrc, CDeviceTexture* pDst)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().CopyResource(pDst->GetBaseTexture(), pSrc->GetBaseTexture());
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceTexture* pSrc, D3DTexture* pDst)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().CopyResource(pDst, pSrc->GetBaseTexture());
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(D3DTexture* pSrc, D3DTexture* pDst)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().CopyResource(pDst, pSrc);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(D3DTexture* pSrc, CDeviceTexture* pDst)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().CopyResource(pDst->GetBaseTexture(), pSrc);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceBuffer* pSrc, CDeviceBuffer* pDst, const SResourceRegionMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	D3D11_BOX box = { region.SourceOffset.Left, region.SourceOffset.Top, region.SourceOffset.Front, region.SourceOffset.Left + region.Extent.Width, region.SourceOffset.Top + region.Extent.Height, region.SourceOffset.Front + region.Extent.Depth };
	rd->GetDeviceContext().CopySubresourcesRegion1(pDst->GetBaseBuffer(), region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc->GetBaseBuffer(), region.SourceOffset.Subresource, &box, region.Flags, region.Extent.Subresources);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(D3DBuffer* pSrc, D3DBuffer* pDst, const SResourceRegionMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	D3D11_BOX box = { region.SourceOffset.Left, region.SourceOffset.Top, region.SourceOffset.Front, region.SourceOffset.Left + region.Extent.Width, region.SourceOffset.Top + region.Extent.Height, region.SourceOffset.Front + region.Extent.Depth };
	rd->GetDeviceContext().CopySubresourcesRegion1(pDst, region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc, region.SourceOffset.Subresource, &box, region.Flags, region.Extent.Subresources);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceTexture* pSrc, CDeviceTexture* pDst, const SResourceRegionMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	D3D11_BOX box = { region.SourceOffset.Left, region.SourceOffset.Top, region.SourceOffset.Front, region.SourceOffset.Left + region.Extent.Width, region.SourceOffset.Top + region.Extent.Height, region.SourceOffset.Front + region.Extent.Depth };
	rd->GetDeviceContext().CopySubresourcesRegion1(pDst->GetBaseTexture(), region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc->GetBaseTexture(), region.SourceOffset.Subresource, &box, region.Flags, region.Extent.Subresources);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(D3DTexture* pSrc, CDeviceTexture* pDst, const SResourceRegionMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	D3D11_BOX box = { region.SourceOffset.Left, region.SourceOffset.Top, region.SourceOffset.Front, region.SourceOffset.Left + region.Extent.Width, region.SourceOffset.Top + region.Extent.Height, region.SourceOffset.Front + region.Extent.Depth };
	rd->GetDeviceContext().CopySubresourcesRegion1(pDst->GetBaseTexture(), region.DestinationOffset.Subresource, region.DestinationOffset.Left, region.DestinationOffset.Top, region.DestinationOffset.Front, pSrc, region.SourceOffset.Subresource, &box, region.Flags, region.Extent.Subresources);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(const void* pSrc, CConstantBuffer* pDst, const SResourceMemoryAlignment& memoryLayout)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().UpdateSubresource(pDst->GetD3D(), 0, nullptr, pSrc, 0, 0);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(const void* pSrc, CDeviceBuffer* pDst, const SResourceMemoryAlignment& memoryLayout)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().UpdateSubresource(pDst->GetBaseBuffer(), 0, nullptr, pSrc, memoryLayout.rowStride, memoryLayout.planeStride);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(const void* pSrc, CDeviceTexture* pDst, const SResourceMemoryAlignment& memoryLayout)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	rd->GetDeviceContext().UpdateSubresource(pDst->GetBaseTexture(), 0, nullptr, pSrc, memoryLayout.rowStride, memoryLayout.planeStride);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(const void* pSrc, CConstantBuffer* pDst, const SResourceMemoryMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	D3D11_BOX box = { region.ResourceOffset.Left, region.ResourceOffset.Top, region.ResourceOffset.Front, region.ResourceOffset.Left + region.Extent.Width, region.ResourceOffset.Top + region.Extent.Height, region.ResourceOffset.Front + region.Extent.Depth };
	rd->GetDeviceContext().UpdateSubresource1(pDst->GetD3D(), region.ResourceOffset.Subresource, &box, pSrc, region.MemoryLayout.rowStride, region.MemoryLayout.planeStride, region.Flags);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(const void* pSrc, CDeviceBuffer* pDst, const SResourceMemoryMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	D3D11_BOX box = { region.ResourceOffset.Left, region.ResourceOffset.Top, region.ResourceOffset.Front, region.ResourceOffset.Left + region.Extent.Width, region.ResourceOffset.Top + region.Extent.Height, region.ResourceOffset.Front + region.Extent.Depth };
	rd->GetDeviceContext().UpdateSubresource1(pDst->GetBaseBuffer(), region.ResourceOffset.Subresource, &box, pSrc, region.MemoryLayout.rowStride, region.MemoryLayout.planeStride, region.Flags);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(const void* pSrc, CDeviceTexture* pDst, const SResourceMemoryMapping& region)
{
	CD3D9Renderer* const __restrict rd = gcpRendD3D;
	D3D11_BOX box = { region.ResourceOffset.Left, region.ResourceOffset.Top, region.ResourceOffset.Front, region.ResourceOffset.Left + region.Extent.Width, region.ResourceOffset.Top + region.Extent.Height, region.ResourceOffset.Front + region.Extent.Depth };
	rd->GetDeviceContext().UpdateSubresource1(pDst->GetBaseTexture(), region.ResourceOffset.Subresource, &box, pSrc, region.MemoryLayout.rowStride, region.MemoryLayout.planeStride, region.Flags);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceBuffer* pSrc, void* pDst, const SResourceMemoryAlignment& memoryLayout)
{
	assert(0);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceTexture* pSrc, void* pDst, const SResourceMemoryAlignment& memoryLayout)
{
	assert(0);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceBuffer* pSrc, void* pDst, const SResourceMemoryMapping& region)
{
	assert(0);
}

void CDeviceCopyCommandInterfaceImpl::CopyImpl(CDeviceTexture* pSrc, void* pDst, const SResourceMemoryMapping& region)
{
	assert(0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDeviceObjectFactory::CDeviceObjectFactory()
	: m_fence_handle(0)
{
	memset(m_NullResources, 0, sizeof(m_NullResources));

	m_frameFenceCounter = 0;
	m_completedFrameFenceCounter = 0;
	for (uint32 i = 0; i < CRY_ARRAY_COUNT(m_frameFences); i++)
		m_frameFences[i] = NULL;

	m_pCoreCommandList.reset(new CDeviceCommandList());
	m_pCoreCommandList->m_sharedState.pCommandList = nullptr;

	m_pDX12Device    = nullptr;
	m_pDX12Scheduler = nullptr;
}

void CDeviceObjectFactory::AssignDevice(D3DDevice* pDevice)
{
	auto pEmulatedDevice = reinterpret_cast<CCryDX12Device*>(pDevice);

	m_pDX12Device    = pEmulatedDevice->GetDX12Device();
	m_pDX12Scheduler = &m_pDX12Device->GetScheduler();

	m_pDX12Scheduler->AddQueueEventCallback(m_pCoreCommandList.get(), lambdaCeaseCallback, NCryDX12::CCommandScheduler::CMDQUEUE_EVENT_CEASE);
	m_pDX12Scheduler->AddQueueEventCallback(m_pCoreCommandList.get(), lambdaResumeCallback, NCryDX12::CCommandScheduler::CMDQUEUE_EVENT_RESUME);

	m_pCoreCommandList->m_sharedState.pCommandList = m_pDX12Scheduler->GetCommandList(CMDQUEUE_GRAPHICS);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CDeviceGraphicsPSOPtr CDeviceObjectFactory::CreateGraphicsPSOImpl(const CDeviceGraphicsPSODesc& psoDesc) const
{
	auto pResult = std::make_shared<CDeviceGraphicsPSO_DX12>(GetDevice());
	pResult->Init(psoDesc);

	return pResult;
}

CDeviceComputePSOPtr CDeviceObjectFactory::CreateComputePSOImpl(const CDeviceComputePSODesc& psoDesc) const
{
	auto pResult = std::make_shared<CDeviceComputePSO_DX12>(GetDevice());
	pResult->Init(psoDesc);

	return pResult;
}

CDeviceResourceSetPtr CDeviceObjectFactory::CreateResourceSet(CDeviceResourceSet::EFlags flags) const
{
	return std::make_shared<CDeviceResourceSet_DX12>(flags);
}

CDeviceResourceLayoutPtr CDeviceObjectFactory::CreateResourceLayoutImpl(const SDeviceResourceLayoutDesc& resourceLayoutDesc) const
{
	auto pResult = std::make_shared<CDeviceResourceLayout_DX12>(GetDevice(), resourceLayoutDesc.GetRequiredResourceBindings());
	if (pResult->Init(resourceLayoutDesc))
		return pResult;

	return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Helper functions for DX12 MultiGPU
ID3D12CommandQueue* CDeviceObjectFactory::GetNativeCoreCommandQueue() const
{
	ID3D12CommandQueue* pD3d12Queue = m_pDX12Scheduler->GetCommandListPool(CMDQUEUE_GRAPHICS).GetD3D12CommandQueue();

#ifdef DX12_LINKEDADAPTER
	if (m_pDX12Device->IsMultiAdapter())
	{
		BroadcastableD3D12CommandQueue<2>* pBroadcastQueue = reinterpret_cast<BroadcastableD3D12CommandQueue<2>*>(pD3d12Queue);
		pD3d12Queue = *(*pBroadcastQueue)[0];
	}
#endif

	return pD3d12Queue;
}

// Acquire one or more command-lists which are independent of the core command-list
// Only one thread is allowed to call functions on this command-list (DX12 restriction).
// The thread that gets the permition is the one calling Begin() on it AFAIS
CDeviceCommandListUPtr CDeviceObjectFactory::AcquireCommandList(EQueueType eQueueType /*= eQueue_Graphics*/)
{
	// In theory this whole function need to be atomic, instead it voids the core command-list(s),
	// Synchronization between different threads acquiring command-lists is deferred to the higher level.
	NCryDX12::CCommandListPool& pQueue = m_pDX12Scheduler->GetCommandListPool(eQueueType);
	m_pDX12Scheduler->CeaseAllCommandQueues(false);

	DX12_PTR(CCommandList) pCL;
	pQueue.AcquireCommandList(pCL);

	m_pDX12Scheduler->ResumeAllCommandQueues();
	auto pResult = stl::make_unique<CDeviceCommandList>();
	pResult->m_sharedState.pCommandList = pCL;
	return pResult;
}

std::vector<CDeviceCommandListUPtr> CDeviceObjectFactory::AcquireCommandLists(uint32 listCount, EQueueType eQueueType /*= eQueue_Graphics*/)
{
	// In theory this whole function need to be atomic, instead it voids the core command-list(s),
	// Synchronization between different threads acquiring command-lists is deferred to the higher level.
	NCryDX12::CCommandListPool& pQueue = m_pDX12Scheduler->GetCommandListPool(eQueueType);
	m_pDX12Scheduler->CeaseAllCommandQueues(false);

	std::vector<CDeviceCommandListUPtr> pCommandLists;
	DX12_PTR(CCommandList) pCLs[256];

	// Allocate in chunks of 256
	for (uint32 n = 0; n < listCount; n += 256U)
	{
		const uint32 chunkCount = std::min(listCount - n, 256U);
		pQueue.AcquireCommandLists(chunkCount, pCLs);

		for (uint32 b = 0; b < chunkCount; ++b)
		{
			pCommandLists.emplace_back(stl::make_unique<CDeviceCommandList>());
			pCommandLists.back()->m_sharedState.pCommandList = pCLs[b];
		}
	}

	m_pDX12Scheduler->ResumeAllCommandQueues();
	return std::move(pCommandLists);
}

// Command-list sinks, will automatically submit command-lists in allocation-order
void CDeviceObjectFactory::ForfeitCommandList(CDeviceCommandListUPtr pCommandList, EQueueType eQueueType /*= eQueue_Graphics*/)
{
	NCryDX12::CCommandListPool& pQueue = m_pDX12Scheduler->GetCommandListPool(eQueueType);

	if (pCommandList)
	{
		pQueue.ForfeitCommandList(pCommandList->m_sharedState.pCommandList);
	}
}

void CDeviceObjectFactory::ForfeitCommandLists(std::vector<CDeviceCommandListUPtr> pCommandLists, EQueueType eQueueType /*= eQueue_Graphics*/)
{
	NCryDX12::CCommandListPool& pQueue = m_pDX12Scheduler->GetCommandListPool(eQueueType);

	const uint32 listCount = pCommandLists.size();
	DX12_PTR(CCommandList) pCLs[256];

	// Deallocate in chunks of 256
	for (uint32 n = 0; n < listCount; n += 256U)
	{
		const uint32 chunkCount = std::min(listCount - n, 256U);
		uint32 validCount = 0;

		for (uint32 b = 0; b < chunkCount; ++b)
		{
			if (pCommandLists[b])
			{
				pCLs[validCount++] = pCommandLists[b]->m_sharedState.pCommandList;
			}
		}

		if (validCount)
		{
			pQueue.ForfeitCommandLists(validCount, pCLs);
		}
	}
}

void CDeviceObjectFactory::ReleaseResourcesImpl()
{
	m_pDX12Device->FlushAndWaitForGPU();
}

////////////////////////////////////////////////////////////////////////////
UINT64 CDeviceObjectFactory::QueryFormatSupport(D3DFormat Format)
{
	CD3D9Renderer* rd = gcpRendD3D;

	D3D12_FEATURE_DATA_FORMAT_SUPPORT data = { Format };
	HRESULT hr = GetDX12Device()->GetD3D12Device()->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &data, sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT));
	if (SUCCEEDED(hr))
	{
		UINT nOptions = data.Support1;

		// *INDENT-OFF*
		return
			(nOptions & D3D11_FORMAT_SUPPORT_BUFFER                      ? FMTSUPPORT_BUFFER                      : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_TEXTURE1D                   ? FMTSUPPORT_TEXTURE1D                   : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_TEXTURE2D                   ? FMTSUPPORT_TEXTURE2D                   : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_TEXTURE3D                   ? FMTSUPPORT_TEXTURE3D                   : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_TEXTURECUBE                 ? FMTSUPPORT_TEXTURECUBE                 : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_IA_VERTEX_BUFFER            ? FMTSUPPORT_IA_VERTEX_BUFFER            : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_IA_INDEX_BUFFER             ? FMTSUPPORT_IA_INDEX_BUFFER             : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_SO_BUFFER                   ? FMTSUPPORT_SO_BUFFER                   : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_MIP                         ? FMTSUPPORT_MIP                         : 0) |
//			(nOptions & D3D11_FORMAT_SUPPORT_CAST_WITHIN_BIT_LAYOUT      ? FMTSUPPORT_SRGB                        : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_SHADER_LOAD                 ? FMTSUPPORT_SHADER_LOAD                 : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_SHADER_GATHER               ? FMTSUPPORT_SHADER_GATHER               : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_SHADER_GATHER_COMPARISON    ? FMTSUPPORT_SHADER_GATHER_COMPARISON    : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE               ? FMTSUPPORT_SHADER_SAMPLE               : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE_COMPARISON    ? FMTSUPPORT_SHADER_SAMPLE_COMPARISON    : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW ? FMTSUPPORT_TYPED_UNORDERED_ACCESS_VIEW : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL               ? FMTSUPPORT_DEPTH_STENCIL               : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_RENDER_TARGET               ? FMTSUPPORT_RENDER_TARGET               : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_BLENDABLE                   ? FMTSUPPORT_BLENDABLE                   : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_DISPLAY                     ? FMTSUPPORT_DISPLAYABLE                 : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_MULTISAMPLE_LOAD            ? FMTSUPPORT_MULTISAMPLE_LOAD            : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE         ? FMTSUPPORT_MULTISAMPLE_RESOLVE         : 0) |
			(nOptions & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET    ? FMTSUPPORT_MULTISAMPLE_RENDERTARGET    : 0);
		// *INDENT-ON*
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////
// Fence API

HRESULT CDeviceObjectFactory::CreateFence(DeviceFenceHandle& query)
{
	HRESULT hr = S_FALSE;
	query = reinterpret_cast<DeviceFenceHandle>(new UINT64);
	hr = query ? S_OK : E_FAIL;
	if (!FAILED(hr))
	{
		IssueFence(query);
	}
	return hr;
}

HRESULT CDeviceObjectFactory::ReleaseFence(DeviceFenceHandle query)
{
	HRESULT hr = query ? S_OK : S_FALSE;
	delete reinterpret_cast<UINT64*>(query);
	return hr;
}

HRESULT CDeviceObjectFactory::IssueFence(DeviceFenceHandle query)
{
	HRESULT hr = query ? S_OK : E_FAIL;
	UINT64* handle = reinterpret_cast<UINT64*>(query);
	if (handle)
	{
		*handle = m_pDX12Scheduler->InsertFence();
	}
	return hr;
}

HRESULT CDeviceObjectFactory::SyncFence(DeviceFenceHandle query, bool block, bool flush)
{
	HRESULT hr = query ? S_FALSE : E_FAIL;
	UINT64* handle = reinterpret_cast<UINT64*>(query);
	if (handle)
	{
		hr = m_pDX12Scheduler->TestForFence(*handle);
		if (hr != S_OK)
		{
			// Can only flush from render thread
			CRY_ASSERT(!flush || !gRenDev->m_pRT || gRenDev->m_pRT->IsRenderThread());

			// Test + Flush + No-Block is okay
			// Test + No-Flush + Block may not be okay, caution advised as it could deadlock
			if (flush)
			{
				m_pDX12Scheduler->FlushToFence(*handle);
			}

			if (block)
			{
				hr = m_pDX12Scheduler->WaitForFence(*handle);
			}
		}
	}
	return hr;
}

////////////////////////////////////////////////////////////////////////////
// SamplerState API

static inline D3D11_TEXTURE_ADDRESS_MODE sAddressMode(ESamplerAddressMode nAddress)
{
	static_assert((eSamplerAddressMode_Wrap   + 1) == D3D11_TEXTURE_ADDRESS_WRAP  , "AddressMode enum not mappable to D3D11 enum by arithmetic");
	static_assert((eSamplerAddressMode_Clamp  + 1) == D3D11_TEXTURE_ADDRESS_CLAMP , "AddressMode enum not mappable to D3D11 enum by arithmetic");
	static_assert((eSamplerAddressMode_Border + 1) == D3D11_TEXTURE_ADDRESS_BORDER, "AddressMode enum not mappable to D3D11 enum by arithmetic");
	static_assert((eSamplerAddressMode_Mirror + 1) == D3D11_TEXTURE_ADDRESS_MIRROR, "AddressMode enum not mappable to D3D11 enum by arithmetic");

	assert(nAddress >= eSamplerAddressMode_Wrap && nAddress <= eSamplerAddressMode_Border);
	return D3D11_TEXTURE_ADDRESS_MODE(nAddress + 1);
}

CDeviceSamplerState* CDeviceObjectFactory::CreateSamplerState(const SSamplerState& pState)
{
	D3D11_SAMPLER_DESC Desc;
	ZeroStruct(Desc);
	CDeviceSamplerState* pSamp = NULL;
	// AddressMode of 0 is INVALIDARG
	Desc.AddressU = sAddressMode(ESamplerAddressMode(pState.m_nAddressU));
	Desc.AddressV = sAddressMode(ESamplerAddressMode(pState.m_nAddressV));
	Desc.AddressW = sAddressMode(ESamplerAddressMode(pState.m_nAddressW));
	ColorF col((uint32)pState.m_dwBorderColor);
	Desc.BorderColor[0] = col.r;
	Desc.BorderColor[1] = col.g;
	Desc.BorderColor[2] = col.b;
	Desc.BorderColor[3] = col.a;
	if (pState.m_bComparison)
		Desc.ComparisonFunc = D3D11_COMPARISON_LESS;
	else
		Desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;

	Desc.MaxAnisotropy = 1;
	Desc.MinLOD = 0;
	if (pState.m_nMipFilter == FILTER_NONE)
		Desc.MaxLOD = 0.0f;
	else
		Desc.MaxLOD = 100.0f;

	Desc.MipLODBias = pState.m_fMipLodBias;

	if (pState.m_bComparison)
	{
		if (pState.m_nMinFilter == FILTER_LINEAR && pState.m_nMagFilter == FILTER_LINEAR && pState.m_nMipFilter == FILTER_LINEAR || pState.m_nMinFilter == FILTER_TRILINEAR || pState.m_nMagFilter == FILTER_TRILINEAR)
		{
			Desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		}
		else if (pState.m_nMinFilter == FILTER_LINEAR && pState.m_nMagFilter == FILTER_LINEAR && (pState.m_nMipFilter == FILTER_NONE || pState.m_nMipFilter == FILTER_POINT) || pState.m_nMinFilter == FILTER_BILINEAR || pState.m_nMagFilter == FILTER_BILINEAR)
			Desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		else if (pState.m_nMinFilter == FILTER_POINT && pState.m_nMagFilter == FILTER_POINT && (pState.m_nMipFilter == FILTER_NONE || pState.m_nMipFilter == FILTER_POINT))
			Desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
		else if (pState.m_nMinFilter >= FILTER_ANISO2X && pState.m_nMagFilter >= FILTER_ANISO2X && pState.m_nMipFilter >= FILTER_ANISO2X)
		{
			Desc.Filter = D3D11_FILTER_COMPARISON_ANISOTROPIC;
			Desc.MaxAnisotropy = pState.m_nAnisotropy;
		}
	}
	else
	{
		if (pState.m_nMinFilter == FILTER_LINEAR && pState.m_nMagFilter == FILTER_LINEAR && pState.m_nMipFilter == FILTER_LINEAR || pState.m_nMinFilter == FILTER_TRILINEAR || pState.m_nMagFilter == FILTER_TRILINEAR)
		{
			Desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		}
		else if (pState.m_nMinFilter == FILTER_LINEAR && pState.m_nMagFilter == FILTER_LINEAR && (pState.m_nMipFilter == FILTER_NONE || pState.m_nMipFilter == FILTER_POINT) || pState.m_nMinFilter == FILTER_BILINEAR || pState.m_nMagFilter == FILTER_BILINEAR)
			Desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		else if (pState.m_nMinFilter == FILTER_POINT && pState.m_nMagFilter == FILTER_POINT && (pState.m_nMipFilter == FILTER_NONE || pState.m_nMipFilter == FILTER_POINT))
			Desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		else if (pState.m_nMinFilter >= FILTER_ANISO2X && pState.m_nMagFilter >= FILTER_ANISO2X && pState.m_nMipFilter >= FILTER_ANISO2X)
		{
			Desc.Filter = D3D11_FILTER_ANISOTROPIC;
			Desc.MaxAnisotropy = pState.m_nAnisotropy;
		}
		else
			assert(0);
	}

	HRESULT hr = gcpRendD3D->GetDevice().CreateSamplerState(&Desc, &pSamp);
	if (SUCCEEDED(hr))
		return pSamp;
	else
		assert(0);
	return nullptr;
}

////////////////////////////////////////////////////////////////////////////
// InputLayout API

CDeviceInputLayout* CDeviceObjectFactory::CreateInputLayout(const SInputLayout& pLayout)
{
	CDeviceInputLayout* Layout = nullptr;
	if (!pLayout.m_pVertexShader || !pLayout.m_pVertexShader->m_pShaderData)
		return Layout;

	const int   nSize = pLayout.m_pVertexShader->m_nDataSize;
	const void* pVSData = pLayout.m_pVertexShader->m_pShaderData;

	HRESULT hr = E_FAIL;
	if (FAILED(hr = gcpRendD3D->GetDevice().CreateInputLayout(&pLayout.m_Declaration[0], pLayout.m_Declaration.size(), pVSData, nSize, &Layout)))
	{
		return Layout;
	}

	return Layout;
}

////////////////////////////////////////////////////////////////////////////
// Low-level resource management API (TODO: remove D3D-dependency by abstraction)

void CDeviceObjectFactory::AllocateNullResources()
{
	for (enum D3D11_RESOURCE_DIMENSION eType = D3D11_RESOURCE_DIMENSION_UNKNOWN; eType <= D3D11_RESOURCE_DIMENSION_TEXTURE3D; eType = D3D11_RESOURCE_DIMENSION(eType + 1))
	{
		D3DResource* pNullResource = m_NullResources[eType];

		if (pNullResource || (S_OK == gcpRendD3D->GetDevice_Unsynchronized().CreateNullResource(eType, &pNullResource)))
		{
			m_NullResources[eType] = pNullResource;
			m_NullResources[eType]->AddRef();
		}
	}
}

void CDeviceObjectFactory::ReleaseNullResources()
{
	for (enum D3D11_RESOURCE_DIMENSION eType = D3D11_RESOURCE_DIMENSION_UNKNOWN; eType <= D3D11_RESOURCE_DIMENSION_TEXTURE3D; eType = D3D11_RESOURCE_DIMENSION(eType + 1))
	{
		if (auto pNullResource = m_NullResources[eType])
		{
			ULONG refCount = pNullResource->Release();
			assert(refCount == 0);
		}
	}
}

//=============================================================================

#ifdef DEVRES_USE_STAGING_POOL
D3DResource* CDeviceObjectFactory::AllocateStagingResource(D3DResource* pForTex, bool bUpload, void*& pMappedAddress)
{
	D3DResource* pStagingResource = nullptr;

	gcpRendD3D->GetDevice_Unsynchronized().CreateStagingResource(pForTex, &pStagingResource, bUpload);

	if (bUpload) // Resources on D3D12_HEAP_TYPE_READBACK heaps do not support persistent map.
	{
		gcpRendD3D->GetDeviceContext_Unsynchronized().MapStagingResource(pStagingResource, bUpload, &pMappedAddress);
	}

	return pStagingResource;
}

void CDeviceObjectFactory::ReleaseStagingResource(D3DResource* pStagingTex)
{
	if (pStagingTex)
	{
		gcpRendD3D->GetDevice_Unsynchronized().ReleaseStagingResource(pStagingTex);
	}
}
#endif

//=============================================================================

HRESULT CDeviceObjectFactory::Create2DTexture(uint32 nWidth, uint32 nHeight, uint32 nMips, uint32 nArraySize, uint32 nUsage, const ColorF& cClearValue, D3DFormat Format, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pTI, int32 nESRAMOffset)
{
	D3D11_TEXTURE2D_DESC Desc;
	ZeroStruct(Desc);

	Desc.Width = nWidth;
	Desc.Height = nHeight;
	Desc.MipLevels = nMips;
	Desc.SampleDesc.Count = pTI ? pTI->m_nDstMSAASamples : 1;
	Desc.SampleDesc.Quality = pTI ? pTI->m_nDstMSAAQuality : 0;
	Desc.Format = (nUsage & USAGE_UAV_READWRITE) ? DeviceFormats::ConvertToTypeless(Format) : Format;
	Desc.ArraySize = nArraySize;
	Desc.BindFlags = ConvertToDX11BindFlags(nUsage);
	Desc.CPUAccessFlags = ConvertToDX11CPUAccessFlags(nUsage);
	Desc.Usage = ConvertToDX11Usage(nUsage);
	Desc.MiscFlags = ConvertToDX11MiscFlags(nUsage);

	return Create2DTexture(Desc, nUsage, cClearValue, ppDevTexture, pTI);
}

HRESULT CDeviceObjectFactory::CreateCubeTexture(uint32 nSize, uint32 nMips, uint32 nArraySize, uint32 nUsage, const ColorF& cClearValue, D3DFormat Format, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pTI)
{
	D3D11_TEXTURE2D_DESC Desc;
	ZeroStruct(Desc);

	Desc.Width = nSize;
	Desc.Height = nSize;
	Desc.MipLevels = nMips;
	Desc.SampleDesc.Count = 1;
	Desc.SampleDesc.Quality = 0;
	Desc.Format = (nUsage & USAGE_UAV_READWRITE) ? DeviceFormats::ConvertToTypeless(Format) : Format;
	Desc.ArraySize = nArraySize; assert(!(nArraySize % 6));
	Desc.BindFlags = ConvertToDX11BindFlags(nUsage);
	Desc.CPUAccessFlags = ConvertToDX11CPUAccessFlags(nUsage);
	Desc.Usage = ConvertToDX11Usage(nUsage);
	Desc.MiscFlags = ConvertToDX11MiscFlags(nUsage) | D3D11_RESOURCE_MISC_TEXTURECUBE;

	return CreateCubeTexture(Desc, nUsage, cClearValue, ppDevTexture, pTI);
}

HRESULT CDeviceObjectFactory::CreateVolumeTexture(uint32 nWidth, uint32 nHeight, uint32 nDepth, uint32 nMips, uint32 nUsage, const ColorF& cClearValue, D3DFormat Format, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pTI)
{
	D3D11_TEXTURE3D_DESC Desc;
	ZeroStruct(Desc);

	Desc.Width = nWidth;
	Desc.Height = nHeight;
	Desc.Depth = nDepth;
	Desc.MipLevels = nMips;
	Desc.Format = (nUsage & USAGE_UAV_READWRITE) ? DeviceFormats::ConvertToTypeless(Format) : Format;
	Desc.BindFlags = ConvertToDX11BindFlags(nUsage);
	Desc.CPUAccessFlags = ConvertToDX11CPUAccessFlags(nUsage);
	Desc.Usage = ConvertToDX11Usage(nUsage);
	Desc.MiscFlags = ConvertToDX11MiscFlags(nUsage);

	return CreateVolumeTexture(Desc, nUsage, cClearValue, ppDevTexture, pTI);
}

HRESULT CDeviceObjectFactory::Create2DTexture(const D3D11_TEXTURE2D_DESC& Desc, uint32 eFlags, const ColorF& cClearValue, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pPayload)
{
	D3D11_SUBRESOURCE_DATA SRD[g_nD3D10MaxSupportedSubres];
	D3DTexture* pD3DTex = NULL; assert(!pPayload || (((Desc.MipLevels * Desc.ArraySize) <= g_nD3D10MaxSupportedSubres)));
	D3D11_SUBRESOURCE_DATA* pSRD = ConvertToDX11Data(Desc.MipLevels * Desc.ArraySize, pPayload, SRD);
	HRESULT hr = S_OK;

	if (Desc.BindFlags & (D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS))
	{
		hr = gcpRendD3D->GetDevice_Unsynchronized().CreateTarget2D(&Desc, (const FLOAT*)&cClearValue, pSRD, &pD3DTex);
	}
	else
	{
		hr = gcpRendD3D->GetDevice_Unsynchronized().CreateTexture2D(&Desc, pSRD, &pD3DTex);
	}

	if (SUCCEEDED(hr) && pD3DTex)
	{
		CDeviceTexture* pDeviceTexture = new CDeviceTexture();

		pDeviceTexture->m_pNativeResource = pD3DTex;
		if (!pDeviceTexture->m_nBaseAllocatedSize)
		{
			pDeviceTexture->m_nBaseAllocatedSize = CDeviceTexture::TextureDataSize(Desc.Width, Desc.Height, 1, Desc.MipLevels, Desc.ArraySize, DeviceFormats::ConvertToTexFormat(Desc.Format), eTM_Optimal, eFlags);
		}

		*ppDevTexture = pDeviceTexture;
	}

	return hr;
}

HRESULT CDeviceObjectFactory::CreateCubeTexture(const D3D11_TEXTURE2D_DESC& Desc, uint32 eFlags, const ColorF& cClearValue, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pPayload)
{
	D3D11_SUBRESOURCE_DATA SRD[g_nD3D10MaxSupportedSubres];
	D3DCubeTexture* pD3DTex = NULL; assert(!pPayload || (((Desc.MipLevels * Desc.ArraySize) <= g_nD3D10MaxSupportedSubres) && !(Desc.ArraySize % 6)));
	D3D11_SUBRESOURCE_DATA* pSRD = ConvertToDX11Data(Desc.MipLevels * Desc.ArraySize, pPayload, SRD);
	HRESULT hr = S_OK;

	if (Desc.BindFlags & (D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS))
	{
		hr = gcpRendD3D->GetDevice_Unsynchronized().CreateTarget2D(&Desc, (const FLOAT*)&cClearValue, pSRD, &pD3DTex);
	}
	else
	{
		hr = gcpRendD3D->GetDevice_Unsynchronized().CreateTexture2D(&Desc, pSRD, &pD3DTex);
	}

	if (SUCCEEDED(hr) && pD3DTex)
	{
		CDeviceTexture* pDeviceTexture = new CDeviceTexture();

		pDeviceTexture->m_pNativeResource = pD3DTex;
		if (!pDeviceTexture->m_nBaseAllocatedSize)
		{
			pDeviceTexture->m_nBaseAllocatedSize = CDeviceTexture::TextureDataSize(Desc.Width, Desc.Height, 1, Desc.MipLevels, Desc.ArraySize, DeviceFormats::ConvertToTexFormat(Desc.Format), eTM_Optimal, eFlags);
		}

		*ppDevTexture = pDeviceTexture;
	}

	return hr;
}

HRESULT CDeviceObjectFactory::CreateVolumeTexture(const D3D11_TEXTURE3D_DESC& Desc, uint32 eFlags, const ColorF& cClearValue, LPDEVICETEXTURE* ppDevTexture, const STexturePayload* pPayload)
{
	D3D11_SUBRESOURCE_DATA SRD[g_nD3D10MaxSupportedSubres];
	D3D11_SUBRESOURCE_DATA* pSRD = ConvertToDX11Data(Desc.MipLevels, pPayload, SRD);
	D3DVolumeTexture* pD3DTex = NULL;
	HRESULT hr = S_OK;

	if (Desc.BindFlags & (D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS))
	{
		hr = gcpRendD3D->GetDevice_Unsynchronized().CreateTarget3D(&Desc, (const FLOAT*)&cClearValue, pSRD, &pD3DTex);
	}
	else
	{
		hr = gcpRendD3D->GetDevice_Unsynchronized().CreateTexture3D(&Desc, pSRD, &pD3DTex);
	}

	if (SUCCEEDED(hr) && pD3DTex)
	{
		CDeviceTexture* pDeviceTexture = new CDeviceTexture();

		pDeviceTexture->m_pNativeResource = pD3DTex;
		if (!pDeviceTexture->m_nBaseAllocatedSize)
		{
			pDeviceTexture->m_nBaseAllocatedSize = CDeviceTexture::TextureDataSize(Desc.Width, Desc.Height, Desc.Depth, Desc.MipLevels, 1, DeviceFormats::ConvertToTexFormat(Desc.Format), eTM_Optimal, eFlags);
		}

		*ppDevTexture = pDeviceTexture;
	}

	return hr;
}

HRESULT CDeviceObjectFactory::CreateBuffer(
	buffer_size_t nSize
	, buffer_size_t elemSize
	, uint32 nUsage
	, uint32 nBindFlags
	, D3DBuffer** ppBuff
	, const void* pData)
{
	FUNCTION_PROFILER(gEnv->pSystem, PROFILE_RENDERER);
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "CreateBuffer");
	HRESULT hr = S_OK;

#ifndef _RELEASE
	// ToDo verify that the usage and bindflags are correctly set (e.g certain
	// bit groups are mutually exclusive).
#endif

	D3D11_BUFFER_DESC BufDesc;
	ZeroStruct(BufDesc);

	BufDesc.StructureByteStride = elemSize;
	BufDesc.ByteWidth = nSize * elemSize;
	if ((nUsage & USAGE_CPU_WRITE))
		BufDesc.ByteWidth = CDeviceBufferManager::AlignElementCountForStreaming(nSize, elemSize) * elemSize;

	BufDesc.CPUAccessFlags = ConvertToDX11CPUAccessFlags(nUsage);
	BufDesc.Usage = ConvertToDX11Usage(nUsage);
	BufDesc.MiscFlags = ConvertToDX11MiscFlags(nUsage);

	if (BufDesc.Usage != D3D11_USAGE_STAGING)
	{
		BufDesc.BindFlags = ConvertToDX11BindFlags(nBindFlags);
		if (nBindFlags & (BIND_STREAM_OUTPUT | BIND_RENDER_TARGET | BIND_DEPTH_STENCIL))
		{
			CryFatalError("trying to create (currently) unsupported buffer type");
		}
	}
	else
	{
		BufDesc.BindFlags = 0;
	}

	D3D11_SUBRESOURCE_DATA* pSRD = NULL;
	D3D11_SUBRESOURCE_DATA SRD;
	if (pData)
	{
		pSRD = &SRD;

		SRD.pSysMem = pData;
		SRD.SysMemPitch = BufDesc.ByteWidth;
		SRD.SysMemSlicePitch = BufDesc.ByteWidth;
	}

	hr = gcpRendD3D->GetDevice_Unsynchronized().CreateBuffer(&BufDesc, pSRD, ppBuff);
	assert(hr == S_OK);
	return hr;
}

//=============================================================================

HRESULT CDeviceObjectFactory::InvalidateCpuCache(void* buffer_ptr, size_t size, size_t offset)
{
	return S_OK;
}

HRESULT CDeviceObjectFactory::InvalidateGpuCache(D3DBuffer* buffer, void* buffer_ptr, buffer_size_t size, buffer_size_t offset)
{
	return S_OK;
}

void CDeviceObjectFactory::InvalidateBuffer(D3DBuffer* buffer, void* base_ptr, buffer_size_t offset, buffer_size_t size, uint32 id)
{
}

//=============================================================================

uint8* CDeviceObjectFactory::Map(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode)
{
	D3D11_MAPPED_SUBRESOURCE mapped_resource = { 0 };
	SIZE_T BeginEndR[2] = { offset, offset + size };
	HRESULT hr = gcpRendD3D->GetDeviceContext_ForMapAndUnmap().Map(
		buffer
		, subresource
		, BeginEndR
		, mode
		, 0
		, &mapped_resource);
	assert(hr == S_OK);
	return reinterpret_cast<uint8*>(mapped_resource.pData);
}

void CDeviceObjectFactory::Unmap(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode)
{
	SIZE_T BeginEndW[2] = { offset, offset + size };
	gcpRendD3D->GetDeviceContext_ForMapAndUnmap().Unmap(
		buffer
		, subresource
		, BeginEndW);
}

template<const bool bDirectAccess>
void CDeviceObjectFactory::UploadContents(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, const void* pInDataCPU, void* pOutDataGPU, UINT numDataBlocks)
{
	gcpRendD3D->GetDeviceContext_ForMapAndUnmap().MappedWriteToSubresource(buffer, subresource, offset, size, mode, pInDataCPU, numDataBlocks);
}

template<const bool bDirectAccess>
void CDeviceObjectFactory::DownloadContents(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, void* pOutDataCPU, const void* pInDataGPU, UINT numDataBlocks)
{
	gcpRendD3D->GetDeviceContext_ForMapAndUnmap().MappedReadFromSubresource(buffer, subresource, offset, size, mode, pOutDataCPU, numDataBlocks);
}

// Explicit instantiation
template
void CDeviceObjectFactory::UploadContents<true>(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, const void* pInDataCPU, void* pOutDataGPU, UINT numDataBlocks);
template
void CDeviceObjectFactory::UploadContents<false>(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, const void* pInDataCPU, void* pOutDataGPU, UINT numDataBlocks);

template
void CDeviceObjectFactory::DownloadContents<true>(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, void* pOutDataCPU, const void* pInDataGPU, UINT numDataBlocks);
template
void CDeviceObjectFactory::DownloadContents<false>(D3DBuffer* buffer, uint32 subresource, buffer_size_t offset, buffer_size_t size, D3D11_MAP mode, void* pOutDataCPU, const void* pInDataGPU, UINT numDataBlocks);

// Shader API
ID3D11VertexShader* CDeviceObjectFactory::CreateVertexShader(const void* pData, size_t bytes)
{
	ID3D11VertexShader* pResult;
	return SUCCEEDED(gcpRendD3D->GetDevice_Unsynchronized().CreateVertexShader(pData, bytes, nullptr, &pResult)) ? pResult : nullptr;
}

ID3D11PixelShader* CDeviceObjectFactory::CreatePixelShader(const void* pData, size_t bytes)
{
	ID3D11PixelShader* pResult;
	return SUCCEEDED(gcpRendD3D->GetDevice_Unsynchronized().CreatePixelShader(pData, bytes, nullptr, &pResult)) ? pResult : nullptr;
}

ID3D11GeometryShader* CDeviceObjectFactory::CreateGeometryShader(const void* pData, size_t bytes)
{
	ID3D11GeometryShader* pResult;
	return SUCCEEDED(gcpRendD3D->GetDevice_Unsynchronized().CreateGeometryShader(pData, bytes, nullptr, &pResult)) ? pResult : nullptr;
}

ID3D11HullShader* CDeviceObjectFactory::CreateHullShader(const void* pData, size_t bytes)
{
	ID3D11HullShader* pResult;
	return SUCCEEDED(gcpRendD3D->GetDevice_Unsynchronized().CreateHullShader(pData, bytes, nullptr, &pResult)) ? pResult : nullptr;
}

ID3D11DomainShader* CDeviceObjectFactory::CreateDomainShader(const void* pData, size_t bytes)
{
	ID3D11DomainShader* pResult;
	return SUCCEEDED(gcpRendD3D->GetDevice_Unsynchronized().CreateDomainShader(pData, bytes, nullptr, &pResult)) ? pResult : nullptr;
}

ID3D11ComputeShader* CDeviceObjectFactory::CreateComputeShader(const void* pData, size_t bytes)
{
	ID3D11ComputeShader* pResult;
	return SUCCEEDED(gcpRendD3D->GetDevice_Unsynchronized().CreateComputeShader(pData, bytes, nullptr, &pResult)) ? pResult : nullptr;
}

// Occlusion Query API
D3DOcclusionQuery* CDeviceObjectFactory::CreateOcclusionQuery()
{
	D3DOcclusionQuery* pResult;
	D3D11_QUERY_DESC desc;
	desc.Query = D3D11_QUERY_OCCLUSION;
	desc.MiscFlags = 0;
	return SUCCEEDED(gcpRendD3D->GetDevice_Unsynchronized().CreateQuery(&desc, &pResult)) ? pResult : nullptr;
}

bool CDeviceObjectFactory::GetOcclusionQueryResults(D3DOcclusionQuery* pQuery, uint64& samplesPassed)
{
	return gcpRendD3D->GetDeviceContext().GetData(pQuery, &samplesPassed, sizeof(uint64), 0) == S_OK;
}
