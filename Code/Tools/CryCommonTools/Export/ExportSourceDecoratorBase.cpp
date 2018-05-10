// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ExportSourceDecoratorBase.h"

ExportSourceDecoratorBase::ExportSourceDecoratorBase(IExportSource* source)
: source(source)
{
}

void ExportSourceDecoratorBase::GetMetaData(SExportMetaData& metaData) const
{
	this->source->GetMetaData(metaData);
}

std::string ExportSourceDecoratorBase::GetDCCFileName() const
{
	return this->source->GetDCCFileName();
}

std::string ExportSourceDecoratorBase::GetExportDirectory() const
{
	return this->source->GetExportDirectory();
}

void ExportSourceDecoratorBase::ReadGeometryFiles(IExportContext* context, IGeometryFileData* geometryFileData)
{
	this->source->ReadGeometryFiles(context, geometryFileData);
}

bool ExportSourceDecoratorBase::ReadMaterials(IExportContext* context, const IGeometryFileData* const geometryFileData, IMaterialData* materialData)
{
	return this->source->ReadMaterials(context, geometryFileData, materialData);
}

void ExportSourceDecoratorBase::ReadModels(const IGeometryFileData* geometryFileData, int geometryFileIndex, IModelData* modelData)
{
	this->source->ReadModels(geometryFileData, geometryFileIndex, modelData);
}

void ExportSourceDecoratorBase::ReadSkinning(IExportContext* context, ISkinningData* skinningData, const IModelData* const modelData, int modelIndex, ISkeletonData* skeletonData)
{
	this->source->ReadSkinning(context, skinningData, modelData, modelIndex, skeletonData);
}

bool ExportSourceDecoratorBase::ReadSkeleton(const IGeometryFileData* const geometryFileData, int geometryFileIndex, const IModelData* const modelData, int modelIndex, const IMaterialData* materialData, ISkeletonData* skeletonData)
{
	return this->source->ReadSkeleton(geometryFileData, geometryFileIndex, modelData, modelIndex, materialData, skeletonData);
}

int ExportSourceDecoratorBase::GetAnimationCount() const
{
	return this->source->GetAnimationCount();
}

std::string ExportSourceDecoratorBase::GetAnimationName(const IGeometryFileData* geometryFileData, int geometryFileIndex, int animationIndex) const
{
	return this->source->GetAnimationName(geometryFileData, geometryFileIndex, animationIndex);
}

void ExportSourceDecoratorBase::GetAnimationTimeSpan(float& start, float& stop, int animationIndex) const
{
	this->source->GetAnimationTimeSpan(start, stop, animationIndex);
}

void ExportSourceDecoratorBase::ReadAnimationFlags(IExportContext* context, IAnimationData* animationData, const IGeometryFileData* const geometryFileData, const IModelData* modelData, int modelIndex, const ISkeletonData* skeletonData, int animationIndex) const
{
	this->source->ReadAnimationFlags(context, animationData, geometryFileData, modelData, modelIndex, skeletonData, animationIndex);
}

IAnimationData * ExportSourceDecoratorBase::ReadAnimation(IExportContext* context, const IGeometryFileData* const geometryFileData, const IModelData* modelData, int modelIndex, const ISkeletonData* skeletonData, int animationIndex, float fps) const
{
	return this->source->ReadAnimation(context, geometryFileData, modelData, modelIndex, skeletonData, animationIndex, fps);
}

bool ExportSourceDecoratorBase::ReadGeometry(IExportContext* context, IGeometryData* geometry, const IModelData* const modelData, const IMaterialData* const materialData, int modelIndex)
{
	return this->source->ReadGeometry(context, geometry, modelData, materialData, modelIndex);
}

bool ExportSourceDecoratorBase::ReadGeometryMaterialData(IExportContext* context, IGeometryMaterialData* geometryMaterialData, const IModelData* const modelData, const IMaterialData* const materialData, int modelIndex) const
{
	return this->source->ReadGeometryMaterialData(context, geometryMaterialData, modelData, materialData, modelIndex);
}

bool ExportSourceDecoratorBase::ReadBoneGeometry(IExportContext* context, IGeometryData* geometry, ISkeletonData* skeletonData, int boneIndex, const IMaterialData* const materialData)
{
	return this->source->ReadBoneGeometry(context, geometry, skeletonData, boneIndex, materialData);
}

bool ExportSourceDecoratorBase::ReadBoneGeometryMaterialData(IExportContext* context, IGeometryMaterialData* geometryMaterialData, ISkeletonData* skeletonData, int boneIndex, const IMaterialData* const materialData) const
{
	return this->source->ReadBoneGeometryMaterialData(context, geometryMaterialData, skeletonData, boneIndex, materialData);
}

void ExportSourceDecoratorBase::ReadMorphs(IExportContext* context, IMorphData* morphData, const IModelData* const modelData, int modelIndex)
{
	this->source->ReadMorphs(context, morphData, modelData, modelIndex);
}

bool ExportSourceDecoratorBase::ReadMorphGeometry(IExportContext* context, IGeometryData* geometry, const IModelData* const modelData, int modelIndex, const IMorphData* const morphData, int morphIndex, const IMaterialData* materialData)
{
	return this->source->ReadMorphGeometry(context, geometry, modelData, modelIndex, morphData, morphIndex, materialData);
}

bool ExportSourceDecoratorBase::HasValidPosController(const IModelData* modelData, int modelIndex) const
{
	return this->source->HasValidPosController(modelData, modelIndex);
}

bool ExportSourceDecoratorBase::HasValidRotController(const IModelData* modelData, int modelIndex) const
{
	return this->source->HasValidRotController(modelData, modelIndex);
}

bool ExportSourceDecoratorBase::HasValidSclController(const IModelData* modelData, int modelIndex) const
{
	return this->source->HasValidSclController(modelData, modelIndex);
}