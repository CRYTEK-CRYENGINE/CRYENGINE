// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "VKBase.hpp"
#include "VKHeap.hpp"

class CDeviceObjectFactory;

namespace NCryVulkan
{

class CImageResource;
class CBufferResource;
class CDynamicOffsetBufferResource;

enum EResourceFlag // Note: Some flags are only present for validation/debugging purposes.
{
	// Flags for buffers and images
	kResourceFlagNone           = 0,
	kResourceFlagNull           = BIT(0),  // Tagged from high-level as a "null resource"
	kResourceFlagReusable       = BIT(1),  // Tagged from high-level as a "reusable resource"
	kResourceFlagCpuReadable    = BIT(2),  // Stored in memory with VK_MEMORY_PROPERTY_HOST_CACHED_BIT
	kResourceFlagCpuWritable    = BIT(3),  // Stored in memory with VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
	kResourceFlagCpuCoherent    = BIT(4),  // Stored in memory with VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	kResourceFlagCopyReadable   = BIT(5),  // Resource has VK_XXX_USAGE_TRANSFER_SRC_BIT
	kResourceFlagCopyWritable   = BIT(6),  // Resource has VK_XXX_USAGE_TRANSFER_DST_BIT
	kResourceFlagShaderReadable = BIT(7),  // Resource has VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT / VK_IMAGE_USAGE_SAMPLED_BIT
	kResourceFlagShaderWritable = BIT(8),  // Resource has VK_BUFFER_XXX_STORAGE_BUFFER_BIT
	kResourceFlagLinear         = BIT(9),  // Resource is stored in linear layout (ie, not tiled)
	// Flags for buffers only
	kBufferFlagTexels           = BIT(10), // Buffer has VK_BUFFER_USAGE_XXX_TEXEL_BUFFER_BIT
	kBufferFlagIndices          = BIT(11), // Can be bound using vkCmdBindIndexBuffer
	kBufferFlagVertices         = BIT(12), // Can be bound using vkCmdBindVertexBuffer
	kBufferFlagDynamicOffset    = BIT(13), // Can be bound with dynamic offset
	// Flags for images only
	kImageFlagSwapchain         = BIT(14), // Flag to tell if the VKImage is owned by swap-chain
	kImageFlagCube              = BIT(15), // Image has VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
	kImageFlagColorAttachment   = BIT(16), // Image has VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
	kImageFlagDepthAttachment   = BIT(17), // Image has VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, and format has a depth aspect
	kImageFlagStencilAttachment = BIT(18), // Image has VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, and format has a stencil aspect
	kImageFlag1D                = BIT(19), // Image is stored as (array-of-)1-dimensional
	kImageFlag2D                = BIT(20), // Image is stored as (array-of-)2-dimensional
	kImageFlag3D                = BIT(21), // Image is stored as 3-dimensional
	kImageFlagMultiSampled      = BIT(22), // Image has MSAA enabled
};
}

DEFINE_ENUM_FLAG_OPERATORS(NCryVulkan::EResourceFlag);

namespace NCryVulkan
{

class CDevice;

class CMemoryResource : public CDeviceObject
{
public:
	friend class CDevice;
	friend class ::CDeviceObjectFactory;

	CMemoryResource(CDevice* device) : CDeviceObject(device) {}
	~CMemoryResource() override;

	CMemoryResource(CMemoryResource&&) = default;
	CMemoryResource& operator=(CMemoryResource&&) = default;

	void Init(EHeapType heapType, CMemoryHandle&& boundMemory);

	EHeapType            GetHeapType() const { return m_heapType; }
	const CMemoryHandle& GetMemory() const { return m_boundMemory; }

	std::string GetName() threadsafe_const
	{
		UINT len = 512;
		char str[512] = "-";

		if (true)
		{
			sprintf(str, "%p", this);
		}

		return str;
	}

	VkDeviceSize                  GetSize() const;
	void                          GetType(D3D11_RESOURCE_DIMENSION* pDim); // Legacy, to be removed!
	bool                          GetFlag(EResourceFlag flag) const       { return (m_flags & flag) != 0; }
	CImageResource*               AsImage() /* safe cast */               { return (m_flags & (kImageFlag1D | kImageFlag2D | kImageFlag3D)) ? (CImageResource*)this : nullptr; }
	CBufferResource*              AsBuffer() /* safe cast */              { return (m_flags & (kImageFlag1D | kImageFlag2D | kImageFlag3D)) ? nullptr : (CBufferResource*)this; }
	CDynamicOffsetBufferResource* AsDynamicOffsetBuffer() /* safe cast */ { return (m_flags & kBufferFlagDynamicOffset) ? (CDynamicOffsetBufferResource*) this : nullptr; }

	void*            Map();
	void             Unmap();

	void TrackAccess(VkAccessFlags access)
	{
		m_eCurrentAccess |= access;
	}

	VkAccessFlags GetAccess() const
	{
		return m_eCurrentAccess;
	}

	void ResetAccess()
	{
		m_eCurrentAccess = 0;
	}

	void TrackAnnouncedAccess(VkAccessFlags access)
	{
		m_eAnnouncedAccess = access;
	}

	VkAccessFlags GetAnnouncedAccess() const
	{
		return m_eAnnouncedAccess;
	}

	void ResetAnnouncedAccess()
	{
		m_eAnnouncedAccess = 0;
	}

	VkAccessFlags AccessBarrier(CCommandList* pCmdList);

#ifndef _RELEASE
	bool DebugSetName(const char* szName);
#endif

protected:
	virtual void Destroy() override; // Handles usage-tracked RefCount of 0

	EResourceFlag m_flags = kResourceFlagNone;

	void SetFlags(EResourceFlag flags) { m_flags  = flags; }
	void AddFlags(EResourceFlag flags) { m_flags |= flags; }
	
	#define VK_BARRIER_VERIFICATION // verify and repair wrong barriers
	#define VK_BARRIER_RELAXATION   // READ-barriers are not changed to more specific subsets like INDEX_BUFFER

	#ifdef VK_BARRIER_RELAXATION
		#define IsIncompatibleAccess(currentAccess, desiredAccess) ((currentAccess & desiredAccess) != desiredAccess || (!currentAccess != !desiredAccess))
	#else
		#define IsIncompatibleAccess(currentAccess, desiredAccess) ((currentAccess) != desiredAccess)
	#endif

	VkAccessFlags m_eCurrentAccess = 0;
	VkAccessFlags m_eAnnouncedAccess = 0;

private:
	EHeapType     m_heapType;
	CMemoryHandle m_boundMemory;
};

}
