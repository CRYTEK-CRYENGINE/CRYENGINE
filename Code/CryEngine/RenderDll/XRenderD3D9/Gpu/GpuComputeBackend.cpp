// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GpuComputeBackend.h"
#include "DriverD3D.h"

namespace gpu
{

CounterReadbackUsed::CounterReadbackUsed()
{
#if CRY_PLATFORM_DURANGO && defined(DEVICE_SUPPORTS_PERFORMANCE_DEVICE) && TODO
	// count buffer
	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	bufferDesc.Usage = D3D11_USAGE_STAGING;
	bufferDesc.BindFlags = 0;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	bufferDesc.MiscFlags = 0;
	bufferDesc.ByteWidth = 16;
	bufferDesc.StructureByteStride = sizeof(uint32);

	HRESULT hr = D3DAllocateGraphicsMemory(1 * sizeof(uint32), 0, 0, D3D11_GRAPHICS_MEMORY_ACCESS_CPU_CACHE_COHERENT, &m_basePtr);
	gcpRendD3D.GetPerformanceDevice().CreatePlacementBuffer(&bufferDesc, m_basePtr, &m_countReadbackBuffer);
#else
	m_countReadbackBuffer = new CGpuBuffer();
	m_countReadbackBuffer->Create(1, sizeof(uint32), DXGI_FORMAT_UNKNOWN, CDeviceObjectFactory::USAGE_CPU_READ, nullptr);
#endif

	m_readbackCalled = false;
}

int CounterReadbackUsed::Retrieve()
{
	if (!m_readbackCalled)
		return 0;

#if CRY_RENDERER_VULKAN
	GetDeviceObjectFactory().GetVKScheduler()->FlushToFence(m_readbackFence);
	GetDeviceObjectFactory().GetVKScheduler()->WaitForFence(m_readbackFence);
#endif

	int result = 0;
	CDeviceObjectFactory::DownloadContents<false>(m_countReadbackBuffer->GetDevBuffer()->GetBuffer(), 0, 0, sizeof(int), D3D11_MAP_READ, &result);

	m_readbackCalled = false;

	return result;
}

void CounterReadbackUsed::Readback(CDeviceBuffer* pBuffer)
{
	if (!pBuffer)
		CryFatalError("pBuffer is 0");

	CDeviceCommandListRef commandList = GetDeviceObjectFactory().GetCoreCommandList();
	commandList.GetCopyInterface()->Copy(pBuffer, m_countReadbackBuffer->GetDevBuffer());
	m_readbackCalled = true;

#if CRY_RENDERER_VULKAN
	m_readbackFence = GetDeviceObjectFactory().GetVKScheduler()->InsertFence();
#endif
}

DataReadbackUsed::DataReadbackUsed(uint32 size, uint32 stride) : m_readback(nullptr)
{
	// Create the Readback Buffer
	// This is used to read the results back to the CPU
#if CRY_PLATFORM_DURANGO && defined(DEVICE_SUPPORTS_PERFORMANCE_DEVICE) && TODO
	D3D11_BUFFER_DESC readback_buffer_desc;
	ZeroMemory(&readback_buffer_desc, sizeof(readback_buffer_desc));
	readback_buffer_desc.ByteWidth = size * stride;
	readback_buffer_desc.Usage = D3D11_USAGE_STAGING;
	readback_buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	readback_buffer_desc.StructureByteStride = stride;

	HRESULT hr = D3DAllocateGraphicsMemory(size * stride, 0, 0, D3D11_GRAPHICS_MEMORY_ACCESS_CPU_CACHE_COHERENT, &m_basePtr);
	gcpRendD3D.GetPerformanceDevice().CreatePlacementBuffer(&readback_buffer_desc, m_basePtr, &m_readback);
#else
	m_readback = new CGpuBuffer();
	m_readback->Create(size, stride, DXGI_FORMAT_UNKNOWN, CDeviceObjectFactory::USAGE_CPU_READ, nullptr);
#endif

	m_stride = stride;
	m_size = size;
	m_readbackCalled = false;
}

const void* DataReadbackUsed::Map(uint32 readLength)
{
	if (!m_readbackCalled)
		return nullptr;

#if CRY_RENDERER_VULKAN
	GetDeviceObjectFactory().GetVKScheduler()->FlushToFence(m_readbackFence);
	GetDeviceObjectFactory().GetVKScheduler()->WaitForFence(m_readbackFence);
#endif

	// Readback the data
	return CDeviceObjectFactory::Map(m_readback->GetDevBuffer()->GetBuffer(), 0, 0, readLength * m_stride, D3D11_MAP_READ);
}

void DataReadbackUsed::Unmap()
{
	CDeviceObjectFactory::Unmap(m_readback->GetDevBuffer()->GetBuffer(), 0, 0, 0, D3D11_MAP_READ);
	m_readbackCalled = false;
}

void DataReadbackUsed::Readback(CGpuBuffer* buf, uint32 readLength)
{
	const SResourceRegionMapping region =
	{
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 0 },
		{ readLength * m_stride, 1, 1, 1 }
	};

	GetDeviceObjectFactory().GetCoreCommandList().GetCopyInterface()->Copy(buf->GetDevBuffer(), m_readback->GetDevBuffer(), region);
	
	m_readbackCalled = true;

#if CRY_RENDERER_VULKAN
	m_readbackFence = GetDeviceObjectFactory().GetVKScheduler()->InsertFence();
#endif
}
}
