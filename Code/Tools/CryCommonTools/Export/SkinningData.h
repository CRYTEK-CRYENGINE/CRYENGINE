// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SKINNINGDATA_H__
#define __SKINNINGDATA_H__

#include "ISkinningData.h"

class SkinningData : public ISkinningData
{
public:
	virtual void SetVertexCount(int vertexCount);
	virtual void AddWeight(int vertexIndex, int boneIndex, float weight);

	int GetVertexCount() const;
	int GetBoneLinkCount(int vertexIndex) const;
	int GetBoneIndex(int vertexIndex, int linkIndex) const;
	float GetWeight(int vertexIndex, int linkIndex) const;

private:
	struct BoneWeight
	{
		BoneWeight(int boneIndex, float weight): boneIndex(boneIndex), weight(weight) {}
		int boneIndex;
		float weight;
	};

	std::vector<std::vector<BoneWeight> > m_weights;
};

#endif //__SKINNINGDATA_H__
