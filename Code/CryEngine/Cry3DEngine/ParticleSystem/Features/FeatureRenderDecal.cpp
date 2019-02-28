// Copyright 2015-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureCommon.h"

namespace pfx2
{

extern TDataType<float> EPDT_Alpha;
extern TDataType<uint8> EPDT_Tile;

class CFeatureRenderDecals : public CParticleFeature, public Cry3DEngineBase
{
private:
	typedef CParticleFeature BaseClass;

public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureRenderDecals()
		: m_thickness(1.0f) {}

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override;
	virtual void Serialize(Serialization::IArchive& ar) override;
	virtual void RenderDeferred(const CParticleComponentRuntime& runtime, const SRenderContext& renderContext) override;

private:
	SFloat m_thickness;
	SFloat m_sortBias;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureRenderDecals, "Render", "Decals", colorRender);

void CFeatureRenderDecals::AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams)
{
	pComponent->RenderDeferred.add(this);
	pComponent->AddParticleData(EPQF_Orientation);
}

void CFeatureRenderDecals::Serialize(Serialization::IArchive& ar)
{
	BaseClass::Serialize(ar);
	ar(m_thickness, "Thickness", "Thickness");
	ar(m_sortBias, "SortBias", "Sort Bias");
}

struct SDecalTiler
{
	SDecalTiler(const SComponentParams& _params, const CParticleContainer& _container)
		: animation(_params.m_textureAnimation)
		, ages(_container.GetIFStream(EPDT_NormalAge))
		, lifetimes(_container.GetIFStream(EPDT_LifeTime))
		, tiles(_container.IStream(EPDT_Tile))
		, frameCount(float(_params.m_textureAnimation.m_frameCount))
		, firstTile(_params.m_shaderData.m_firstTile)
		, tileSize(Vec2(_params.m_shaderData.m_tileSize[0], _params.m_shaderData.m_tileSize[1]))
		, hasAbsFrameRate(_params.m_textureAnimation.HasAbsoluteFrameRate())
		, hasAnimation(_params.m_textureAnimation.IsAnimating())
	{}

	void GetTextureRect(RectF& rectTex, Vec3& texBlend, const TParticleId particleId) const
	{
		const float age = ages.Load(particleId);
		float frameIdx = float(tiles.SafeLoad(particleId));
		texBlend.z = 0.0f;

		if (hasAnimation)
		{
			frameIdx += hasAbsFrameRate ?
				animation.GetAnimPosAbsolute(age * lifetimes.Load(particleId))
				: animation.GetAnimPosRelative(age);

			if (animation.m_frameBlending)
			{
				texBlend.z = frameIdx - floor(frameIdx);
				float fFrameBlend = floor(frameIdx) + 1.f;
				if (fFrameBlend >= firstTile + frameCount)
					fFrameBlend = firstTile;

				texBlend.x = fFrameBlend * tileSize.x;
				texBlend.y = floor(texBlend.x) * tileSize.y;
				texBlend.x -= floor(texBlend.x);
			}

			frameIdx = floor(frameIdx);
		}

		rectTex.x = frameIdx * tileSize.x;
		rectTex.y = floor(rectTex.x) * tileSize.y;
		rectTex.x -= floor(rectTex.x);
		rectTex.w = tileSize.x;
		rectTex.h = tileSize.y;
	}

	const STextureAnimation& animation;
	const IFStream ages;
	const IFStream lifetimes;
	const TIStream<uint8> tiles;
	const Vec2 tileSize;
	const float frameCount;
	const float firstTile;
	const bool hasAbsFrameRate;
	const bool hasAnimation;
};

void CFeatureRenderDecals::RenderDeferred(const CParticleComponentRuntime& runtime, const SRenderContext& renderContext)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	if (renderContext.m_passInfo.IsRecursivePass())
		return;
	if (!runtime.IsCPURuntime())
		return;

	const auto& passInfo = renderContext.m_passInfo;
	
	IRenderer* pRenderer = GetRenderer();
	const SComponentParams& params = runtime.ComponentParams();
	const CParticleContainer& container = runtime.GetContainer();
	const IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
	const IQuatStream orientations = container.GetIQuatStream(EPQF_Orientation);
	const IFStream sizes = container.GetIFStream(EPDT_Size);
	const IFStream alphas = container.GetIFStream(EPDT_Alpha, 1.0f);
	const IFStream angles = container.GetIFStream(EPDT_Angle2D);
	const bool hasAngles2D = container.HasData(EPDT_Angle2D);
	const bool hasBlending = params.m_textureAnimation.m_frameBlending;

	SDecalTiler decalTiler(params, container);
	SDeferredDecal decal;
	decal.pMaterial = params.m_pMaterial;
	decal.nSortOrder = clamp_tpl(int(m_sortBias * 100.f), 0, 255);
	decal.nFlags = 0;

	for (auto particleId : runtime.FullRange())
	{
		const float size = sizes.Load(particleId);
		if (size <= 0.0f)
			continue;

		const Vec3 position = positions.Load(particleId);
		const Quat orientation = orientations.Load(particleId);

		Vec3 texBlend;
		decalTiler.GetTextureRect(decal.rectTexture, texBlend, particleId);

		Vec3 xAxis = orientation.GetColumn0() * size;
		Vec3 yAxis = orientation.GetColumn1() * size;
		Vec3 zAxis = orientation.GetColumn2() * (size * m_thickness);
		if (hasAngles2D)
		{
			const float angle = angles.Load(particleId);
			RotateAxes(&xAxis, &yAxis, angle);
		}
		decal.projMatrix.SetFromVectors(xAxis, yAxis, zAxis, position);

		const float alpha = alphas.SafeLoad(particleId);
		decal.fAlpha = alpha * (1.0f - texBlend.z);

		pRenderer->EF_AddDeferredDecal(decal, passInfo);

		if (hasBlending)
		{
			decal.rectTexture.x = texBlend.x;
			decal.rectTexture.y = texBlend.y;
			decal.fAlpha = alpha * texBlend.z;
			pRenderer->EF_AddDeferredDecal(decal, passInfo);
		}
	}
}

}
