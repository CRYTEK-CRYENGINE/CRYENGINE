// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SkinningData.h"

void SkinningData::SetVertexCount(int vertexCount)
{
	m_weights.resize(vertexCount);
}

void SkinningData::AddWeight(int vertexIndex, int boneIndex, float weight)
{
	m_weights[vertexIndex].push_back(BoneWeight(boneIndex, weight));
}

int SkinningData::GetVertexCount() const
{
	return int(m_weights.size());
}

int SkinningData::GetBoneLinkCount(int vertexIndex) const
{
	return int(m_weights[vertexIndex].size());
}

int SkinningData::GetBoneIndex(int vertexIndex, int linkIndex) const
{
	return m_weights[vertexIndex][linkIndex].boneIndex;
}

float SkinningData::GetWeight(int vertexIndex, int linkIndex) const
{
	return m_weights[vertexIndex][linkIndex].weight;
}
