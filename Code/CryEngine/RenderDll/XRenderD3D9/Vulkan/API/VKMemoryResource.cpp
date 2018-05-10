// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VKMemoryResource.hpp"
#include "VKExtensions.hpp"

namespace NCryVulkan
{

CMemoryResource::~CMemoryResource()
{
	GetDevice()->GetHeap().Deallocate(m_boundMemory);
}

void CMemoryResource::Init(EHeapType heapType, CMemoryHandle&& boundMemory)
{
	m_heapType = heapType;
	m_boundMemory = std::move(boundMemory);

	m_flags &= ~(kResourceFlagCpuReadable | kResourceFlagCpuWritable | kResourceFlagCpuCoherent);
	if (m_flags & kResourceFlagLinear)
	{
		const uint32_t heapFlags = GetDevice()->GetHeap().GetFlags(m_boundMemory);

		m_flags |=
			((heapFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT  ) ? kResourceFlagCpuReadable : kResourceFlagNone) |
			((heapFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ) ? kResourceFlagCpuWritable : kResourceFlagNone) |
			((heapFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ? kResourceFlagCpuCoherent : kResourceFlagNone);
	}
}

VkDeviceSize CMemoryResource::GetSize() const
{
	return GetDevice()->GetHeap().GetSize(m_boundMemory);
}

void* CMemoryResource::Map()
{
	return GetDevice()->GetHeap().Map(m_boundMemory);
}

void CMemoryResource::Unmap()
{
	return GetDevice()->GetHeap().Unmap(m_boundMemory);
}

void CMemoryResource::Destroy()
{
	VK_ASSERT(false);
}

//---------------------------------------------------------------------------------------------------------------------
const char* AccessToString(VkAccessFlags accessFlags)
{
	static int buf = 0;
	static char str[2][512], *ret;

	ret = str[buf ^= 1];
	*ret = '\0';

	if (!accessFlags)
	{
		strcat(ret, " Empty");
		return ret;
	}

	if (accessFlags & VK_ACCESS_HOST_READ_BIT)
	{
		strcat(ret, " Download");
	}
	if (accessFlags & VK_ACCESS_HOST_WRITE_BIT)
	{
		strcat(ret, " Upload");
	}
	if (accessFlags & VK_ACCESS_MEMORY_READ_BIT)
	{
		strcat(ret, " MemR");
	}
	if (accessFlags & VK_ACCESS_MEMORY_WRITE_BIT)
	{
		strcat(ret, " MemW");
	}
	if (accessFlags & VK_ACCESS_UNIFORM_READ_BIT)
	{
		strcat(ret, " ConstBuf");
	}
	if (accessFlags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT)
	{
		strcat(ret, " IA");
	}
	if (accessFlags & VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)
	{
		strcat(ret, " V/C Buffer");
	}
	if (accessFlags & VK_ACCESS_INDEX_READ_BIT)
	{
		strcat(ret, " I Buffer");
	}
	if (accessFlags & VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
	{
		strcat(ret, " TargetR");
	}
	if (accessFlags & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
	{
		strcat(ret, " TargetW");
	}
	if (accessFlags & VK_ACCESS_SHADER_READ_BIT)
	{
		strcat(ret, " SR");
	}
	if (accessFlags & VK_ACCESS_SHADER_WRITE_BIT)
	{
		strcat(ret, " UA");
	}
	if (accessFlags & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
	{
		strcat(ret, " DepthW");
	}
	if (accessFlags & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT)
	{
		strcat(ret, " DepthR");
	}
	if (accessFlags & VK_ACCESS_INDIRECT_COMMAND_READ_BIT)
	{
		strcat(ret, " Indirect Arg");
	}
	if (accessFlags & VK_ACCESS_TRANSFER_WRITE_BIT)
	{
		strcat(ret, " CopyD");
	}
	if (accessFlags & VK_ACCESS_TRANSFER_READ_BIT)
	{
		strcat(ret, " CopyS");
	}

	return ret;
}

VkAccessFlags CMemoryResource::AccessBarrier(CCommandList* pCmdList)
{
	VkAccessFlags ePreviousAccess = m_eCurrentAccess;

	if (IsIncompatibleAccess(m_eCurrentAccess, m_eAnnouncedAccess))
	{
		VK_LOG(VK_BARRIER_ANALYZER, "Memory resource barrier change %s (Vk: %p): %s ->%s", GetName().c_str(), 0, AccessToString(m_eCurrentAccess), AccessToString(m_eAnnouncedAccess));

		VkMemoryBarrier barrierDesc;
		barrierDesc.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		barrierDesc.pNext = nullptr;
		barrierDesc.srcAccessMask = m_eCurrentAccess;
		barrierDesc.dstAccessMask = m_eAnnouncedAccess;

		pCmdList->ResourceBarrier(1, &barrierDesc);

		m_eCurrentAccess = m_eAnnouncedAccess;
		m_eAnnouncedAccess = 0;
	}

	return ePreviousAccess;
}

#ifndef _RELEASE

bool CMemoryResource::DebugSetName(const char* szName)
{
	if (Extensions::EXT_debug_marker::IsSupported)
	{
		VkDebugMarkerObjectNameInfoEXT nameInfo;
		nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		nameInfo.pNext = nullptr;
		nameInfo.pObjectName = szName;

		if (CBufferResource* pBuffer = AsBuffer())
		{
			if (!AsDynamicOffsetBuffer()) // Naming constant buffers does not make much sense since we constantly suballocate
			{
				nameInfo.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT;
				nameInfo.object = uint64(pBuffer->GetHandle());

				if (Extensions::EXT_debug_marker::SetObjectName(GetDevice()->GetVkDevice(), &nameInfo) == VK_SUCCESS)
					return true;
			}
		}
		else if (CImageResource* pImage = AsImage())
		{
			nameInfo.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT;
			nameInfo.object = uint64(pImage->GetHandle());

			if (Extensions::EXT_debug_marker::SetObjectName(GetDevice()->GetVkDevice(), &nameInfo) == VK_SUCCESS)
				return true;
		}	
	}

	return false;
}

#endif

}
