// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __EXPORTSOURCEDECORATORBASE_H__
#define __EXPORTSOURCEDECORATORBASE_H__

#include "IExportSource.h"

class ExportSourceDecoratorBase : public IExportSource
{
public:
	explicit ExportSourceDecoratorBase(IExportSource* source);

	virtual std::string GetResourceCompilerPath() const { return std::string(""); };
	virtual void GetMetaData(SExportMetaData& metaData) const;
	virtual std::string GetDCCFileName() const;
	virtual std::string GetExportDirectory() const;
	virtual void ReadGeometryFiles(IExportContext* context, IGeometryFileData* geometryFileData);
	virtual bool ReadMaterials(IExportContext* context, const IGeometryFileData* geometryFileData, IMaterialData* materialData);
	virtual void ReadModels(const IGeometryFileData* geometryFileData, int geometryFileIndex, IModelData* modelData);
	virtual void ReadSkinning(IExportContext* context, ISkinningData* skinningData, const IModelData* modelData, int modelIndex, ISkeletonData* skeletonData);
	virtual bool ReadSkeleton(const IGeometryFileData* geometryFileData, int geometryFileIndex, const IModelData* modelData, int modelIndex, const IMaterialData* materialData, ISkeletonData* skeletonData);
	virtual int GetAnimationCount() const;
	virtual std::string GetAnimationName(const IGeometryFileData* geometryFileData, int geometryFileIndex, int animationIndex) const;
	virtual void GetAnimationTimeSpan(float& start, float& stop, int animationIndex) const;
	virtual void ReadAnimationFlags(IExportContext* context, IAnimationData* animationData, const IGeometryFileData* geometryFileData, const IModelData* modelData, int modelIndex, const ISkeletonData* skeletonData, int animationIndex) const;
	virtual IAnimationData * ReadAnimation(IExportContext* context, const IGeometryFileData* geometryFileData, const IModelData* modelData, int modelIndex, const ISkeletonData* skeletonData, int animationIndex, float fps) const;
	virtual bool ReadGeometry(IExportContext* context, IGeometryData* geometry, const IModelData* modelData, const IMaterialData* materialData, int modelIndex);
	virtual bool ReadGeometryMaterialData(IExportContext* context, IGeometryMaterialData* geometryMaterialData, const IModelData* modelData, const IMaterialData* materialData, int modelIndex) const;
	virtual bool ReadBoneGeometry(IExportContext* context, IGeometryData* geometry, ISkeletonData* skeletonData, int boneIndex, const IMaterialData* materialData);
	virtual bool ReadBoneGeometryMaterialData(IExportContext* context, IGeometryMaterialData* geometryMaterialData, ISkeletonData* skeletonData, int boneIndex, const IMaterialData* materialData) const;
	virtual void ReadMorphs(IExportContext* context, IMorphData* morphData, const IModelData* modelData, int modelIndex);
	virtual bool ReadMorphGeometry(IExportContext* context, IGeometryData* geometry, const IModelData* modelData, int modelIndex, const IMorphData* morphData, int morphIndex, const IMaterialData* materialData);
	virtual bool HasValidPosController(const IModelData* modelData, int modelIndex) const;
	virtual bool HasValidRotController(const IModelData* modelData, int modelIndex) const;
	virtual bool HasValidSclController(const IModelData* modelData, int modelIndex) const;
protected:
	IExportSource* source;
};

#endif //__EXPORTSOURCEDECORATORBASE_H__
