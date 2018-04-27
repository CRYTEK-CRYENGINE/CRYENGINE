// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SINGLEANIMATIONEXPORTSOURCEADAPTER_H__
#define __SINGLEANIMATIONEXPORTSOURCEADAPTER_H__

#include "ExportSourceDecoratorBase.h"

class SingleAnimationExportSourceAdapter : public ExportSourceDecoratorBase
{
public:
	SingleAnimationExportSourceAdapter(IExportSource* source, IGeometryFileData* geometryData, int geometryFileIndex, int animationIndex);

	virtual void ReadGeometryFiles(IExportContext* context, IGeometryFileData* geometryFileData);
	virtual void ReadModels(const IGeometryFileData* geometryFileData, int geometryFileIndex, IModelData* modelData);
	virtual void ReadSkinning(IExportContext* context, ISkinningData* skinningData, const IModelData* modelData, int modelIndex, ISkeletonData* skeletonData);
	virtual bool ReadSkeleton(const IGeometryFileData* geometryFileData, int geometryFileIndex, const IModelData* modelData, int modelIndex, const IMaterialData* materialData, ISkeletonData* skeletonData);
	virtual int GetAnimationCount() const;
	virtual std::string GetAnimationName(const IGeometryFileData* geometryFileData, int geometryFileIndex, int animationIndex) const;
	virtual void GetAnimationTimeSpan(float& start, float& stop, int animationIndex) const;
	virtual void ReadAnimationFlags(IExportContext* context, IAnimationData* animationData, const IGeometryFileData* geometryFileData, const IModelData* modelData, int modelIndex, const ISkeletonData* skeletonData, int animationIndex) const;
	virtual IAnimationData * ReadAnimation(IExportContext* context, const IGeometryFileData* geometryFileData, const IModelData* modelData, int modelIndex, const ISkeletonData* skeletonData, int animationIndex, float fps) const;

private:
	int animationIndex;
	IGeometryFileData* geometryFileData;
	int geometryFileIndex;
};

#endif //__SINGLEANIMATIONEXPORTSOURCEADAPTER_H__
