// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "RoadObject.h"

/*!
 *	CWaterWaveObject used to create shore waves
 *
 */

class CWaterWaveObject : public CRoadObject
{
public:
	DECLARE_DYNCREATE(CWaterWaveObject)

	CWaterWaveObject();

	virtual void       Serialize(CObjectArchive& ar);
	virtual XmlNodeRef Export(const string& levelPath, XmlNodeRef& xmlNode);

	//! CRoadObject override: Needed for snapping correctly to water plane - If there's better solution feel free to change
	virtual void SetPoint(int index, const Vec3& pos);
	//! CRoadObject override: Needed for snapping correctly to water plane - If there's better solution feel free to change
	virtual int  MouseCreateCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int flags);

	virtual void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

protected:
	//! CRoadObject override: Needed for snapping correctly to water plane - If there's better solution feel free to change
	Vec3         MapViewToCP(CViewport* view, CPoint point);

	virtual bool Init(CBaseObject* prev, const string& file);
	virtual void InitVariables();
	virtual void UpdateSectors();

	virtual void OnWaterWaveParamChange(IVariable* var);
	virtual void OnWaterWavePhysicsParamChange(IVariable* var);

	void         Physicalize();
	bool         IsValidSector(const CRoadSector& pSector);

	//////////////////////////////////////////////////////////////////////////
	// Overrides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////

	virtual void Done();

protected:

	uint64           m_nID;

	CVariable<float> mv_surfaceUScale;
	CVariable<float> mv_surfaceVScale;

	CVariable<float> mv_speed;
	CVariable<float> mv_speedVar;

	CVariable<float> mv_lifetime;
	CVariable<float> mv_lifetimeVar;

	CVariable<float> mv_height;
	CVariable<float> mv_heightVar;

	CVariable<float> mv_posVar;

	IRenderNode*     m_pRenderNode;
};

