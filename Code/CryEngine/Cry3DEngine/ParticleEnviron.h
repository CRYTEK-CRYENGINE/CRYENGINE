// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef PARTICLE_ENVIRON_H
#define PARTICLE_ENVIRON_H

#include <CryParticleSystem/ParticleParams.h>
#include "ParticleEffect.h"
#include <CryCore/Containers/CryPtrArray.h>

class CParticleEmitter;

//////////////////////////////////////////////////////////////////////////
// Physical environment management.
//
struct SPhysEnviron : Cry3DEngineBase
{
	// PhysArea caching.
	SPhysForces m_UniformForces;
	ETrinary    m_tUnderWater;
	uint32      m_nNonUniformFlags;         // EParticleEnviron flags of non-uniform areas found.
	uint32      m_nNonCachedFlags;          // Env flags of areas requiring physics system access.

	// Nonuniform area support.
	struct SArea
	{
		const SPhysEnviron*
		              m_pEnviron;               // Parent environment.
		_smart_ptr<IPhysicalEntity>
		              m_pArea;
		volatile int* m_pLock;                  // Copy of lock for area.

		SPhysForces   m_Forces;
		AABB          m_bbArea;                 // Bounds of area, for quick checks.
		uint32        m_nFlags;
		bool          m_bOutdoorOnly;           // Force only for outdoor areas.

		// Area params, for simple evaluation.
		bool     m_bCacheForce;                 // Quick cached force info.
		bool     m_bRadial;                     // Forces are radial from center.
		uint8    m_nGeomShape;                  // GEOM_BOX, GEOM_SPHERE, ...
		Vec3     m_vCenter;                     // Area position (for radial forces).
		Matrix33 m_matToLocal;                  // Convert to unit sphere space.
		float    m_fFalloffScale;               // For scaling to inner/outer force bounds.

		SArea()
		{ ZeroStruct(*this); }
		void  GetForces(SPhysForces& forces, Vec3 const& vPos, uint32 nFlags) const;
		void  GetForcesPhys(SPhysForces& forces, Vec3 const& vPos) const;
		float GetWaterPlane(Plane& plane, Vec3 const& vPos, float fMaxDist = -WATER_LEVEL_UNKNOWN) const;
		void  GetMemoryUsage(ICrySizer* pSizer) const
		{}

		void AddRef()
		{
			m_nRefCount++;
		}
		void Release()
		{
			assert(m_nRefCount >= 0);
			if (--m_nRefCount == 0)
				delete this;
		}

	private:
		int m_nRefCount;
	};

	SPhysEnviron()
	{
		Clear();
	}

	// Phys areas
	void Clear();
	void FreeMemory();

	// Query world phys areas.
	void GetWorldPhysAreas(uint32 nFlags = ~0, bool bNonUniformAreas = true);

	// Query subset of phys areas.
	void GetPhysAreas(SPhysEnviron const& envSource, AABB const& box, bool bIndoors, uint32 nFlags = ~0, bool bNonUniformAreas = true, const CParticleEmitter* pEmitterSkip = 0);

	bool IsCurrent() const
	{
		return (m_nNonUniformFlags & EFF_LOADED) != 0;
	}

	void OnPhysAreaChange(const EventPhysAreaChange& event);

	void LockAreas(uint32 nFlags, int iLock) const
	{
		if (m_nNonCachedFlags & nFlags)
		{
			for (auto& area : m_NonUniformAreas)
			{
				if (area.m_nFlags & nFlags)
					if (!area.m_bCacheForce)
						CryInterlockedAdd(area.m_pLock, iLock);
			}
		}
	}

	void GetForces(SPhysForces& forces, Vec3 const& vPos, uint32 nFlags) const
	{
		forces = m_UniformForces;
		if (m_nNonUniformFlags & nFlags & ENV_PHYS_AREA)
			GetNonUniformForces(forces, vPos, nFlags);
	}

	float GetWaterPlane(Plane& plWater, Vec3 const& vPos, float fMaxDist = -WATER_LEVEL_UNKNOWN) const
	{
		plWater = m_UniformForces.plWater;
		float fDist = m_UniformForces.plWater.DistFromPlane(vPos);
		if (fDist > 0.f)
		{
			if (m_nNonUniformFlags & ENV_WATER)
				fDist = GetNonUniformWaterPlane(plWater, vPos, min(fDist, fMaxDist));
		}
		return fDist;
	}

	// Phys collision
	static bool PhysicsCollision(ray_hit& hit, Vec3 const& vStart, Vec3 const& vEnd, float fRadius, uint32 nEnvFlags, IPhysicalEntity* pThisEntity = 0);

	void        GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddContainer(m_NonUniformAreas);
	}

protected:

	SmartPtrArray<SArea> m_NonUniformAreas;

	void  GetNonUniformForces(SPhysForces& forces, Vec3 const& vPos, uint32 nFlags) const;
	float GetNonUniformWaterPlane(Plane& plWater, Vec3 const& vPos, float fMaxDist = -WATER_LEVEL_UNKNOWN) const;
};

//////////////////////////////////////////////////////////////////////////
// Vis area management.
//
struct SVisEnviron : Cry3DEngineBase
{
	SVisEnviron()
	{
		Clear();
	}

	void Clear()
	{
		memset(this, 0, sizeof(*this));
	}

	void Invalidate()
	{
		m_bValid = false;
	}

	void      Update(Vec3 const& vOrigin, AABB const& bbExtents);

	IVisArea* GetClipVisArea(IVisArea* pVisAreaCam, AABB const& bb) const;

	bool      ClipVisAreas(IVisArea* pClipVisArea, Sphere& sphere, Vec3 const& vNormal) const
	{
		// Clip inside or outside specified area.
		return pClipVisArea->ClipToVisArea(pClipVisArea == m_pVisArea, sphere, vNormal);
	}

	bool OriginIndoors() const
	{
		return m_pVisArea != 0;
	}

	void OnVisAreaDeleted(IVisArea* pVisArea)
	{
		if (m_pVisArea == pVisArea)
			Clear();
	}

protected:
	bool      m_bValid;                     // True if VisArea determination up to date.
	bool      m_bCrossesVisArea;
	IVisArea* m_pVisArea;                   // VisArea emitter is in, if needed and if any.
	void*     m_pVisNodeCache;
};

#endif
