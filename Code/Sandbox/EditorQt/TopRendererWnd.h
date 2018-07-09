// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(AFX_TOPRENDERERWND_H__5E850B78_3791_40B5_BD0A_850682E8CAF9__INCLUDED_)
#define AFX_TOPRENDERERWND_H__5E850B78_3791_40B5_BD0A_850682E8CAF9__INCLUDED_

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

#include "2DViewport.h"

class CPopupMenuItem;

// Predeclare because of friend declaration
class CTopRendererWnd : public C2DViewport
{
	DECLARE_DYNCREATE(CTopRendererWnd)
public:
	CTopRendererWnd();
	virtual ~CTopRendererWnd();

	/** Get type of this viewport.
	 */
	virtual EViewportType GetType() const { return ET_ViewportMap; }
	virtual void          SetType(EViewportType type);

	virtual void          ResetContent();
	virtual void          UpdateContent(int flags);
	void                  InitHeightmapAlignment();

	//! Map viewport position to world space position.
	virtual Vec3 ViewToWorld(CPoint vp, bool* collideWithTerrain = 0, bool onlyTerrain = false, bool bSkipVegetation = false, bool bTestRenderMesh = false);

	void         SerializeDisplayOptions(Serialization::IArchive& ar);

	void         SetShowWater(bool bShow) { m_bShowWater = bShow; }
	bool         GetShowWater() const     { return m_bShowWater; };

protected:
	void ResetSurfaceTexture();
	void UpdateSurfaceTexture(int flags);

	// Draw everything.
	virtual void Draw(CObjectRenderHelper& objRenderHelper);

	void         DrawStaticObjects();

private:
	bool m_bContentsUpdated;

	//CImage* m_pSurfaceTexture;
	int   m_terrainTextureId;

	CSize m_textureSize;

	// Size of heightmap in meters.
	CSize    m_heightmapSize;

	CImageEx m_terrainTexture;

	CImageEx m_vegetationTexture;
	CPoint   m_vegetationTexturePos;
	CSize    m_vegetationTextureSize;
	int      m_vegetationTextureId;
	bool     m_bFirstTerrainUpdate;

public:
	// Display options.
	bool m_bDisplayLabels;
	bool m_bShowHeightmap;
	bool m_bLastShowHeightmapState;
	bool m_bShowStatObjects;
	bool m_bShowWater;
};

#endif // !defined(AFX_TOPRENDERERWND_H__5E850B78_3791_40B5_BD0A_850682E8CAF9__INCLUDED_)

