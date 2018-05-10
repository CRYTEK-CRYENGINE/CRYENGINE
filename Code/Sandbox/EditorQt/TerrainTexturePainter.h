// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __terraintexturepainter_h__
#define __terraintexturepainter_h__

#if _MSC_VER > 1000
	#pragma once
#endif

class CHeightmap;
class CLayer;

/** Terrain Texture brush types.
 */
enum ETextureBrushType
{
	ET_BRUSH_PAINT = 1,
};

/** Terrain texture brush.
 */
struct CTextureBrush
{
	ETextureBrushType type;                 // Type of this brush.
	float             radius;               // Radius of brush in meters
	float             hardness;             // How hard this brush.
	float             value;                //
	bool              bErase;               //

	bool              bDetailLayer;         // apply changes to the detail layer data as well
	bool              bMask_Altitude;       //
	bool              bMaskByLayerSettings; //

	float             minRadius;            //
	float             maxRadius;            //
	ColorF            m_cFilterColor;       //
	uint32            m_dwMaskLayerId;      // 0xffffffff if not used

	CTextureBrush()
	{
		type = ET_BRUSH_PAINT;
		radius = 4;
		hardness = 1.0f;
		value = 255;
		bErase = false;
		minRadius = 0.01f;
		maxRadius = 256.0f;
		bDetailLayer = true;
		bMaskByLayerSettings = true;
		m_dwMaskLayerId = 0xffffffff;
		m_cFilterColor = ColorF(1, 1, 1);
	}
};

//////////////////////////////////////////////////////////////////////////
class CTerrainTexturePainter : public CEditTool
{
	DECLARE_DYNCREATE(CTerrainTexturePainter)
public:
	CTerrainTexturePainter();
	virtual ~CTerrainTexturePainter();

	virtual string GetDisplayName() const override { return "Paint Texture"; }

	virtual void   Display(DisplayContext& dc);

	// Ovverides from CEditTool
	bool MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags);

	// Key down.
	bool OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags);
	bool OnKeyUp(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags) { return false; };

	// Delete itself.
	void DeleteThis()                         { delete this; };

	void SetBrush(const CTextureBrush& brush) { m_brush = brush; };
	void GetBrush(CTextureBrush& brush) const { brush = m_brush; };
	void SelectLayer(const CLayer* pLayer);
	void ReloadLayers();

	void Action_Flood();
	void Action_Paint();
	void Action_PickLayerId();
	void Action_FixMaterialCount();

	//Undo
	void         Action_StartUndo();
	void         Action_CollectUndo(float x, float y, float radius);
	void         Action_StopUndo();

	static void  Command_Activate();

	virtual void Serialize(Serialization::IArchive& ar) override;

private:
	bool    CheckSectorPalette(CLayer* pLayer, const Vec3& center);
	void    PaintLayer(CLayer* pLayer, const Vec3& center, bool bFlood);
	CLayer* GetSelectedLayer() const;

	Vec3   m_pointerPos;
	QPoint m_lastMousePoint;

	//! Flag is true if painting mode. Used for Undo.
	bool                   m_bIsPainting;

	struct CUndoTPElement* m_pTPElem;

	// Cache often used interfaces.
	I3DEngine*           m_3DEngine;
	IRenderer*           m_renderer;
	CHeightmap*          m_heightmap;

	static CTextureBrush m_brush;

	friend class CUndoTexturePainter;
};

#endif // __terraintexturepainter_h__

