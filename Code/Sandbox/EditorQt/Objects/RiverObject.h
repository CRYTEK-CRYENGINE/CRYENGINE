// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "RoadObject.h"

/*!
 *	CRiverObject used to create rivers.
 *
 */
class CRiverObject : public CRoadObject
{
public:
	DECLARE_DYNCREATE(CRiverObject)

	CRiverObject();

	virtual void       Serialize(CObjectArchive& ar);
	virtual XmlNodeRef Export(const string& levelPath, XmlNodeRef& xmlNode);

	virtual void       SetMaterial(IEditorMaterial* mtl);

	virtual void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

protected:
	virtual bool Init(CBaseObject* prev, const string& file);
	virtual void InitVariables();
	virtual void UpdateSectors();
	virtual void UpdateSector(CRoadSector& sector);

	virtual void OnRiverParamChange(IVariable* var);
	virtual void OnRiverPhysicsParamChange(IVariable* var);

	Vec3         GetPremulFogColor() const;

	void         Physicalize();

protected:
	CVariable<float> mv_waterVolumeDepth;
	CVariable<float> mv_waterStreamSpeed;
	CVariable<float> mv_waterFogDensity;
	CVariable<Vec3>  mv_waterFogColor;
	CVariable<float> mv_waterFogColorMultiplier;
	CVariable<bool>  mv_waterFogColorAffectedBySun;
	CVariable<float> mv_waterFogShadowing;
	CVariable<float> mv_waterSurfaceUScale;
	CVariable<float> mv_waterSurfaceVScale;
	CVariable<bool>  mv_waterCapFogAtVolumeDepth;
	CVariable<bool>  mv_waterCaustics;
	CVariable<float> mv_waterCausticIntensity;
	CVariable<float> mv_waterCausticTiling;
	CVariable<float> mv_waterCausticHeight;

	uint64           m_waterVolumeID;
	Plane            m_fogPlane;
};

