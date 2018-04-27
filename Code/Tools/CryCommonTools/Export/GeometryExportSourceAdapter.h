// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GEOMETRYEXPORTSOURCEADAPTER_H__
#define __GEOMETRYEXPORTSOURCEADAPTER_H__

#include "ExportSourceDecoratorBase.h"

class GeometryExportSourceAdapter : public ExportSourceDecoratorBase
{
public:
	GeometryExportSourceAdapter(IExportSource* source, IGeometryFileData* geometryFileData, const std::vector<int>& geometryFileIndices);

	virtual void ReadGeometryFiles(IExportContext* context, IGeometryFileData* geometryFileData);
	virtual void ReadModels(const IGeometryFileData* geometryFileData, int geometryFileIndex, IModelData* modelData);
	virtual bool ReadSkeleton(const IGeometryFileData* geometryFileData, int geometryFileIndex, const IModelData* modelData, int modelIndex, const IMaterialData* materialData, ISkeletonData* skeletonData);

private:
	IGeometryFileData* m_geometryFileData;
	std::vector<int> m_geometryFileIndices;
};

#endif //__GEOMETRYEXPORTSOURCEADAPTER_H__
