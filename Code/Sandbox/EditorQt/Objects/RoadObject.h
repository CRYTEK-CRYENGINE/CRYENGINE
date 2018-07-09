// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SplineObject.h"

// Road Sector
struct CRoadSector
{
	std::vector<Vec3> points;
	IRenderNode*      m_pRoadSector;
	float             t0, t1;

	CRoadSector()
	{
		m_pRoadSector = 0;
	}
	~CRoadSector()
	{
		Release();
	}

	CRoadSector(const CRoadSector& old)
	{
		points = old.points;
		t0 = old.t0;
		t1 = old.t1;

		m_pRoadSector = 0;
	}

	void Release();
};

typedef std::vector<CRoadSector> CRoadSectorVector;

/*!
 *	CRoadObject is an object that represent named 3d position in world.
 *
 */
class CRoadObject : public CSplineObject
{
protected:
	CRoadObject();

public:
	DECLARE_DYNCREATE(CRoadObject)

	//////////////////////////////////////////////////////////////////////////
	// Ovverides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	void InitVariables();
	void Done();

	void Display(CObjectRenderHelper& objRenderHelper);
	void DrawRoadObject(DisplayContext& dc, COLORREF col);
	void DrawSectorLines(DisplayContext& dc);

	//////////////////////////////////////////////////////////////////////////

	void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

	void         SetHidden(bool bHidden);
	void         UpdateVisibility(bool visible);

	virtual void RegisterOnEngine() override;
	virtual void UnRegisterFromEngine() override;
	virtual void UpdateHighlightPassState(bool bSelected, bool bHighlighted) override;

	void         Serialize(CObjectArchive& ar);
	XmlNodeRef   Export(const string& levelPath, XmlNodeRef& xmlNode);

	virtual void SetMaterial(IEditorMaterial* pMaterial);
	virtual void SetMaterialLayersMask(uint32 nLayersMask);
	virtual void SetMinSpec(uint32 nSpec, bool bSetChildren = true);

	void         SetSelected(bool bSelect);
	//////////////////////////////////////////////////////////////////////////

	void         SetRoadSectors();
	int          GetRoadSectorCount(int index);

	void         AlignHeightMap();
	void         EraseVegetation();

	virtual void OnEvent(ObjectEvent event);

	// from CSplineObject
	void  OnUpdate() override;
	void  SetLayerId(uint16 nLayerId) override;
	void  SetPhysics(bool isPhysics) override;
	float GetAngleRange() const override { return 25.0f; }

protected:
	// from CSplineObject
	float        GetWidth() const override    { return mv_width; }
	float        GetStepSize() const override { return mv_step; }

	virtual void UpdateSectors();

	// Ignore default draw highlight.
	void  DrawHighlight(DisplayContext& dc) {};

	float GetLocalWidth(int index, float t);

	//overrided from CBaseObject.
	void InvalidateTM(int nWhyFlags);

	//! Called when Road variable changes.
	void OnParamChange(IVariable* var);

	void DeleteThis() { delete this; };

	void InitBaseVariables();

protected:
	CRoadSectorVector m_sectors;

	CVariable<float>  mv_width;
	CVariable<float>  mv_borderWidth;
	CVariable<float>  mv_eraseVegWidth;
	CVariable<float>  mv_eraseVegWidthVar;
	CVariable<float>  mv_step;
	CVariable<float>  mv_tileLength;
	CVariable<int>    mv_ratioViewDist;
	CVariable<int>    mv_sortPrio;
	CVariable<bool>   m_ignoreTerrainHoles;
	CVariable<bool>   m_physicalize;

	//! Forces Road to be always 2D. (all vertices lie on XY plane).
	bool m_bIgnoreParamUpdate;
	bool m_bNeedUpdateSectors;
};

