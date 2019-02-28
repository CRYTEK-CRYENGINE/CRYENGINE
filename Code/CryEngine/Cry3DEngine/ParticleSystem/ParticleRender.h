// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ParticleSystem/ParticleFeature.h"

namespace pfx2
{

const uint vertexBufferSize = 16 * 1024;
const uint vertexChunckSize = 4 * 1024;

template<class T, uint bufferSize, uint chunckSize>
class CWriteCombinedBuffer
{
public:
	CWriteCombinedBuffer(FixedDynArray<T>& destMem);
	~CWriteCombinedBuffer();
	FixedDynArray<T>& Array();
	T*                CheckAvailable(int elems);

private:
	uint UnflushedBytes() const;
	void FlushData(uint flushBytes);
	void WrapBuffer();

	FixedDynArray<T>* m_pDestMem;                                             // Destination array in VMEM.
	byte*             m_pDestMemBegin;                                        // Cached pointer to start of VMEM.
	uint              m_flushedBytes;                                         // How much written from src buffer.
	uint              m_writtenDestBytes;                                     // Total written to dest buffer.
	FixedDynArray<T>  m_srcArray;                                             // Array for writing, references following buffer:
	byte*             m_pSrcMemBegin;                                         // Aligned pointer into SrcBuffer;
	byte              m_srcBuffer[bufferSize + CRY_PFX2_PARTICLES_ALIGNMENT]; // Raw buffer for writing.
};

class CParticleRenderBase : public CParticleFeature, public Cry3DEngineBase
{
public:
	virtual EFeatureType GetFeatureType() override;
	virtual void         AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override;
	virtual void         Render(CParticleComponentRuntime& runtime, const SRenderContext& renderContext) override;

protected:
	ILINE C3DEngine* Get3DEngine() const          { return static_cast<C3DEngine*>(gEnv->p3DEngine); }
	virtual bool     SupportsWaterCulling() const { return false; }
	void             PrepareRenderObject(const CParticleComponentRuntime& runtime, uint renderObjectId, uint threadId, uint64 objFlags);
	void             AddRenderObject(CParticleComponentRuntime& runtime, const SRenderContext& renderContext, uint renderObjectId, uint threadId, uint64 objFlags);

	// Frustum culling support
	struct SFrustumTest
	{
		const CCamera& camera;
		Vec3 camPos;
		Matrix34 invViewTM;
		float projectH;
		float projectV;
		bool  doTest;

		SFrustumTest(const CCamera& camera, const AABB& bounds)
			: camera(camera)
			, camPos(camera.GetPosition())
			, doTest(camera.IsAABBVisible_FH(bounds) != CULL_INCLUSION)
		{
			if (doTest)
			{
				invViewTM = camera.GetViewMatrix();
				Vec3 frustum = camera.GetEdgeP();
				float invX = abs(1.0f / frustum.x);
				float invZ = abs(1.0f / frustum.z);

				non_const(invViewTM.GetRow4(0)) *= frustum.y * invX;
				non_const(invViewTM.GetRow4(2)) *= frustum.y * invZ;

				projectH = sqrt(sqr(frustum.x) + sqr(frustum.y)) * invX;
				projectV = sqrt(sqr(frustum.z) + sqr(frustum.y)) * invZ;
			}
		}

		ILINE bool IsVisible(const Vec3& pos, float size) const
		{
			if (!doTest)
				return true;
			const Vec3 posCam = invViewTM * pos;
			const float x0 = max( abs(posCam.x) - size * projectH, abs(posCam.z) - size * projectV);
			return x0 < posCam.y;
		}
		ILINE float VisibleArea(Vec3 pos0, float size0, Vec3 pos1, float size1, float maxArea) const
		{
			if (doTest)
			{
				if (!ClipSegment(pos0, size0, pos1))
					return 0.0f;
			}

			const float len = (pos1 - pos0).GetLengthFast() + size0;
			const float distSq0 = (pos0 - camPos).GetLengthSquared();
			const float distSq1 = (pos1 - camPos).GetLengthSquared();
			const float area = div_min(
				len * (size0 * distSq1 + size1 * distSq0), 
				distSq0 * distSq1,
				maxArea);
			return area;
		}

	private:
		bool ClipSegment(Vec3& pos0, float size0, Vec3& pos1) const
		{
			// Clip to 4 frustum planes
			for (int p = FR_PLANE_RIGHT; p <= FR_PLANE_BOTTOM; ++p)
			{
				const Plane& plane = *camera.GetFrustumPlane(p);
				float d0 = (plane | pos0) + size0;
				float d1 = plane | pos1;

				if (d0 * d1 < 0.0f) // crosses plane
				{
					const float invLen = rcp_fast(d1 - d0);
					if (d0 > 0.0f) // clip pos0
						pos0 += (pos0 - pos1) * (d0 * invLen);
					else // clip pos1
						pos1 += (pos0 - pos1) * (d1 * invLen);
				}
				else if (d0 > 0.0f) // both outside
					return false;
			}
			return true;
		}
	};

	static float CullArea(float area, float areaLimit, TParticleIdArray& ids, TVarArray<float> alphas, TConstArray<float> areas);

private:
	uint m_renderObjectBeforeWaterId = -1;
	uint m_renderObjectAfterWaterId  = -1;
	bool m_waterCulling              = false;
};

}

#include "ParticleRenderImpl.h"
