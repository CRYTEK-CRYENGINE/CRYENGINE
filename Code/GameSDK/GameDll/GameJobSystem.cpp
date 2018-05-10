// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Effects/RenderNodes/LightningNode.h"
#include "Effects/GameEffects/LightningGameEffect.h"
#include "Utility/Hermite.h"
#include <CryRenderer/IRenderAuxGeom.h>


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
const AABB CLightningRenderNode::GetBBox() const
{
	if (m_dirtyBBox)
	{
		const float borderSize = m_pLightningDesc ? m_pLightningDesc->m_beamSize : 0.0f;
		AABB box(m_emmitterPosition);
		box.Add(m_receiverPosition);

		for (unsigned int i = 0; i < m_segments.size(); ++i)
		{
			const CSegment& segment = m_segments[i];
			for (int j = 0; j < segment.GetNumPoints(); ++j)
			{
				if(m_pLightningDesc)
					box.Add(segment.GetPoint(*m_pLightningDesc, m_pointData, j, m_deviationMult));
			}
		}

		box.min -= Vec3(borderSize, borderSize, borderSize);
		box.max += Vec3(borderSize, borderSize, borderSize);

		m_aabb = box;
		m_dirtyBBox = false;
	}
	return m_aabb;
}

///////////////////////////////////////////////////////////////////////////////
Vec3 CLightningRenderNode::CSegment::GetPoint(const SLightningParams& desc, const SPointData& pointData, int point, float deviationMult) const
{
	const int numSegs = desc.m_strikeNumSegments;
	const int numSubSegs = desc.m_strikeNumPoints;

	const float deviation = desc.m_lightningDeviation;
	const float fuzzyness = desc.m_lightningFuzzyness;

	int i = point / numSubSegs;
	int j = point % numSubSegs;

	int idx[4] =
	{
		max(i-1, 0),
		i,
		min(i+1, numSegs),
		min(i+2, numSegs)
	};

	Vec3 positions[4];
	for (int l = 0; l < 4; ++l)
	{
		positions[l] = LERP(m_origin, m_destany, idx[l] / float(numSegs));
		positions[l] += pointData.m_points[m_firstPoint+idx[l]] * deviation * deviationMult;
		positions[l] += pointData.m_velocity[m_firstPoint+idx[l]] * desc.m_lightningVelocity * m_time * deviationMult;
	}

	float x = j / float(numSubSegs);
	int k = i*numSubSegs + j;
	Vec3 result = CatmullRom(
		positions[0], positions[1],
		positions[2], positions[3], x);
	result = result + pointData.m_fuzzyPoints[m_firstFuzzyPoint+k] * fuzzyness * deviationMult;

	return result;
}

///////////////////////////////////////////////////////////////////////////////
void CLightningRenderNode::FillBBox(AABB &aabb) 
{ 
	aabb = CLightningRenderNode::GetBBox();
}

///////////////////////////////////////////////////////////////////////////////
EERType CLightningRenderNode::GetRenderNodeType()
{
	return eERType_GameEffect;
}

///////////////////////////////////////////////////////////////////////////////
float CLightningRenderNode::GetMaxViewDist()
{
	// TODO : needs to use standard view distance ratio calculation used by other render nodes
	const float maxViewDistance = 2000.0f;
	return maxViewDistance;
}

///////////////////////////////////////////////////////////////////////////////
Vec3 CLightningRenderNode::GetPos(bool bWorldOnly) const
{
	return Vec3(ZERO);
}

///////////////////////////////////////////////////////////////////////////////
IMaterial* CLightningRenderNode::GetMaterial(Vec3* pHitPos) const
{
	return m_pMaterial;
}
