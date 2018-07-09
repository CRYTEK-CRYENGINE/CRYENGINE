// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditTool.h"
#include <CryMath/Cry_Vector2.h>

enum BrushType
{
	eBrushFlatten = 1,
	eBrushSmooth,
	eBrushRiseLower,
	eBrushPickHeight,
	eBrushTypeLast
};

struct CTerrainBrush
{
	// Type of this brush.
	BrushType type;
	//! Outside Radius of brush.
	float     radius;
	//! Inside Radius of brush.
	float     radiusInside;
	//! Height where to paint.
	float     height;
	//! Min and Max height of the brush.
	Vec2      heightRange;
	//! Height that will be reised/lowered
	float     riseHeight;
	//! How hard this brush.
	float     hardness;
	//! Is this brush have displacement map.
	bool      bDisplacement;
	//! Scale of applied displacement.
	float     displacementScale;
	//! True if objects should be repositioned on modified terrain.
	bool      bRepositionObjects;
	//! True if vegetation should be repositioned on modified terrain.
	bool      bRepositionVegetation;
	//! Displacement map name
	string		displacementMapName;
	//! Displacement map data
	CImageEx	displacementMapData;

	CTerrainBrush()
	{
		type = eBrushFlatten;
		radius = 2;
		radiusInside = 0;       // default 0 is preferred by designers
		height = 1;
		riseHeight = 1.0f;
		bDisplacement = false;
		hardness = 0.2f;
		displacementScale = 50;
		bRepositionObjects = false;
		bRepositionVegetation = true;
		heightRange = Vec2(0, 0);
	}
};

class CBrushTool : public CEditTool
{
public:
	enum EPaintMode
	{
		ePaintMode_None = 0,
		ePaintMode_Ready,
		ePaintMode_InProgress,
	};

	DECLARE_DYNAMIC(CBrushTool)

	CBrushTool();
	virtual ~CBrushTool() override;

	virtual bool MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags) override;
	virtual bool OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags) override;
	virtual void DeleteThis()                                 { delete this; };
	virtual void SetUserData(const char* key, void* userData) { m_pBrush = (CTerrainBrush*)userData; }
 
protected:
	void           SetBrushRadius(float inner, float outer);
	void           SetRiseHeight(float height);
	void           SetHeight(float height);
	virtual bool   PickHeight(QPoint point)                                                             { return false; }
	void           DrawTool(DisplayContext& dc, bool innerCircle, bool line, bool terrainCircle);
	virtual void   PaintTerrain(CHeightmap* pHeightmap, int tx, int ty, int tsize, float fInsideRadius, float brushSpeed) {}
	bool           getShareBrushParams()                                                                { return s_shareBrushParameters; }
	void           setShareBrushParams(bool share)                                                      { s_shareBrushParameters = share; signalPropertiesChanged(this); }
	CTerrainBrush& getBrush()                                                                           { return s_shareBrushParameters && m_pBrush ? *m_pBrush : m_brush; }

private:
	void Paint();
	void ApplyObjectsReposition();

protected:
	static Vec3    s_pointerPos;
	static Vec3    s_pointerPosPrev;
	static QPoint  s_lastMousePoint;
	bool           m_bPickHeight;
	CTerrainBrush* m_pBrush;
	std::map<CBaseObject*, float> m_objElevations;
	AABB m_vegBox = AABB::RESET;

private:
	static bool   s_shareBrushParameters;
	CTerrainBrush m_brush;
	EPaintMode    m_nPaintingMode;
};

class CTerrainTool : public CBrushTool
{
public:
	DECLARE_DYNAMIC(CTerrainTool)

	inline CTerrainTool() {}
	virtual ~CTerrainTool() override {}

	virtual void DeleteThis() { delete this; };
};

class CFlattenTool : public CTerrainTool
{
public:
	DECLARE_DYNCREATE(CFlattenTool)

	virtual string GetDisplayName() const override { return "Flatten Terrain"; }
	virtual void   Serialize(Serialization::IArchive& ar) override;
	virtual void   Display(DisplayContext& dc)     { DrawTool(dc, true, true, true); }
	virtual void   PaintTerrain(CHeightmap* pHeightmap, int tx, int ty, int tsize, float fInsideRadius, float brushSpeed) override;
	virtual bool   PickHeight(QPoint point) override;
};

class CSmoothTool : public CTerrainTool
{
public:
	CSmoothTool() : m_pPrevToolClass(nullptr) {}

	DECLARE_DYNCREATE(CSmoothTool)

	virtual bool   OnKeyUp(CViewport* view, uint32 key, uint32 nRepCnt, uint32 nFlags) override;
	virtual string GetDisplayName() const override                 { return "Smooth Terrain"; }
	virtual void   Serialize(Serialization::IArchive& ar) override;
	virtual void   Display(DisplayContext& dc)                     { DrawTool(dc, false, false, false); }
	virtual void   PaintTerrain(CHeightmap* pHeightmap, int tx, int ty, int tsize, float fInsideRadius, float brushSpeed) override;
	void           SetPrevToolClass(CRuntimeClass* pPrevToolClass) { m_pPrevToolClass = pPrevToolClass; }
	CRuntimeClass* GetPrevToolClass() const                        { return m_pPrevToolClass; }

private:
	CRuntimeClass* m_pPrevToolClass;
};

class CRiseLowerTool : public CTerrainTool, public IAutoEditorNotifyListener
{
public:
	CRiseLowerTool() : m_lowerTool(false) {}

	DECLARE_DYNCREATE(CRiseLowerTool)

	virtual bool MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags) override;
	virtual bool   OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags) override;
	virtual bool   OnKeyUp(CViewport* view, uint32 key, uint32 nRepCnt, uint32 nFlags) override;
	virtual string GetDisplayName() const override { return "Raise/Lower Terrain"; }
	virtual void   Serialize(Serialization::IArchive& ar) override;
	virtual void   Display(DisplayContext& dc)     { DrawTool(dc, true, false, false); }
	virtual void   PaintTerrain(CHeightmap* pHeightmap, int tx, int ty, int tsize, float fInsideRadius, float brushSpeed) override;
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event) override;

private:
	bool m_lowerTool;
};

class CHolesTool : public CTerrainTool
{
public:
	DECLARE_DYNAMIC(CHolesTool)

	virtual string GetDisplayName() const override { return "Terrain Holes"; }
	virtual void   Serialize(Serialization::IArchive& ar) override;
	virtual void   Display(DisplayContext& dc) override;
	virtual bool   MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags) override;

protected:
	bool CalculateHoleArea(const Vec3& pointer, float radius, Vec2i& min, Vec2i& max);

	ColorF m_toolColor;
};

class CMakeHolesTool : public CHolesTool
{
public:
	DECLARE_DYNCREATE(CMakeHolesTool)

	virtual string GetDisplayName() const override { return "Make Terrain Holes"; }
	CMakeHolesTool() { m_toolColor = ColorF(1, 0, 0, 1); }

	virtual void PaintTerrain(CHeightmap* pHeightmap, int tx, int ty, int tsize, float fInsideRadius, float brushSpeed) override;
};

class CFillHolesTool : public CHolesTool
{
public:
	DECLARE_DYNCREATE(CFillHolesTool)

	virtual string GetDisplayName() const override { return "Fill Terrain Holes"; }
	CFillHolesTool() { m_toolColor = ColorF(0, 0, 1, 1); }

	virtual void PaintTerrain(CHeightmap* pHeightmap, int tx, int ty, int tsize, float fInsideRadius, float brushSpeed) override;
};

