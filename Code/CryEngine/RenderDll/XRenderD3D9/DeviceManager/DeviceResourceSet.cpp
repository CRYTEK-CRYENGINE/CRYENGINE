// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DeviceObjects.h"
#include "DriverD3D.h"
#include "xxhash.h"
#include "Common/ReverseDepth.h"
#include "Common/Textures/TextureHelpers.h"
#include "../GraphicsPipeline/Common/GraphicsPipelineStateSet.h"
#include "DeviceResourceSet.h"

#define REDUNDANCY_ASSERT(...)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool SResourceBinding::IsValid() const
{
	switch (type)
	{
	case EResourceType::ConstantBuffer: return pConstantBuffer && (pConstantBuffer->IsNullBuffer() || pConstantBuffer->GetD3D());
	case EResourceType::Texture:        return pTexture && pTexture->GetDevTexture();
	case EResourceType::Buffer:         return pBuffer && pBuffer->GetDevBuffer();
	case EResourceType::Sampler:        return samplerState != SamplerStateHandle::Unspecified;
	case EResourceType::InvalidType:    return false;
	default: CRY_ASSERT(false);         return false;
	}
}

bool SResourceBinding::IsVolatile() const
{
	switch (type)
	{
	case EResourceType::ConstantBuffer: return true;
	case EResourceType::Texture:        return pTexture && !(pTexture->GetFlags() & FT_DONT_RELEASE);
	case EResourceType::Buffer:         return true;
	case EResourceType::Sampler:        return samplerState == SamplerStateHandle::Unspecified;
	case EResourceType::InvalidType:    return true;
	default: CRY_ASSERT(false);         return true;
	}
}

const std::pair<SResourceView, CDeviceResourceView*>* SResourceBinding::GetDeviceResourceViewInfo() const
{
	CDeviceResource* pDeviceResource = nullptr;

	switch (type)
	{
	case EResourceType::Texture:  pDeviceResource = pTexture->GetDevTexture(); break;
	case EResourceType::Buffer:   pDeviceResource = pBuffer->GetDevBuffer();   break;
	default:                      CRY_ASSERT(false);                           break;
	}

	if (pDeviceResource)
	{
		return &pDeviceResource->LookupResourceView(view);
	}

	return nullptr;
}

template<typename T> T* SResourceBinding::GetDeviceResourceView() const
{
	if (fastCompare)
	{
		if (auto pResourceViewInfo = GetDeviceResourceViewInfo())
		{
			return reinterpret_cast<T*>(pResourceViewInfo->second);
		}
	}

	return nullptr;
}

template D3DSurface*      SResourceBinding::GetDeviceResourceView<D3DSurface>() const;
template D3DDepthSurface* SResourceBinding::GetDeviceResourceView<D3DDepthSurface>() const;

DXGI_FORMAT SResourceBinding::GetResourceFormat() const
{
	if (fastCompare)
	{
		// try to get format from device resource view first
		if (auto pResourceViewInfo = GetDeviceResourceViewInfo())
			return DXGI_FORMAT(pResourceViewInfo->first.m_Desc.nFormat);

		// fall back to resource format
		CRY_ASSERT(view == EDefaultResourceViews::Default || view == EDefaultResourceViews::RasterizerTarget); // only safe with default views
		CRY_ASSERT(type == EResourceType::Texture && pTexture); // TODO implement for buffers

		return DeviceFormats::ConvertFromTexFormat(pTexture->GetDstFormat());
	}

	return DXGI_FORMAT_UNKNOWN;
}

SResourceBindPoint::SResourceBindPoint(const SResourceBinding& resource, uint8 _slotNumber, EShaderStage shaderStages)
{
	slotNumber = _slotNumber;
	stages = shaderStages;

	switch (resource.type)
	{
	case SResourceBinding::EResourceType::ConstantBuffer:
	{
		slotType = ESlotType::ConstantBuffer;
		flags = EFlags::None;
	}
	break;

	case SResourceBinding::EResourceType::Texture:
	{
		CRY_ASSERT(resource.view == EDefaultResourceViews::Default || resource.pTexture->GetDevTexture());
		const bool bSrv = resource.view == EDefaultResourceViews::Default || SResourceView::IsShaderResourceView(resource.pTexture->GetDevTexture()->LookupResourceView(resource.view).first.m_Desc.Key);

		slotType = bSrv ? ESlotType::TextureAndBuffer : ESlotType::UnorderedAccessView;
		flags = EFlags::IsTexture;
	}
	break;

	case SResourceBinding::EResourceType::Buffer:
	{
		CRY_ASSERT(resource.view == EDefaultResourceViews::Default || resource.pBuffer->GetDevBuffer());
		const bool bSrv = resource.view == EDefaultResourceViews::Default || SResourceView::IsShaderResourceView(resource.pBuffer->GetDevBuffer()->LookupResourceView(resource.view).first.m_Desc.Key);
		const bool bStructured = !!(resource.pBuffer->GetFlags() & CDeviceObjectFactory::USAGE_STRUCTURED);

		slotType = bSrv ? ESlotType::TextureAndBuffer : ESlotType::UnorderedAccessView;
		flags = bStructured ? EFlags::IsStructured : EFlags::None;
	}
	break;

	case SResourceBinding::EResourceType::Sampler:
	{
		slotType = ESlotType::Sampler;
		flags = EFlags::None;
	}
	break;
	}
}

SResourceBindPoint::SResourceBindPoint(ESlotType _type, uint8 _slotNumber, EShaderStage _shaderStages, EFlags _flags)
	: slotType(_type)
	, flags(_flags)
	, slotNumber(_slotNumber)
	, stages(_shaderStages)
{}

CDeviceResourceSetDesc::EDirtyFlags CDeviceResourceSetDesc::SetResources(const CDeviceResourceSetDesc& other)
{
	CRY_ASSERT(m_resources.size() == 0);
	for (const auto& it : other.m_resources)
	{
		auto insertResult = m_resources.insert(it);

		SResourceBinding&   existingBinding = insertResult.first->second;
		SResourceBindPoint& existingBindPoint = insertResult.first->first;

		if (m_invalidateCallback)
		{
			it.second.AddInvalidateCallback(m_invalidateCallbackOwner, existingBindPoint, m_invalidateCallback);
		}
	}

	return EDirtyFlags::eDirtyAll;
}

CDeviceResourceSetDesc::CDeviceResourceSetDesc(const CDeviceResourceSetDesc& other)
{
	m_invalidateCallbackOwner = this;
	m_invalidateCallback = OnResourceInvalidated;
	m_dirtyFlags = other.m_dirtyFlags.load();

	SetResources(other);
}

CDeviceResourceSetDesc::CDeviceResourceSetDesc(const CDeviceResourceSetDesc& other, void* pInvalidateCallbackOwner, const SResourceBinding::InvalidateCallbackFunction& invalidateCallback)
{
	m_invalidateCallbackOwner = pInvalidateCallbackOwner;
	m_invalidateCallback = invalidateCallback;
	m_dirtyFlags = other.m_dirtyFlags.load();

	SetResources(other);
}

CDeviceResourceSetDesc::~CDeviceResourceSetDesc()
{
	ClearResources();
}

template<SResourceBinding::EResourceType resourceType>
bool CompareBindings(const SResourceBinding& resourceA, const SResourceBinding& resourceB)
{
	return resourceA.fastCompare == resourceB.fastCompare && resourceA.view == resourceB.view;
}

template<>
bool CompareBindings<SResourceBinding::EResourceType::ConstantBuffer>(const SResourceBinding& resourceA, const SResourceBinding& resourceB)
{
	return resourceA.fastCompare == resourceB.fastCompare && resourceA.view == resourceB.view;
}

template<>
bool CompareBindings<SResourceBinding::EResourceType::Sampler>(const SResourceBinding& resourceA, const SResourceBinding& resourceB)
{
	return resourceA.samplerState == resourceB.samplerState;
}

template<SResourceBinding::EResourceType resourceType>
CDeviceResourceSetDesc::EDirtyFlags CDeviceResourceSetDesc::UpdateResource(SResourceBindPoint bindPoint, const SResourceBinding& resource)
{
	EDirtyFlags dirtyFlags = EDirtyFlags::eNone;

	auto insertResult = m_resources.insert(std::make_pair(bindPoint, resource));

	SResourceBinding&   existingBinding = insertResult.first->second;
	SResourceBindPoint& existingBindPoint = insertResult.first->first;

	const bool bChangedBindPoint = (insertResult.second || existingBindPoint.fastCompare != bindPoint.fastCompare);
	if (bChangedBindPoint || existingBinding.type != resource.type || !CompareBindings<resourceType>(existingBinding, resource))
	{
		// remove invalidate callback from existing binding
		if (existingBinding.fastCompare && m_invalidateCallback)
		{
			existingBinding.RemoveInvalidateCallback(m_invalidateCallbackOwner, existingBindPoint);
		}

		existingBindPoint = bindPoint;
		existingBinding = resource;

		// add invalidate callback to new binding
		if (existingBinding.fastCompare && m_invalidateCallback)
		{
			existingBinding.AddInvalidateCallback(m_invalidateCallbackOwner, existingBindPoint, m_invalidateCallback);
		}

		m_dirtyFlags |= (dirtyFlags = EDirtyFlags::eDirtyBinding | (EDirtyFlags)(EDirtyFlags::eDirtyBindPoint * bChangedBindPoint));
	}
	else
	{
		// UpdateResource call is redundant if the texture stays the same and can't be deleted (read: will never change)
		REDUNDANCY_ASSERT(existingBinding.IsVolatile() || resource.IsVolatile());
	}

	return dirtyFlags;
}

// explicit instantiation
template CDeviceResourceSetDesc::EDirtyFlags CDeviceResourceSetDesc::UpdateResource<SResourceBinding::EResourceType::ConstantBuffer>(SResourceBindPoint bindPoint, const SResourceBinding& binding);
template CDeviceResourceSetDesc::EDirtyFlags CDeviceResourceSetDesc::UpdateResource<SResourceBinding::EResourceType::Texture>(SResourceBindPoint bindPoint, const SResourceBinding& binding);
template CDeviceResourceSetDesc::EDirtyFlags CDeviceResourceSetDesc::UpdateResource<SResourceBinding::EResourceType::Buffer>(SResourceBindPoint bindPoint, const SResourceBinding& binding);
template CDeviceResourceSetDesc::EDirtyFlags CDeviceResourceSetDesc::UpdateResource<SResourceBinding::EResourceType::Sampler>(SResourceBindPoint bindPoint, const SResourceBinding& binding);

CDeviceResourceSetDesc::EDirtyFlags CDeviceResourceSetDesc::ClearResources()
{
	EDirtyFlags dirtyFlags = EDirtyFlags::eNone;

	for (const auto& it : m_resources)
	{
		const SResourceBinding& existingBinding = it.second;
		if (existingBinding.fastCompare && m_invalidateCallback)
		{
			existingBinding.RemoveInvalidateCallback(m_invalidateCallbackOwner);
		}
	}

	if (m_resources.size())
	{
		m_resources.clear();

		m_dirtyFlags |= (dirtyFlags = EDirtyFlags::eDirtyAll);
	}

	return dirtyFlags;
}

bool CDeviceResourceSetDesc::OnResourceInvalidated(void* pThis, SResourceBindPoint bindPoint, UResourceReference pResource, uint32 flags) threadsafe
{
	CDeviceResourceSetDesc* pSet = reinterpret_cast<CDeviceResourceSetDesc*>(pThis);
	EDirtyFlags dirtyFlags = EDirtyFlags::eNone;

	// Device-resource views have changed
	if (flags & eDeviceResourceViewDirty)
		dirtyFlags = EDirtyFlags::eDirtyBinding;
	// Device-resource has changed
	if (flags & eDeviceResourceDirty)
		dirtyFlags = EDirtyFlags::eDirtyBinding;
	// Resource has been destructed
	if (flags & eResourceDestroyed)
	{
		auto it = pSet->m_resources.find(bindPoint);
		if (it != pSet->m_resources.end())
		{
			SResourceBinding& existingBinding = it->second;
			UResourceReference pFoundResource = CryInterlockedCompareExchangePointer((void* volatile*)&existingBinding.fastCompare, nullptr, pResource.pAnonymous);

			if (pFoundResource.pAnonymous == pResource.pAnonymous)
			{
				dirtyFlags |= EDirtyFlags::eDirtyBinding;
			}
		}
	}

	if (dirtyFlags != EDirtyFlags::eNone)
		pSet->m_dirtyFlags |= dirtyFlags;

	// Don't keep the pointer and unregister the callback when the resource goes out of scope
	return !(flags & eResourceDestroyed);
}

CDeviceResourceSet::CDeviceResourceSet(EFlags flags)
	: m_bValid(false)
	, m_Flags(flags)
{}

CDeviceResourceSet::~CDeviceResourceSet()
{}

bool CDeviceResourceSet::Update(CDeviceResourceSetDesc& desc)
{
	// If nothing changed, nothing will become valid automagically
	CDeviceResourceSetDesc::EDirtyFlags dirtyFlags = desc.GetDirtyFlags();
	if (dirtyFlags != CDeviceResourceSetDesc::eNone)
	{
		// If it the given changes lead to a valid object, reset the change-flag
		if ((m_bValid = UpdateImpl(desc, dirtyFlags)))
		{
			desc.AcceptAllChanges();
		}
		else
		{
			desc.AcceptChangedBindPoints();
		}
	}

	return m_bValid;
}

bool CDeviceResourceSet::UpdateWithReevaluation(CDeviceResourceSetPtr& pSet, CDeviceResourceSetDesc& desc)
{
	if (!desc.IsEmpty())
	{
		if (pSet == nullptr)
			pSet = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);

		return pSet->Update(desc);
	}

	pSet = nullptr;
	desc.AcceptAllChanges();

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SDeviceResourceLayoutDesc::SetResourceSet(uint32 bindSlot, const CDeviceResourceSetDesc& resourceSet)
{
	SLayoutBindPoint bindPoint = { SDeviceResourceLayoutDesc::ELayoutSlotType::ResourceSet, bindSlot };
	m_resourceBindings[bindPoint] = resourceSet.GetResources();
}

void SDeviceResourceLayoutDesc::SetConstantBuffer(uint32 bindSlot, EConstantBufferShaderSlot shaderSlot, ::EShaderStage shaderStages)
{
	SResourceBinding resource((CConstantBuffer*)nullptr, 0);
	SResourceBindPoint resourceBindPoint(resource, shaderSlot, shaderStages);
	SLayoutBindPoint layoutBindPoint = { SDeviceResourceLayoutDesc::ELayoutSlotType::InlineConstantBuffer, bindSlot };

	m_resourceBindings[layoutBindPoint].clear();
	m_resourceBindings[layoutBindPoint].insert(std::make_pair(resourceBindPoint, resource));
}

bool SDeviceResourceLayoutDesc::operator<(const SDeviceResourceLayoutDesc& other) const
{
	if (m_resourceBindings.size() != other.m_resourceBindings.size())
		return m_resourceBindings.size() < other.m_resourceBindings.size();

	for (auto rs0 = m_resourceBindings.begin(), rs1 = other.m_resourceBindings.begin(); rs0 != m_resourceBindings.end(); ++rs0, ++rs1)
	{
		// compare layout bind slots first
		if (!(rs0->first == rs1->first))
			return rs0->first < rs1->first;

		// now compare bind points for current layout slot
		if (rs0->second.size() != rs1->second.size())
			return rs0->second.size() < rs1->second.size();

		for (auto bp0 = rs0->second.begin(), bp1 = rs1->second.begin(); bp0 != rs0->second.end(); ++bp0, ++bp1)
		{
			// compare bind point
			if (bp0->first.fastCompare != bp1->first.fastCompare)
				return bp0->first.fastCompare < bp1->first.fastCompare;

			// Samplers are always static and injected into the layout, so we need to compare the actual sampler state here
			if (bp0->first.slotType == SResourceBindPoint::ESlotType::Sampler)
			{
				if (bp0->second.samplerState != bp1->second.samplerState)
					return bp0->second.samplerState < bp1->second.samplerState;
			}
		}
	}

	return false;
}

UsedBindSlotSet SDeviceResourceLayoutDesc::GetRequiredResourceBindings() const
{
	UsedBindSlotSet result = 0;

	for (auto& it : m_resourceBindings)
		result[it.first.layoutSlot] = true;

	return result;
}

uint64 SDeviceResourceLayoutDesc::GetHash() const
{
	XXH64_state_t hashState;
	XXH64_reset(&hashState, 0);

	for (auto& itLayoutBinding : m_resourceBindings)
	{
		const SLayoutBindPoint& layoutBindPoint = itLayoutBinding.first;
		XXH64_update(&hashState, &layoutBindPoint, sizeof(layoutBindPoint));

		for (auto itResource : itLayoutBinding.second)
		{
			SResourceBindPoint resourceBindPoint = itResource.first;
			XXH64_update(&hashState, &resourceBindPoint.fastCompare, sizeof(resourceBindPoint.fastCompare));

			const SResourceBinding& resource = itResource.second;
			if (resource.type == SResourceBinding::EResourceType::Sampler)
			{
				auto samplerDesc = GetDeviceObjectFactory().LookupSamplerState(resource.samplerState).first;

				uint32 hashedValues[] =
				{
					samplerDesc.m_nMinFilter, samplerDesc.m_nMagFilter,     samplerDesc.m_nMipFilter,
					samplerDesc.m_nAddressU,  samplerDesc.m_nAddressV,      samplerDesc.m_nAddressW,
					samplerDesc.m_nAnisotropy, samplerDesc.m_dwBorderColor, samplerDesc.m_bComparison,
				};

				XXH64_update(&hashState, hashedValues, sizeof(hashedValues));
			}
		}
	}

	return XXH64_digest(&hashState);
}

bool SDeviceResourceLayoutDesc::IsValid() const
{
	// VALIDATION RULES:
	// * Cannot bind multiple resources to same layout (CPU side) slot
	// * Cannot have gaps in layout (CPU side) slots
	// * Cannot bind multiple resources to same shader slot
	//   -> Due to dx12, even things like tex0 => (t0, EShaderStage_Vertex), tex1 => (t0, EShaderStage_Pixel) are invalid
	// * There is a restriction on the GpuBuffer count per resource set. check 'ResourceSetBufferCount'

	auto GetResourceName = [](const SResourceBinding& resource)
	{
		switch (resource.type)
		{
		case SResourceBinding::EResourceType::Texture:        return resource.pTexture->GetName();
		case SResourceBinding::EResourceType::Buffer:         return "GpuBuffer";
		case SResourceBinding::EResourceType::ConstantBuffer: return "ConstantBuffer";
		case SResourceBinding::EResourceType::Sampler:        return "Sampler";
		case SResourceBinding::EResourceType::InvalidType:    return "Void";
		};
		return "Unknown";
	};

	auto GetBindPointName = [](SResourceBindPoint bindPoint)
	{
		static char buffer[64];
		char slotPrefix[] = { 'b', 't', 'u', 's' };

		cry_sprintf(buffer, "%c%d", slotPrefix[min((size_t)bindPoint.slotType, CRY_ARRAY_COUNT(slotPrefix) - 1)], bindPoint.slotNumber);

		return buffer;
	};

	std::set<uint32> usedLayoutBindSlots;
	std::map<int16, SResourceBinding> usedShaderBindSlotsS[(uint8)SResourceBindPoint::ESlotType::Count][eHWSC_Num]; // used slot numbers per slot type and shader stage
	std::map<int16, SResourceBinding> usedShaderBindSlotsL[(uint8)SResourceBindPoint::ESlotType::Count][EResourceLayoutSlot_Max + 1];

	auto validateLayoutSlot = [&](uint32 layoutSlot)
	{
		if (usedLayoutBindSlots.insert(layoutSlot).second == false)
		{
			CRY_ASSERT_TRACE(false, ("Invalid Resource Layout: Multiple resources on layout (CPU side) slot %d", layoutSlot));
			return false;
		}

		return true;
	};

	auto validateResourceBindPoint = [&](uint8 layoutSlot, SResourceBindPoint bindPoint, const SResourceBinding& resource)
	{
		// Shader stages are ordered by usage-frequency and loop exists according to usage-frequency (VS+PS fast, etc.)
		int validShaderStages = bindPoint.stages;
		for (EHWShaderClass shaderClass = eHWSC_Vertex; validShaderStages; shaderClass = EHWShaderClass(shaderClass + 1), validShaderStages >>= 1)
		{
			if (validShaderStages & 1)
			{
				// Across all layouts, no stage-local slot can be referenced twice
				auto insertResultS = usedShaderBindSlotsS[uint8(bindPoint.slotType)][shaderClass].insert(std::make_pair(bindPoint.slotNumber, resource));
				if (insertResultS.second == false)
				{
					auto& existingResource = insertResultS.first->second;
					auto& currentResource = resource;

					CRY_ASSERT_TRACE(false, ("Invalid Resource Layout : Multiple resources bound to shader slot %s across multiple layoutSlots: A: %s - B: %s",
						GetBindPointName(bindPoint), GetResourceName(existingResource), GetResourceName(currentResource)));

					return false;
				}
			}
		}

		// Across all stages, no layout-local slot can be referenced twice
		auto insertResultL = usedShaderBindSlotsL[uint8(bindPoint.slotType)][layoutSlot].insert(std::make_pair(bindPoint.slotNumber, resource));
		if (insertResultL.second == false)
		{
			auto& existingResource = insertResultL.first->second;
			auto& currentResource = resource;

			CRY_ASSERT_TRACE(false, ("Invalid Resource Layout : Multiple resources bound to shader slot %s within the same layoutSlot: A: %s - B: %s",
				GetBindPointName(bindPoint), GetResourceName(existingResource), GetResourceName(currentResource)));

			return false;
		}

		return true;
	};

	// validate all resource bindings
	for (auto& itLayoutBinding : m_resourceBindings)
	{
		if (!validateLayoutSlot(itLayoutBinding.first.layoutSlot))
			return false;

		for (auto itResource : itLayoutBinding.second)
		{
			if (!validateResourceBindPoint(itLayoutBinding.first.layoutSlot, itResource.first, itResource.second))
				return false;
		}
	}

	// Make sure there are no 'holes' in the used binding slots
	{
		int previousSlot = -1;
		for (auto slot : usedLayoutBindSlots)
		{
			if (slot != previousSlot + 1)
			{
				CRY_ASSERT_MESSAGE(false, "Invalid Resource Layout: gap in layout (CPU side) slots");
				return false;
			}

			previousSlot = slot;
		}

		if (previousSlot > EResourceLayoutSlot_Max)
			return false;
	}

	return true;
}

