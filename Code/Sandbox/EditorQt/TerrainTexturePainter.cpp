// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TerrainTexturePainter.h"
#include "Viewport.h"
#include "Objects/DisplayContext.h"

#include "CryEditDoc.h"
#include "Terrain/Layer.h"

#include "QtUtil.h"
#include "Util/ImagePainter.h"

#include <Cry3DEngine/I3DEngine.h>

#include "Terrain/TerrainManager.h"
#include "Terrain/SurfaceType.h"

#include "BoostPythonMacros.h"

struct CUndoTPSector
{
	CUndoTPSector()
	{
		m_pImg = 0;
		m_pImgRedo = 0;
	}

	CPoint    tile;
	int       x, y;
	int       w;
	uint32    dwSize;
	CImageEx* m_pImg;
	CImageEx* m_pImgRedo;
};

struct CUndoTPLayerIdSector
{
	CUndoTPLayerIdSector()
	{
		m_pImg = 0;
		m_pImgRedo = 0;
	}

	int             x, y;
	CSurfTypeImage* m_pImg;
	CSurfTypeImage* m_pImgRedo;
};

struct CUndoTPElement
{
	std::vector<CUndoTPSector>        sects;
	std::vector<CUndoTPLayerIdSector> layerIds;

	~CUndoTPElement()
	{
		Clear();
	}

	void AddSector(float fpx, float fpy, float radius)
	{
		CHeightmap* heightmap = GetIEditorImpl()->GetHeightmap();

		CRGBLayer* pRGBLayer = GetIEditorImpl()->GetTerrainManager()->GetRGBLayer();
		uint32 dwMaxRes = pRGBLayer->CalcMaxLocalResolution(0, 0, 1, 1);

		uint32 dwTileCountX = pRGBLayer->GetTileCountX();
		uint32 dwTileCountY = pRGBLayer->GetTileCountY();

		float gx1 = fpx - radius - 2.0f / dwMaxRes;
		float gx2 = fpx + radius + 2.0f / dwMaxRes;
		float gy1 = fpy - radius - 2.0f / dwMaxRes;
		float gy2 = fpy + radius + 2.0f / dwMaxRes;

		gx1 = SATURATE(gx1);
		gy1 = SATURATE(gy1);
		gx2 = SATURATE(gx2);
		gy2 = SATURATE(gy2);

		CRect recTiles = CRect(
		  (uint32)floor(gx1 * dwTileCountX),
		  (uint32)floor(gy1 * dwTileCountY),
		  (uint32)ceil(gx2 * dwTileCountX),
		  (uint32)ceil(gy2 * dwTileCountY));

		for (uint32 dwTileY = recTiles.top; dwTileY < recTiles.bottom; ++dwTileY)
			for (uint32 dwTileX = recTiles.left; dwTileX < recTiles.right; ++dwTileX)
			{
				uint32 dwLocalSize = pRGBLayer->GetTileResolution(dwTileX, dwTileY);
				uint32 dwSize = pRGBLayer->GetTileResolution(dwTileX, dwTileY) * pRGBLayer->GetTileCountX();

				uint32 gwid = dwSize * radius * 4;
				if (gwid < 32)
					gwid = 32;
				else if (gwid < 64)
					gwid = 64;
				else if (gwid < 128)
					gwid = 128;
				else
					gwid = 256;

				float x1 = gx1;
				float x2 = gx2;
				float y1 = gy1;
				float y2 = gy2;

				if (x1 < (float)dwTileX / dwTileCountX) x1 = (float)dwTileX / dwTileCountX;
				if (y1 < (float)dwTileY / dwTileCountY) y1 = (float)dwTileY / dwTileCountY;
				if (x2 > (float)(dwTileX + 1) / dwTileCountX) x2 = (float)(dwTileX + 1) / dwTileCountX;
				if (y2 > (float)(dwTileY + 1) / dwTileCountY) y2 = (float)(dwTileY + 1) / dwTileCountY;

				uint32 wid = gwid;

				if (wid > dwLocalSize)
					wid = dwLocalSize;

				CRect recSects = CRect(
				  ((int)floor(x1 * dwSize / wid)) * wid,
				  ((int)floor(y1 * dwSize / wid)) * wid,
				  ((int)ceil(x2 * dwSize / wid)) * wid,
				  ((int)ceil(y2 * dwSize / wid)) * wid);

				for (uint32 sy = recSects.top; sy < recSects.bottom; sy += wid)
					for (uint32 sx = recSects.left; sx < recSects.right; sx += wid)
					{
						bool bFind = false;
						for (int i = sects.size() - 1; i >= 0; i--)
						{
							CUndoTPSector* pSect = &sects[i];
							if (pSect->tile.x == dwTileX && pSect->tile.y == dwTileY)
							{
								if (pSect->x == sx && pSect->y == sy)
								{
									bFind = true;
									break;
								}
							}
						}
						if (!bFind)
						{
							CUndoTPSector newSect;
							newSect.x = sx;
							newSect.y = sy;
							newSect.w = wid;
							newSect.tile.x = dwTileX;
							newSect.tile.y = dwTileY;
							newSect.dwSize = dwSize;

							newSect.m_pImg = new CImageEx();
							newSect.m_pImg->Allocate(newSect.w, newSect.w);

							CUndoTPSector* pSect = &newSect;

							pRGBLayer->GetSubImageStretched((float)pSect->x / pSect->dwSize, (float)pSect->y / pSect->dwSize,
							                                (float)(pSect->x + pSect->w) / pSect->dwSize, (float)(pSect->y + pSect->w) / pSect->dwSize, *pSect->m_pImg);

							sects.push_back(newSect);
						}
					}
			}

		// Store LayerIDs
		const uint32 layerSize = 64;

		CRect reclids = CRect(
		  (uint32)floor(gx1 * heightmap->GetWidth() / layerSize),
		  (uint32)floor(gy1 * heightmap->GetHeight() / layerSize),
		  (uint32)ceil(gx2 * heightmap->GetWidth() / layerSize),
		  (uint32)ceil(gy2 * heightmap->GetHeight() / layerSize));

		for (uint32 ly = reclids.top; ly < reclids.bottom; ly++)
			for (uint32 lx = reclids.left; lx < reclids.right; lx++)
			{
				bool bFind = false;
				for (int i = layerIds.size() - 1; i >= 0; i--)
				{
					CUndoTPLayerIdSector* pSect = &layerIds[i];
					if (pSect->x == lx && pSect->y == ly)
					{
						bFind = true;
						break;
					}
				}
				if (!bFind)
				{
					CUndoTPLayerIdSector newSect;
					CUndoTPLayerIdSector* pSect = &newSect;

					pSect->m_pImg = new CSurfTypeImage;
					pSect->x = lx;
					pSect->y = ly;
					heightmap->GetLayerInfoBlock(pSect->x * layerSize, pSect->y * layerSize, layerSize, layerSize, *pSect->m_pImg);

					layerIds.push_back(newSect);
				}
			}
	}

	void Paste(bool bIsRedo = false)
	{
		CHeightmap* heightmap = GetIEditorImpl()->GetHeightmap();
		CRGBLayer* pRGBLayer = GetIEditorImpl()->GetTerrainManager()->GetRGBLayer();

		bool bFirst = true;
		CPoint gminp;
		CPoint gmaxp;

		AABB aabb(AABB::RESET);

		for (int i = sects.size() - 1; i >= 0; i--)
		{
			CUndoTPSector* pSect = &sects[i];
			pRGBLayer->SetSubImageStretched((float)pSect->x / pSect->dwSize, (float)pSect->y / pSect->dwSize,
			                                (float)(pSect->x + pSect->w) / pSect->dwSize, (float)(pSect->y + pSect->w) / pSect->dwSize, *(bIsRedo ? pSect->m_pImgRedo : pSect->m_pImg));

			aabb.Add(Vec3((float)pSect->x / pSect->dwSize, (float)pSect->y / pSect->dwSize, 0));
			aabb.Add(Vec3((float)(pSect->x + pSect->w) / pSect->dwSize, (float)(pSect->y + pSect->w) / pSect->dwSize, 0));
		}

		// LayerIDs
		for (int i = layerIds.size() - 1; i >= 0; i--)
		{
			CUndoTPLayerIdSector* pSect = &layerIds[i];
			heightmap->SetLayerIdBlock(pSect->x * pSect->m_pImg->GetWidth(), pSect->y * pSect->m_pImg->GetHeight(), *(bIsRedo ? pSect->m_pImgRedo : pSect->m_pImg));
			heightmap->UpdateEngineTerrain(pSect->x * pSect->m_pImg->GetWidth(), pSect->y * pSect->m_pImg->GetHeight(), pSect->m_pImg->GetWidth(), pSect->m_pImg->GetHeight(), CHeightmap::ETerrainUpdateType::Elevation);
		}

		if (!aabb.IsReset())
		{
			RECT rc = {
				aabb.min.x* heightmap->GetWidth(),    aabb.min.y* heightmap->GetHeight(),
				   aabb.max.x* heightmap->GetWidth(), aabb.max.y* heightmap->GetHeight()
			};
			heightmap->UpdateLayerTexture(rc);
		}
	}

	void StoreRedo()
	{
		CHeightmap* heightmap = GetIEditorImpl()->GetHeightmap();

		for (int i = sects.size() - 1; i >= 0; i--)
		{
			CUndoTPSector* pSect = &sects[i];
			if (!pSect->m_pImgRedo)
			{
				pSect->m_pImgRedo = new CImageEx();
				pSect->m_pImgRedo->Allocate(pSect->w, pSect->w);

				CRGBLayer* pRGBLayer = GetIEditorImpl()->GetTerrainManager()->GetRGBLayer();
				pRGBLayer->GetSubImageStretched((float)pSect->x / pSect->dwSize, (float)pSect->y / pSect->dwSize,
				                                (float)(pSect->x + pSect->w) / pSect->dwSize, (float)(pSect->y + pSect->w) / pSect->dwSize, *pSect->m_pImgRedo);
			}
		}

		// LayerIds
		for (int i = layerIds.size() - 1; i >= 0; i--)
		{
			CUndoTPLayerIdSector* pSect = &layerIds[i];
			if (!pSect->m_pImgRedo)
			{
				pSect->m_pImgRedo = new CSurfTypeImage();
				heightmap->GetLayerInfoBlock(pSect->x * pSect->m_pImg->GetWidth(), pSect->y * pSect->m_pImg->GetHeight(), pSect->m_pImg->GetWidth(), pSect->m_pImg->GetHeight(), *pSect->m_pImgRedo);
			}
		}
	}

	void Clear()
	{
		for (int i = 0; i < sects.size(); i++)
		{
			CUndoTPSector* pSect = &sects[i];
			delete pSect->m_pImg;
			pSect->m_pImg = 0;

			delete pSect->m_pImgRedo;
			pSect->m_pImgRedo = 0;
		}

		for (int i = 0; i < layerIds.size(); i++)
		{
			CUndoTPLayerIdSector* pSect = &layerIds[i];
			delete pSect->m_pImg;
			pSect->m_pImg = 0;

			delete pSect->m_pImgRedo;
			pSect->m_pImgRedo = 0;
		}
	}

	int GetSize()
	{
		int size = 0;
		for (int i = 0; i < sects.size(); i++)
		{
			CUndoTPSector* pSect = &sects[i];
			if (pSect->m_pImg)
				size += pSect->m_pImg->GetSize();
			if (pSect->m_pImgRedo)
				size += pSect->m_pImgRedo->GetSize();
		}
		for (int i = 0; i < layerIds.size(); i++)
		{
			CUndoTPLayerIdSector* pSect = &layerIds[i];
			if (pSect->m_pImg)
				size += pSect->m_pImg->GetSize();
			if (pSect->m_pImgRedo)
				size += pSect->m_pImgRedo->GetSize();
		}
		return size;
	}
};

//////////////////////////////////////////////////////////////////////////
//! Undo object.
class CUndoTexturePainter : public IUndoObject
{
public:
	CUndoTexturePainter(CTerrainTexturePainter* pTool)
	{
		m_pUndo = pTool->m_pTPElem;
		pTool->m_pTPElem = 0;
	}

protected:
	virtual void Release()
	{
		delete m_pUndo;
		delete this;
	};

	virtual const char* GetDescription() { return "Terrain Painter Modify"; };

	virtual void        Undo(bool bUndo)
	{
		if (bUndo)
			m_pUndo->StoreRedo();

		if (m_pUndo)
			m_pUndo->Paste();
	}
	virtual void Redo()
	{
		if (m_pUndo)
			m_pUndo->Paste(true);
	}

private:
	CUndoTPElement* m_pUndo;
};

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CTerrainTexturePainter, CEditTool)

CTextureBrush CTerrainTexturePainter::m_brush;

namespace {
int s_toolPanelId = 0;
};

//////////////////////////////////////////////////////////////////////////
CTerrainTexturePainter::CTerrainTexturePainter()
{
	m_brush.maxRadius = 1000.0f;

	m_heightmap = GetIEditorImpl()->GetHeightmap();
	assert(m_heightmap);

	m_3DEngine = GetIEditorImpl()->Get3DEngine();
	assert(m_3DEngine);

	m_renderer = GetIEditorImpl()->GetRenderer();
	assert(m_renderer);

	m_pointerPos(0, 0, 0);
	m_lastMousePoint = QPoint(0, 0);
	GetIEditorImpl()->ClearSelection();

	//////////////////////////////////////////////////////////////////////////
	// Initialize sectors.
	//////////////////////////////////////////////////////////////////////////
	SSectorInfo sectorInfo;
	m_heightmap->GetSectorsInfo(sectorInfo);

	m_bIsPainting = false;
	m_pTPElem = 0;
}

//////////////////////////////////////////////////////////////////////////
CTerrainTexturePainter::~CTerrainTexturePainter()
{
	m_pointerPos(0, 0, 0);

	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
		GetIEditorImpl()->GetIUndoManager()->Cancel();
}

//////////////////////////////////////////////////////////////////////////
bool CTerrainTexturePainter::MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	if (eMouseWheel == event)
		return false;

	bool bPainting = false;
	bool bHitTerrain = false;
	Vec3 pointerPos = view->ViewToWorld(point, &bHitTerrain, true);

	m_brush.bErase = false;

	if (m_bIsPainting)
	{
		if (event == eMouseLDown || event == eMouseLUp)
		{
			Action_StopUndo();
		}
	}

	if (MK_ALT & flags)
	{
		if (event == eMouseMove && bHitTerrain && QtUtil::IsMouseButtonDown(Qt::LeftButton))
		{
			Vec3 lastPointer = view->ViewToWorld(CPoint(m_lastMousePoint.x(), m_lastMousePoint.y()), &bHitTerrain, true);
			Vec2 offset(lastPointer.x - pointerPos.x, lastPointer.y - pointerPos.y);

			if (bHitTerrain)
			{
				m_brush.radius = min(m_brush.maxRadius, max(m_brush.minRadius, m_brush.radius + ((0 > (point.x - m_lastMousePoint.x())) ? -offset.GetLength() : offset.GetLength())));
				signalPropertiesChanged(this);
			}
		}
	}
	else
	{
		m_pointerPos = pointerPos;

		if ((flags & MK_CONTROL) != 0)                             // pick layerid
		{
			if (event == eMouseLDown)
				Action_PickLayerId();
		}
		else if (event == eMouseLDown || (event == eMouseMove && (flags & MK_LBUTTON)))   // paint
			Action_Paint();
	}
	m_lastMousePoint = QPoint(point.x, point.y);

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::Display(DisplayContext& dc)
{
	dc.SetColor(0, 1, 0, 1);

	if (m_pointerPos.x == 0 && m_pointerPos.y == 0 && m_pointerPos.z == 0)
		return;   // mouse cursor not in window

	dc.DepthTestOff();
	dc.DrawTerrainCircle(m_pointerPos, m_brush.radius, 0.2f);
	dc.DepthTestOn();
}

//////////////////////////////////////////////////////////////////////////
bool CTerrainTexturePainter::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	bool bProcessed = false;

	if (nChar == VK_ADD)
	{
		m_brush.radius += 1;
		bProcessed = true;
	}
	if (nChar == VK_SUBTRACT)
	{
		if (m_brush.radius > 1)
			m_brush.radius -= 1;
		bProcessed = true;
	}
	if (m_brush.radius < m_brush.minRadius)
		m_brush.radius = m_brush.minRadius;
	if (m_brush.radius > m_brush.maxRadius)
		m_brush.radius = m_brush.maxRadius;

	signalPropertiesChanged(this);

	return bProcessed;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::SelectLayer(const CLayer* pLayer)
{
	for (int i = 0; i < GetIEditorImpl()->GetTerrainManager()->GetLayerCount(); i++)
	{
		CLayer* pLayerIt = GetIEditorImpl()->GetTerrainManager()->GetLayer(i);
		if (pLayerIt == pLayer)
		{
			GetIEditorImpl()->GetTerrainManager()->SelectLayer(i);
			break;
		}
	}

	ReloadLayers();
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::ReloadLayers()
{
	CLayer* pSelectedLayer = 0;

	for (int i = 0; i < GetIEditorImpl()->GetTerrainManager()->GetLayerCount(); i++)
	{
		CLayer* pLayer = GetIEditorImpl()->GetTerrainManager()->GetLayer(i);

		if (pLayer->IsSelected())
		{
			pSelectedLayer = pLayer;
		}
	}

	m_brush.m_dwMaskLayerId = 0xffffffff;

	if (pSelectedLayer)
	{
		m_brush.m_cFilterColor = pSelectedLayer->m_cLayerFilterColor;
	}
	signalPropertiesChanged(this);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::Action_PickLayerId()
{
	int iTerrainSize = m_3DEngine->GetTerrainSize();                                      // in m
	int hmapWidth = m_heightmap->GetWidth();
	int hmapHeight = m_heightmap->GetHeight();

	int iX = (int)((m_pointerPos.y * hmapWidth) / iTerrainSize);      // maybe +0.5f is needed
	int iY = (int)((m_pointerPos.x * hmapHeight) / iTerrainSize);     // maybe +0.5f is needed

	if (iX >= 0 && iX < iTerrainSize)
		if (iY >= 0 && iY < iTerrainSize)
		{
			uint32 dwLayerid = m_heightmap->GetLayerIdAt(iX, iY);
			CLayer* pLayer = GetIEditorImpl()->GetTerrainManager()->FindLayerByLayerId(dwLayerid);
			SelectLayer(pLayer);
		}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::Action_FixMaterialCount()
{
	CCryEditDoc* pDoc = GetIEditorImpl()->GetDocument();
	if (!pDoc)
		return;

	// Find the boundaries of the engine terrain sector:
	int nSectorSize = m_3DEngine->GetTerrainSectorSize();

	int nTerrainSize = m_3DEngine->GetTerrainSize();  // in m
	int hmapWidth = m_heightmap->GetWidth();
	int hmapHeight = m_heightmap->GetHeight();

	int iX = (int)((m_pointerPos.y * hmapWidth) / nTerrainSize);
	int iY = (int)((m_pointerPos.x * hmapHeight) / nTerrainSize);

	int iMinX = (iX / nSectorSize) * nSectorSize;
	int iMinY = (iY / nSectorSize) * nSectorSize;
	int iMaxX = iMinX + nSectorSize;
	int iMaxY = iMinY + nSectorSize;

	int UsageCounters[MAX_SURFACE_TYPE_ID_COUNT + 1];
	int iUsedSurfaceIDs = 0;
	ZeroMemory(UsageCounters, sizeof(UsageCounters));
	for (int y = iMinY; y <= iMaxY; y++)
		for (int x = iMinX; x <= iMaxX; x++)
		{
			SSurfaceTypeItem& st = m_heightmap->GetLayerInfoAt(x, y);

			for (int s = 0; s < SSurfaceTypeItem::kMaxSurfaceTypesNum; s++)
			{
				if (st.we[s])
				{
					if (UsageCounters[st.ty[s]] == 0)
					{
						iUsedSurfaceIDs++;
					}

					UsageCounters[st.ty[s]] += st.we[s];
				}
			}
		}

	// The layer currently selected in the GUI won't be removed even if it is the least used one.
	CLayer* pSelectedLayer = GetSelectedLayer();
	int iSelected = pSelectedLayer ? pSelectedLayer->GetCurrentLayerId() : -1;

	// While the layers are too many, we take the least used one and overwrite it with the most used one.
	while (iUsedSurfaceIDs >= 15)
	{
		int iLeastUsed = -1;
		int iMostUsed = -1;

		for (int i = 0; i < MAX_SURFACE_TYPE_ID_COUNT + 1; i++)
			if ((iLeastUsed < 0 || UsageCounters[i] && UsageCounters[i] < UsageCounters[iLeastUsed]) && i != iSelected)
				iLeastUsed = i;

		for (int i = 0; i < MAX_SURFACE_TYPE_ID_COUNT + 1; i++)
			if (iMostUsed < 0 || i != iLeastUsed && UsageCounters[i] > UsageCounters[iMostUsed])
				iMostUsed = i;

		for (int y = iMinY; y <= iMaxY; y++)
			for (int x = iMinX; x <= iMaxX; x++)
				if (m_heightmap->GetLayerIdAt(x, y) == iLeastUsed)
				{
					SSurfaceTypeItem st;
					st = iMostUsed;
					m_heightmap->SetLayerIdAt(x, y, st);
				}

		UsageCounters[iMostUsed] += UsageCounters[iLeastUsed];
		UsageCounters[iLeastUsed] = 0;

		iUsedSurfaceIDs--;
	}

	m_heightmap->UpdateEngineTerrain(iMinX, iMinY, nSectorSize - 1, nSectorSize - 1, CHeightmap::ETerrainUpdateType::Paint);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::Action_Paint()
{
	//////////////////////////////////////////////////////////////////////////
	// Paint spot on selected layer.
	//////////////////////////////////////////////////////////////////////////
	CLayer* pLayer = GetSelectedLayer();
	if (!pLayer)
		return;

	Vec3 center(m_pointerPos.x, m_pointerPos.y, 0);
	if (!CheckSectorPalette(pLayer, center))
		return;

	static bool bPaintLock = false;
	if (bPaintLock)
		return;

	bPaintLock = true;

	PaintLayer(pLayer, center, false);

	GetIEditorImpl()->GetDocument()->SetModifiedFlag(TRUE);
	GetIEditorImpl()->UpdateViews(eUpdateHeightmap);

	bPaintLock = false;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::Action_Flood()
{
	//////////////////////////////////////////////////////////////////////////
	// Paint spot on selected layer.
	//////////////////////////////////////////////////////////////////////////
	CLayer* pLayer = GetSelectedLayer();
	if (!pLayer)
	{
		Warning("No layer is selected.");
		return;
	}

	float oldRadius = m_brush.radius;
	float fRadius = m_3DEngine->GetTerrainSize() * 0.5f;
	m_brush.radius = fRadius;
	Vec3 center(fRadius, fRadius, 0);

	if (!CheckSectorPalette(pLayer, center))
		return;

	static bool bPaintLock = false;
	if (bPaintLock)
		return;

	bPaintLock = true;

	PaintLayer(pLayer, center, true);

	GetIEditorImpl()->GetDocument()->SetModifiedFlag(TRUE);
	GetIEditorImpl()->UpdateViews(eUpdateHeightmap);

	m_brush.radius = oldRadius;
	bPaintLock = false;
}

//////////////////////////////////////////////////////////////////////////
bool CTerrainTexturePainter::CheckSectorPalette(CLayer* pLayer, const Vec3& center)
{
	////////////////////////////////////////////////////////////////////////////////////
	// Before everything else, check if the action would cause sector palette problems.
	////////////////////////////////////////////////////////////////////////////////////
	if (m_brush.bDetailLayer && pLayer->GetSurfaceType())
	{
		if (!GetIEditorImpl()->Get3DEngine() || !GetIEditorImpl()->Get3DEngine()->GetITerrain())
			return false;

		int nTerrainSizeInUnits = m_3DEngine->GetTerrainSize();
		int x = pos_directed_rounding(center.y * (float)m_heightmap->GetWidth() / nTerrainSizeInUnits);
		int y = pos_directed_rounding(center.x * (float)m_heightmap->GetHeight() / nTerrainSizeInUnits);

		float unitSize = m_heightmap->GetUnitSize();
		int r = pos_directed_rounding(m_brush.radius / unitSize);

		if (!GetIEditorImpl()->Get3DEngine()->GetITerrain()->CanPaintSurfaceType(x, y, r, pLayer->GetEngineSurfaceTypeId()))
			return false;
	}
	return true;
}

void CTerrainTexturePainter::Serialize(Serialization::IArchive& ar)
{
	if (ar.openBlock("brush", "Brush"))
	{
		ar(Serialization::Range(m_brush.radius, m_brush.minRadius, m_brush.maxRadius), "range", "Range");
		ar(Serialization::Range(m_brush.hardness, 0.0f, 1.0f), "hardness", "Hardness");
		ar(m_brush.bDetailLayer, "detailLayer", "Paint Layer ID");
		ar(m_brush.bMaskByLayerSettings, "altitudeAndSlope", "Mask by Altitude and Slope");

		string selectedLayer("<none>");
		Serialization::StringList layers;
		layers.push_back(selectedLayer.GetBuffer());

		CTerrainManager* pTerrainManager = GetIEditorImpl()->GetTerrainManager();
		if (pTerrainManager)
		{
			int layerCount = pTerrainManager->GetLayerCount();
			for (int i = 0; i < layerCount; ++i)
			{
				CLayer* pLayer = GetIEditorImpl()->GetTerrainManager()->GetLayer(i);
				if (pLayer)
				{
					string layerName = pLayer->GetLayerName();
					layers.push_back(layerName.GetBuffer());

					if (pLayer->GetOrRequestLayerId() == m_brush.m_dwMaskLayerId)
						selectedLayer = layerName;
				}
			}

			Serialization::StringListValue layersValue(layers, selectedLayer);
			ar(layersValue, "maskedLayer", "Mask by");

			if (ar.isInput())
			{
				m_brush.m_dwMaskLayerId = 0xffffffff;
				for (int i = 0; i < layerCount; ++i)
				{
					CLayer* pLayer = GetIEditorImpl()->GetTerrainManager()->GetLayer(i);
					if (pLayer)
					{
						if (pLayer->GetLayerName() == layersValue.c_str())
						{
							m_brush.m_dwMaskLayerId = pLayer->GetOrRequestLayerId();
							break;
						}
					}
				}
			}
		}

		ar.closeBlock();
	}
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::PaintLayer(CLayer* pLayer, const Vec3& center, bool bFlood)
{
	float fTerrainSize = (float)m_3DEngine->GetTerrainSize();

	SEditorPaintBrush br(*GetIEditorImpl()->GetHeightmap(), *pLayer, m_brush.bMaskByLayerSettings, m_brush.m_dwMaskLayerId, bFlood);

	br.m_cFilterColor = pLayer->GetLayerFilterColor();
	br.fRadius = m_brush.radius / fTerrainSize;
	br.hardness = m_brush.hardness;
	br.color = m_brush.value;
	if (m_brush.bErase)
		br.color = 0;

	// Paint spot on layer mask.
	{
		float fX = center.y / fTerrainSize;             // 0..1
		float fY = center.x / fTerrainSize;             // 0..1

		// paint terrain materials
		if (m_brush.bDetailLayer)
		{
			// get unique layerId
			uint32 dwLayerId = pLayer->GetOrRequestLayerId();

			m_heightmap->PaintLayerId(fX, fY, br, dwLayerId);
		}

		// paint terrain base texture (must be after terrain materials paint because of e_TerrainAutoGenerateBaseTexture feature)
		{
			CRGBLayer* pRGBLayer = GetIEditorImpl()->GetTerrainManager()->GetRGBLayer();

			assert(pRGBLayer->CalcMinRequiredTextureExtend());

			// load m_texture is needed/possible
			pLayer->PrecacheTexture();

			if (pLayer->m_texture.IsValid())
			{
				Action_CollectUndo(fX, fY, br.fRadius);
				pRGBLayer->PaintBrushWithPatternTiled(fX, fY, br, pLayer->m_texture);
			}
		}
	}

	Vec3 vMin = center - Vec3(m_brush.radius, m_brush.radius, 0);
	Vec3 vMax = center + Vec3(m_brush.radius, m_brush.radius, 0);

	//////////////////////////////////////////////////////////////////////////
	// Update Terrain textures.
	//////////////////////////////////////////////////////////////////////////
	{
		int iTerrainSize = m_3DEngine->GetTerrainSize();                        // in meters
		int iTexSectorSize = m_3DEngine->GetTerrainTextureNodeSizeMeters();     // in meters

		if (!iTexSectorSize)
		{
			assert(0);            // maybe we should visualized this to the user
			return;               // you need to calculated the surface texture first
		}

		int iMinSecX = (int)floor(vMin.x / (float)iTexSectorSize);
		int iMinSecY = (int)floor(vMin.y / (float)iTexSectorSize);
		int iMaxSecX = (int)ceil(vMax.x / (float)iTexSectorSize);
		int iMaxSecY = (int)ceil(vMax.y / (float)iTexSectorSize);

		iMinSecX = max(iMinSecX, 0);
		iMinSecY = max(iMinSecY, 0);
		iMaxSecX = min(iMaxSecX, iTerrainSize / iTexSectorSize);
		iMaxSecY = min(iMaxSecY, iTerrainSize / iTexSectorSize);

		float fTerrainSize = (float)m_3DEngine->GetTerrainSize();                                     // in m

		// update rectangle in 0..1 range
		float fGlobalMinX = vMin.x / fTerrainSize, fGlobalMinY = vMin.y / fTerrainSize;
		float fGlobalMaxX = vMax.x / fTerrainSize, fGlobalMaxY = vMax.y / fTerrainSize;

		for (int iY = iMinSecY; iY < iMaxSecY; ++iY)
			for (int iX = iMinSecX; iX < iMaxSecX; ++iX)
			{
				m_heightmap->UpdateSectorTexture(CPoint(iY, iX), fGlobalMinY, fGlobalMinX, fGlobalMaxY, fGlobalMaxX);
			}
	}

	//////////////////////////////////////////////////////////////////////////
	// Update surface types.
	//////////////////////////////////////////////////////////////////////////
	// Build rectangle in heightmap coordinates.
	{
		float fTerrainSize = (float)m_3DEngine->GetTerrainSize();                                     // in m
		int hmapWidth = m_heightmap->GetWidth();
		int hmapHeight = m_heightmap->GetHeight();

		float fX = center.y * hmapWidth / fTerrainSize;
		float fY = center.x * hmapHeight / fTerrainSize;

		float unitSize = m_heightmap->GetUnitSize();
		float fHMRadius = m_brush.radius / unitSize;
		CRect hmaprc;

		// clip against heightmap borders
		hmaprc.left = max((int)floor(fX - fHMRadius), 0);
		hmaprc.top = max((int)floor(fY - fHMRadius), 0);
		hmaprc.right = min((int)ceil(fX + fHMRadius), hmapWidth);
		hmaprc.bottom = min((int)ceil(fY + fHMRadius), hmapHeight);

		// Update surface types at 3d engine terrain.
		m_heightmap->UpdateEngineTerrain(hmaprc.left, hmaprc.top, hmaprc.Width(), hmaprc.Height(), CHeightmap::ETerrainUpdateType::Paint);
	}
}

//////////////////////////////////////////////////////////////////////////
CLayer* CTerrainTexturePainter::GetSelectedLayer() const
{
	for (int i = 0; i < GetIEditorImpl()->GetTerrainManager()->GetLayerCount(); i++)
	{
		CLayer* pLayer = GetIEditorImpl()->GetTerrainManager()->GetLayer(i);
		if (pLayer->IsSelected())
		{
			return pLayer;
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::Action_StartUndo()
{
	if (m_bIsPainting)
		return;

	m_pTPElem = new CUndoTPElement;

	m_bIsPainting = true;
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::Action_CollectUndo(float x, float y, float radius)
{
	if (!m_bIsPainting)
		Action_StartUndo();

	m_pTPElem->AddSector(x, y, radius);
}

//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::Action_StopUndo()
{
	if (!m_bIsPainting)
		return;

	GetIEditorImpl()->GetIUndoManager()->Begin();

	if (CUndo::IsRecording())
		CUndo::Record(new CUndoTexturePainter(this));

	GetIEditorImpl()->GetIUndoManager()->Accept("Texture Layer Painting");

	m_bIsPainting = false;
}

//////////////////////////////////////////////////////////////////////////
// Class description.
//////////////////////////////////////////////////////////////////////////
class CTerrainTexturePainter_ClassDesc : public IClassDesc
{
	//! This method returns an Editor defined GUID describing the class this plugin class is associated with.
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_EDITTOOL; }

	//! This method returns the human readable name of the class.
	virtual const char* ClassName() { return "EditTool.TerainPainter"; };

	//! This method returns Category of this class, Category is specifing where this plugin class fits best in
	//! create panel.
	virtual const char*    Category()        { return "Terrain"; };

	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CTerrainTexturePainter); }
};

REGISTER_CLASS_DESC(CTerrainTexturePainter_ClassDesc);

//////////////////////////////////////////////////////////////////////////
void CTerrainTexturePainter::Command_Activate()
{
	CEditTool* pTool = GetIEditorImpl()->GetEditTool();
	if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CTerrainTexturePainter)))
	{
		// Already active.
		return;
	}
	pTool = new CTerrainTexturePainter();
	GetIEditorImpl()->SetEditTool(pTool);
}

REGISTER_PYTHON_COMMAND(CTerrainTexturePainter::Command_Activate, edit_mode, terrain_painter, "Activates terrain texture painting mode");

