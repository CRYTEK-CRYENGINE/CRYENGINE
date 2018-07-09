// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ParticleRender.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Evolved:     2005 by J Scott Peter
//  Description: Rendering functions for CParticle and CParticleContainer
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "Particle.h"
#include "ParticleManager.h"
#include "ParticleEmitter.h"
#include "ObjMan.h"
#include "3dEngine.h"
#include "ClipVolumeManager.h"

#include <CryThreading/IJobManager_JobDelegator.h>

template<class T>
class SaveRestore
{
public:
	SaveRestore(T& var)
		: m_pVar(&var), m_Val(var)
	{}
	SaveRestore(T& var, T const& newval)
		: m_pVar(&var), m_Val(var)
	{
		*m_pVar = newval;
	}
	~SaveRestore()
	{
		*m_pVar = m_Val;
	}
protected:
	T* m_pVar;
	T  m_Val;
};

struct SParticleRenderData
{
	// Computed params needed for rendering.
	float fSize;              // Scale for sprite or geometry.
	float fDistCamSq;         // Distance^2 from camera.
	Vec3  vCamDir;            // camera direction
	Vec3  vOffsetCam;         // Offset from camera.

	Vec3  GetDirCam(float fBlendDist) const
	{
		if (fBlendDist <= 0.0f)
		{
			return vOffsetCam * isqrt_tpl(fDistCamSq);
		}
		else
		{
			const float camDist = sqrt_tpl(fDistCamSq);
			const float fOrientLerpFactor = min(camDist, fBlendDist) / fBlendDist;

			const Vec3 vToPlane = vCamDir;
			const Vec3 vToCam = vOffsetCam * isqrt_tpl(fDistCamSq);
			const Vec3 vOrient = Lerp(vToPlane, vToCam, fOrientLerpFactor);

			return vOrient.GetNormalizedFast();
		}
	}
};

struct SParticleVertexContext
{
	// Constant data.
	const ResourceParticleParams&
	            m_Params;
	SCameraInfo m_CamInfo;
	Vec3        m_vCamPos;
	Vec3        m_vRenderOffset;

	bool        m_bFrustumCull,                 // Culling data.
	             m_bOcclusionCull,
	             m_bComputeBoundingSphere;
	IVisArea* m_pClipVisArea;                   // Vis env to clip against, if needed.
	float     m_fClipWaterSense;                // Direction of water to clip against, else 0.
	Matrix34  m_matInvCamera;
	float     m_fProjectH, m_fProjectV;

	uint64    m_uVertexFlags;
	int       m_nMaxParticleVertices,
	          m_nMaxParticleIndices;
	UCol      m_TexInfo;                        // Default tex coords for vertex.

	float     m_fInvMinPix;
	float     m_fMaxPixFactor;
	float     m_fMinDrawAngle;
	float     m_fFillFactor;
	float     m_fFillMax;
	float     m_fFillFade;
	float     m_fDistFuncCoefs[3];              // Coefficients for alpha(dist^2) function.
	float     m_fMinAlphaMod;                   // Minimum alpha modulation (relative to effect max alpha) to render.
	Vec2      m_vTexAspect;                     // Multipliers for non-square textures (max is always 1).
	float     m_fPivotYScale;
	float     m_fAnimPosScale;                  // Multiplier for texture animation pos.

	// Modified data.
	float m_fPixelsProcessed;
	float m_fPixelsRendered;
	int   m_nParticlesCulled;
	int   m_nParticlesClipped;

	// Connected-particle info.
	int          m_nPrevSequence;
	int          m_nSequenceParticles;
	Vec3         m_vPrevPos;
	SVF_Particle m_PrevVert;

	SParticleVertexContext(CParticleContainer* pContainer, const SCameraInfo& camInfo, uint64 uVertexFlags = 0, float fMaxContainerPixels = fHUGE)
		: m_Params(pContainer->GetParams()), m_CamInfo(camInfo), m_vCamPos(camInfo.pCamera->GetPosition()), m_uVertexFlags(uVertexFlags)
	{
		Init(fMaxContainerPixels, pContainer);
	}

	void         Init(float fMaxContainerPixels, CParticleContainer* pContainer);

	inline float DistFunc(float fDist) const
	{
		return clamp_tpl(m_fDistFuncCoefs[0] + m_fDistFuncCoefs[1] * fDist + m_fDistFuncCoefs[2] * fDist * fDist, 0.f, 1.f);
	}
};

///////////////////////////////////////////////////////////////////////////////
// adapter class to vertices to VMEM code path, since regular writes to VMEM
// are slow, we collect them in a stack memory space and write chunks to VMEM

// Dependent types and functions.
static void CopyToVideoMemory(void* pDst, void* pSrc, uint32 nSize, uint32 nTransferID)
{
	assert(((uintptr_t)pDst & 127) == ((uintptr_t)pSrc & 127));
	memcpy(pDst, pSrc, nSize);
}

///////////////////////////////////////////////////////////////////////////////
template<class T, uint nBUFFER_SIZE, uint nCHUNK_SIZE, int nMEM_TRANSFER_ID>
struct SBufferedWriter
{
	SBufferedWriter(FixedDynArray<T>& aDestMem)
		: m_pDestMem(&aDestMem)
		, m_aDestMemBegin((char*)aDestMem.begin())
		, m_nFlushedBytes(0)
		, m_nWrittenDestBytes(0)
	{
		uint nAlignOffset = alias_cast<uint>(m_aDestMemBegin - (char*)m_aSrcBuffer) & 127;
		m_aSrcMemBegin = m_aSrcBuffer + nAlignOffset;
		int nCapacity = (sizeof(m_aSrcBuffer) - nAlignOffset) / sizeof(T);
		nCapacity = min(nCapacity, aDestMem.capacity());
		m_aSrcArray.set(ArrayT((T*)m_aSrcMemBegin, nCapacity));
	}

	~SBufferedWriter()
	{
		// Write remaining data.
		FlushData(UnflushedBytes());

		// Set final count of elems written to dest buffer.
		assert(m_nWrittenDestBytes % sizeof(T) == 0);
		m_pDestMem->resize(m_nWrittenDestBytes / sizeof(T), eNoInit);
	}

	ILINE FixedDynArray<T>& Array()
	{
		return m_aSrcArray;
	}

	ILINE T* CheckAvailable(int nElems)
	{
		// Transfer data chunks no longer needed
		uint nUnFlushedBytes = UnflushedBytes();
		if (nUnFlushedBytes >= nCHUNK_SIZE)
		{
			FlushData(nUnFlushedBytes & ~(nCHUNK_SIZE - 1));
		}

		// If buffer is too full for new elements, clear out flushed data.
		if (m_aSrcArray.available() < nElems)
		{
			WrapBuffer();
			if (m_aSrcArray.available() < nElems)
				return 0;
		}
		return m_aSrcArray.end();
	}

private:

	FixedDynArray<T>* m_pDestMem;                       // Destination array in VMEM.
	char*             m_aDestMemBegin;                  // Cached pointer to start of VMEM.
	uint              m_nFlushedBytes;                  // How much written from src buffer.
	uint              m_nWrittenDestBytes;              // Total written to dest buffer.
	FixedDynArray<T>  m_aSrcArray;                      // Array for writing, references following buffer:
	char*             m_aSrcMemBegin;                   // Aligned pointer into SrcBuffer;
	char              m_aSrcBuffer[nBUFFER_SIZE + 128]; // Raw buffer for writing.

	uint              UnflushedBytes() const
	{
		return check_cast<uint>((char*)m_aSrcArray.end() - m_aSrcMemBegin - m_nFlushedBytes);
	}

	void FlushData(uint nFlushBytes)
	{
		if (nFlushBytes)
		{
			assert(m_nWrittenDestBytes + nFlushBytes <= m_pDestMem->capacity() * sizeof(T));
			CopyToVideoMemory(m_aDestMemBegin + m_nWrittenDestBytes, m_aSrcMemBegin + m_nFlushedBytes, nFlushBytes, nMEM_TRANSFER_ID);
			m_nWrittenDestBytes += nFlushBytes;
			m_nFlushedBytes += nFlushBytes;
		}
	}

	void WrapBuffer()
	{
		// Copy unflushed bytes to the buffer beginning
		uint nUnFlushedBytes = UnflushedBytes();
		memcpy(m_aSrcMemBegin, m_aSrcMemBegin + m_nFlushedBytes, nUnFlushedBytes);
		uint nAvailableBytes = check_cast<uint>(m_aSrcBuffer + sizeof(m_aSrcBuffer) - m_aSrcMemBegin - nUnFlushedBytes);
		nAvailableBytes = min(nAvailableBytes, m_pDestMem->capacity() * (uint)sizeof(T) - m_nWrittenDestBytes - nUnFlushedBytes);
		m_aSrcArray.set(ArrayT((T*)(m_aSrcMemBegin + nUnFlushedBytes), int(nAvailableBytes / sizeof(T))));
		m_nFlushedBytes = 0;
	}

};

///////////////////////////////////////////////////////////////////////////////
struct CRY_ALIGN(128) SLocalRenderVertices
{
	SLocalRenderVertices(SRenderVertices & DestVerts)
		: aVertices(DestVerts.aVertices)
		  , aIndices(DestVerts.aIndices)
		  , nBaseVertexIndex(0)
	{
	}

	// Reserve space in SRenderVertices, flushing data if needed.
	bool CheckAvailable(int nVertices, int nIndices)
	{
		return aVertices.CheckAvailable(nVertices) && aIndices.CheckAvailable(nIndices);
	}

	ILINE static void DuplicateVertices(SVF_Particle* pDest, int nCount, const SVF_Particle& Src)
	{
		static const uint nStride = sizeof(SVF_Particle) / sizeof(uint);
		CryPrefetch(pDest + nCount);
		const uint* ps = reinterpret_cast<const uint*>(&Src);
		uint* pd = reinterpret_cast<uint*>(pDest);
		while (nCount-- > 0)
		{
			for (uint i = 0; i < nStride; i++)
				*pd++ = ps[i];
		}
	}

	ILINE void AddVertex(const SVF_Particle& vert)
	{
		aVertices.Array().push_back(vert);
	}
	ILINE void AddDualVertices(const SVF_Particle& vert)
	{
		// Expand in transverse axis.
		aVertices.Array().push_back(vert);
		aVertices.Array().push_back(vert)->st.x = 255;
	}

	ILINE static void SetQuadVertices(SVF_Particle aV[4])
	{
		aV[1].st.x = 255;
		aV[2].st.y = 255;
		aV[3].st.x = 255;
		aV[3].st.y = 255;
	}

	ILINE void ExpandQuadVertices()
	{
		SVF_Particle& vert = aVertices.Array().back();
		DuplicateVertices(aVertices.Array().append(3, eNoInit), 3, vert);
		SetQuadVertices(&vert);
	}

	ILINE static void SetOctVertices(SVF_Particle aV[8])
	{
		aV[0].st.x = 75;
		aV[1].st.x = 180;
		aV[2].st.x = 255;
		aV[2].st.y = 75;
		aV[3].st.x = 255;
		aV[3].st.y = 180;
		aV[4].st.x = 180;
		aV[4].st.y = 255;
		aV[5].st.x = 75;
		aV[5].st.y = 255;
		aV[6].st.y = 180;
		aV[7].st.y = 75;
	}

	ILINE void ExpandOctVertices()
	{
		SVF_Particle& vert = aVertices.Array().back();
		DuplicateVertices(aVertices.Array().append(7, eNoInit), 7, vert);
		SetOctVertices(&vert);
	}

	ILINE void AddQuadIndices(int nVertAdvance = 4, int bStrip = 0)
	{
		uint16* pIndices = aIndices.Array().append(bStrip ? 4 : 6, eNoInit);

		pIndices[0] = 0 + nBaseVertexIndex;
		pIndices[1] = 1 + nBaseVertexIndex;
		pIndices[2] = 2 + nBaseVertexIndex;
		pIndices[3] = 3 + nBaseVertexIndex;

		if (!bStrip)
		{
			pIndices[4] = 2 + nBaseVertexIndex;
			pIndices[5] = 1 + nBaseVertexIndex;
		}

		nBaseVertexIndex += nVertAdvance;
	}

	ILINE void AddOctIndices()
	{
		uint16* pIndices = aIndices.Array().append(18, eNoInit);

		pIndices[0] = 0 + nBaseVertexIndex;
		pIndices[1] = 1 + nBaseVertexIndex;
		pIndices[2] = 2 + nBaseVertexIndex;
		pIndices[3] = 0 + nBaseVertexIndex;
		pIndices[4] = 2 + nBaseVertexIndex;
		pIndices[5] = 4 + nBaseVertexIndex;
		pIndices[6] = 2 + nBaseVertexIndex;
		pIndices[7] = 3 + nBaseVertexIndex;
		pIndices[8] = 4 + nBaseVertexIndex;
		pIndices[9] = 0 + nBaseVertexIndex;
		pIndices[10] = 4 + nBaseVertexIndex;
		pIndices[11] = 6 + nBaseVertexIndex;
		pIndices[12] = 4 + nBaseVertexIndex;
		pIndices[13] = 5 + nBaseVertexIndex;
		pIndices[14] = 6 + nBaseVertexIndex;
		pIndices[15] = 6 + nBaseVertexIndex;
		pIndices[16] = 7 + nBaseVertexIndex;
		pIndices[17] = 0 + nBaseVertexIndex;

		nBaseVertexIndex += 8;
	}

	void AddPolyIndices(int nVerts, int bStrip = 0)
	{
		nVerts >>= 1;
		while (--nVerts > 0)
			AddQuadIndices(2, bStrip);

		// Final quad.
		nBaseVertexIndex += 2;
	}

private:

	// Individual buffers for vertices and indices
	SBufferedWriter<SVF_Particle, 16* 1024, 4* 1024, 5> aVertices;
	SBufferedWriter<uint16, 4* 1024, 1* 1024, 6> aIndices;
	int nBaseVertexIndex;

};

///////////////////////////////////////////////////////////////////////////////
//
// Vertex manipulation functions.
//

static ILINE void RotateAxes(Vec3& v0, Vec3& v1, const Vec2& vRot)
{
	Vec3 vXAxis = v0 * vRot.y - v1 * vRot.x;
	Vec3 vYAxis = v0 * vRot.x + v1 * vRot.y;
	v0 = vXAxis;
	v1 = vYAxis;
}

static ILINE void RotateAxes(Vec3& v0, Vec3& v1, const Vec3& v1Align)
{
	// Rotate within axis plane to align v1 to v1Align.
	RotateAxes(v0, v1, Vec2(v0 * v1Align, v1 * v1Align).GetNormalized());
}

static ILINE void ExpandAxis(Vec3& vAxis, const Vec3& vExpand, float fLengths)
{
	if (fLengths > FLT_MIN)
		vAxis += vExpand * ((vExpand | vAxis) / fLengths);
}

///////////////////////////////////////////////////////////////////////////////
bool CParticle::RenderGeometry(SRendParams& RenParamsShared, SParticleVertexContext& Context, const SRenderingPassInfo& passInfo) const
{
	// Render 3d object
	if (!m_pMeshObj)
		return false;

	ResourceParticleParams const& params = Context.m_Params;
	float fRelativeAge = GetRelativeAge();
	float fRadius = m_pMeshObj->GetRadius();

	SParticleRenderData RenderData;
	ComputeRenderData(RenderData, Context, fRadius);
	if (RenderData.fSize == 0.f)
		return false;

	// Color and alpha.
	ColorF cColor = params.cColor.GetValueFromMod(m_BaseMods.Color, fRelativeAge);
	cColor.a = ComputeRenderAlpha(RenderData, fRelativeAge, Context);
	if (cColor.a < Context.m_fMinAlphaMod)
		return false;

	cColor.a *= RenParamsShared.fAlpha * params.GetAlphaFromMod(cColor.a);

	if (passInfo.IsShadowPass())
	{
		// Shadow alpha (and color) not supported, scale size instead.
		RenderData.fSize *= cColor.a;
		cColor = Col_White;
	}

	// Get matrix.
	Matrix34 matPart;
	GetRenderMatrix(matPart, m_Loc, RenderData, Context);

	if ((params.eFacing == params.eFacing.Camera || params.eFacing == params.eFacing.CameraX) && GetEmitter())
	{
		// Re-apply parameter-derived rotation, using emitter orientation as the base
		matPart = matPart * Matrix33(GetSource().GetLocation().q.GetInverted() * m_Loc.q);
	}

	if (m_pMeshObj && params.IsGeometryCentered())
	{
		// Recenter object pre-rotation.
		Vec3 vCenter = m_pMeshObj->GetAABB().GetCenter();
		matPart.SetTranslation(matPart * -vCenter);
	}

	if (params.bDrawNear)
	{
		// DrawNear is now rendered in "Camera Space", particles aren't in "camera space" because there's no
		// clear relationship between them and the camera. They need to be rendered in the nearest pass to
		// avoid clipping, so remove camera pos here to put them into "camera space". Not ideal, but we merged
		// "camera space" and "Nearest" so don't have another option.
		matPart.AddTranslation(-Context.m_vCamPos);
	}

	// Render separate draw call.
	SaveRestore<ColorF> SaveColor(RenParamsShared.AmbientColor);
	SaveRestore<float> SaveAlpha(RenParamsShared.fAlpha);

	// Apply particle color to RenParams.Ambient, tho it's not quite the same thing.
	RenParamsShared.AmbientColor.r *= cColor.r;
	RenParamsShared.AmbientColor.g *= cColor.g;
	RenParamsShared.AmbientColor.b *= cColor.b;
	RenParamsShared.fAlpha = cColor.a;

	RenParamsShared.fDistance = max(sqrt_fast_tpl(RenderData.fDistCamSq) - RenderData.fSize * fRadius, 0.f);

	RenParamsShared.pMatrix = &matPart;

	RenParamsShared.pInstance = &non_const(*this);
	m_pMeshObj->Render(RenParamsShared, passInfo);

	return true;
}

void CParticleContainer::RenderGeometry(const SRendParams& RenParams, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_CONTAINER(this);

	assert(!NeedsUpdate());

	if (m_Particles.empty())
		return;

	const ResourceParticleParams& params = GetParams();
	SRendParams RenParamsGeom = RenParams;
	RenParamsGeom.dwFObjFlags |= FOB_TRANS_MASK;
	RenParamsGeom.pMaterial = params.pMaterial;
	if (params.bDrawNear)
	{
		RenParamsGeom.dwFObjFlags |= FOB_NEAREST;
		//RenParamsGeom.nRenderList = EFSLIST_POSTPROCESS;
	}
	RenParamsGeom.fDistance += params.fSortOffset;

	// Trigger transparency rendering if any particles can be transparent.
	if (!passInfo.IsShadowPass() && params.fAlpha(VMIN) < 1.f)
		RenParamsGeom.fAlpha *= 0.999f;

	float fEmissive = params.fEmissiveLighting;
	bool bHDRModeEnabled = false;
	GetRenderer()->EF_Query(EFQ_HDRModeEnabled, bHDRModeEnabled);
	if (bHDRModeEnabled)
		fEmissive *= HDRDynamicMultiplier; // Assuming HDRDynamic 1.0
	RenParamsGeom.AmbientColor *= params.fDiffuseLighting + fEmissive;

	// Set up shared and unique geom rendering.
	SParticleVertexContext Context(this, passInfo);

	m_Counts.components.rendered += 1.f;

	for (const auto& part : m_Particles)
	{
		m_Counts.particles.rendered += part.RenderGeometry(RenParamsGeom, Context, passInfo);
	}
}

void CParticle::GetTextureRect(RectF& rectTex, Vec3& vTexBlend) const
{
	ResourceParticleParams const& params = GetParams();

	rectTex.w = 1.f / params.TextureTiling.nTilesX;
	rectTex.h = 1.f / params.TextureTiling.nTilesY;

	vTexBlend.z = 0.f;
	float fFrame = m_nTileVariant;

	if (params.TextureTiling.nAnimFramesCount > 1)
	{
		float fAnimPos = params.TextureTiling.GetAnimPos(m_fAge, GetRelativeAge());
		fAnimPos *= params.TextureTiling.GetAnimPosScale();
		fFrame += fAnimPos;

		if (params.TextureTiling.bAnimBlend)
		{
			vTexBlend.z = fFrame - floor(fFrame);
			float fFrameBlend = floor(fFrame) + 1.f;
			if (fFrameBlend >= params.TextureTiling.nFirstTile + params.TextureTiling.nAnimFramesCount)
				fFrameBlend = params.TextureTiling.nFirstTile;

			vTexBlend.x = fFrameBlend * rectTex.w;
			vTexBlend.y = floor(vTexBlend.x) * rectTex.h;
			vTexBlend.x -= floor(vTexBlend.x);
		}

		fFrame = floor(fFrame);
	}

	rectTex.x = fFrame * rectTex.w;
	rectTex.y = floor(rectTex.x) * rectTex.h;
	rectTex.x -= floor(rectTex.x);
}

void CParticleContainer::RenderDecals(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_CONTAINER(this);

	assert(GetEnvironmentFlags() & REN_DECAL);

	ResourceParticleParams const& params = GetParams();

	SParticleVertexContext Context(this, passInfo);

	SDeferredDecal decal;
	decal.pMaterial = params.pMaterial;
	decal.nSortOrder = clamp_tpl(int(params.fSortOffset * 100.f), 0, 255);
	decal.nFlags = 0;

	for (const auto& part : m_Particles)
	{
		SParticleRenderData RenderData;

		part.ComputeRenderData(RenderData, Context);

		if (RenderData.fSize < FLT_EPSILON)
			continue;

		Vec3 av[4];
		part.GetRenderMatrix(av[0], av[1], av[2], av[3], part.GetLocation(), RenderData, Context);
		decal.projMatrix.SetFromVectors(av[0], av[2], av[1], av[3]);

		Vec3 vTexBlend;
		part.GetTextureRect(decal.rectTexture, vTexBlend);
		float fAlphaMod = part.GetAlphaMod();
		float fAlphaScale = params.GetAlphaFromMod(fAlphaMod);
		decal.fAlpha = fAlphaScale * (1.f - vTexBlend.z);
		decal.fGrowAlphaRef = div_min(params.AlphaClip.fSourceMin.Interp(fAlphaMod), fAlphaScale, 1.f);

		GetRenderer()->EF_AddDeferredDecal(decal, passInfo);

		if (vTexBlend.z > 0.f)
		{
			// Render decal for blended anim frame
			decal.rectTexture.x = vTexBlend.x;
			decal.rectTexture.y = vTexBlend.y;
			decal.fAlpha = fAlphaScale * vTexBlend.z;
			GetRenderer()->EF_AddDeferredDecal(decal, passInfo);
		}
	}
}

void CParticle::AddLight(const SRendParams& RenParams, const SRenderingPassInfo& passInfo) const
{
	if (!(GetCVars()->e_DynamicLights && !passInfo.IsRecursivePass()))
		return;

	ParticleParams const& params = GetParams();
	CCamera const& cam = passInfo.GetCamera();

	float fRelativeAge = GetRelativeAge();
	const float fLightIntensity = params.LightSource.fIntensity.GetValueFromMod(m_BaseMods.LightSourceIntensity, fRelativeAge);
	const float fLightRadius = params.LightSource.fRadius.GetValueFromMod(m_BaseMods.LightSourceRadius, fRelativeAge);

	SRenderLight dl;
	dl.SetRadius(fLightRadius);
	dl.SetLightColor(params.cColor.GetValueFromMod(m_BaseMods.Color, fRelativeAge) * Color3F(fLightIntensity));

	if (m_Loc.t.GetSquaredDistance(cam.GetPosition()) < sqr(dl.m_fRadius * GetFloatCVar(e_ParticlesLightsViewDistRatio))
	    && cam.IsSphereVisible_F(Sphere(m_Loc.t, dl.m_fRadius)))
	{
		// Deferred light.
		dl.SetPosition(m_Loc.t);
		dl.m_nStencilRef[0] = params.LightSource.bAffectsThisAreaOnly ? RenParams.nClipVolumeStencilRef : CClipVolumeManager::AffectsEverythingStencilRef;
		dl.m_nStencilRef[1] = CClipVolumeManager::InactiveVolumeStencilRef;
		dl.m_Flags |= DLF_DEFERRED_LIGHT;
		if (params.LightSource.eLightAffectsFog == params.LightSource.eLightAffectsFog.FogOnly)
			dl.m_Flags |= DLF_VOLUMETRIC_FOG_ONLY | DLF_VOLUMETRIC_FOG;
		else if (params.LightSource.eLightAffectsFog == params.LightSource.eLightAffectsFog.Both)
			dl.m_Flags |= DLF_VOLUMETRIC_FOG;

		Get3DEngine()->SetupLightScissors(&dl, passInfo);
		dl.m_n3DEngineUpdateFrameID = passInfo.GetMainFrameID();
		Get3DEngine()->AddLightToRenderer(dl, 1.f, passInfo);
	}
}

void CParticleContainer::RenderLights(const SRendParams& RenParams, const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_CONTAINER(this);

	assert(!NeedsUpdate());

	assert(GetEnvironmentFlags() & REN_LIGHTS);

	for (const auto& part : m_Particles)
	{
		part.AddLight(RenParams, passInfo);
	}
}

void CParticle::GetRenderMatrix(Vec3& vX, Vec3& vY, Vec3& vZ, Vec3& vT, const QuatTS& loc, const SParticleRenderData& RenderData, const SParticleVertexContext& Context) const
{
	ResourceParticleParams const& params = Context.m_Params;
	float fRelativeAge = GetRelativeAge();

	vT = loc.t + Context.m_vRenderOffset;

	ParticleParams::EFacing eFacing = params.eFacing == params.eFacing.Camera && params.GetTailSteps() ? params.eFacing.CameraX : +params.eFacing;
	if (eFacing == eFacing.Camera)
	{
		// Set axes to camera plane, keeping vX horizontal.
		vY = RenderData.GetDirCam(params.fPlaneAlignBlendDistance);
		vX = Vec3(vY.y, -vY.x + FLT_EPSILON, 0).GetNormalizedFast();
		vZ = vX ^ vY;

		if (params.bOrientToVelocity)
		{
			// Already rotated so Z is in velocity dir.
			Vec3 vVelDir = loc.q.GetColumn2();
			RotateAxes(vX, vZ, vVelDir);

			// Allow Z axis to project into screen.
			if (params.fTexAspect < 1.f)
			{
				float fProj = abs(vVelDir * vY);
				fProj = max(fProj, params.fTexAspect);
				vZ *= fProj;
			}
		}
	}
	else
	{
		vZ = loc.q.GetColumn2();
		if (eFacing == eFacing.CameraX)
			vX = (RenderData.vOffsetCam ^ vZ).GetNormalized();
		else
			vX = loc.q.GetColumn0();
		vY = vZ ^ vX;
	}

	if (m_fAngle != 0.f)
	{
		// Apply planar rotation.
		Vec2 vDir;
		sincos_tpl(m_fAngle, &vDir.x, &vDir.y);
		RotateAxes(vX, vZ, vDir);
	}

	// Scale axes by particle size, and texture aspect/thickness
	Vec3 vScale
	(
	  RenderData.fSize * Context.m_vTexAspect.x * (m_bFlippedTexture ? -1.f : 1.f)
	  * params.fAspect.GetValueFromMod(m_BaseMods.Aspect, fRelativeAge),
	  RenderData.fSize * params.fThickness,
	  RenderData.fSize * Context.m_vTexAspect.y
	);

	vX *= vScale.x;
	vY *= vScale.y;
	vZ *= vScale.z;

	// Stretch.
	if (params.fStretch && !params.GetTailSteps())
	{
		// Disallow stretching further back than starting position.
		float fStretch = params.fStretch.GetValueFromMod(m_BaseMods.StretchOrTail, fRelativeAge);
		if (fStretch * (1.f - params.fStretch.fOffsetRatio) > m_fAge)
			fStretch = m_fAge / (1.f - params.fStretch.fOffsetRatio);

		Vec3 vStretch = m_Vel.vLin * fStretch;
		fStretch = vStretch.GetLengthFast();
		ExpandAxis(vX, vStretch, fStretch * abs(vScale.x));
		ExpandAxis(vY, vStretch, fStretch * abs(vScale.y));
		ExpandAxis(vZ, vStretch, fStretch * abs(vScale.z));
		vT += vStretch * params.fStretch.fOffsetRatio;
	}

	if (params.fPivotX)
	{
		vT += vX * params.fPivotX.GetValueFromMod(m_BaseMods.PivotX, fRelativeAge);
	}
	if (params.fPivotY)
	{
		vT += vZ * (params.fPivotY.GetValueFromMod(m_BaseMods.PivotY, fRelativeAge) * Context.m_fPivotYScale);
	}
}

static void AlignVert(SVF_Particle& vert, const ParticleParams& params, const Vec3& vYAxis)
{
	// Rotate to match vYAxis exactly.
	float fYLen2 = vYAxis.GetLengthSquared();
	if (fYLen2 > FLT_MIN)
	{
		if (params.eFacing == params.eFacing.Camera)
		{
			Vec2 vProj(vert.xaxis * vYAxis, vert.yaxis * vYAxis);
			float fProjLen2 = vProj.GetLength2();
			if (fProjLen2 > FLT_MIN)
			{
				vProj *= isqrt_fast_tpl(fProjLen2);
				vert.xaxis = vert.xaxis * vProj.y - vert.yaxis * vProj.x;
			}
		}
		else
		{
			float fPrevYLen2 = vert.yaxis.GetLengthSquared();
			if (fPrevYLen2 > FLT_MIN)
			{
				Quat qAlign = Quat::CreateRotationV0V1(vert.yaxis * isqrt_fast_tpl(fPrevYLen2), vYAxis * isqrt_fast_tpl(fYLen2));
				vert.xaxis = qAlign * vert.xaxis;
			}
		}
		vert.yaxis = vYAxis;
	}
}

void FinishConnectedSequence(SLocalRenderVertices& alloc, SParticleVertexContext& Context)
{
	if (Context.m_nSequenceParticles++ > 0)
	{
		// End of connected polygon
		AlignVert(Context.m_PrevVert, Context.m_Params, (Context.m_PrevVert.xyz - Context.m_vPrevPos));
		Context.m_PrevVert.color.a = 0;
		alloc.AddDualVertices(Context.m_PrevVert);
		alloc.AddQuadIndices(4, Context.m_uVertexFlags & FOB_ALLOW_TESSELLATION);
	}
}

void CParticle::ComputeRenderData(SParticleRenderData& RenderData, const SParticleVertexContext& Context, float fObjectSize) const
{
	RenderData.vCamDir = Context.m_CamInfo.pCamera->GetViewdir();
	RenderData.vOffsetCam = m_Loc.t - Context.m_vCamPos;
	RenderData.fDistCamSq = RenderData.vOffsetCam.GetLengthSquared();

	RenderData.fSize = m_Loc.s;

	IF (Context.m_fMinDrawAngle != 0, false)
	{
		float fRadius = RenderData.fSize * fObjectSize;
		if (sqr(fRadius) < sqr(Context.m_fMinDrawAngle) * RenderData.fDistCamSq)
			RenderData.fSize = Context.m_fMinDrawAngle * sqrt_fast_tpl(RenderData.fDistCamSq) / fObjectSize;
	}
}

static const float fMIN_OCCLUSION_CULL_PIX = 64.f;

float CParticle::ComputeRenderAlpha(const SParticleRenderData& RenderData, float fRelativeAge, SParticleVertexContext& Context) const
{
	ResourceParticleParams const& params = Context.m_Params;

	Sphere sphere;
	if (Context.m_bComputeBoundingSphere)
	{
		sphere.center = m_Loc.t + Context.m_vRenderOffset;
		sphere.radius = RenderData.fSize * GetBaseRadius();

		if (m_aPosHistory && m_aPosHistory[0].IsUsed())
		{
			// Add oldest element.
			Vec3 vTailAxis = (m_aPosHistory[0].Loc.t - m_Loc.t) * 0.5f;
			sphere.center += vTailAxis;
			sphere.radius += vTailAxis.GetLengthFast();
		}
		else if (params.fStretch)
		{
			float fStretch = params.fStretch.GetValueFromMod(m_BaseMods.StretchOrTail, GetRelativeAge());
			float fStretchLen = fStretch * m_Vel.vLin.GetLengthFast();
			sphere.radius += fStretchLen * (1.f + abs(params.fStretch.fOffsetRatio));
		}
	}

	if (Context.m_bFrustumCull)
	{
		// Cull against view frustum
		Vec3 vPosCam = Context.m_matInvCamera * sphere.center;
		if (max(abs(vPosCam.x) - sphere.radius * Context.m_fProjectH, abs(vPosCam.z) - sphere.radius * Context.m_fProjectV) > vPosCam.y)
		{
			Context.m_nParticlesCulled++;
			return 0.f;
		}
	}

	// Area of particle in square radians.
	float fFillPix = div_min(square(RenderData.fSize) * Context.m_fFillFactor, RenderData.fDistCamSq, Context.m_fFillMax);

	if (Context.m_bOcclusionCull)
	{
		// Cull against the occlusion buffer
		if (fFillPix >= fMIN_OCCLUSION_CULL_PIX)
		{
			// Compute exact axes
			SVF_Particle Vert;
			SetVertexLocation(Vert, m_Loc, RenderData, Context);

			if (!GetObjManager()->CheckOcclusion_TestQuad(Vert.xyz, Vert.xaxis, Vert.yaxis))
			{
				Context.m_nParticlesCulled++;
				return 0.f;
			}
		}
	}

	// Get particle alpha, adjusted for screen fill limit.
	float fAlpha = params.fAlpha.GetValueFromBase(m_BaseMods.Alpha, fRelativeAge);

	// Fade near size limits.
	fAlpha *=
	  clamp_tpl(fFillPix * Context.m_fInvMinPix - 1.f, 0.f, 1.f) *
	  clamp_tpl(fFillPix * Context.m_fMaxPixFactor + 2.f, 0.f, 1.f) *
	  Context.DistFunc(RenderData.fDistCamSq);

	// fade out when angle to camera gets too steep
	if (params.eFacing != params.eFacing.Camera && params.fFadeAtViewCosAngle > 0)
	{
		const Vec3 vDist = m_Loc.t - Context.m_vCamPos;
		const Vec3 vNormal = GetNormal();
		const Vec3 vCamDir = vDist / sqrt_fast_tpl(RenderData.fDistCamSq);
		const float cosViewAngle = abs(vCamDir.Dot(vNormal));

		const float fFadeCoefficient = CLAMP(cosViewAngle / params.fFadeAtViewCosAngle, 0.f, 1.f);
		fAlpha *= powf(fFadeCoefficient, 8.f);
	}

	if (Context.m_fClipWaterSense != 0.f)
	{
		// Clip against water plane
		Plane pl;
		float fWaterDist = GetMain().GetPhysEnviron().GetWaterPlane(pl, sphere.center, sphere.radius) * Context.m_fClipWaterSense;
		if (Context.m_uVertexFlags & FOB_AFTER_WATER)
			fWaterDist += sphere.radius * 2.f;
		if (fWaterDist <= 0.f)
		{
			Context.m_nParticlesClipped++;
			return 0.f;
		}
		else if (fWaterDist < sphere.radius)
		{
			fAlpha *= fWaterDist / sphere.radius;
		}
	}

	// Clip against visibility areas
	IF (Context.m_pClipVisArea, false)
	{
		Vec3 vNormal;
		if (params.eFacing == params.eFacing.Camera)
			vNormal = RenderData.GetDirCam(params.fPlaneAlignBlendDistance);
		else
			vNormal = GetNormal();

		float fRadius = sphere.radius;
		sphere.radius += fRadius;
		if (GetMain().GetVisEnviron().ClipVisAreas(Context.m_pClipVisArea, sphere, vNormal))
		{
			sphere.radius -= fRadius;
			if (sphere.radius <= 0.f)
			{
				Context.m_nParticlesClipped++;
				return 0.f;
			}
			else
			{
				fAlpha *= sphere.radius / fRadius;
			}
		}
	}

	if (fAlpha < Context.m_fMinAlphaMod)
		return 0.f;

	// Track screen fill.
	fFillPix *= params.fFillRateCost;
	Context.m_fPixelsProcessed += fFillPix;

	// Cull nearest (latest) particles to enforce max screen fill.
	float fAdjust = 2.f - Context.m_fPixelsRendered * Context.m_fFillFade;
	if (fAdjust < 1.f)
	{
		fAlpha *= fAdjust;
		if (fAlpha < Context.m_fMinAlphaMod)
			return 0.f;
	}

	Context.m_fPixelsRendered += fFillPix;
	return fAlpha;
}

//////////////////////////////////////////////////////////////////////////
void CParticle::SetVertices(SLocalRenderVertices& alloc, SParticleVertexContext& Context, uint8 uAlpha) const
{
	ResourceParticleParams const& params = Context.m_Params;

	float fRelativeAge = GetRelativeAge();

	SParticleRenderData RenderData;
	ComputeRenderData(RenderData, Context);

	SVF_Particle BaseVert;

	// Get color, convert to 8-bit, and use pre-computed alpha.
	ColorF cColor = params.cColor.GetValueFromMod(m_BaseMods.Color, fRelativeAge);
	BaseVert.color.r = float_to_ufrac8(cColor.r);
	BaseVert.color.g = float_to_ufrac8(cColor.g);
	BaseVert.color.b = float_to_ufrac8(cColor.b);
	BaseVert.color.a = uAlpha;

	SetVertexLocation(BaseVert, m_Loc, RenderData, Context);

	// Texture info.
	BaseVert.st = Context.m_TexInfo;
	BaseVert.st.z = m_nTileVariant;
	if (params.TextureTiling.nAnimFramesCount > 1)
	{
		// Select anim frame based on particle or emitter age.
		float fAnimPos = params.Connection ?
		                 params.TextureTiling.GetAnimPos(GetEmitter()->GetAge(), GetEmitter()->GetRelativeAge()) :
		                 params.TextureTiling.GetAnimPos(m_fAge, fRelativeAge);

		// z = integer tile, w = blend fraction
		fAnimPos *= Context.m_fAnimPosScale;
		BaseVert.st.z += int(fAnimPos);
		BaseVert.st.w = float_to_ufrac8(fAnimPos - int(fAnimPos));
	}

	if (params.Connection)
	{
		// Fill in basic texture information.
		float fTexY;
		if (params.Connection.eTextureMapping == params.Connection.eTextureMapping.PerStream)
		{
			float fEmitterAge = GetEmitter()->GetAge();
			float fAgeAdjust = max(fEmitterAge - GetEmitter()->GetStopAge(), 0.f);
			fTexY = div_min(max(m_fAge, 0.f) - fAgeAdjust, min(m_fStopAge - fAgeAdjust, fEmitterAge - GetEmitter()->GetStartAge()), 1.f);
		}
		else
		{
			int nIndex = Context.m_nPrevSequence == GetEmitterSequence() ? Context.m_nSequenceParticles : -1;
			if (GetEmitter()->GetSequence() == GetEmitterSequence())
				nIndex -= GetEmitter()->GetEmitIndex() - 1;
			fTexY = float(nIndex) + params.fCount.GetMaxValue() - min(m_fAge, 0.f) / m_fStopAge * params.fCount.GetMaxValue();
		}

		fTexY = params.Connection.bTextureMirror ? fabs(fmod(fTexY, 2.f) - 1.f) : fmod(fTexY, 1.f);
		if (params.Connection.fTextureFrequency < 0.f)
			fTexY = 1.f - fTexY;
		BaseVert.st.y = float_to_ufrac8(fTexY);

		// Defer writing vertices one iteration.
		if (Context.m_nPrevSequence == GetEmitterSequence())
		{
			// Connected to previous particle. Write previous vertex, aligning to neighboring particles.
			if (Context.m_nSequenceParticles++ == 0)
				// Write first vertex
				AlignVert(Context.m_PrevVert, params, (BaseVert.xyz - Context.m_PrevVert.xyz));
			else
			{
				// Write subsequent vertex
				alloc.AddQuadIndices(2, Context.m_uVertexFlags & FOB_ALLOW_TESSELLATION);
				AlignVert(Context.m_PrevVert, params, (BaseVert.xyz - Context.m_vPrevPos) * 0.5f);
			}
			Context.m_PrevVert.color.a = BaseVert.color.a;
			alloc.AddDualVertices(Context.m_PrevVert);
			Context.m_vPrevPos = Context.m_PrevVert.xyz;
		}
		else
		{
			FinishConnectedSequence(alloc, Context);

			// Start of new polygon
			Context.m_nPrevSequence = GetEmitterSequence();
			Context.m_nSequenceParticles = 0;
		}

		Context.m_PrevVert = BaseVert;

		return;
	}

	if (m_aPosHistory)
	{
		return SetTailVertices(BaseVert, RenderData, alloc, Context);
	}

	// Store vertex
	alloc.AddVertex(BaseVert);

	if (!(Context.m_uVertexFlags & (FOB_POINT_SPRITE | FOB_ALLOW_TESSELLATION)))
	{
		// Expand vertices, create indices.
		if (Context.m_uVertexFlags & FOB_OCTAGONAL)
		{
			// Expand to 8 verts.
			alloc.ExpandOctVertices();
			alloc.AddOctIndices();
		}
		else
		{
			// Expand to 4 verts.
			alloc.ExpandQuadVertices();
			alloc.AddQuadIndices();
		}
	}
}

void CParticle::SetTailVertices(const SVF_Particle& BaseVert, SParticleRenderData RenderData, SLocalRenderVertices& alloc, SParticleVertexContext const& Context) const
{
	SVF_Particle TailVert = BaseVert;

	// Expand first segment backward.
	TailVert.xyz -= TailVert.yaxis;
	alloc.AddDualVertices(TailVert);

	TailVert.xyz = BaseVert.xyz;

	float fRelativeAge = GetRelativeAge();
	float fTailLength = min(Context.m_Params.fTailLength.GetValueFromMod(m_BaseMods.StretchOrTail, fRelativeAge), m_fAge);

	int nPos = GetContainer().GetHistorySteps() - 1;
	for (; nPos >= 0; nPos--)
	{
		if (m_aPosHistory[nPos].IsUsed())
			break;
	}
	if (nPos < 0 || fTailLength == 0.f)
	{
		// Return regular quad.
		TailVert.xyz += TailVert.yaxis;
		TailVert.st.y = 255;
		alloc.AddDualVertices(TailVert);
		alloc.AddQuadIndices(4, Context.m_uVertexFlags & FOB_ALLOW_TESSELLATION);
		return;
	}

	// Center of current particle. Estimate tex coord from number of active segments.
	const float fTexEnd = div_min(RenderData.fSize, m_Vel.vLin.GetLengthFast() * fTailLength + RenderData.fSize + RenderData.fSize, 0.5f);
	const float fTexTimeGradient = (1.f - fTexEnd - fTexEnd) / fTailLength;
	const float fYAxisScale = 0.5f / (RenderData.fSize * abs(Context.m_vTexAspect.y));

	TailVert.st.y = float_to_ufrac8(fTexEnd);
	alloc.AddDualVertices(TailVert);

	// Fill with past positions.
	Vec3 vPrevPos = m_Loc.t;
	int nVerts = 4;
	for (; nPos >= 0; nPos--)
	{
		QuatTS qtsLoc = m_aPosHistory[nPos].Loc;

		float fAgeDelta = m_fAge - m_aPosHistory[nPos].fAge;
		if (fAgeDelta > fTailLength)
		{
			// Interpolate oldest 2 locations if necessary.
			if (m_fAge - fTailLength > m_aPosHistory[nPos + 1].fAge)
				break;
			float fT = (m_fAge - fTailLength - m_aPosHistory[nPos].fAge) / (m_aPosHistory[nPos + 1].fAge - m_aPosHistory[nPos].fAge);
			if (fT > 0.f)
				qtsLoc.SetNLerp(qtsLoc, m_aPosHistory[nPos + 1].Loc, fT);
			fAgeDelta = fTailLength;
		}

		RenderData.vOffsetCam = qtsLoc.t - Context.m_vCamPos;
		RenderData.fDistCamSq = RenderData.vOffsetCam.GetLengthSquared();

		SetVertexLocation(TailVert, qtsLoc, RenderData, Context);

		if (nPos > 0)
		{
			float fLen = (m_aPosHistory[nPos - 1].Loc.t - vPrevPos).GetLengthFast();
			TailVert.yaxis *= fLen * fYAxisScale;
			vPrevPos = m_aPosHistory[nPos].Loc.t;
		}

		float fTexY = fTexEnd + fTexTimeGradient * fAgeDelta;
		TailVert.st.y = float_to_ufrac8(fTexY);
		alloc.AddDualVertices(TailVert);
		nVerts += 2;
	}

	// Half particle at tail end. Expand on y axis.
	TailVert.xyz += TailVert.yaxis;
	TailVert.st.y = 255;
	alloc.AddDualVertices(TailVert);
	nVerts += 2;

	// Create indices for variable-length particles.
	alloc.AddPolyIndices(nVerts, Context.m_uVertexFlags & FOB_ALLOW_TESSELLATION);
}

// Determine which particles are visible, save computed alphas in array for reuse, return required particle, vertex, and index count, update stats.
int CParticleContainer::CullParticles(SParticleVertexContext& Context, int& nVertices, int& nIndices, uint8 auParticleAlpha[])
{
	FUNCTION_PROFILER_CONTAINER(this);

	assert(!NeedsUpdate());

	int nParticlesRendered = 0;

	for (const auto& part : m_Particles)
	{
		// Get particle size and area.
		SParticleRenderData RenderData;
		part.ComputeRenderData(RenderData, Context);

		// Save final alpha value.
		float fAlpha = part.ComputeRenderAlpha(RenderData, part.GetRelativeAge(), Context);
		*auParticleAlpha = float_to_ufrac8(fAlpha);
		if (*auParticleAlpha + Context.m_Params.Connection)
			nParticlesRendered++;
		++auParticleAlpha;
	}

	nVertices = Context.m_nMaxParticleVertices * nParticlesRendered;
	nIndices = Context.m_nMaxParticleIndices * nParticlesRendered;

	m_Counts.particles.rendered += nParticlesRendered;
	m_Counts.components.rendered += 1.f * !!nParticlesRendered;
	m_Counts.pixels.updated += Context.m_fPixelsProcessed;
	m_Counts.pixels.rendered += Context.m_fPixelsRendered;
	m_Counts.particles.clip += (float)(Context.m_nParticlesClipped + Context.m_nParticlesCulled);

	return nParticlesRendered;
}

void CParticleContainer::WriteVerticesToVMEM(SParticleVertexContext& Context, SRenderVertices& RenderVerts, const uint8 auParticleAlpha[])
PREFAST_SUPPRESS_WARNING(6262)   // Function uses '33724' bytes of stack: exceeds /analyze:stacksize'16384'.
{
	FUNCTION_PROFILER_CONTAINER(this);

	assert(!NeedsUpdate());

	// Adapter to buffer writes to vmem.
	SLocalRenderVertices alloc(RenderVerts);
	uint8 uAlphaCullMask = Context.m_Params.Connection ? 1 : 0;

	for (const auto& part : m_Particles)
	{
		uint8 uAlpha = *auParticleAlpha++;
		if (!(uAlpha | uAlphaCullMask))
			// Culled from previous pass
			continue;

		if (!alloc.CheckAvailable(Context.m_nMaxParticleVertices, Context.m_nMaxParticleIndices))
			// Out of vertex/index memory.
			break;

		part.SetVertices(alloc, Context, uAlpha);
	}

	if (Context.m_Params.Connection)
	{
		if (alloc.CheckAvailable(Context.m_nMaxParticleVertices, Context.m_nMaxParticleIndices))
			FinishConnectedSequence(alloc, Context);
	}

	RenderVerts.fPixels = Context.m_fPixelsProcessed;
}

void SParticleVertexContext::Init(float fMaxContainerPixels, CParticleContainer* pContainer)
{
	ResourceParticleParams const& params = m_Params;
	CParticleEmitter const& emitterMain = pContainer->GetMain();
	CVars const& cvars = *emitterMain.GetCVars();

	m_TexInfo.dcolor = 0;

	// Compute max vertices and indices needed per particle.
	if (pContainer->GetHistorySteps())
	{
		m_nMaxParticleVertices = (pContainer->GetHistorySteps() + 3) * 2;
		m_nMaxParticleIndices = (m_nMaxParticleVertices - 2) * (m_uVertexFlags & FOB_ALLOW_TESSELLATION ? 2 : 3);
	}
	else if (params.Connection)
	{
		m_nMaxParticleVertices = 2;
		m_nMaxParticleIndices = m_uVertexFlags & FOB_ALLOW_TESSELLATION ? 4 : 6;
	}
	else if (m_uVertexFlags & (FOB_POINT_SPRITE | FOB_ALLOW_TESSELLATION))
	{
		m_nMaxParticleVertices = 1;
		m_nMaxParticleIndices = 0;
		if (m_uVertexFlags & FOB_OCTAGONAL)
			// Flag for instanced vertex shader
			m_TexInfo.x = 8;
	}
	else if (m_uVertexFlags & FOB_OCTAGONAL)
	{
		m_nMaxParticleVertices = 8;
		m_nMaxParticleIndices = 18;
	}
	else
	{
		m_nMaxParticleVertices = 4;
		m_nMaxParticleIndices = 6;
	}

	m_vRenderOffset = (emitterMain.GetLocation().t - m_vCamPos).GetNormalized() * params.fCameraDistanceOffset;
	m_vCamPos -= m_vRenderOffset;

	const float fAngularRes = m_CamInfo.pCamera->GetAngularResolution();
	m_fMaxPixFactor = params.nEnvFlags & REN_SPRITE ?
	                  -2.f * params.fFillRateCost / sqr(max(cvars.e_ParticlesMaxDrawScreen * fAngularRes, 1.f))
	                  : 0.f;
	m_fFillMax = (float) m_CamInfo.pCamera->GetViewSurfaceX() * m_CamInfo.pCamera->GetViewSurfaceZ();

	m_vTexAspect.x = m_vTexAspect.y = m_fPivotYScale = 1.f;
	if (!(params.nEnvFlags & REN_GEOMETRY))
	{
		m_vTexAspect.y = -1.f;
		if (params.fTexAspect < 1.f)
			m_vTexAspect.x = params.fTexAspect;
		else
			m_vTexAspect.y /= params.fTexAspect;
	}
	if (params.nEnvFlags & REN_SPRITE)
		m_fPivotYScale = -1.f;

	m_fFillFactor = 4.f * abs(m_vTexAspect.x * m_vTexAspect.y) * m_fFillMax;
	if (m_uVertexFlags & FOB_OCTAGONAL)
		m_fFillFactor *= 0.8285f;
	m_fFillFade = 2.f / fMaxContainerPixels;

	// Avoid division by zero fp exception
	if (abs(m_fFillFactor) > std::numeric_limits<float>::denorm_min())
		m_fInvMinPix = sqr(CParticleManager::Instance()->GetMaxAngularDensity(*m_CamInfo.pCamera) * emitterMain.GetViewDistRatioFloat() * params.fViewDistanceAdjust)
		/ m_fFillFactor;
	else
		m_fInvMinPix = std::numeric_limits<float>::infinity();

	m_fDistFuncCoefs[0] = 1.f;
	m_fDistFuncCoefs[1] = m_fDistFuncCoefs[2] = 0.f;

	// Minimum alpha mod value to render, based on param alpha scaling
	m_fMinAlphaMod = params.Connection ? 0.f : div_min(cvars.e_ParticlesMinDrawAlpha, params.fAlpha.GetMaxValue() * params.AlphaClip.fScale.Max, 1.f);

	// Size = Pix/2 * Dist / AngularRes
	const float fMinPixels = max(+params.fMinPixels, float(params.Connection) * 0.125f);
	m_fMinDrawAngle = fMinPixels * 0.5f / fAngularRes;

	// Zoom factor is angular res ratio between render and main camera.
	float fZoom = 1.f / m_CamInfo.pCamera->GetFov();
	if (params.fCameraMaxDistance > 0.f)
	{
		float fNear = sqr(params.fCameraMinDistance * fZoom);
		float fFar = sqr(params.fCameraMaxDistance * fZoom);
		float fBorder = (fFar - fNear) * 0.1f;
		if (fNear == 0.f)
			// No border on near side.
			fNear = -fBorder;

		/*	f(x) = (1 - ((x-C)/R)^2) / s		; C = (N+F)/2, R = (F-N)/2
		      = (-NF + (N+F) x - x^2) / s
		    f(N+B) = 1
		    s = B(F-N-B)
		 */
		float fS = 1.f / (fBorder * (fFar - fNear - fBorder));
		m_fDistFuncCoefs[0] = -fNear * fFar * fS;
		m_fDistFuncCoefs[1] = (fNear + fFar) * fS;
		m_fDistFuncCoefs[2] = -fS;
	}
	else if (params.fCameraMinDistance > 0.f)
	{
		float fBorder = params.fCameraMinDistance * fZoom * 0.25f;
		m_fDistFuncCoefs[1] = 1.f / fBorder;
		m_fDistFuncCoefs[0] = -4.f;
	}

	m_fAnimPosScale = params.TextureTiling.GetAnimPosScale();

	// Culling context.
	m_bFrustumCull = cvars.e_ParticlesCullAgainstViewFrustum
	                 && (params.nEnvFlags & REN_SPRITE) && !params.Connection;
	m_bOcclusionCull = cvars.e_ParticlesCullAgainstOcclusionBuffer
	                   && (params.nEnvFlags & REN_SPRITE) && !params.Connection && !pContainer->GetHistorySteps()
	                   && !params.bDrawNear && !params.bDrawOnTop && !emitterMain.GetSpawnParams().bNowhere;

	if (m_bFrustumCull)
	{
		m_matInvCamera = m_CamInfo.pCamera->GetViewMatrix();
		Vec3 vFrust = m_CamInfo.pCamera->GetEdgeP();
		float fInvX = abs(1.f / vFrust.x),
		      fInvZ = abs(1.f / vFrust.z);

		non_const(m_matInvCamera.GetRow4(0)) *= vFrust.y * fInvX;
		non_const(m_matInvCamera.GetRow4(2)) *= vFrust.y * fInvZ;

		m_fProjectH = sqrt_tpl(sqr(vFrust.x) + sqr(vFrust.y)) * fInvX;
		m_fProjectV = sqrt_tpl(sqr(vFrust.z) + sqr(vFrust.y)) * fInvZ;
	}

	m_fClipWaterSense = 0.f;
	const bool bAllowClip = !emitterMain.GetSpawnParams().bIgnoreVisAreas;
	const SPhysEnviron& PhysEnv = emitterMain.GetPhysEnviron();
	if (bAllowClip && params.eFacing != params.eFacing.Water && PhysEnv.m_tUnderWater == ETrinary())
	{
		if (params.nEnvFlags & REN_SPRITE)
			m_fClipWaterSense = ((m_CamInfo.bCameraUnderwater ^ m_CamInfo.bRecursivePass) == !!(m_uVertexFlags & FOB_AFTER_WATER)) ? -1.f : 1.f;
		else if (params.nEnvFlags & REN_GEOMETRY)
			m_fClipWaterSense = params.tVisibleUnderwater == ETrinary(false) ? 1.f : params.tVisibleUnderwater == ETrinary(true) ? -1.f : 0.f;
	}

	// Vis env to clip against, if needed.
	m_pClipVisArea = bAllowClip ? emitterMain.GetVisEnviron().GetClipVisArea(m_CamInfo.pCameraVisArea, pContainer->GetBounds()) : 0;

	m_bComputeBoundingSphere = m_bFrustumCull || m_fClipWaterSense != 0.f || m_pClipVisArea;

	m_fPixelsProcessed = m_fPixelsRendered = 0.f;
	m_nParticlesCulled = m_nParticlesClipped = 0;

	// Init connected-particle context
	m_nPrevSequence = -1;
	m_nSequenceParticles = 0;
	m_PrevVert.xyz = m_vPrevPos = emitterMain.GetLocation().t;
}

void CParticleContainer::ComputeVertices(const SCameraInfo& camInfo, CREParticle* pRE, uint64 uVertexFlags, float fMaxPixels)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	PARTICLE_LIGHT_PROFILER();

	// Update, if not yet done.
	assert(GetEnvironmentFlags() & REN_SPRITE);
	assert(!m_pParams->pStatObj);
	assert(!NeedsUpdate());

	if (!m_Particles.empty())
	{
		SParticleVertexContext Context(this, camInfo, uVertexFlags, fMaxPixels);

		// Culling stage
		int nVertices = 0, nIndices = 0;
		STACK_ARRAY(uint8, auParticleAlpha, m_Particles.size());

		if (CullParticles(Context, nVertices, nIndices, auParticleAlpha))
		{
			// Rendering stage
			SRenderVertices* pRenderVerts = pRE->AllocVertices(nVertices, nIndices);
			assert(pRenderVerts);
			if (pRenderVerts->aVertices.capacity() < nVertices || pRenderVerts->aIndices.capacity() < nIndices)
				CParticleManager::Instance()->MarkAsOutOfMemory();
			WriteVerticesToVMEM(Context, *pRenderVerts, auParticleAlpha);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
struct SVertexIndexPoolUsageCmp
{
	// sort that the bigger entry is always at positon 0
	ILINE bool operator()(const SVertexIndexPoolUsage& rA, const SVertexIndexPoolUsage& rB) const
	{
		return rA.nVertexMemory > rB.nVertexMemory;
	}
};

void CParticleManager::AddVertexIndexPoolUsageEntry(uint32 nVertexMemory, uint32 nIndexMemory, const char* pContainerName)
{
#if defined(PARTICLE_COLLECT_VERT_IND_POOL_USAGE)
	SVertexIndexPoolUsage arrPoolUsage[nVertexIndexPoolUsageEntries];
	memcpy(arrPoolUsage, m_arrVertexIndexPoolUsage, sizeof(arrPoolUsage));
	if (nVertexMemory > arrPoolUsage[nVertexIndexPoolUsageEntries - 1].nVertexMemory)
	{
		// for SPUs simulation time, we need to generate a stack copy of the list to sort

		SVertexIndexPoolUsage newEntry = { nVertexMemory, nIndexMemory, pContainerName };
		arrPoolUsage[nVertexIndexPoolUsageEntries - 1] = newEntry;
		// sort to move the biggest elements to the front, which has the nice side effect that empty
		// elements with a nVertexMemory of 0 are moved to the back, thus the algorithm handles
		// removing of empty and smallest elements in the same way
		std::sort(arrPoolUsage, arrPoolUsage + nVertexIndexPoolUsageEntries, SVertexIndexPoolUsageCmp());
	}
	memcpy(m_arrVertexIndexPoolUsage, arrPoolUsage, sizeof(arrPoolUsage));
	m_nRequieredVertexPoolMemory += nVertexMemory;
	m_nRequieredIndexPoolMemory += nIndexMemory;
	m_nRendererParticleContainer += 1;
#endif
}

void CParticleManager::MarkAsOutOfMemory()
{
#if defined(PARTICLE_COLLECT_VERT_IND_POOL_USAGE)
	m_bOutOfVertexIndexPoolMemory = true;
#endif
}
