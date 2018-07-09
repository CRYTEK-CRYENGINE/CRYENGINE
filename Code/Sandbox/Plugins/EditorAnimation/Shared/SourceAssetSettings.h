// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Serialization.h"

namespace SourceAsset
{

struct SkeletonImport
{
	string nodeName;

	void   Serialize(IArchive& ar)
	{
		if (ar.isEdit() && ar.isOutput())
			ar(nodeName, "node", "^!");
		ar(nodeName, "nodeName", "Node Name");
	}
};
typedef vector<SkeletonImport> SkeletonImports;

struct MeshImport
{
	string nodeName;

	bool   use8Weights;
	bool   fp32Precision;

	MeshImport()
		: use8Weights(false)
		, fp32Precision(false)
	{
	}

	void Serialize(IArchive& ar)
	{
		if (ar.isEdit() && ar.isOutput())
			ar(nodeName, "node", "^!");
		ar(nodeName, "nodeName", "Node Name");
		ar(use8Weights, "use8Weights", "Use 8 Joint Weights");
		ar(fp32Precision, "fp32Precision", "High Precision Positions (FP32)");
	}
};
typedef vector<MeshImport> MeshImports;

struct Settings
{
	MeshImports     meshes;
	SkeletonImports skeletons;

	void            Serialize(IArchive& ar)
	{
		ar(meshes, "meshes", "+Meshes");
		ar(skeletons, "skeletons", "+Skeletons");
	}

	bool UsedAsMesh(const char* nodeName) const
	{
		for (size_t i = 0; i < meshes.size(); ++i)
			if (meshes[i].nodeName == nodeName)
				return true;
		return false;
	}

	bool UsedAsSkeleton(const char* nodeName) const
	{
		for (size_t i = 0; i < skeletons.size(); ++i)
			if (skeletons[i].nodeName == nodeName)
				return true;
		return false;
	}
};

}

