// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Area.h"

// forward declarations.
struct SEntityEvent;

//////////////////////////////////////////////////////////////////////////
// Description:
//    Handles sounds in the entity.
//////////////////////////////////////////////////////////////////////////
struct CEntityComponentArea : public IEntityAreaComponent
{
	CRY_ENTITY_COMPONENT_CLASS_GUID(CEntityComponentArea, IEntityAreaComponent, "CEntityComponentArea", "feb82854-291c-4aba-9652-ddd7f403f24a"_cry_guid);

	CEntityComponentArea();
	virtual ~CEntityComponentArea();

public:
	static void ResetTempState();

public:
	//////////////////////////////////////////////////////////////////////////
	// IEntityComponent interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void   Initialize() override;
	virtual void   ProcessEvent(const SEntityEvent& event) override;
	virtual uint64 GetEventMask() const final;
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IEntityComponent interface implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual EEntityProxy GetProxyType() const override { return ENTITY_PROXY_AREA; }
	virtual void         Release() final               { delete this; };
	virtual void         LegacySerializeXML(XmlNodeRef& entityNode, XmlNodeRef& componentNode, bool bLoading) override;
	virtual void         GameSerialize(TSerialize ser) override;
	virtual bool         NeedGameSerialize() override { return false; }
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// IEntityAreaComponent interface.
	//////////////////////////////////////////////////////////////////////////
	virtual void            SetFlags(int nAreaProxyFlags) override { m_nFlags = nAreaProxyFlags; }
	virtual int             GetFlags() override                    { return m_nFlags; }

	virtual EEntityAreaType GetAreaType() const override           { return m_pArea->GetAreaType(); }
	virtual IArea*          GetArea() const override               { return m_pArea; }

	virtual void            SetPoints(Vec3 const* const pPoints, bool const* const pSoundObstructionSegments, size_t const numLocalPoints, bool const bClosed, float const height) override;
	virtual void            SetBox(const Vec3& min, const Vec3& max, const bool* const pabSoundObstructionSides, size_t const nSideCount) override;
	virtual void            SetSphere(const Vec3& center, float fRadius) override;

	virtual void            BeginSettingSolid(const Matrix34& worldTM) override;
	virtual void            AddConvexHullToSolid(const Vec3* verticesOfConvexHull, bool bObstruction, int numberOfVertices) override;
	virtual void            EndSettingSolid() override;

	virtual int             GetPointsCount() override                         { return m_localPoints.size(); }
	virtual const Vec3*     GetPoints() override;
	virtual void            SetHeight(float const value) override             { m_pArea->SetHeight(value); }
	virtual float           GetHeight() const override                        { return m_pArea->GetHeight(); }
	virtual void            GetBox(Vec3& min, Vec3& max) override             { m_pArea->GetBox(min, max); }
	virtual void            GetSphere(Vec3& vCenter, float& fRadius) override { m_pArea->GetSphere(vCenter, fRadius); }

	virtual void            SetGravityVolume(const Vec3* pPoints, int nNumPoints, float fRadius, float fGravity, bool bDontDisableInvisible, float fFalloff, float fDamping) override;

	virtual void            SetID(const int id) override              { m_pArea->SetID(id); }
	virtual int             GetID() const override                    { return m_pArea->GetID(); }
	virtual void            SetGroup(const int id) override           { m_pArea->SetGroup(id); }
	virtual int             GetGroup() const override                 { return m_pArea->GetGroup(); }
	virtual void            SetPriority(const int nPriority) override { m_pArea->SetPriority(nPriority); }
	virtual int             GetPriority() const override              { return m_pArea->GetPriority(); }

	virtual void            SetSoundObstructionOnAreaFace(size_t const index, bool const bObstructs) override;

	virtual void            AddEntity(EntityId id) override                                                                                 { m_pArea->AddEntity(id); }
	virtual void            AddEntity(EntityGUID guid) override                                                                             { m_pArea->AddEntity(guid); }
	virtual void            RemoveEntity(EntityId const id) override                                                                        { m_pArea->RemoveEntity(id); }
	virtual void            RemoveEntity(EntityGUID const guid) override                                                                    { m_pArea->RemoveEntity(guid); }
	virtual void            RemoveEntities() override                                                                                       { m_pArea->RemoveEntities(); }

	virtual void            SetProximity(float prx) override                                                                                { m_pArea->SetProximity(prx); }
	virtual float           GetProximity() override                                                                                         { return m_pArea->GetProximity(); }

	virtual float           ClosestPointOnHullDistSq(EntityId const nEntityID, Vec3 const& Point3d, Vec3& OnHull3d) override                { return m_pArea->ClosestPointOnHullDistSq(nEntityID, Point3d, OnHull3d, false); }
	virtual float           CalcPointNearDistSq(EntityId const nEntityID, Vec3 const& Point3d, Vec3& OnHull3d) override                     { return m_pArea->CalcPointNearDistSq(nEntityID, Point3d, OnHull3d, false); }
	virtual bool            CalcPointWithin(EntityId const nEntityID, Vec3 const& Point3d, bool const bIgnoreHeight = false) const override { return m_pArea->CalcPointWithin(nEntityID, Point3d, bIgnoreHeight); }

	virtual size_t          GetNumberOfEntitiesInArea() const override;
	virtual EntityId        GetEntityInAreaByIdx(size_t const index) const override;

	virtual float           GetInnerFadeDistance() const override               { return m_pArea->GetInnerFadeDistance(); }
	virtual void            SetInnerFadeDistance(float const distance) override { m_pArea->SetInnerFadeDistance(distance); }

	virtual void            GetMemoryUsage(ICrySizer* pSizer) const override
	{
		SIZER_COMPONENT_NAME(pSizer, "CAreaProxy");
		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddContainer(m_localPoints);
		pSizer->AddContainer(m_bezierPoints);
		pSizer->AddContainer(m_bezierPointsTmp);
		if (m_pArea)
			m_pArea->GetMemoryUsage(pSizer);
	}
private:
	void OnMove();
	void Reset();

	void OnEnable(bool bIsEnable, bool bIsCallScript = true);

	void ReadPolygonsForAreaSolid(CCryFile& file, int numberOfPolygons, bool bObstruction);

private:
	static std::vector<Vec3> s_tmpWorldPoints;

private:
	int m_nFlags;

	typedef std::vector<bool> tSoundObstruction;

	// Managed area.
	CArea*                                   m_pArea;
	std::vector<Vec3>                        m_localPoints;
	tSoundObstruction                        m_abObstructSound; // Each bool flag per point defines if segment to next point is obstructed
	Vec3                                     m_vCenter;
	float                                    m_fRadius;
	float                                    m_fGravity;
	float                                    m_fFalloff;
	float                                    m_fDamping;
	bool                                     m_bDontDisableInvisible;

	pe_params_area                           m_gravityParams;

	std::vector<Vec3>                        m_bezierPoints;
	std::vector<Vec3>                        m_bezierPointsTmp;
	SEntityPhysicalizeParams::AreaDefinition m_areaDefinition;
	bool                                     m_bIsEnable;
	bool                                     m_bIsEnableInternal;
	float                                    m_lastFrameTime;
};
