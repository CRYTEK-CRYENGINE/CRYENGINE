// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "TopRendererWnd.h"
#include ".\Terrain\Heightmap.h"
#include "Vegetation/VegetationMap.h"
#include "ViewManager.h"
#include <Preferences/ViewportPreferences.h>

#include ".\Terrain\TerrainTexGen.h"
#include "Controls/DynamicPopupMenu.h"

// Size of the surface texture
#define SURFACE_TEXTURE_WIDTH 512

#define MARKER_SIZE           6.0f
#define MARKER_DIR_SIZE       10.0f
#define SELECTION_RADIUS      30.0f

#define GL_RGBA               0x1908
#define GL_BGRA               0x80E1

// Used to give each static object type a different color
static uint32 sVegetationColors[16] =
{
	0xFFFF0000,
	0xFF00FF00,
	0xFF0000FF,
	0xFFFFFFFF,
	0xFFFF00FF,
	0xFFFFFF00,
	0xFF00FFFF,
	0xFF7F00FF,
	0xFF7FFF7F,
	0xFFFF7F00,
	0xFF00FF7F,
	0xFF7F7F7F,
	0xFFFF0000,
	0xFF00FF00,
	0xFF0000FF,
	0xFFFFFFFF,
};

IMPLEMENT_DYNCREATE(CTopRendererWnd, C2DViewport)

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CTopRendererWnd::CTopRendererWnd()
{
	////////////////////////////////////////////////////////////////////////
	// Set the window type member of the base class to the correct type and
	// create the initial surface texture
	////////////////////////////////////////////////////////////////////////

	if (gViewportPreferences.mapViewportSwapXY)
		SetAxis(VPA_YX);
	else
		SetAxis(VPA_XY);

	m_bShowHeightmap = false;
	m_bShowStatObjects = false;
	m_bLastShowHeightmapState = false;

	////////////////////////////////////////////////////////////////////////
	// Surface texture
	////////////////////////////////////////////////////////////////////////

	m_textureSize.cx = gViewportPreferences.mapViewportResolution;
	m_textureSize.cy = gViewportPreferences.mapViewportResolution;

	m_heightmapSize = CSize(1, 1);

	m_terrainTextureId = 0;

	m_vegetationTextureId = 0;
	m_bFirstTerrainUpdate = true;
	m_bShowWater = false;

	// Create a new surface texture image
	ResetSurfaceTexture();

	m_bContentsUpdated = false;

	m_gridAlpha = 0.3f;
	m_colorGridText = RGB(255, 255, 255);
	m_colorAxisText = RGB(255, 255, 255);
	m_colorBackground = RGB(128, 128, 128);
}

//////////////////////////////////////////////////////////////////////////
CTopRendererWnd::~CTopRendererWnd()
{
	////////////////////////////////////////////////////////////////////////
	// Destroy the attached render and free the surface texture
	////////////////////////////////////////////////////////////////////////
}

//////////////////////////////////////////////////////////////////////////
void CTopRendererWnd::SetType(EViewportType type)
{
	m_viewType = type;
	m_axis = VPA_YX;
	SetAxis(m_axis);
}

//////////////////////////////////////////////////////////////////////////
void CTopRendererWnd::ResetContent()
{
	C2DViewport::ResetContent();
	ResetSurfaceTexture();

	// Reset texture ids.
	m_terrainTextureId = 0;
	m_vegetationTextureId = 0;
}

//////////////////////////////////////////////////////////////////////////
void CTopRendererWnd::UpdateContent(int flags)
{
	if (gViewportPreferences.mapViewportSwapXY)
		SetAxis(VPA_YX);
	else
		SetAxis(VPA_XY);

	C2DViewport::UpdateContent(flags);
	if (!GetIEditorImpl()->GetDocument())
		return;

	CHeightmap* heightmap = GetIEditorImpl()->GetHeightmap();
	if (!heightmap)
		return;

	if (!IsWindowVisible())
		return;

	m_heightmapSize.cx = heightmap->GetWidth() * heightmap->GetUnitSize();
	m_heightmapSize.cy = heightmap->GetHeight() * heightmap->GetUnitSize();

	UpdateSurfaceTexture(flags);
	m_bContentsUpdated = true;

	// if first update.
	if (m_bFirstTerrainUpdate)
	{
		InitHeightmapAlignment();
	}
	m_bFirstTerrainUpdate = false;
}

//////////////////////////////////////////////////////////////////////////
void CTopRendererWnd::InitHeightmapAlignment()
{
	CHeightmap* heightmap = GetIEditorImpl()->GetHeightmap();
	if (heightmap)
	{
		SSectorInfo si;
		heightmap->GetSectorsInfo(si);
		float sizeMeters = si.numSectors * si.sectorSize;
		float mid = sizeMeters / 2;
		//SetScrollOffset( 0,-sizeMeters );

		SetZoom(0.95f * m_rcClient.Width() / sizeMeters, CPoint(m_rcClient.Width() / 2, m_rcClient.Height() / 2));
		SetScrollOffset(-10, -10);
	}
}

//////////////////////////////////////////////////////////////////////////
void CTopRendererWnd::UpdateSurfaceTexture(int flags)
{
	////////////////////////////////////////////////////////////////////////
	// Generate a new surface texture
	////////////////////////////////////////////////////////////////////////
	if (flags & eUpdateHeightmap)
	{
		bool bShowHeightmap = m_bShowHeightmap;

		bool bRedrawFullTexture = m_bLastShowHeightmapState != bShowHeightmap;

		m_bLastShowHeightmapState = bShowHeightmap;
		if (!bShowHeightmap)
		{
			m_textureSize.cx = gViewportPreferences.mapViewportResolution;
			m_textureSize.cy = gViewportPreferences.mapViewportResolution;

			m_terrainTexture.Allocate(m_textureSize.cx, m_textureSize.cy);

			int flags = ETTG_LIGHTING | ETTG_FAST_LLIGHTING | ETTG_ABGR | ETTG_BAKELIGHTING;
			if (m_bShowWater)
				flags |= ETTG_SHOW_WATER;
			// Fill in the surface data into the array. Apply lighting and water, use
			// the settings from the document
			CTerrainTexGen texGen;
			texGen.GenerateSurfaceTexture(flags, m_terrainTexture);
		}
		else
		{
			int cx = gViewportPreferences.mapViewportResolution;
			int cy = gViewportPreferences.mapViewportResolution;
			if (cx != m_textureSize.cx || cy != m_textureSize.cy || !m_terrainTexture.GetData())
			{
				m_textureSize.cx = cx;
				m_textureSize.cy = cy;
				m_terrainTexture.Allocate(m_textureSize.cx, m_textureSize.cy);
			}

			AABB box = GetIEditorImpl()->GetViewManager()->GetUpdateRegion();
			CRect updateRect = GetIEditorImpl()->GetHeightmap()->WorldBoundsToRect(box);
			CRect* pUpdateRect = &updateRect;
			if (bRedrawFullTexture)
				pUpdateRect = 0;
			GetIEditorImpl()->GetHeightmap()->GetPreviewBitmap((DWORD*)m_terrainTexture.GetData(), m_textureSize.cx, false, false, pUpdateRect, m_bShowWater);
		}
	}

	if (flags == eUpdateStatObj)
	{
		// If The only update flag is Update of static objects, display them.
		m_bShowStatObjects = true;
	}

	if (flags & eUpdateStatObj)
	{
		if (m_bShowStatObjects)
		{
			DrawStaticObjects();
		}
	}
	m_bContentValid = false;
}

//////////////////////////////////////////////////////////////////////////
void CTopRendererWnd::DrawStaticObjects()
{
	if (!m_bShowStatObjects)
		return;

	CVegetationMap* vegetationMap = GetIEditorImpl()->GetVegetationMap();
	int srcW = vegetationMap->GetNumSectors();
	int srcH = vegetationMap->GetNumSectors();

	CRect rc = CRect(0, 0, srcW, srcH);
	AABB updateRegion = GetIEditorImpl()->GetViewManager()->GetUpdateRegion();
	if (updateRegion.min.x > -10000)
	{
		// Update region valid.
		CRect urc;
		CPoint p1 = CPoint(vegetationMap->WorldToSector(updateRegion.min.y), vegetationMap->WorldToSector(updateRegion.min.x));
		CPoint p2 = CPoint(vegetationMap->WorldToSector(updateRegion.max.y), vegetationMap->WorldToSector(updateRegion.max.x));
		urc = CRect(p1.x - 1, p1.y - 1, p2.x + 1, p2.y + 1);
		rc &= urc;
	}

	int trgW = rc.right - rc.left;
	int trgH = rc.bottom - rc.top;

	if (trgW <= 0 || trgH <= 0)
		return;

	m_vegetationTexturePos = CPoint(rc.left, rc.top);
	m_vegetationTextureSize = CSize(srcW, srcH);
	m_vegetationTexture.Allocate(trgW, trgH);

	uint32* trg = m_vegetationTexture.GetData();
	vegetationMap->DrawToTexture(trg, trgW, trgH, rc.left, rc.top);
}

//////////////////////////////////////////////////////////////////////////
void CTopRendererWnd::ResetSurfaceTexture()
{
	////////////////////////////////////////////////////////////////////////
	// Create a surface texture that consists entirely of water
	////////////////////////////////////////////////////////////////////////

	unsigned int i, j;
	DWORD* pSurfaceTextureData = NULL;
	DWORD* pPixData = NULL, * pPixDataEnd = NULL;
	CBitmap bmpLoad;
	BOOL bReturn;

	// Load the water texture out of the ressource
	bReturn = bmpLoad.Attach(::LoadBitmap(AfxGetApp()->m_hInstance, MAKEINTRESOURCE(IDB_WATER)));
	ASSERT(bReturn);

	// Allocate new memory to hold the bitmap data
	CImageEx waterImage;
	waterImage.Allocate(128, 128);

	// Retrieve the bits from the bitmap
	bmpLoad.GetBitmapBits(128 * 128 * sizeof(DWORD), waterImage.GetData());

	// Allocate memory for the surface texture
	m_terrainTexture.Allocate(m_textureSize.cx, m_textureSize.cy);

	// Fill the surface texture with the water texture, tile as needed
	for (j = 0; j < m_textureSize.cy; j++)
		for (i = 0; i < m_textureSize.cx; i++)
		{
			m_terrainTexture.ValueAt(i, j) = waterImage.ValueAt(i & 127, j & 127);
		}

	m_bContentValid = false;
}

//////////////////////////////////////////////////////////////////////////
void CTopRendererWnd::Draw(CObjectRenderHelper& objRenderHelper)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);

	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();

	////////////////////////////////////////////////////////////////////////
	// Perform the rendering for this window
	////////////////////////////////////////////////////////////////////////
	if (!m_bContentsUpdated)
		UpdateContent(0xFFFFFFFF);

	////////////////////////////////////////////////////////////////////////
	// Render the 2D map
	////////////////////////////////////////////////////////////////////////
	if (!m_terrainTextureId)
	{
		//GL_BGRA_EXT
		if (m_terrainTexture.IsValid())
			m_terrainTextureId = gEnv->pRenderer->UploadToVideoMemory((unsigned char*)m_terrainTexture.GetData(), m_textureSize.cx, m_textureSize.cy, eTF_R8G8B8A8, eTF_R8G8B8A8, 0, 0, 0);
	}

	if (m_terrainTextureId && m_terrainTexture.IsValid())
		gEnv->pRenderer->UpdateTextureInVideoMemory(m_terrainTextureId, (unsigned char*)m_terrainTexture.GetData(), 0, 0, m_textureSize.cx, m_textureSize.cy, eTF_R8G8B8A8);

	if (m_bShowStatObjects)
	{
		if (m_vegetationTexture.IsValid())
		{
			int w = m_vegetationTexture.GetWidth();
			int h = m_vegetationTexture.GetHeight();
			uint32* tex = m_vegetationTexture.GetData();
			if (!m_vegetationTextureId)
			{
				m_vegetationTextureId = gEnv->pRenderer->UploadToVideoMemory((unsigned char*)tex, w, h, eTF_R8G8B8A8, eTF_R8G8B8A8, 0, 0, FILTER_NONE);
			}
			else
			{
				int px = m_vegetationTexturePos.x;
				int py = m_vegetationTexturePos.y;
				gEnv->pRenderer->UpdateTextureInVideoMemory(m_vegetationTextureId, (unsigned char*)tex, px, py, w, h, eTF_R8G8B8A8);
			}
			m_vegetationTexture.Release();
		}
	}

	const Matrix34 tm = GetScreenTM();

	float s[4], t[4];
	float w;
	float h;

	if (m_axis == VPA_YX)
	{
		s[0] = 0;
		t[0] = 0;
		s[1] = 0;
		t[1] = 1;
		s[2] = 1;
		t[2] = 1;
		s[3] = 1;
		t[3] = 0;
		w = tm.m01 * m_heightmapSize.cx;
		h = tm.m10 * m_heightmapSize.cy;
	}
	else
	{
		s[0] = 0;
		t[0] = 0;
		s[1] = 1;
		t[1] = 0;
		s[2] = 1;
		t[2] = 1;
		s[3] = 0;
		t[3] = 1;
		w = tm.m00 * m_heightmapSize.cx;
		h = tm.m11 * m_heightmapSize.cy;
	}

	SVF_P3F_C4B_T2F tempVertices[6];
	SVF_P3F_C4B_T2F* pVertex = tempVertices;

	const float xpos = tm.m03;
	const float ypos = tm.m13;
	const float z = 0.0f;
	const uint32 color = 0xFFFFFFFF;

	pVertex->xyz.x = xpos;
	pVertex->xyz.y = ypos;
	pVertex->xyz.z = z;
	pVertex->st = Vec2(s[0], t[0]);
	pVertex->color.dcolor = color;

	++pVertex;

	pVertex->xyz.x = xpos + w;
	pVertex->xyz.y = ypos;
	pVertex->xyz.z = z;
	pVertex->st = Vec2(s[1], t[1]);
	pVertex->color.dcolor = color;

	++pVertex;

	pVertex->xyz.x = xpos;
	pVertex->xyz.y = ypos + h;
	pVertex->xyz.z = z;
	pVertex->st = Vec2(s[3], t[3]);
	pVertex->color.dcolor = color;

	++pVertex;

	pVertex->xyz.x = xpos;
	pVertex->xyz.y = ypos + h;
	pVertex->xyz.z = z;
	pVertex->st = Vec2(s[3], t[3]);
	pVertex->color.dcolor = color;

	++pVertex;

	pVertex->xyz.x = xpos + w;
	pVertex->xyz.y = ypos;
	pVertex->xyz.z = z;
	pVertex->st = Vec2(s[1], t[1]);
	pVertex->color.dcolor = color;

	++pVertex;

	pVertex->xyz.x = xpos + w;
	pVertex->xyz.y = ypos + h;
	pVertex->xyz.z = z;
	pVertex->st = Vec2(s[2], t[2]);
	pVertex->color.dcolor = color;

	// To render 2d map behind all other aux-geom stuffs, custom render flag is made to be the lowest sort priority.
	SAuxGeomRenderFlags renderFlags;
	renderFlags.SetMode2D3DFlag(e_Mode3D);
	renderFlags.SetAlphaBlendMode(e_AlphaNone);
	renderFlags.SetDrawInFrontMode(e_DrawInFrontOff);
	renderFlags.SetFillMode(e_FillModeSolid);
	renderFlags.SetCullMode(e_CullModeNone);
	renderFlags.SetDepthWriteFlag(e_DepthWriteOff);
	renderFlags.SetDepthTestFlag(e_DepthTestOn);

	const SAuxGeomRenderFlags prevRenderFlags = gEnv->pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
	gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(renderFlags);
	gEnv->pRenderer->GetIRenderAuxGeom()->SetTexture(m_terrainTextureId);

	gEnv->pRenderer->GetIRenderAuxGeom()->DrawBuffer(tempVertices, 6, true);

	gEnv->pRenderer->GetIRenderAuxGeom()->SetTexture(-1);
	gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(prevRenderFlags);

	C2DViewport::Draw(objRenderHelper);
}

//////////////////////////////////////////////////////////////////////////
Vec3 CTopRendererWnd::ViewToWorld(CPoint vp, bool* collideWithTerrain, bool onlyTerrain, bool bSkipVegetation, bool bTestRenderMesh)
{
	Vec3 wp = C2DViewport::ViewToWorld(vp, collideWithTerrain, onlyTerrain, bSkipVegetation, bTestRenderMesh);
	wp.z = GetIEditorImpl()->GetTerrainElevation(wp.x, wp.y);
	return wp;
}

// Serialization struct for XY viewport
class CTopRendererWndSettings
{
public:
	CTopRendererWndSettings(CTopRendererWnd* viewport)
		: m_pViewport(viewport)
	{
	}

	void Serialize(Serialization::IArchive& ar)
	{
		bool bShowHeightMap = m_pViewport->m_bShowHeightmap;
		ar(bShowHeightMap, "showheightmap", "Show Heightmap");

		bool bShowStaticObjs = m_pViewport->m_bShowStatObjects;
		ar(bShowStaticObjs, "showstatobjs", "Show Static Objects");

		bool bShowWater = m_pViewport->m_bShowWater;
		ar(bShowWater, "showwater", "Show Water");

		if (ar.isInput())
		{
			if (bShowHeightMap != m_pViewport->m_bShowHeightmap)
			{
				m_pViewport->m_bShowHeightmap = bShowHeightMap;
				m_pViewport->UpdateContent(0xFFFFFFFF);
			}

			if (bShowStaticObjs != m_pViewport->m_bShowStatObjects)
			{
				m_pViewport->m_bShowStatObjects = bShowStaticObjs;
				if (m_pViewport->m_bShowStatObjects)
					m_pViewport->UpdateContent(eUpdateStatObj);
			}

			if (bShowWater != m_pViewport->m_bShowWater)
			{
				m_pViewport->m_bShowWater = bShowWater;
				if (m_pViewport->m_bShowWater)
					m_pViewport->UpdateContent(eUpdateStatObj);
			}
		}
	}

private:
	CTopRendererWnd* m_pViewport;
};

// Serialize Properties for viewports
void CTopRendererWnd::SerializeDisplayOptions(Serialization::IArchive& ar)
{
	CTopRendererWndSettings ser(this);

	ar(ser, "mapviewport", "Map Viewport");

	C2DViewport::SerializeDisplayOptions(ar);
}

