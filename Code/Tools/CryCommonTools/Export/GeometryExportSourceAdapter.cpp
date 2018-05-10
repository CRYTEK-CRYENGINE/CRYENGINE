// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GeometryExportSourceAdapter.h"
#include "IGeometryFileData.h"
#include <cassert>

GeometryExportSourceAdapter::GeometryExportSourceAdapter(IExportSource* source, IGeometryFileData* geometryFileData, const std::vector<int>& geometryFileIndices)
	: ExportSourceDecoratorBase(source)
	, m_geometryFileData(geometryFileData)
	, m_geometryFileIndices(geometryFileIndices)
{
	assert(m_geometryFileIndices.size() <= m_geometryFileData->GetGeometryFileCount());
	for (size_t i = 0; i < m_geometryFileIndices.size(); ++i)
	{
		int const geometryFileIndex = m_geometryFileIndices[i];
		assert(geometryFileIndex >= 0 && geometryFileIndex < m_geometryFileData->GetGeometryFileCount());
	}
}

void GeometryExportSourceAdapter::ReadGeometryFiles(IExportContext* context, IGeometryFileData* geometryFileData)
{
	for (size_t i = 0; i < m_geometryFileIndices.size(); ++i)
	{
		int const geometryFileIndex = m_geometryFileIndices[i];
		geometryFileData->AddGeometryFile(
			m_geometryFileData->GetGeometryFileHandle(geometryFileIndex),
			m_geometryFileData->GetGeometryFileName(geometryFileIndex),
			m_geometryFileData->GetProperties(geometryFileIndex));
	}
}

void GeometryExportSourceAdapter::ReadModels(const IGeometryFileData* geometryFileData, int geometryFileIndex, IModelData* modelData)
{
	assert(geometryFileIndex >= 0 && geometryFileIndex < m_geometryFileIndices.size());
	this->source->ReadModels(m_geometryFileData, m_geometryFileIndices[geometryFileIndex], modelData);
}

bool GeometryExportSourceAdapter::ReadSkeleton(const IGeometryFileData* geometryFileData, int geometryFileIndex, const IModelData* modelData, int modelIndex, const IMaterialData* materialData, ISkeletonData* skeletonData)
{
	assert(geometryFileIndex >= 0 && geometryFileIndex < m_geometryFileIndices.size());
	return this->source->ReadSkeleton(m_geometryFileData, m_geometryFileIndices[geometryFileIndex], modelData, modelIndex, materialData, skeletonData);
}
