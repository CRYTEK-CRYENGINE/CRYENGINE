// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ShapeObject.h"

struct IWaterVolumeRenderNode;

class CWaterShapeObject : public CShapeObject
{
	DECLARE_DYNCREATE(CWaterShapeObject)
public:
	CWaterShapeObject();

	//////////////////////////////////////////////////////////////////////////
	// Overrides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////

	virtual void       SetName(const string& name);
	virtual void       SetMaterial(IEditorMaterial* mtl);
	virtual void       Serialize(CObjectArchive& ar);
	virtual XmlNodeRef Export(const string& levelPath, XmlNodeRef& xmlNode);
	virtual void       SetMinSpec(uint32 nSpec, bool bSetChildren = true);
	virtual void       SetMaterialLayersMask(uint32 nLayersMask);
	//void Display( DisplayContext& dc );
	virtual void       Validate()
	{
		CBaseObject::Validate();
	}
	virtual void         SetHidden(bool bHidden);
	virtual void         UpdateVisibility(bool visible);

	virtual IRenderNode* GetEngineNode() const { return m_pWVRN; }

	virtual void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

protected:
	virtual bool Init(CBaseObject* prev, const string& file);
	virtual void InitVariables();
	virtual bool CreateGameObject();
	virtual void PostLoad(CObjectArchive& ar);
	virtual void Done();
	virtual void UpdateGameArea();

	virtual void OnParamChange(IVariable* var);
	virtual void OnWaterParamChange(IVariable* var);
	virtual void OnWaterPhysicsParamChange(IVariable* var);

	Vec3         GetPremulFogColor() const;
	void         Physicalize();

protected:
	CVariable<float>        mv_waterVolumeDepth;
	CVariable<float>        mv_waterStreamSpeed;
	CVariable<float>        mv_waterFogDensity;
	CVariable<Vec3>         mv_waterFogColor;
	CVariable<float>        mv_waterFogColorMultiplier;
	CVariable<bool>         mv_waterFogColorAffectedBySun;
	CVariable<float>        mv_waterFogShadowing;
	CVariable<float>        mv_waterSurfaceUScale;
	CVariable<float>        mv_waterSurfaceVScale;
	CVariable<bool>         mv_waterCapFogAtVolumeDepth;
	CVariable<int>          mv_ratioViewDist;
	CVariable<bool>         mv_waterCaustics;
	CVariable<float>        mv_waterCausticIntensity;
	CVariable<float>        mv_waterCausticTiling;
	CVariable<float>        mv_waterCausticHeight;

	CSmartVariableArray     mv_GroupAdv;
	CVariable<float>        mv_waterVolume;
	CVariable<float>        mv_accVolume;
	CVariable<float>        mv_borderPad;
	CVariable<bool>         mv_convexBorder;
	CVariable<float>        mv_objVolThresh;
	CVariable<float>        mv_waterCell;
	CVariable<float>        mv_waveSpeed;
	CVariable<float>        mv_waveDamping;
	CVariable<float>        mv_waveTimestep;
	CVariable<float>        mv_simAreaGrowth;
	CVariable<float>        mv_minVel;
	CVariable<float>        mv_simDepth;
	CVariable<float>        mv_velResistance;
	CVariable<float>        mv_hlimit;

	IWaterVolumeRenderNode* m_pWVRN;

	uint64                  m_waterVolumeID;
	Plane                   m_fogPlane;
};

/*!
 * Class Description of ShapeObject.
 */
class CWaterShapeObjectClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_VOLUME; };
	const char*    ClassName()         { return "WaterVolume"; };
	const char*    Category()          { return "Area"; };
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CWaterShapeObject); };
};

