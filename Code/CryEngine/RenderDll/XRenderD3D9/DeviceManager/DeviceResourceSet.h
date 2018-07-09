// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Containers/VectorMap.h>   // VectorMap
#include <CryRenderer/VertexFormats.h>
#include <CryRenderer/ITexture.h>
#include <array>
#include <bitset>
#include <atomic>

#include "DeviceResources.h"                // CDeviceBuffer, CDeviceTexture, CDeviceInputStream
#include "Common/CommonRender.h"            // SResourceView, SSamplerState, SInputLayout
#include "Common/Shaders/ShaderCache.h"     // UPipelineState

class CHWShader_D3D;
class CShader;
class CTexture;
class CCryNameTSCRC;
class CCryDeviceWrapper;
class CConstantBuffer;
class CDeviceBuffer;
class CDeviceTexture;
class CShaderResources;
struct SGraphicsPipelineStateDescription;
struct SComputePipelineStateDescription;
class CDeviceRenderPass;
typedef std::shared_ptr<CDeviceRenderPass> CDeviceRenderPassPtr;
/////////////////////////////////////////////////////////////////////////////////

struct SProfilingStats
{
	int  numPSOSwitches;
	int  numLayoutSwitches;
	int  numResourceSetSwitches;
	int  numInlineSets;
	int  numTopologySets;
	int  numDIPs;
	int  numPolygons;
	int  numInvalidDIPs;

	void Reset()
	{
		ZeroStruct(*this);
	}

	void Merge(const SProfilingStats& other)
	{
		CryInterlockedAdd(&numPSOSwitches, other.numPSOSwitches);
		CryInterlockedAdd(&numLayoutSwitches, other.numLayoutSwitches);
		CryInterlockedAdd(&numResourceSetSwitches, other.numResourceSetSwitches);
		CryInterlockedAdd(&numInlineSets, other.numInlineSets);
		CryInterlockedAdd(&numTopologySets, other.numTopologySets);
		CryInterlockedAdd(&numDIPs, other.numDIPs);
		CryInterlockedAdd(&numPolygons, other.numPolygons);
		CryInterlockedAdd(&numInvalidDIPs, other.numInvalidDIPs);
	}
};

////////////////////////////////////////////////////////////////////////////
struct SResourceMemoryAlignment
{
	UINT typeStride;   // in bytes
	UINT rowStride;    // in bytes
	UINT planeStride;  // in bytes
	UINT volumeStride; // in bytes

	template<typename T>
	static inline SResourceMemoryAlignment Linear(UINT width = 1, UINT height = 1, UINT depth = 1)
	{
		SResourceMemoryAlignment linear =
		{
			sizeof(T),
			sizeof(T) * width,
			sizeof(T) * width * height,
			sizeof(T) * width * height * depth
		};

		return linear;
	}

	static inline SResourceMemoryAlignment Linear(UINT type, UINT width = 1, UINT height = 1, UINT depth = 1)
	{
		SResourceMemoryAlignment linear =
		{
			type,
			type * width,
			type * width * height,
			type * width * height * depth
		};

		return linear;
	}
};

struct SResourceCoordinate
{
	UINT Left;         // Buffer: in bytes      Texture: in texels
	UINT Top;          // Buffer: N/A           Texture: in texels
	UINT Front;        // Buffer: N/A           Texture: in texels
	UINT Subresource;  // Buffer: arraySlice    Texture: arraySlice * tex->numMips + mipSlice
};

struct SResourceDimension
{
	UINT Width;        // Buffer: in bytes      Texture: in texels
	UINT Height;       // Buffer: N/A           Texture: in texels
	UINT Depth;        // Buffer: N/A           Texture: in texels
	UINT Subresources; // Buffer: arraySlices   Texture: arraySlices * tex->numMips + mipSlices (mipSlices currently must be a multiple of tex->numMips)
};

struct SResourceRegion
{
	SResourceCoordinate Offset;
	SResourceDimension Extent;
};

struct SResourceMemoryMapping
{
	SResourceMemoryAlignment MemoryLayout;
	SResourceCoordinate ResourceOffset;
	SResourceDimension Extent;
	UINT /*D3D11_COPY_FLAGS*/ Flags; // TODO: abstract flags
};

struct SResourceRegionMapping
{
	SResourceCoordinate SourceOffset;
	SResourceCoordinate DestinationOffset;
	SResourceDimension Extent;
	UINT /*D3D11_COPY_FLAGS*/ Flags; // TODO: abstract flags
};

// -------------------------------------------------------------------------

struct SSubresourcePayload
{
	SResourceMemoryAlignment m_sSysMemAlignment;
	const void*              m_pSysMem;
};

struct STexturePayload
{
	SSubresourcePayload* m_pSysMemSubresourceData;
	ETEX_TileMode        m_eSysMemTileMode;
	uint8                m_nDstMSAASamples;
	uint8                m_nDstMSAAQuality;

	STexturePayload()
	{
		m_pSysMemSubresourceData = nullptr;
		m_eSysMemTileMode = eTM_None;
		m_nDstMSAASamples = 1;
		m_nDstMSAAQuality = 0;
	}

	~STexturePayload()
	{
		SAFE_DELETE_ARRAY(m_pSysMemSubresourceData);
	}
};

////////////////////////////////////////////////////////////////////////////

const int ResourceSetBufferCount = 8;

class CDeviceResourceSetDesc : NoCopy
{
public:
	enum EDirtyFlags : uint32
	{
		eNone = 0,
		eDirtyBindPoint = BIT(0),
		eDirtyBinding = BIT(1),

		eDirtyAll = eDirtyBindPoint | eDirtyBinding
	};

public:
	CDeviceResourceSetDesc();
	CDeviceResourceSetDesc(void* pInvalidateCallbackOwner, const SResourceBinding::InvalidateCallbackFunction& invalidateCallback);
	CDeviceResourceSetDesc(const CDeviceResourceSetDesc& other);
	CDeviceResourceSetDesc(const CDeviceResourceSetDesc& other, void* pInvalidateCallbackOwner, const SResourceBinding::InvalidateCallbackFunction& invalidateCallback);
	~CDeviceResourceSetDesc();

	bool IsEmpty() const;
	EDirtyFlags GetDirtyFlags() const;
	bool HasChanged() const;
	void AcceptAllChanges();

	void MarkBindingChanged();

	bool HasChangedBindPoints();
	void AcceptChangedBindPoints();

	EDirtyFlags SetTexture(int shaderSlot, CTexture* pTexture, ResourceViewHandle hView, ::EShaderStage shaderStages);
	EDirtyFlags SetSampler(int shaderSlot, SamplerStateHandle hState, ::EShaderStage shaderStages);
	EDirtyFlags SetConstantBuffer(int shaderSlot, CConstantBuffer* pBuffer, ::EShaderStage shaderStages);
	EDirtyFlags SetBuffer(int shaderSlot, CGpuBuffer* pBuffer, ResourceViewHandle hView, ::EShaderStage shaderStages);
	EDirtyFlags ClearResources();

	const VectorMap <SResourceBindPoint, SResourceBinding>& GetResources() const { return m_resources; }

	static bool OnResourceInvalidated(void* pThis, SResourceBindPoint bindPoint, UResourceReference pResource, uint32 flags) threadsafe;

protected:
	EDirtyFlags SetResources(const CDeviceResourceSetDesc& other);

	template<SResourceBinding::EResourceType resourceType>
	EDirtyFlags UpdateResource(SResourceBindPoint bindPoint, const SResourceBinding& binding);

private:
	VectorMap <SResourceBindPoint, SResourceBinding> m_resources;
	SResourceBinding::InvalidateCallbackFunction     m_invalidateCallback;
	void*                                            m_invalidateCallbackOwner;

	std::atomic<uint32>                              m_dirtyFlags;
};
DEFINE_ENUM_FLAG_OPERATORS(CDeviceResourceSetDesc::EDirtyFlags)

class CDeviceResourceSet : NoCopy
{
	friend class CDeviceObjectFactory;
	friend struct SDeviceResourceLayoutDesc;
	friend class CDeviceResourceLayout;
	friend class CDeviceResourceLayout_DX12;
	friend class CRenderPassScheduler;

public:
	enum EFlags
	{
		EFlags_None = 0,
		EFlags_ForceSetAllState = BIT(0), // Dx11 only: don't rely on shader reflection, set all resources contained in the resource set
	};

	CDeviceResourceSet(EFlags flags);
	virtual ~CDeviceResourceSet();

	bool         IsValid()  const { return m_bValid; }
	EFlags       GetFlags() const { return m_Flags; }

	bool         Update(CDeviceResourceSetDesc& desc);
	static bool  UpdateWithReevaluation(std::shared_ptr<CDeviceResourceSet>/*CDeviceResourceSetPtr*/& pRenderPass, CDeviceResourceSetDesc& desc);

protected:
	virtual bool UpdateImpl(const CDeviceResourceSetDesc& desc, CDeviceResourceSetDesc::EDirtyFlags dirtyFlags) = 0;

protected:
	EFlags m_Flags;

private:
	bool   m_bValid;
};
DEFINE_ENUM_FLAG_OPERATORS(CDeviceResourceSet::EFlags)

typedef std::shared_ptr<CDeviceResourceSet> CDeviceResourceSetPtr;

////////////////////////////////////////////////////////////////////////////

typedef std::bitset<EResourceLayoutSlot_Max + 1> UsedBindSlotSet;

struct SDeviceResourceLayoutDesc
{
	enum class ELayoutSlotType : uint8
	{
		InlineConstantBuffer,
		ResourceSet
	};

	struct SLayoutBindPoint
	{
		ELayoutSlotType slotType;
		uint8           layoutSlot;

		bool operator==(const SLayoutBindPoint& other) const;
		bool operator< (const SLayoutBindPoint& other) const;
	};

	void            SetConstantBuffer(uint32 bindSlot, EConstantBufferShaderSlot shaderSlot, EShaderStage shaderStages);
	void            SetResourceSet(uint32 bindSlot, const CDeviceResourceSetDesc& resourceSet);

	bool            IsValid() const;
	UsedBindSlotSet GetRequiredResourceBindings() const;

	uint64          GetHash() const;

	VectorMap<SLayoutBindPoint, VectorMap<SResourceBindPoint, SResourceBinding> > m_resourceBindings;

	bool operator<(const SDeviceResourceLayoutDesc& other) const;
};

static_assert(sizeof(SDeviceResourceLayoutDesc::SLayoutBindPoint) == sizeof(uint8) + sizeof(SDeviceResourceLayoutDesc::ELayoutSlotType),
	"SDeviceResourceLayoutDesc::SLayoutBindPoint must not have padding since we directly calculate hashes based on the struct data");


class CDeviceResourceLayout
{
public:
	CDeviceResourceLayout(UsedBindSlotSet requiredBindings)
		: m_requiredResourceBindings(requiredBindings)
	{}

	UsedBindSlotSet GetRequiredResourceBindings() const { return m_requiredResourceBindings; }

protected:
	UsedBindSlotSet m_requiredResourceBindings;
};

typedef std::shared_ptr<const CDeviceResourceLayout> CDeviceResourceLayoutConstPtr;
typedef std::weak_ptr<const CDeviceResourceLayout>   CDeviceResourceLayoutConstWPtr;

typedef std::shared_ptr<CDeviceResourceLayout>       CDeviceResourceLayoutPtr;
typedef std::weak_ptr<CDeviceResourceLayout>         CDeviceResourceLayoutWPtr;

