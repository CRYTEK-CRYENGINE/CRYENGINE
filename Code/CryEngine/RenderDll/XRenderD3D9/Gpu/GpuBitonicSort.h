// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "GpuComputeBackend.h"
#include "GraphicsPipeline/Common/ComputeRenderPass.h"

namespace gpu
{
// same definition as in shader
const UINT BITONIC_BLOCK_SIZE = 1024;
const UINT TRANSPOSE_BLOCK_SIZE = 16;
const UINT NUM_ELEMENTS = (BITONIC_BLOCK_SIZE * BITONIC_BLOCK_SIZE);

class CBitonicSort
{
public:
	struct SBitonicSortItem
	{
		uint32 key;
		uint32 payload;
	};

	struct CParams
	{
		uint32 iLevel;
		uint32 iLevelMask;
		uint32 iWidth;
		uint32 iHeight;
	};
	CBitonicSort();
	void Sort(uint32 numElements, CDeviceCommandListRef RESTRICT_REFERENCE commandList);
	CGpuBuffer& GetBuffer() { return m_data.GetBuffer(); }
private:
	void        SyncParams(uint32 iLevel, uint32 iLevelMask, uint32 iWidth, uint32 iHeight);
	CTypedConstantBuffer<CParams>                          m_params;
	CStructuredResource<SBitonicSortItem, BufferFlagsReadWrite> m_data;
	CStructuredResource<SBitonicSortItem, BufferFlagsReadWrite> m_transposeData;
	CComputeRenderPass m_computePassBitonicSort;
	CComputeRenderPass m_computePassBitonicTranspose;
};
}
