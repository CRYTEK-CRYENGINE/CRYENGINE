// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GpuBitonicSort.h"

namespace gpu
{

CBitonicSort::CBitonicSort() : m_data(NUM_ELEMENTS), m_transposeData(NUM_ELEMENTS)
{
	m_params.CreateDeviceBuffer();

	CShader* pShader = gcpRendD3D.m_cEF.mfForName("BitonicSort", EF_SYSTEM, 0, 0);
	m_computePassBitonicSort.SetTechnique(pShader, CCryNameTSCRC("BitonicSort"), 0);
	m_computePassBitonicTranspose.SetTechnique(pShader, CCryNameTSCRC("BitonicTranspose"), 0);
}

void CBitonicSort::Sort(uint32 numElements, CDeviceCommandListRef RESTRICT_REFERENCE commandList)
{
	PROFILE_LABEL_SCOPE("Bitonic Sort");

	// Note: Minimum elements must be fixed !!!!!!!!!!!
	numElements = min(numElements, NUM_ELEMENTS);
	const size_t MATRIX_WIDTH = BITONIC_BLOCK_SIZE;
	const size_t MATRIX_HEIGHT = numElements / BITONIC_BLOCK_SIZE;

	// First sort the rows for the levels <= to the block size
	m_computePassBitonicSort.SetBuffer(0, &m_data.GetBuffer());
	m_computePassBitonicSort.SetBuffer(1, &m_transposeData.GetBuffer());
	m_computePassBitonicSort.SetInlineConstantBuffer(8, m_params.GetDeviceConstantBuffer());
	m_computePassBitonicTranspose.SetInlineConstantBuffer(8, m_params.GetDeviceConstantBuffer());
	m_computePassBitonicSort.SetDispatchSize(numElements / BITONIC_BLOCK_SIZE, 1, 1);

	m_computePassBitonicSort.PrepareResourcesForUse(commandList);
	for (size_t level = 2; level <= BITONIC_BLOCK_SIZE; level <<= 1)
	{
		SyncParams(level, level, MATRIX_HEIGHT, MATRIX_WIDTH);
		m_computePassBitonicSort.Execute(commandList);
	}

	// Then sort the rows and columns for the levels > than the block size
	// Transpose. Sort the Columns. Transpose. Sort the Rows.
	for (size_t level = (BITONIC_BLOCK_SIZE << 1); level <= numElements; level <<= 1)
	{
		// Transpose the data from buffer 1 into buffer 2
		SyncParams((level / BITONIC_BLOCK_SIZE), (level & ~size_t(numElements)) / BITONIC_BLOCK_SIZE,
		           MATRIX_WIDTH, MATRIX_HEIGHT);

		m_computePassBitonicTranspose.SetBuffer(0, &m_transposeData.GetBuffer());
		m_computePassBitonicTranspose.SetBuffer(1, &m_data.GetBuffer());

		m_computePassBitonicTranspose.SetDispatchSize(MATRIX_WIDTH / TRANSPOSE_BLOCK_SIZE, MATRIX_HEIGHT / TRANSPOSE_BLOCK_SIZE, 1);

		m_computePassBitonicTranspose.PrepareResourcesForUse(commandList);
		m_computePassBitonicTranspose.Execute(commandList);

		m_computePassBitonicSort.SetBuffer(0, &m_transposeData.GetBuffer());
		m_computePassBitonicSort.SetBuffer(1, &m_data.GetBuffer());

		// Sort the transposed column data
		m_computePassBitonicSort.PrepareResourcesForUse(commandList);
		m_computePassBitonicSort.Execute(commandList);

		// Transpose the data from buffer 2 back into buffer 1
		SyncParams(BITONIC_BLOCK_SIZE, level, MATRIX_HEIGHT / TRANSPOSE_BLOCK_SIZE, MATRIX_WIDTH / TRANSPOSE_BLOCK_SIZE);

		m_computePassBitonicTranspose.SetBuffer(0, &m_data.GetBuffer());
		m_computePassBitonicTranspose.SetBuffer(1, &m_transposeData.GetBuffer());

		m_computePassBitonicTranspose.SetDispatchSize(MATRIX_HEIGHT / TRANSPOSE_BLOCK_SIZE, MATRIX_WIDTH / TRANSPOSE_BLOCK_SIZE, 1);
		m_computePassBitonicTranspose.PrepareResourcesForUse(commandList);
		m_computePassBitonicTranspose.Execute(commandList);

		m_computePassBitonicSort.SetBuffer(0, &m_data.GetBuffer());
		m_computePassBitonicSort.SetBuffer(1, &m_transposeData.GetBuffer());
		// Sort the row data
		m_computePassBitonicSort.PrepareResourcesForUse(commandList);
		m_computePassBitonicSort.Execute(commandList);
	}
}

void CBitonicSort::SyncParams(uint32 iLevel, uint32 iLevelMask, uint32 iWidth, uint32 iHeight)
{
	m_params->iLevel = iLevel;
	m_params->iLevelMask = iLevelMask;
	m_params->iWidth = iWidth;
	m_params->iHeight = iHeight;
	m_params.CopyToDevice();
}

}
