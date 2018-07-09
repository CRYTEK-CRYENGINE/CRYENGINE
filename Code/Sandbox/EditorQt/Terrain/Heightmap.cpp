// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Noise.h"
#include "Layer.h"
#include "CryEditDoc.h"
#include "TerrainFormulaDlg.h"
#include "Vegetation/VegetationMap.h"
#include "TerrainGrid.h"
#include "Util/DynamicArray2D.h"
#include "GameEngine.h"

#include "Terrain/TerrainManager.h"
#include "SurfaceType.h"
#include "Util/ImagePainter.h"

#include <Cry3DEngine/I3DEngine.h>

#include "QT/Widgets/QWaitProgress.h"

#include "Controls/QuestionDialog.h"

#include <CryMath/Random.h>

#define DEFAULT_TEXTURE_SIZE 4096

//! Size of terrain sector in units, sector size (in meters) depends from heightmap unit size - it gives more efficient heightmap triangulation
#define SECTOR_SIZE_IN_UNITS 32

//! Default Max Height value.
#define HEIGHTMAP_MAX_HEIGHT 1024

//! Size of noise array.
#define NOISE_ARRAY_SIZE 512
#define NOISE_RANGE      255.0f

//! Filename used when Holding/Fetching heightmap.
#define HEIGHTMAP_HOLD_FETCH_FILE       "Heightmap.hld"

//! Archive block names
#define HEIGHTMAP_LAYERINFO_BLOCK_NAME      "HeightmapLayerIdBitmap_ver3"
#define HEIGHTMAP_LAYERINFO_BLOCK_NAME_OLD  "HeightmapLayerIdBitmap_ver2"
#define HEIGHTMAP_RGB_BLOCK_NAME            "HeightmapRgbBitmap"

#define OVVERIDE_LAYER_SURFACETYPE_FROM 128

const int CHeightmap::kInitHeight = 32;

enum ERegionRotation
{
	eRR_0,
	eRR_90,
	eRR_180,
	eRR_270,
};

namespace Private_Heightmap
{
CRect GetRectMargin(const CRect& rc, const CRect& rcCrop, int nRot)
{
	CRect rcMar(rcCrop.left - rc.left, rcCrop.top - rc.top, rc.right - rcCrop.right, rc.bottom - rcCrop.bottom);
	if (nRot == eRR_90)
		return CRect(rcMar.top, rcMar.right, rcMar.bottom, rcMar.left);
	if (nRot == eRR_180)
		return CRect(rcMar.right, rcMar.bottom, rcMar.left, rcMar.top);
	if (nRot == eRR_270)
		return CRect(rcMar.bottom, rcMar.left, rcMar.top, rcMar.right);
	return rcMar;
}

CRect CropRect(const CRect& rc, const CRect& rcMar)
{
	return CRect(rc.left + rcMar.left, rc.top + rcMar.top, rc.right - rcMar.right, rc.bottom - rcMar.bottom);
}

struct SRgbRegion
{
	float left;
	float top;
	float right;
	float bottom;
	int32 width;
	int32 height;

	bool IsEmpty() const
	{
		return !width || !height;
	}
};

SRgbRegion MapToRgbRegion(const CRect& rc, CHeightmap& heightmap)
{
	SRgbRegion region{};
	MAKE_SURE(heightmap.GetRGBLayer(), return region);

	region.left = crymath::clamp(0.0f, 1.0f, (float)rc.left / heightmap.GetWidth());
	region.top = crymath::clamp(0.0f, 1.0f, (float)rc.top / heightmap.GetHeight());
	region.right = crymath::clamp(0.0f, 1.0f, (float)rc.right / heightmap.GetWidth());
	region.bottom = crymath::clamp(0.0f, 1.0f, (float)rc.bottom / heightmap.GetHeight());

	CRGBLayer& rgbLayer = *heightmap.GetRGBLayer();
	const uint32 maxTileSize = rgbLayer.CalcMaxLocalResolution(region.left, region.top, region.right, region.bottom);
	if (maxTileSize)
	{
		const int32 width = maxTileSize * rgbLayer.GetTileCountX();
		const int32 height = maxTileSize * rgbLayer.GetTileCountY();
		const int32 minX = (int32)floor(region.left * width);
		const int32 minY = (int32)floor(region.top * height);
		const int32 maxX = (int32)ceil(region.right * width);
		const int32 maxY = (int32)ceil(region.bottom * height);

		region.width = maxX - minX;
		region.height = maxY - minY;
	}
	return region;
}

}

//////////////////////////////////////////////////////////////////////////
//! Undo object for heightmap modifications.
class CUndoHeightmapModify : public IUndoObject
{
public:
	CUndoHeightmapModify(int x1, int y1, int width, int height, CHeightmap* heightmap)
	{
		m_hmap.Attach(heightmap->GetData(), heightmap->GetWidth(), heightmap->GetHeight());
		// Store heightmap block.
		m_rc = CRect(x1, y1, x1 + width, y1 + height);
		m_rc &= CRect(0, 0, m_hmap.GetWidth(), m_hmap.GetHeight());
		m_hmap.GetSubImage(m_rc.left, m_rc.top, m_rc.Width(), m_rc.Height(), m_undo);
	}

	CUndoHeightmapModify(CHeightmap* heightmap)
	{
		m_hmap.Attach(heightmap->GetData(), heightmap->GetWidth(), heightmap->GetHeight());
		// Store heightmap block.
		m_rc = CRect(0, 0, m_hmap.GetWidth(), m_hmap.GetHeight());
		m_hmap.GetSubImage(m_rc.left, m_rc.top, m_rc.Width(), m_rc.Height(), m_undo);
	}
protected:
	virtual void        Release()        { delete this; };
	virtual const char* GetDescription() { return "Heightmap Modify"; };

	virtual void        Undo(bool bUndo)
	{
		if (bUndo)
		{
			// Store for redo.
			m_hmap.GetSubImage(m_rc.left, m_rc.top, m_rc.Width(), m_rc.Height(), m_redo);
		}
		// Restore image.
		m_hmap.SetSubImage(m_rc.left, m_rc.top, m_undo);

		int w = m_rc.Width();
		if (w < m_rc.Height()) w = m_rc.Height();

		GetIEditorImpl()->GetHeightmap()->UpdateEngineTerrain(m_rc.left, m_rc.top, w, w, CHeightmap::ETerrainUpdateType::Elevation);
		//GetIEditorImpl()->GetHeightmap()->UpdateEngineTerrain( m_rc.left,m_rc.top,m_rc.Width(),m_rc.Height(),true,false );

		if (bUndo)
		{
			AABB box;
			box.min = GetIEditorImpl()->GetHeightmap()->HmapToWorld(CPoint(m_rc.left, m_rc.top));
			box.max = GetIEditorImpl()->GetHeightmap()->HmapToWorld(CPoint(m_rc.left + w, m_rc.top + w));
			box.min.z -= 10000;
			box.max.z += 10000;
			GetIEditorImpl()->UpdateViews(eUpdateHeightmap, &box);
		}
	}
	virtual void Redo()
	{
		if (m_redo.IsValid())
		{
			// Restore image.
			m_hmap.SetSubImage(m_rc.left, m_rc.top, m_redo);
			GetIEditorImpl()->GetHeightmap()->UpdateEngineTerrain(m_rc.left, m_rc.top, m_rc.Width(), m_rc.Height(), CHeightmap::ETerrainUpdateType::Elevation);

			{
				AABB box;
				box.min = GetIEditorImpl()->GetHeightmap()->HmapToWorld(CPoint(m_rc.left, m_rc.top));
				box.max = GetIEditorImpl()->GetHeightmap()->HmapToWorld(CPoint(m_rc.left + m_rc.Width(), m_rc.top + m_rc.Height()));
				box.min.z -= 10000;
				box.max.z += 10000;
				GetIEditorImpl()->UpdateViews(eUpdateHeightmap, &box);
			}
		}
	}

private:
	CRect         m_rc;
	TImage<float> m_undo;
	TImage<float> m_redo;

	TImage<float> m_hmap;   // memory data is shared
};

class CUndoWaterLevel : public IUndoObject
{
public:
	CUndoWaterLevel(CHeightmap* heightmap)
		: m_pHeightmap(heightmap), m_oldWaterLevel(WATER_LEVEL_UNKNOWN), m_newWaterLevel(WATER_LEVEL_UNKNOWN)
	{
		if (GetIEditorImpl()->GetSystem()->GetI3DEngine())
			m_oldWaterLevel = GetIEditorImpl()->GetSystem()->GetI3DEngine()->GetWaterLevel();
	}

protected:
	virtual void Undo(bool bUndo)
	{
		if (bUndo && GetIEditorImpl()->GetSystem()->GetI3DEngine())
			m_newWaterLevel = GetIEditorImpl()->GetSystem()->GetI3DEngine()->GetWaterLevel();

		m_pHeightmap->SetWaterLevel(m_oldWaterLevel);
	}

	virtual void Redo()
	{
		m_pHeightmap->SetWaterLevel(m_newWaterLevel);
	}

	virtual const char* GetDescription() { return "Water Level Modify"; };

private:
	CHeightmap* m_pHeightmap;
	float       m_oldWaterLevel;
	float       m_newWaterLevel;
};

//////////////////////////////////////////////////////////////////////////
//! Undo object for heightmap modifications.
class CUndoHeightmapInfo : public IUndoObject
{
public:
	CUndoHeightmapInfo(int x1, int y1, int width, int height, CHeightmap* heightmap)
	{
		m_hmap.Attach(heightmap->m_LayerIdBitmap.GetData(), heightmap->GetWidth(), heightmap->GetHeight());
		// Store heightmap block.
		m_hmap.GetSubImage(x1, y1, width, height, m_undo);
		m_rc = CRect(x1, y1, x1 + width, y1 + height);
	}
protected:
	virtual void        Release()        { delete this; };
	virtual const char* GetDescription() { return "Heightmap Hole"; };

	virtual void        Undo(bool bUndo)
	{
		if (bUndo)
		{
			// Store for redo.
			m_hmap.GetSubImage(m_rc.left, m_rc.top, m_rc.Width(), m_rc.Height(), m_redo);
		}
		// Restore image.
		m_hmap.SetSubImage(m_rc.left, m_rc.top, m_undo);
		GetIEditorImpl()->GetHeightmap()->UpdateEngineHole(m_rc.left, m_rc.top, m_rc.Width(), m_rc.Height());
	}
	virtual void Redo()
	{
		if (m_redo.IsValid())
		{
			// Restore image.
			m_hmap.SetSubImage(m_rc.left, m_rc.top, m_redo);
			GetIEditorImpl()->GetHeightmap()->UpdateEngineHole(m_rc.left, m_rc.top, m_rc.Width(), m_rc.Height());
		}
	}

private:
	CRect          m_rc;
	CSurfTypeImage m_undo;
	CSurfTypeImage m_redo;
	CSurfTypeImage m_hmap;
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CHeightmap::CHeightmap()
	: m_kfBrMultiplier(8.0f)
	, m_TerrainRGBTexture("TerrainTexture.pak")
{
	m_fMaxHeight = HEIGHTMAP_MAX_HEIGHT;

	m_pNoise = NULL;
	m_pHeightmap = NULL;

	m_iWidth = 0;
	m_iHeight = 0;
	m_fWaterLevel = 16;
	m_unitSize = 2;
	m_numSectors = 0;
	m_updateModSectors = false;

	m_textureSize = DEFAULT_TEXTURE_SIZE;

	m_terrainGrid = new CTerrainGrid();
}

CHeightmap::~CHeightmap()
{
	// Reset the heightmap
	CleanUp();

	delete m_terrainGrid;

	// Remove the noise array
	if (m_pNoise)
	{
		delete m_pNoise;
		m_pNoise = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::CleanUp()
{
	////////////////////////////////////////////////////////////////////////
	// Free the data
	////////////////////////////////////////////////////////////////////////

	if (m_pHeightmap)
	{
		delete[] m_pHeightmap;
		m_pHeightmap = NULL;
	}

	m_TerrainRGBTexture.FreeData();

	m_iWidth = 0;
	m_iHeight = 0;
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::Resize(int iWidth, int iHeight, float unitSize, bool bCleanOld, bool bForceKeepVegetation /*=false*/)
{
	////////////////////////////////////////////////////////////////////////
	// Resize the heightmap
	////////////////////////////////////////////////////////////////////////
	GetIEditorImpl()->Get3DEngine()->SetEditorHeightmapCallback(this);

	ASSERT(iWidth && iHeight);

	int prevWidth, prevHeight, prevUnitSize;
	prevWidth = m_iWidth;
	prevHeight = m_iHeight;
	prevUnitSize = m_unitSize;

	t_hmap* prevHeightmap = 0;
	TImage<unsigned char> prevLayerIdBitmap;

	if (bCleanOld)
	{
		// Free old heightmap
		CleanUp();
	}
	else
	{
		if (m_pHeightmap)
		{
			// Remember old state.
			prevHeightmap = m_pHeightmap;
			m_pHeightmap = 0;
		}
		if (m_LayerIdBitmap.IsValid())
		{
			prevLayerIdBitmap.Allocate(m_iWidth, m_iHeight);
			memcpy(prevLayerIdBitmap.GetData(), m_LayerIdBitmap.GetData(), prevLayerIdBitmap.GetSize());
		}
	}

	// Save width and height
	m_iWidth = iWidth;
	m_iHeight = iHeight;
	m_unitSize = unitSize;

	int sectorSize = m_unitSize * SECTOR_SIZE_IN_UNITS;
	m_numSectors = (m_iWidth * m_unitSize) / sectorSize;

	bool boChangedTerrainDimensions(prevWidth != m_iWidth || prevHeight != m_iHeight || prevUnitSize != m_unitSize);
	ITerrain* pTerrain = GetIEditorImpl()->Get3DEngine()->GetITerrain();

	uint32 dwTerrainSizeInMeters = m_iWidth * unitSize;
	uint32 dwTerrainTextureResolution = dwTerrainSizeInMeters * 2.f; // by default use 1 texel = 0.5 unit
	dwTerrainTextureResolution = min(dwTerrainTextureResolution, (uint32)16384);

	uint32 dwTileResolution = 512;      // to avoid having too many tiles we try to get big ones

										// As dwTileCount must be at least 1, dwTerrainTextureResolution must be at most equals to
										// dwTerrainTextureResolution.
	if (dwTerrainTextureResolution <= dwTileResolution)
		dwTileResolution = dwTerrainTextureResolution;

	uint32 dwTileCount = dwTerrainTextureResolution / dwTileResolution;

	m_TerrainRGBTexture.AllocateTiles(dwTileCount, dwTileCount, dwTileResolution);

	// Allocate new data
	if (m_pHeightmap)
		delete[] m_pHeightmap;
	m_pHeightmap = new t_hmap[iWidth * iHeight];

	CryLog("allocating editor height map (%dx%d)*4", iWidth, iHeight);

	Verify();

	m_LayerIdBitmap.Allocate(iWidth, iHeight);

	if (prevWidth < m_iWidth || prevHeight < m_iHeight || prevUnitSize < m_unitSize)
		Clear();

	if (bCleanOld)
	{
		// Set to zero
		Clear();
	}
	else
	{
		// Copy from previous data.
		if (prevHeightmap && prevLayerIdBitmap.IsValid())
		{
			CWaitCursor wait;
			if (prevUnitSize == m_unitSize)
				CopyFrom(prevHeightmap, prevLayerIdBitmap.GetData(), prevWidth);
			else
				CopyFromInterpolate(prevHeightmap, prevLayerIdBitmap.GetData(), prevWidth, prevUnitSize);
		}
	}

	if (prevHeightmap)
		delete[] prevHeightmap;

	m_terrainGrid->InitSectorGrid(m_numSectors);
	m_terrainGrid->SetResolution(m_textureSize);

	// This must run only when creating a new terrain.
	if (!boChangedTerrainDimensions || !pTerrain)
	{
		CVegetationMap* pVegetationMap = GetIEditorImpl()->GetVegetationMap();
		if (pVegetationMap)
			pVegetationMap->Allocate(m_unitSize * max(m_iWidth, m_iHeight), bForceKeepVegetation | !bCleanOld);
	}

	if (!bCleanOld)
	{
		int numLayers = GetIEditorImpl()->GetTerrainManager()->GetLayerCount();
		for (int i = 0; i < numLayers; i++)
		{
			CLayer* pLayer = GetIEditorImpl()->GetTerrainManager()->GetLayer(i);
			pLayer->AllocateMaskGrid();
		}
	}

	if (boChangedTerrainDimensions)
	{
		if (pTerrain)
		{
			static bool bNoReentrant = false;
			if (!bNoReentrant)
			{
				bNoReentrant = true;

				// disable render world calls during resize operation
				GetIEditorImpl()->GetGameEngine()->SetLevelLoaded(false);

				// collect all objects in level
				AABB aabbAll(Vec3(-100000), Vec3(100000));
				int numObjects = gEnv->p3DEngine->GetObjectsInBox(aabbAll, 0);
				std::vector<IRenderNode*> lstInstances;
				lstInstances.resize(numObjects);
				IRenderNode ** pFirst = &lstInstances[0];
				gEnv->p3DEngine->GetObjectsInBox(aabbAll, pFirst);

				// unregister all objects from scene graph
				for (int i = 0; i < lstInstances.size(); i++)
				{
					gEnv->p3DEngine->UnRegisterEntityDirect(lstInstances[i]);
				}

				// recreate terrain in the engine
				GetIEditorImpl()->GetHeightmap()->InitTerrain();
				GetIEditorImpl()->GetHeightmap()->UpdateEngineTerrain();
				
				// update vegetation object types (lost in terrain re-creation)
				GetIEditorImpl()->GetVegetationMap()->SetEngineObjectsParams();

				// register all objects back into scene graph
				for (int i = 0; i < lstInstances.size(); i++)
				{
					gEnv->p3DEngine->RegisterEntity(lstInstances[i]);
				}

				// restore level rendering
				GetIEditorImpl()->GetGameEngine()->SetLevelLoaded(true);

				bNoReentrant = false;
			}
		}
	}

	// We modified the heightmap.
	GetIEditorImpl()->GetTerrainManager()->SetModified();
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::PaintLayerId(const float fpx, const float fpy, const SEditorPaintBrush& brush, const uint32 dwLayerId)
{
	assert(dwLayerId <= CLayer::e_hole);
	uint8 ucLayerInfoData = dwLayerId & (CLayer::e_hole | CLayer::e_undefined);

	SEditorPaintBrush cpyBrush = brush;

	cpyBrush.bBlended = cpyBrush.hardness < 1.f;
	cpyBrush.color = ucLayerInfoData;

	CImagePainter painter;

	painter.PaintBrush(fpx, fpy, m_LayerIdBitmap, cpyBrush);
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::EraseLayerID(uchar id)
{
	for (uint32 y = 0; y < m_iHeight; ++y)
		for (uint32 x = 0; x < m_iWidth; ++x)
		{
			SSurfaceTypeItem& st = m_LayerIdBitmap.ValueAt(x, y);

			if (st.GetDominatingSurfaceType() == id)
			{
				bool hole = st.GetHole();
				st = CLayer::e_undefined;
				st.SetHole(hole);
			}
		}
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::MarkUsedLayerIds(bool bFree[CLayer::e_undefined]) const
{
	for (uint32 dwY = 0; dwY < m_iHeight; ++dwY)
	{
		for (uint32 dwX = 0; dwX < m_iWidth; ++dwX)
		{
			SSurfaceTypeItem st = GetLayerInfoAt(dwX, dwY);

			for (int s = 0; s < SSurfaceTypeItem::kMaxSurfaceTypesNum; s++)
			{
				if (st.we[s] && st.ty[s] < CLayer::e_undefined)
				{
					bFree[st.ty[s]] = false; // it's possible to have undefined layers here
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CPoint CHeightmap::WorldToHmap(const Vec3& wp)
{
	//swap x/y.
	return CPoint(wp.y / m_unitSize, wp.x / m_unitSize);
}

//////////////////////////////////////////////////////////////////////////
Vec3 CHeightmap::HmapToWorld(CPoint hpos)
{
	return Vec3(hpos.y * m_unitSize, hpos.x * m_unitSize, 0);
}

//////////////////////////////////////////////////////////////////////////
CRect CHeightmap::WorldBoundsToRect(const AABB& worldBounds)
{
	CPoint p1 = WorldToHmap(worldBounds.min);
	CPoint p2 = WorldToHmap(worldBounds.max);
	if (p1.x > p2.x)
		std::swap(p1.x, p2.x);
	if (p1.y > p2.y)
		std::swap(p1.y, p2.y);
	CRect rc;
	rc.SetRect(p1, p2);
	rc.IntersectRect(rc, CRect(0, 0, m_iWidth, m_iHeight));
	return rc;
}

void CHeightmap::Clear(bool bClearLayerBitmap)
{
	if (m_iWidth && m_iHeight)
	{
		if (CUndo::IsRecording())
			CUndo::Record(new CUndoHeightmapModify(this));

		InitHeight(kInitHeight);

		if (bClearLayerBitmap)
		{
			m_LayerIdBitmap.Clear();
		}
	}
}

void CHeightmap::InitHeight(float fHeight)
{
	if (fHeight > m_fMaxHeight)
		fHeight = m_fMaxHeight;

	for (int i = 0; i < m_iWidth; ++i)
		for (int j = 0; j < m_iHeight; ++j)
		{
			SetXY(i, j, fHeight);
		}
}

void CHeightmap::SetMaxHeight(float fMaxHeight)
{
	if (CUndo::IsRecording())
		CUndo::Record(new CUndoHeightmapModify(this));

	m_fMaxHeight = fMaxHeight;
	if (m_fMaxHeight < 1.0f)
		m_fMaxHeight = 1.0f;

	// Clamp heightmap to the max height.
	int nSize = GetWidth() * GetHeight();
	for (int i = 0; i < nSize; i++)
	{
		if (m_pHeightmap[i] > m_fMaxHeight)
			m_pHeightmap[i] = m_fMaxHeight;
	}
}

void CHeightmap::LoadImage(LPCSTR pszFileName)
{
	////////////////////////////////////////////////////////////////////////
	// Load a BMP file as current heightmap
	////////////////////////////////////////////////////////////////////////
	CImageEx image;
	CImageEx tmpImage;
	CImageEx hmap;

	if (!CImageUtil::LoadImage(pszFileName, tmpImage))
	{
		CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Load image failed."));
		return;
	}

	image.RotateOrt(tmpImage, 3);

	if (image.GetWidth() != m_iWidth || image.GetHeight() != m_iHeight)
	{
		hmap.Allocate(m_iWidth, m_iHeight);
		CImageUtil::ScaleToFit(image, hmap);
	}
	else
	{
		hmap.Attach(image);
	}

	float fInvPrecisionScale = 1.0f / GetBytePrecisionScale();

	uint32* pData = hmap.GetData();
	int size = std::min(hmap.GetWidth() * hmap.GetHeight(), int(m_iWidth * m_iHeight));
	for (int i = 0; i < size; i++)
	{
		// Extract a color channel and rescale the value
		// before putting it into the heightmap
		int col = pData[i] & 0x000000FF;
		m_pHeightmap[i] = (float)col * fInvPrecisionScale;
	}

	// We modified the heightmap.
	GetIEditorImpl()->GetTerrainManager()->SetModified();

	CryLog("Heightmap loaded from file %s", pszFileName);
}

void CHeightmap::SaveImage(LPCSTR pszFileName)
{
	uint32* pImageData = NULL;
	unsigned int i, j;
	uint8 iColor;
	t_hmap* pHeightmap = NULL;
	UINT iWidth = GetWidth();
	UINT iHeight = GetHeight();

	// Allocate memory to export the heightmap
	CImageEx image;
	image.Allocate(iWidth, iHeight);
	pImageData = image.GetData();

	// Get a pointer to the heightmap data
	pHeightmap = GetData();

	float fPrecisionScale = GetBytePrecisionScale();

	// BMP
	// Write heightmap into the image data array
	for (j = 0; j < iHeight; j++)
	{
		for (i = 0; i < iWidth; i++)
		{
			// Get a normalized grayscale value from the heigthmap
			iColor = (uint8)__min((pHeightmap[i + j * iWidth] * fPrecisionScale + 0.5f), 255.0f);

			// Create a BGR grayscale value and store it in the image
			// data array
			pImageData[i + j * iWidth] = (iColor << 16) | (iColor << 8) | iColor;
		}
	}

	CImageEx newImage;
	newImage.RotateOrt(image, 1);

	// Save the heightmap into the bitmap
	CImageUtil::SaveImage(pszFileName, newImage);
}

void CHeightmap::SavePGM(const string& pgmFile)
{
	CImageEx image;
	image.Allocate(m_iWidth, m_iHeight);

	float fPrecisionScale = GetShortPrecisionScale();
	for (int j = 0; j < m_iHeight; j++)
	{
		for (int i = 0; i < m_iWidth; i++)
		{
			unsigned int h = int(GetXY(i, j) * fPrecisionScale + 0.5f);
			if (h > 0xFFFF) h = 0xFFFF;
			image.ValueAt(i, j) = int(h);
		}
	}

	CImageEx newImage;
	newImage.RotateOrt(image, 1);
	CImageUtil::SaveImage(pgmFile, newImage);
}

void CHeightmap::LoadPGM(const string& pgmFile)
{
	CImageEx image;
	CImageEx tmpImage;
	CImageUtil::LoadImage(pgmFile, tmpImage);

	image.RotateOrt(tmpImage, 3);

	if (image.GetWidth() != m_iWidth || image.GetHeight() != m_iHeight)
	{
		string str;
		str.Format("PGM dimensions do not match dimensions of heighmap.\nPGM size is %dx%d,\nHeightmap size is %dx%d.\nProcceed with clipping?",
		           image.GetWidth(), image.GetHeight(), m_iWidth, m_iHeight);

		if (QDialogButtonBox::StandardButton::Yes != CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr(str)), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::No)
		{
			return;
		}
	}

	float fPrecisionScale = GetShortPrecisionScale();

	for (int j = 0, minheight = min(m_iHeight, uint64(image.GetHeight())); j < minheight; j++)
	{
		for (int i = 0, minwidth = min(m_iWidth, uint64(image.GetWidth())); i < minwidth; i++)
		{
			GetXY(i, j) = image.ValueAt(i, j) / fPrecisionScale;
		}
	}

	// We modified the heightmap.
	GetIEditorImpl()->GetTerrainManager()->SetModified();
}

//! Save heightmap in RAW format.
void CHeightmap::SaveRAW(const string& rawFile)
{
	string str;
	FILE* file = fopen(rawFile, "wb");
	if (!file)
	{
		str.Format("Error saving file %s", (const char*)rawFile);
		CryMessageBox(str, "Warning", eMB_Error);
		return;
	}

	CWordImage image;
	image.Allocate(m_iWidth, m_iHeight);

	float fPrecisionScale = GetShortPrecisionScale();

	for (int j = 0; j < m_iHeight; j++)
	{
		for (int i = 0; i < m_iWidth; i++)
		{
			unsigned int h = int(GetXY(i, j) * fPrecisionScale + 0.5f);
			if (h > 0xFFFF) h = 0xFFFF;
			image.ValueAt(i, j) = h;
		}
	}

	CWordImage newImage;
	newImage.RotateOrt(image, 1);

	fwrite(newImage.GetData(), newImage.GetSize(), 1, file);

	fclose(file);
}

//! Load heightmap from RAW format.
void CHeightmap::LoadRAW(const string& rawFile)
{
	string str;
	FILE* file = fopen(rawFile, "rb");
	if (!file)
	{
		str.Format("Error loading file %s", (const char*)rawFile);
		CryMessageBox(str, "Warning", eMB_Error);
		return;
	}
	fseek(file, 0, SEEK_END);
	int fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);

	float fPrecisionScale = GetShortPrecisionScale();
	if (fileSize != m_iWidth * m_iHeight * 2)
	{
		str.Format("Bad RAW file, RAW file must be %dxd 16bit image", m_iWidth, m_iHeight);
		CryMessageBox( str, "Warning", eMB_Error);
		fclose(file);
		return;
	}
	CWordImage image;
	CWordImage tmpImage;
	tmpImage.Allocate(m_iWidth, m_iHeight);
	fread(tmpImage.GetData(), tmpImage.GetSize(), 1, file);
	fclose(file);

	image.RotateOrt(tmpImage, 3);

	for (int j = 0; j < m_iHeight; j++)
	{
		for (int i = 0; i < m_iWidth; i++)
		{
			SetXY(i, j, image.ValueAt(i, j) / fPrecisionScale);
		}
	}

	// We modified the heightmap.
	GetIEditorImpl()->GetTerrainManager()->SetModified();
}

void CHeightmap::Noise()
{
	////////////////////////////////////////////////////////////////////////
	// Add noise to the heightmap
	////////////////////////////////////////////////////////////////////////
	InitNoise();
	assert(m_pNoise);

	Verify();

	const float fInfluence = 10.0f;

	// Calculate the way we have to swap the noise. We do this to avoid
	// singularities when a noise array is aplied more than once
	srand(clock());
	UINT iNoiseSwapMode = rand() % 5;

	// Calculate a noise offset for the same reasons
	UINT iNoiseOffsetX = rand() % NOISE_ARRAY_SIZE;
	UINT iNoiseOffsetY = rand() % NOISE_ARRAY_SIZE;

	UINT iNoiseX, iNoiseY;

	for (uint64 j = 1; j < m_iHeight - 1; j++)
	{
		// Precalculate for better speed
		uint64 iCurPos = j * m_iWidth + 1;

		for (uint64 i = 1; i < m_iWidth - 1; i++)
		{
			// Next pixel
			iCurPos++;

			// Skip amnything below the water level
			if (m_pHeightmap[iCurPos] > 0.0f && m_pHeightmap[iCurPos] >= m_fWaterLevel)
			{
				// Swap the noise
				switch (iNoiseSwapMode)
				{
				case 0:
					iNoiseX = i;
					iNoiseY = j;
					break;
				case 1:
					iNoiseX = j;
					iNoiseY = i;
					break;
				case 2:
					iNoiseX = m_iWidth - i;
					iNoiseY = j;
					break;
				case 3:
					iNoiseX = i;
					iNoiseY = m_iHeight - j;
					break;
				case 4:
					iNoiseX = m_iWidth - i;
					iNoiseY = m_iHeight - j;
					break;
				}

				// Add the random noise offset
				iNoiseX += iNoiseOffsetX;
				iNoiseY += iNoiseOffsetY;

				float fInfluenceValue = m_pNoise->m_Array[iNoiseX % NOISE_ARRAY_SIZE][iNoiseY % NOISE_ARRAY_SIZE] / NOISE_RANGE * fInfluence - fInfluence / 2;

				// Add the signed noise
				m_pHeightmap[iCurPos] = __min(m_fMaxHeight, __max(m_fWaterLevel, m_pHeightmap[iCurPos] + fInfluenceValue));
			}
		}
	}

	// We modified the heightmap.
	GetIEditorImpl()->GetTerrainManager()->SetModified();
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::Smooth(CFloatImage& hmap, const CRect& rect)
{
	int w = hmap.GetWidth();
	int h = hmap.GetHeight();

	int x1 = max((int)rect.left + 2, 1);
	int y1 = max((int)rect.top + 2, 1);
	int x2 = min((int)rect.right - 2, w - 1);
	int y2 = min((int)rect.bottom - 2, h - 1);

	t_hmap* pData = hmap.GetData();

	int i, j, pos;
	// Smooth it
	for (j = y1; j < y2; j++)
	{
		pos = j * w;
		for (i = x1; i < x2; i++)
		{
			pData[i + pos] =
			  (pData[i + pos] +
			   pData[i + 1 + pos] +
			   pData[i - 1 + pos] +
			   pData[i + pos + w] +
			   pData[i + pos - w] +
			   pData[(i - 1) + pos - w] +
			   pData[(i + 1) + pos - w] +
			   pData[(i - 1) + pos + w] +
			   pData[(i + 1) + pos + w])
			  * (1.0f / 9.0f);
		}
	}
	/*
	   for (j=y2-1; j > y1; j--)
	   {
	   pos = j*w;
	   for (i=x2-1; i > x1; i--)
	   {
	    pData[i + pos] =
	      (pData[i + pos] +
	      pData[i + 1 + pos] +
	      pData[i - 1 + pos] +
	      pData[i + pos+w] +
	      pData[i + pos-w] +
	      pData[(i - 1) + pos-w] +
	      pData[(i + 1) + pos-w] +
	      pData[(i - 1) + pos+w] +
	      pData[(i + 1) + pos+w])
	 * (1.0f/9.0f);
	   }
	   }
	 */
}

void CHeightmap::Smooth()
{
	////////////////////////////////////////////////////////////////////////
	// Smooth the heightmap
	////////////////////////////////////////////////////////////////////////
	if (!IsAllocated())
		return;

	if (CUndo::IsRecording())
		CUndo::Record(new CUndoHeightmapModify(this));

	unsigned int i, j;

	Verify();

	// Smooth it
	for (i = 1; i < m_iWidth - 1; i++)
		for (j = 1; j < m_iHeight - 1; j++)
		{
			m_pHeightmap[i + j * m_iWidth] =
			  (m_pHeightmap[i + j * m_iWidth] +
			   m_pHeightmap[(i + 1) + j * m_iWidth] +
			   m_pHeightmap[i + (j + 1) * m_iWidth] +
			   m_pHeightmap[(i + 1) + (j + 1) * m_iWidth] +
			   m_pHeightmap[(i - 1) + j * m_iWidth] +
			   m_pHeightmap[i + (j - 1) * m_iWidth] +
			   m_pHeightmap[(i - 1) + (j - 1) * m_iWidth] +
			   m_pHeightmap[(i + 1) + (j - 1) * m_iWidth] +
			   m_pHeightmap[(i - 1) + (j + 1) * m_iWidth])
			  / 9.0f;
		}

	// We modified the heightmap.
	GetIEditorImpl()->GetTerrainManager()->SetModified();
}

void CHeightmap::Invert()
{
	////////////////////////////////////////////////////////////////////////
	// Invert the heightmap
	////////////////////////////////////////////////////////////////////////

	if (CUndo::IsRecording())
		CUndo::Record(new CUndoHeightmapModify(this));

	unsigned int i;

	Verify();

	for (i = 0; i < m_iWidth * m_iHeight; i++)
	{
		m_pHeightmap[i] = m_fMaxHeight - m_pHeightmap[i];
		if (m_pHeightmap[i] > m_fMaxHeight)
			m_pHeightmap[i] = m_fMaxHeight;
		if (m_pHeightmap[i] < 0)
			m_pHeightmap[i] = 0;
	}

	// We modified the heightmap.
	GetIEditorImpl()->GetTerrainManager()->SetModified();
}

void CHeightmap::Normalize()
{
	////////////////////////////////////////////////////////////////////////
	// Normalize the heightmap to a 0 - m_fMaxHeight range
	////////////////////////////////////////////////////////////////////////

	if (CUndo::IsRecording())
		CUndo::Record(new CUndoHeightmapModify(this));

	unsigned int i, j;
	float fLowestPoint = 512000.0f, fHighestPoint = -512000.0f;
	float fValueRange;
	float fHeight;

	Verify();

	// Find the value range
	for (i = 0; i < m_iWidth; i++)
		for (j = 0; j < m_iHeight; j++)
		{
			fLowestPoint = __min(fLowestPoint, GetXY(i, j));
			fHighestPoint = __max(fHighestPoint, GetXY(i, j));
		}

	// Storing the value range in this way saves us a division and a multiplication
	fValueRange = (1.0f / (fHighestPoint - (float) fLowestPoint)) * m_fMaxHeight;

	// Normalize the heightmap
	for (i = 0; i < m_iWidth; i++)
		for (j = 0; j < m_iHeight; j++)
		{
			fHeight = GetXY(i, j);

			//			fHeight += (float) fabs(fLowestPoint);
			fHeight -= fLowestPoint;
			fHeight *= fValueRange;

			//			fHeight=128.0f;

			SetXY(i, j, fHeight);
		}

	fLowestPoint = 512000.0f, fHighestPoint = -512000.0f;

	// Find the value range
	/*	for (i=0; i<m_iWidth; i++)
	    for (j=0; j<m_iHeight; j++)
	    {
	      fLowestPoint = __min(fLowestPoint, GetXY(i, j));
	      fHighestPoint = __max(fHighestPoint, GetXY(i, j));
	    }*/

	// We modified the heightmap.
	GetIEditorImpl()->GetTerrainManager()->SetModified();
}

bool CHeightmap::GetDataEx(t_hmap* pData, UINT iDestWidth, bool bSmooth, bool bNoise)
{
	////////////////////////////////////////////////////////////////////////
	// Retrieve heightmap data. Scaling and noising is optional
	////////////////////////////////////////////////////////////////////////

	unsigned int i, j;
	long iXSrcFl, iXSrcCe, iYSrcFl, iYSrcCe;
	float fXSrc, fYSrc;
	float fHeight[4];
	float fHeightWeight[4];
	float fHeightBottom;
	float fHeightTop;
	UINT dwHeightmapWidth = GetWidth();
	t_hmap* pDataStart = pData;

	Verify();

	bool bProgress = iDestWidth > 1024;
	// Only log significant allocations. This also prevents us from cluttering the
	// log file during the lightmap preview generation
	if (iDestWidth > 1024)
		CryLog("Retrieving heightmap data (Width: %i)...", iDestWidth);

	CWaitProgress wait("Scaling Heightmap", bProgress);

	// Loop trough each field of the new image and interpolate the value
	// from the source heightmap
	for (j = 0; j < iDestWidth; j++)
	{
		if (bProgress)
		{
			if (!wait.Step(j * 100 / iDestWidth))
				return false;
		}

		// Calculate the average source array position
		fYSrc = ((float) j / (float) iDestWidth) * dwHeightmapWidth;
		assert(fYSrc >= 0.0f && fYSrc <= dwHeightmapWidth);

		// Precalculate floor and ceiling values. Use fast asm integer floor and
		// fast asm float / integer conversion
		iYSrcFl = (int)floor(fYSrc);
		iYSrcCe = iYSrcFl + 1;

		// Clamp the ceiling coordinates to a save range
		if (iYSrcCe >= (int) dwHeightmapWidth)
			iYSrcCe = dwHeightmapWidth - 1;

		// Distribution between top and bottom height values
		fHeightWeight[3] = fYSrc - (float)iYSrcFl;
		fHeightWeight[2] = 1.0f - fHeightWeight[3];

		for (i = 0; i < iDestWidth; i++)
		{
			// Calculate the average source array position
			fXSrc = ((float) i / (float) iDestWidth) * dwHeightmapWidth;
			assert(fXSrc >= 0.0f && fXSrc <= dwHeightmapWidth);

			// Precalculate floor and ceiling values. Use fast asm integer floor and
			// fast asm float / integer conversion
			iXSrcFl = (int)floor(fXSrc);
			iXSrcCe = iXSrcFl + 1;

			// Distribution between left and right height values
			fHeightWeight[1] = fXSrc - (float)iXSrcFl;
			fHeightWeight[0] = 1.0f - fHeightWeight[1];
			/*
			      // Avoid error when floor() and ceil() return the same value
			      if (fHeightWeight[0] == 0.0f && fHeightWeight[1] == 0.0f)
			      {
			        fHeightWeight[0] = 0.5f;
			        fHeightWeight[1] = 0.5f;
			      }

			      // Calculate how much weight each height value has

			      // Avoid error when floor() and ceil() return the same value
			      if (fHeightWeight[2] == 0.0f && fHeightWeight[3] == 0.0f)
			      {
			        fHeightWeight[2] = 0.5f;
			        fHeightWeight[3] = 0.5f;
			      }
			 */

			if (iXSrcCe >= (int) dwHeightmapWidth)
				iXSrcCe = dwHeightmapWidth - 1;

			// Get the four nearest height values
			fHeight[0] = (float) m_pHeightmap[iXSrcFl + iYSrcFl * dwHeightmapWidth];
			fHeight[1] = (float) m_pHeightmap[iXSrcCe + iYSrcFl * dwHeightmapWidth];
			fHeight[2] = (float) m_pHeightmap[iXSrcFl + iYSrcCe * dwHeightmapWidth];
			fHeight[3] = (float) m_pHeightmap[iXSrcCe + iYSrcCe * dwHeightmapWidth];

			// Interpolate between the four nearest height values

			// Get the height for the given X position trough interpolation between
			// the left and the right height
			fHeightBottom = (fHeight[0] * fHeightWeight[0] + fHeight[1] * fHeightWeight[1]);
			fHeightTop = (fHeight[2] * fHeightWeight[0] + fHeight[3] * fHeightWeight[1]);

			// Set the new value in the destination heightmap
			*pData++ = (t_hmap) (fHeightBottom * fHeightWeight[2] + fHeightTop * fHeightWeight[3]);
		}
	}

	if (bNoise)
	{
		InitNoise();

		pData = pDataStart;
		// Smooth it
		for (i = 1; i < iDestWidth - 1; i++)
			for (j = 1; j < iDestWidth - 1; j++)
			{
				*pData++ += cry_random(0.0f, 1.0f / 16.0f);
			}
	}

	if (bSmooth)
	{
		CFloatImage img;
		img.Attach(pDataStart, iDestWidth, iDestWidth);
		Smooth(img, CRect(0, 0, iDestWidth, iDestWidth));
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CHeightmap::GetData(const CRect& srcRect, const int resolution, const CPoint& vTexOffset, CFloatImage& hmap, bool bSmooth, bool bNoise)
{
	////////////////////////////////////////////////////////////////////////
	// Retrieve heightmap data. Scaling and noising is optional
	////////////////////////////////////////////////////////////////////////

	if (!m_pHeightmap)
		return false;

	unsigned int i, j;
	int iXSrcFl, iXSrcCe, iYSrcFl, iYSrcCe;
	float fXSrc, fYSrc;
	float fHeight[4];
	float fHeightWeight[4];
	float fHeightBottom;
	float fHeightTop;
	UINT dwHeightmapWidth = GetWidth();

	int width = hmap.GetWidth();

	// clip within source
	int x1 = max((int)srcRect.left, 0);
	int y1 = max((int)srcRect.top, 0);
	int x2 = min((int)srcRect.right, resolution);
	int y2 = min((int)srcRect.bottom, resolution);

	// clip within dest hmap
	x1 = max(x1, (int)vTexOffset.x);
	y1 = max(y1, (int)vTexOffset.y);
	x2 = min(x2, hmap.GetWidth() + (int)vTexOffset.x);
	y2 = min(y2, hmap.GetHeight() + (int)vTexOffset.y);

	float fScaleX = m_iWidth / (float)hmap.GetWidth();
	float fScaleY = m_iHeight / (float)hmap.GetHeight();

	t_hmap* pDataStart = hmap.GetData();

	int trgW = x2 - x1;

	bool bProgress = trgW > 1024;
	CWaitProgress wait("Scaling Heightmap", bProgress);

	// Loop trough each field of the new image and interpolate the value
	// from the source heightmap
	for (j = y1; j < y2; j++)
	{
		if (bProgress)
		{
			if (!wait.Step((j - y1) * 100 / (y2 - y1)))
				return false;
		}

		t_hmap* pData = &pDataStart[(j - vTexOffset.y) * width + x1 - vTexOffset.x];

		// Calculate the average source array position
		fYSrc = j * fScaleY;
		assert(fYSrc >= 0.0f && fYSrc <= dwHeightmapWidth);

		// Precalculate floor and ceiling values. Use fast asm integer floor and
		// fast asm float / integer conversion
		iYSrcFl = (int)floor(fYSrc);
		iYSrcCe = iYSrcFl + 1;

		// Clamp the ceiling coordinates to a save range
		if (iYSrcCe >= (int) dwHeightmapWidth)
			iYSrcCe = dwHeightmapWidth - 1;

		// Distribution between top and bottom height values
		fHeightWeight[3] = fYSrc - (float)iYSrcFl;
		fHeightWeight[2] = 1.0f - fHeightWeight[3];

		for (i = x1; i < x2; i++)
		{
			// Calculate the average source array position
			fXSrc = i * fScaleX;
			assert(fXSrc >= 0.0f && fXSrc <= dwHeightmapWidth);

			// Precalculate floor and ceiling values. Use fast asm integer floor and
			// fast asm float / integer conversion
			iXSrcFl = (int)floor(fXSrc);
			iXSrcCe = iXSrcFl + 1;

			if (iXSrcCe >= (int) dwHeightmapWidth)
				iXSrcCe = dwHeightmapWidth - 1;

			// Distribution between left and right height values
			fHeightWeight[1] = fXSrc - (float)iXSrcFl;
			fHeightWeight[0] = 1.0f - fHeightWeight[1];

			// Get the four nearest height values
			fHeight[0] = (float) m_pHeightmap[iXSrcFl + iYSrcFl * dwHeightmapWidth];
			fHeight[1] = (float) m_pHeightmap[iXSrcCe + iYSrcFl * dwHeightmapWidth];
			fHeight[2] = (float) m_pHeightmap[iXSrcFl + iYSrcCe * dwHeightmapWidth];
			fHeight[3] = (float) m_pHeightmap[iXSrcCe + iYSrcCe * dwHeightmapWidth];

			// Interpolate between the four nearest height values

			// Get the height for the given X position trough interpolation between
			// the left and the right height
			fHeightBottom = (fHeight[0] * fHeightWeight[0] + fHeight[1] * fHeightWeight[1]);
			fHeightTop = (fHeight[2] * fHeightWeight[0] + fHeight[3] * fHeightWeight[1]);

			// Set the new value in the destination heightmap
			*pData++ = (t_hmap) (fHeightBottom * fHeightWeight[2] + fHeightTop * fHeightWeight[3]);
		}
	}

	// Only if requested resolution, higher then current resolution.
	if (resolution > m_iWidth)
		if (bNoise)
		{
			InitNoise();

			int ye = hmap.GetHeight();
			int xe = hmap.GetWidth();

			t_hmap* pData = pDataStart;

			// add noise
			for (int y = 0; y < ye; y++)
			{
				for (int x = 0; x < xe; x++)
				{
					*pData++ += cry_random(0.0f, 1.0f / 16.0f);
					//*pData++ += cry_random(0.0f, 1.0f / 8.0f)
				}
			}
		}

	if (bSmooth)
		Smooth(hmap, srcRect - vTexOffset);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CHeightmap::GetPreviewBitmap(DWORD* pBitmapData, int width, bool bSmooth, bool bNoise, CRect* pUpdateRect, bool bShowWater)
{
	if (!m_pHeightmap)
		return false;

	CRect bounds(0, 0, width, width);

	CFloatImage hmap;
	hmap.Allocate(width, width);
	t_hmap* pHeightmap = hmap.GetData();

	if (pUpdateRect)
	{
		CRect destUpdateRect = *pUpdateRect;
		float fScale = (float)width / m_iWidth;
		destUpdateRect.left *= fScale;
		destUpdateRect.right *= fScale;
		destUpdateRect.top *= fScale;
		destUpdateRect.bottom *= fScale;

		if (!GetData(destUpdateRect, width, CPoint(0, 0), hmap, bSmooth, bNoise))
			return false;

		bounds.IntersectRect(bounds, destUpdateRect);
	}
	else
	{
		if (!GetDataEx(pHeightmap, width, bSmooth, bNoise))
			return false;
	}

	float fByteScale = GetBytePrecisionScale();
	const uint32* pWaterTexData = 0;
	int32 nWaterLevel = int(m_fWaterLevel * fByteScale);

	if (bShowWater)
	{
		// Load water texture.
		static CImageEx waterImage;
		if (waterImage.GetSize() == 0)
		{
			waterImage.Allocate(128, 128);
			CBitmap bmpLoad;
			VERIFY(bmpLoad.Attach(::LoadBitmap(AfxGetApp()->m_hInstance, MAKEINTRESOURCE(IDB_WATER))));
			VERIFY(bmpLoad.GetBitmapBits(128 * 128 * sizeof(DWORD), waterImage.GetData()));
			waterImage.SwapRedAndBlue();
		}
		pWaterTexData = waterImage.GetData();
	}

	// Fill the preview with the heightmap image
	for (int iY = bounds.top; iY < bounds.bottom; iY++)
		for (int iX = bounds.left; iX < bounds.right; iX++)
		{
			// Use the height value as grayscale if the current heightmap point is above the water
			// level. Use a texel from the tiled water texture when it is below the water level
			int32 val = int(pHeightmap[iX + iY * width] * fByteScale);
			pBitmapData[iX + iY * width] = (bShowWater && val < nWaterLevel) ? pWaterTexData[(iX % 128) + (iY % 128) * 128] : RGB(val, val, val);
		}

	return true;
}

void CHeightmap::GenerateTerrain(const SNoiseParams& noiseParam)
{
	////////////////////////////////////////////////////////////////////////
	// Generate a new terrain with the parameters stored in sParam
	////////////////////////////////////////////////////////////////////////

	if (CUndo::IsRecording())
		CUndo::Record(new CUndoHeightmapModify(this));

	// set reasonable max height depending on level size
	SetMaxHeight(min((float)GetIEditorImpl()->Get3DEngine()->GetTerrainSize() / 4.f + m_fWaterLevel, (float)HEIGHTMAP_MAX_HEIGHT));

	CDynamicArray2D cHeightmap(GetWidth(), GetHeight());
	CNoise cNoise;
	float fYScale = 255.0f;
	SNoiseParams sParam = noiseParam;

	ASSERT(sParam.iWidth == m_iWidth && sParam.iHeight == m_iHeight);

	//////////////////////////////////////////////////////////////////////
	// Generate the noise array
	//////////////////////////////////////////////////////////////////////

	AfxGetMainWnd()->BeginWaitCursor();

	CLogFile::WriteLine("Noise...");

	// Set the random value
	gEnv->pSystem->GetRandomGenerator().Seed(sParam.iRandom);

	// Process layers
	for (unsigned int i = 0; i < sParam.iPasses; i++)
	{
		// Apply the fractal noise function to the array
		cNoise.FracSynthPass(&cHeightmap, sParam.fFrequency, fYScale,
		                     sParam.iWidth, sParam.iHeight, FALSE);

		// Modify noise generation parameters
		sParam.fFrequency *= sParam.fFrequencyStep;
		if (sParam.fFrequency > 16000.f)
			sParam.fFrequency = 16000.f;
		fYScale *= sParam.fFade;
	}

	//////////////////////////////////////////////////////////////////////
	// Store the generated terrain in the heightmap
	//////////////////////////////////////////////////////////////////////

	for (unsigned int j = 0; j < m_iHeight; j++)
		for (unsigned int i = 0; i < m_iWidth; i++)
			SetXY(i, j, MAX(MIN(cHeightmap.m_Array[i][j], 512000.0f), -512000.0f));

	//////////////////////////////////////////////////////////////////////
	// Perform some modifications on the heightmap
	//////////////////////////////////////////////////////////////////////

	// Smooth the heightmap and normalize it
	for (unsigned int i = 0; i < sParam.iSmoothness; i++)
		Smooth();

	Normalize();
	MakeIsle();

	//////////////////////////////////////////////////////////////////////
	// Finished
	//////////////////////////////////////////////////////////////////////

	// We modified the heightmap.
	GetIEditorImpl()->GetTerrainManager()->SetModified();

	// All layers need to be generated from scratch
	GetIEditorImpl()->GetTerrainManager()->InvalidateLayers();

	AfxGetMainWnd()->EndWaitCursor();
}

float CHeightmap::CalcHeightScale() const
{
	return 1.0f / (float)(GetWidth() * GetUnitSize());
}

void CHeightmap::SmoothSlope()
{
	//////////////////////////////////////////////////////////////////////
	// Remove areas with high slope from the heightmap
	//////////////////////////////////////////////////////////////////////

	if (!IsAllocated())
		return;

	if (CUndo::IsRecording())
		CUndo::Record(new CUndoHeightmapModify(this));

	UINT iCurPos;
	float fAverage;
	unsigned int i, j;

	CLogFile::WriteLine("Smoothing the slope of the heightmap...");

	// Remove the high slope areas (horizontal)
	for (j = 1; j < m_iHeight - 1; j++)
	{
		// Precalculate for better speed
		iCurPos = j * m_iWidth + 1;

		for (i = 1; i < m_iWidth - 1; i++)
		{
			// Next pixel
			iCurPos++;

			// Get the average value for this area
			fAverage =
			  (m_pHeightmap[iCurPos] + m_pHeightmap[iCurPos + 1] + m_pHeightmap[iCurPos + m_iWidth] +
			   m_pHeightmap[iCurPos + m_iWidth + 1] + m_pHeightmap[iCurPos - 1] + m_pHeightmap[iCurPos - m_iWidth] +
			   m_pHeightmap[iCurPos - m_iWidth - 1] + m_pHeightmap[iCurPos - m_iWidth + 1] + m_pHeightmap[iCurPos + m_iWidth - 1])
			  * 0.11111111111f;

			// Clamp the surrounding values to the given level
			ClampToAverage(&m_pHeightmap[iCurPos], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos + 1], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos + m_iWidth], fAverage);
			// TODO: ClampToAverage(&m_pHeightmap[iCurPos + m_iWidth + 1], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos - 1], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos - m_iWidth], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos - m_iWidth - 1], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos - m_iWidth + 1], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos + m_iWidth - 1], fAverage);
		}
	}

	// Remove the high slope areas (vertical)
	for (i = 1; i < m_iWidth - 1; i++)
	{
		// Precalculate for better speed
		iCurPos = i;

		for (j = 1; j < m_iHeight - 1; j++)
		{
			// Next pixel
			iCurPos += m_iWidth;

			// Get the average value for this area
			fAverage =
			  (m_pHeightmap[iCurPos] + m_pHeightmap[iCurPos + 1] + m_pHeightmap[iCurPos + m_iWidth] +
			   m_pHeightmap[iCurPos + m_iWidth + 1] + m_pHeightmap[iCurPos - 1] + m_pHeightmap[iCurPos - m_iWidth] +
			   m_pHeightmap[iCurPos - m_iWidth - 1] + m_pHeightmap[iCurPos - m_iWidth + 1] + m_pHeightmap[iCurPos + m_iWidth - 1])
			  * 0.11111111111f;

			// Clamp the surrounding values to the given level
			ClampToAverage(&m_pHeightmap[iCurPos], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos + 1], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos + m_iWidth], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos + m_iWidth + 1], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos - 1], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos - m_iWidth], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos - m_iWidth - 1], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos - m_iWidth + 1], fAverage);
			ClampToAverage(&m_pHeightmap[iCurPos + m_iWidth - 1], fAverage);
		}
	}

	// We modified the heightmap.
	GetIEditorImpl()->GetTerrainManager()->SetModified();
}

void CHeightmap::ClampToAverage(t_hmap* pValue, float fAverage)
{
	//////////////////////////////////////////////////////////////////////
	// Used during slope removal to clamp height values into a normalized
	// rage
	//////////////////////////////////////////////////////////////////////

	float fClampedVal;

	// Does the heightvalue differ heavily from the average value ?
	if (fabs(*pValue - fAverage) > fAverage * 0.001f)
	{
		// Negativ / Positiv ?
		if (*pValue < fAverage)
			fClampedVal = fAverage - (fAverage * 0.001f);
		else
			fClampedVal = fAverage + (fAverage * 0.001f);

		// Renormalize it
		ClampHeight(fClampedVal);

		*pValue = fClampedVal;
	}
}

void CHeightmap::MakeIsle()
{
	//////////////////////////////////////////////////////////////////////
	// Convert any terrain into an isle
	//////////////////////////////////////////////////////////////////////

	int i, j;
	t_hmap* pHeightmapData = m_pHeightmap;
	//	UINT m_iHeightmapDiag;
	float fDeltaX, fDeltaY;
	float fDistance;
	float fCurHeight, fFade;

	CLogFile::WriteLine("Modifying heightmap to an isle...");
	if (CUndo::IsRecording())
		CUndo::Record(new CUndoHeightmapModify(this));
	// Calculate the length of the diagonale trough the heightmap
	//	m_iHeightmapDiag = (UINT) sqrt(GetWidth() * GetWidth() +
	//		GetHeight() * GetHeight());
	float fMaxDistance = sqrtf((GetWidth() / 2) * (GetWidth() / 2) + (GetHeight() / 2) * (GetHeight() / 2));

	for (j = 0; j < m_iHeight; j++)
	{
		// Calculate the distance delta
		fDeltaY = (float)abs((int)(j - m_iHeight / 2));

		for (i = 0; i < m_iWidth; i++)
		{
			// Calculate the distance delta
			fDeltaX = (float)abs((int)(i - m_iWidth / 2));

			// Calculate the distance
			fDistance = (float) sqrt(fDeltaX * fDeltaX + fDeltaY * fDeltaY);

			// Calculate the fade-off
			float fCosX = sinf((float)i / (float)m_iWidth * 3.1416f);
			float fCosY = sinf((float)j / (float)m_iHeight * 3.1416f);
			fFade = fCosX * fCosY;
			fFade = 1.0 - ((1.0f - fFade) * (1.0f - fFade));

			// Modify the value
			fCurHeight = *pHeightmapData;
			fCurHeight *= fFade;

			// Clamp
			ClampHeight(fCurHeight);

			// Write the value back and andvance
			*pHeightmapData++ = fCurHeight;
		}
	}

	// We modified the heightmap.
	GetIEditorImpl()->GetTerrainManager()->SetModified();
}

void CHeightmap::Flatten(float fFactor)
{
	////////////////////////////////////////////////////////////////////////
	// Increase the number of flat areas on the heightmap (TODO: Fix !)
	////////////////////////////////////////////////////////////////////////

	t_hmap* pHeightmapData = m_pHeightmap;
	t_hmap* pHeightmapDataEnd = &m_pHeightmap[m_iWidth * m_iHeight];
	float fRes;

	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
		GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CUndoHeightmapModify(this));

	CLogFile::WriteLine("Flattening heightmap...");

	// Perform the conversion
	while (pHeightmapData != pHeightmapDataEnd)
	{
		// Get the exponential value for this height value
		fRes = ExpCurve(*pHeightmapData, 128.0f, 0.985f);

		// Is this part of the landscape a potential flat area ?
		// Only modify parts of the landscape that are above the water level
		if (fRes < 100 && *pHeightmapData > m_fWaterLevel)
		{
			// Yes, apply the factor to it
			*pHeightmapData = (t_hmap) (*pHeightmapData * fFactor);

			// When we use a factor greater than 0.5, we don't want to drop below
			// the water level
			*pHeightmapData++ = __max(m_fWaterLevel, *pHeightmapData);
		}
		else
		{
			// No, use the exponential function to make smooth transitions
			*pHeightmapData++ = (t_hmap) fRes;
		}
	}

	// We modified the heightmap.
	GetIEditorImpl()->GetTerrainManager()->SetModified();
}

float CHeightmap::ExpCurve(float v, float fCover, float fSharpness)
{
	//////////////////////////////////////////////////////////////////////
	// Exponential function
	//////////////////////////////////////////////////////////////////////

	float c;

	c = v - fCover;

	if (c < 0)
		c = 0;

	return m_fMaxHeight - (float) ((pow(fSharpness, c)) * m_fMaxHeight);
}

void CHeightmap::Copy(const AABB& srcBox, int targetRot, const Vec3& targetPos, const Vec3& dym, const Vec3& sourcePos, bool bOnlyVegetation, bool bOnlyTerrain)
{
	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
		GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CUndoHeightmapModify(this));

	LONG bx1 = WorldToHmap(srcBox.min).x;
	LONG by1 = WorldToHmap(srcBox.min).y;
	LONG bx2 = WorldToHmap(srcBox.max).x;
	LONG by2 = WorldToHmap(srcBox.max).y;

	LONG xc = (bx1 + bx2) / 2;
	LONG yc = (by1 + by2) / 2;

	LONG cx1 = bx1;
	LONG cy1 = by1;

	CPoint hmapPosStart = WorldToHmap(targetPos - dym / 2) - CPoint(cx1, cy1);

	if (targetRot == eRR_90 || targetRot == eRR_270)
	{
		cx1 = xc - (by2 - by1) / 2;
		cy1 = yc - (bx2 - bx1) / 2;
		hmapPosStart = WorldToHmap(targetPos - Vec3(dym.y / 2, dym.x / 2, dym.z)) - CPoint(cx1, cy1);
	}

#if CRY_PLATFORM_64BIT
	LONG blocksize = 4096;
#else
	LONG blocksize = 512;
#endif

	for (LONG by = by1; by < by2; by += blocksize)
		for (LONG bx = bx1; bx < bx2; bx += blocksize)
		{
			LONG cx = bx;
			LONG cy = by;

			if (targetRot == eRR_90)
			{
				cx = xc - (yc - by);
				cy = yc + (xc - bx) - blocksize;
				if (cy < cy1) cy = cy1;
			}
			else if (targetRot == eRR_270)
			{
				cx = xc + (yc - by) - blocksize;
				cy = yc - (xc - bx);
				if (cx < cx1) cx = cx1;
			}
			else if (targetRot == eRR_180)
			{
				cx = bx2 - 1 + bx1 - bx - blocksize;
				if (cx < bx1) cx = bx1;

				cy = by2 - 1 + by1 - by - blocksize;
				if (cy < by1) cy = by1;
			}

			// Move terrain heightmap block.

			CRect rcHeightmap(0, 0, GetWidth(), GetHeight());

			// left-top position of destination rectangle in Heightmap space
			CPoint posDst = hmapPosStart + CPoint(cx, cy);

			// source rectangle in Heightmap space
			CRect rcSrc(bx, by, min(bx + blocksize + 1, bx2), min(by + blocksize + 1, by2));
			// destination rectangle in Heightmap space
			CRect rcDst(posDst.x, posDst.y, posDst.x + rcSrc.Width(), posDst.y + rcSrc.Height());
			if (targetRot == eRR_90 || targetRot == eRR_270)
			{
				rcDst.right = posDst.x + rcSrc.Height();
				rcDst.bottom = posDst.y + rcSrc.Width();
			}

			// Crop destination region by terrain outside
			CRect rcCropDst(rcDst & rcHeightmap);
			CRect rcMar = Private_Heightmap::GetRectMargin(rcDst, rcCropDst, 4 - targetRot);
			CRect rcCropSrc = Private_Heightmap::CropRect(rcSrc, rcMar); // sync src with dst

			if (!rcCropSrc.IsRectEmpty())
			{
				CXmlArchive* archive = new CXmlArchive("Root");

				// don't need crop by source outside for Heightmap moving (only destination crop)
				// data (height values) from source outside will be filled by 0
				archive->bLoading = false;
				ExportBlock(rcCropSrc, *archive, false);

				archive->bLoading = true;
				ImportBlock(*archive, CPoint(rcCropDst.left, rcCropDst.top), true, (targetPos - sourcePos).z, bOnlyVegetation && !bOnlyTerrain, targetRot);

				delete archive;
			}

			// Crop source region by terrain outside
			rcCropSrc &= rcHeightmap;
			rcMar = Private_Heightmap::GetRectMargin(rcSrc, rcCropSrc, targetRot);
			rcCropDst = Private_Heightmap::CropRect(rcDst, rcMar); // sync dst with src

			if (!rcCropSrc.IsRectEmpty() && (!bOnlyVegetation || bOnlyTerrain))
			{
				CRGBLayer* pRGBLayer = GetIEditorImpl()->GetTerrainManager()->GetRGBLayer();
				uint32 nRes = pRGBLayer->CalcMaxLocalResolution((float)rcCropSrc.left / GetWidth(), (float)rcCropSrc.top / GetHeight(), (float)rcCropSrc.right / GetWidth(), (float)rcCropSrc.bottom / GetHeight());

				{
					CImageEx image;
					CImageEx tmpImage;
					image.Allocate((rcCropSrc.Width()) * nRes / GetWidth(), (rcCropSrc.Height()) * nRes / GetHeight());
					pRGBLayer->GetSubImageStretched(
					  (float)rcCropSrc.left / GetWidth(), (float)rcCropSrc.top / GetHeight(),
					  (float)rcCropSrc.right / GetWidth(), (float)rcCropSrc.bottom / GetHeight(), image);

					if (targetRot != eRR_0)
						tmpImage.RotateOrt(image, targetRot);

					pRGBLayer->SetSubImageStretched(
					  ((float)rcCropDst.left) / GetWidth(), ((float)rcCropDst.top) / GetHeight(),
					  ((float)rcCropDst.right) / GetWidth(), ((float)rcCropDst.bottom) / GetHeight(), targetRot ? tmpImage : image);
				}

				for (int x = 0; x <= rcCropSrc.Width(); x++)
					for (int y = 0; y <= rcCropSrc.Height(); y++)
					{
						SSurfaceTypeItem lid = GetLayerInfoAt(x + rcCropSrc.left, y + rcCropSrc.top);
						if (targetRot == eRR_90)
							SetLayerIdAt(rcCropDst.right - y, x, lid);
						else if (targetRot == eRR_180)
							SetLayerIdAt(rcCropDst.right - x, rcCropDst.bottom - y, lid);
						else if (targetRot == eRR_270)
							SetLayerIdAt(y, rcCropDst.bottom - x, lid);
						else
							SetLayerIdAt(x + rcCropDst.left, y + rcCropDst.top, lid);
					}

				UpdateLayerTexture(rcCropDst);
			}
		}
}

void CHeightmap::MakeBeaches()
{
	//////////////////////////////////////////////////////////////////////
	// Create flat areas around beaches
	//////////////////////////////////////////////////////////////////////
	if (!IsAllocated())
		return;

	CTerrainFormulaDlg TerrainFormulaDlg;
	TerrainFormulaDlg.m_dParam1 = 5;
	TerrainFormulaDlg.m_dParam2 = 1000;
	TerrainFormulaDlg.m_dParam3 = 1;
	if (TerrainFormulaDlg.DoModal() != IDOK)
		return;

	CLogFile::WriteLine("Applying formula ...");

	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
		GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CUndoHeightmapModify(this));

	unsigned int i, j;
	t_hmap* pHeightmapData = NULL;
	t_hmap* pHeightmapDataStart = NULL;
	double dCurHeight;

	// Get the water level
	double dWaterLevel = m_fWaterLevel;

	// Make the beaches
	for (j = 0; j < m_iHeight; j++)
	{
		for (i = 0; i < m_iWidth; i++)
		{
			dCurHeight = m_pHeightmap[i + j * m_iWidth];

			// Center water level at zero
			dCurHeight -= dWaterLevel;

			// do nothing with small values but increase big values
			dCurHeight = dCurHeight * (1.0 + fabs(dCurHeight) * TerrainFormulaDlg.m_dParam1);

			// scale back (can be automated)
			dCurHeight /= TerrainFormulaDlg.m_dParam2;

			// Convert the coordinates back to the old range
			dCurHeight += dWaterLevel;

			// move flat area up out of water
			dCurHeight += TerrainFormulaDlg.m_dParam3;

			// check range
			if (dCurHeight > m_fMaxHeight)
				dCurHeight = m_fMaxHeight;
			else if (dCurHeight < 0)
				dCurHeight = 0;

			// Write the value back and andvance to the next pixel
			m_pHeightmap[i + j * m_iWidth] = dCurHeight;
		}
	}

	m_pHeightmap[0] = 0;
	m_pHeightmap[1] = 255;

	// Normalize because the beach creation distorted the value range
	///	Normalize();

	// We modified the heightmap.
	GetIEditorImpl()->GetTerrainManager()->SetModified();
}

void CHeightmap::LowerRange(float fFactor)
{
	//////////////////////////////////////////////////////////////////////
	// Lower the value range of the heightmap, effectively making it
	// more flat
	//////////////////////////////////////////////////////////////////////

	unsigned int i;
	float fWaterHeight = m_fWaterLevel;

	CLogFile::WriteLine("Lowering range...");

	if (CUndo::IsRecording())
		CUndo::Record(new CUndoHeightmapModify(this));

	// Lower the range, make sure we don't put anything below the water level
	for (i = 0; i < m_iWidth * m_iHeight; i++)
		m_pHeightmap[i] = ((m_pHeightmap[i] - fWaterHeight) * fFactor) + fWaterHeight;

	// We modified the heightmap.
	GetIEditorImpl()->GetTerrainManager()->SetModified();
}

void CHeightmap::Randomize()
{
	////////////////////////////////////////////////////////////////////////
	// Add a small amount of random noise
	////////////////////////////////////////////////////////////////////////

	unsigned int i;

	CLogFile::WriteLine("Lowering range...");

	// Add the noise
	for (i = 0; i < m_iWidth * m_iHeight; i++)
		m_pHeightmap[i] += cry_random(-4.0f, 4.0f);

	// Normalize because we might have valid the valid range
	Normalize();

	// We modified the heightmap.
	GetIEditorImpl()->GetTerrainManager()->SetModified();
}

void CHeightmap::DrawSpot(int iX, int iY, int radius, float insideRadius, float fHeight, float fHardness, CImageEx* pDisplacementMap, float displacementScale)
{
	////////////////////////////////////////////////////////////////////////
	// Draw an attenuated spot on the map
	////////////////////////////////////////////////////////////////////////
	int i, j;
	int iPosX, iPosY, iIndex;
	float fMaxDist, fAttenuation, fYSquared;
	float fCurHeight;

	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
		GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CUndoHeightmapModify(iX - radius, iY - radius, radius * 2, radius * 2, this));

	// Calculate the maximum distance
	fMaxDist = radius;

	for (j = (long) -radius; j < radius; j++)
	{
		// Precalculate
		iPosY = iY + j;
		fYSquared = (float) (j * j);

		for (i = (long) -radius; i < radius; i++)
		{
			// Calculate the position
			iPosX = iX + i;

			// Skip invalid locations
			if (iPosX < 0 || iPosY < 0 || iPosX > m_iWidth - 1 || iPosY > m_iHeight - 1)
				continue;

			// Only circle.
			float dist = sqrtf(fYSquared + i * i);
			if (dist > fMaxDist)
				continue;

			// Calculate the array index
			iIndex = iPosX + iPosY * m_iWidth;

			// Calculate attenuation factor
			if (dist <= insideRadius)
				fAttenuation = 1.0f;
			else
				fAttenuation = 1.0f - __min(1.0f, (dist - insideRadius) / max(0.01f, fMaxDist - insideRadius));

			// Set to height mode, modify the location towards the specified height
			fCurHeight = m_pHeightmap[iIndex];
			float dh = fHeight - fCurHeight;

			if (pDisplacementMap)
			{
				int sizeX = pDisplacementMap->GetWidth();
				int sizeY = pDisplacementMap->GetHeight();

				int nx = int((iPosX - iX) / fMaxDist * (float)sizeX * 0.5f);
				int ny = int((iPosY - iY) / fMaxDist * (float)sizeY * 0.5f);

				nx = CLAMP(nx + sizeX / 2, 0, sizeX - 1);
				ny = CLAMP(ny + sizeY / 2, 0, sizeY - 1);

				float displacementValue = (1.f / 255.f) * (float)ColorB(pDisplacementMap->GetData()[nx * sizeY + ny]).g;

				dh = (fHeight + displacementScale * displacementValue) - fCurHeight;
			}

			float h = fCurHeight + (fAttenuation)* dh * fHardness;

			// Clamp
			ClampHeight(h);
			m_pHeightmap[iIndex] = h;
		}
	}

	// We modified the heightmap.
	GetIEditorImpl()->GetTerrainManager()->SetModified(iX - radius, iY - radius, iX + radius, iY + radius);
}

void CHeightmap::RiseLowerSpot(int iX, int iY, int radius, float insideRadius, float fHeight, float fHardness, class CImageEx* pDisplacementMap, float displacementScale)
{
	////////////////////////////////////////////////////////////////////////
	// Draw an attenuated spot on the map
	////////////////////////////////////////////////////////////////////////
	int i, j;
	int iPosX, iPosY, iIndex;
	float fMaxDist, fAttenuation, fYSquared;
	float fCurHeight;

	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
		GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CUndoHeightmapModify(iX - radius, iY - radius, radius * 2, radius * 2, this));

	// Calculate the maximum distance
	fMaxDist = radius;

	for (j = (long) -radius; j < radius; j++)
	{
		// Precalculate
		iPosY = iY + j;
		fYSquared = (float) (j * j);

		for (i = (long) -radius; i < radius; i++)
		{
			// Calculate the position
			iPosX = iX + i;

			// Skip invalid locations
			if (iPosX < 0 || iPosY < 0 || iPosX > m_iWidth - 1 || iPosY > m_iHeight - 1)
				continue;

			// Only circle.
			float dist = sqrtf(fYSquared + i * i);
			if (dist > fMaxDist)
				continue;

			// Calculate the array index
			iIndex = iPosX + iPosY * m_iWidth;

			// Calculate attenuation factor
			if (dist <= insideRadius)
				fAttenuation = 1.0f;
			else
				fAttenuation = 1.0f - __min(1.0f, (dist - insideRadius) / max(0.01f, fMaxDist - insideRadius));

			// Set to height mode, modify the location towards the specified height
			fCurHeight = m_pHeightmap[iIndex];
			float dh = fHeight;

			float h = fCurHeight + (fAttenuation) * dh * fHardness;

			if (pDisplacementMap)
			{
				int sizeX = pDisplacementMap->GetWidth();
				int sizeY = pDisplacementMap->GetHeight();

				int nx = int((iPosX - iX) / fMaxDist * (float)sizeX * 0.5f);
				int ny = int((iPosY - iY) / fMaxDist * (float)sizeY * 0.5f);

				nx = CLAMP(nx + sizeX / 2, 0, sizeX - 1);
				ny = CLAMP(ny + sizeY / 2, 0, sizeY - 1);

				float displacementValue = (1.f / 255.f) * (float)ColorB(pDisplacementMap->GetData()[nx * sizeY + ny]).g;

				h += fAttenuation * displacementScale * displacementValue * fHardness;
			}

			// Clamp
			ClampHeight(h);

			m_pHeightmap[iIndex] = h;
		}
	}

	// We modified the heightmap.
	GetIEditorImpl()->GetTerrainManager()->SetModified(iX - radius, iY - radius, iX + radius, iY + radius);
}

void CHeightmap::SmoothSpot(int iX, int iY, int radius, float fHeight, float fHardness)
{
	////////////////////////////////////////////////////////////////////////
	// Draw an attenuated spot on the map
	////////////////////////////////////////////////////////////////////////
	int i, j;
	int iPosX, iPosY;
	float fMaxDist, fYSquared;

	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
		GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CUndoHeightmapModify(iX - radius, iY - radius, radius * 2, radius * 2, this));

	// Calculate the maximum distance
	fMaxDist = radius;

	for (j = (long) -radius; j < radius; j++)
	{
		// Precalculate
		iPosY = iY + j;
		fYSquared = (float) (j * j);

		// Skip invalid locations
		if (iPosY < 1 || iPosY > m_iHeight - 2)
			continue;

		for (i = (long) -radius; i < radius; i++)
		{
			// Calculate the position
			iPosX = iX + i;

			// Skip invalid locations
			if (iPosX < 1 || iPosX > m_iWidth - 2)
				continue;

			// Only circle.
			float dist = sqrtf(fYSquared + i * i);
			if (dist > fMaxDist)
				continue;

			int pos = iPosX + iPosY * m_iWidth;
			float h;
			h = (m_pHeightmap[pos] +
			     m_pHeightmap[pos + 1] +
			     m_pHeightmap[pos - 1] +
			     m_pHeightmap[pos + m_iWidth] +
			     m_pHeightmap[pos - m_iWidth] +
			     m_pHeightmap[pos + 1 + m_iWidth] +
			     m_pHeightmap[pos + 1 - m_iWidth] +
			     m_pHeightmap[pos - 1 + m_iWidth] +
			     m_pHeightmap[pos - 1 - m_iWidth])
			    / 9.0f;

			float currH = m_pHeightmap[pos];
			m_pHeightmap[pos] = currH + (h - currH) * fHardness;
		}
	}

	// We modified the heightmap.
	GetIEditorImpl()->GetTerrainManager()->SetModified(iX - radius, iY - radius, iX + radius, iY + radius);
}

float GetMedian(int count, float* arr)
{
	std::sort(arr, arr + count);
	return arr[count / 2];
}

struct RampSegment
{
	Vec2  p1, p2;
	float h1, h2;
	Vec2  offsetP1, offsetP2;
};

Vec2 OffsetCurveIntersection(float* pA, float* pB, Vec2 p1, Vec2 p2, Vec2 p3, float offset)
{
	//CryLogAlways("offset = %f, points = [(%f, %f) (%f, %f) (%f, %f)]", offset, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
	Vec2 a = (p1 - p2).GetNormalized();
	Vec2 b = (p3 - p2).GetNormalized();
	float ab = a.Dot(b);

	if (ab < -0.999f)
	{
		*pA = 1.0f;
		*pB = 0.0f;
		return p2 + 0.5f * (Vec2(a.y - b.y, b.x - a.x)) * offset;
	}
	else
	{
		float x = offset / (ab * b - a).GetLength();
		float y = offset / (ab * a - b).GetLength();
		//CryLogAlways("  x = %f, y = %f", x, y);
		Vec2 result = p2 + x * a + y * b;
		*pA = (result - p1).Dot(p2 - p1) / (p2 - p1).Dot(p2 - p1);
		*pB = (result - p2).Dot(p3 - p2) / (p3 - p2).Dot(p3 - p2);
		return result;
	}
}

void CreateSegments(std::vector<RampSegment>& segs, float h1, float h2, Vec2* points, int count, float innerRadius)
{
	segs.clear();
	for (int i = 0; i < count - 1; ++i)
	{
		RampSegment seg;
		seg.p1 = points[i];
		seg.p2 = points[i + 1];
		seg.offsetP1 = seg.p1;
		seg.offsetP2 = seg.p2;
		segs.push_back(seg);
	}
	for (int i = 0; i < count - 2; ++i)
	{
		RampSegment* pSeg1 = &segs[i];
		RampSegment* pSeg2 = &segs[i + 1];
		float a, b;
		Vec2 offsetP = OffsetCurveIntersection(&a, &b, pSeg1->p1, pSeg1->p2, pSeg2->p2, innerRadius);
		pSeg1->offsetP2 = offsetP;
		pSeg1->p2 = pSeg1->p1 + a * (pSeg1->p2 - pSeg1->p1);
		pSeg2->offsetP1 = offsetP;
		pSeg2->p1 = pSeg2->p1 + b * (pSeg2->p2 - pSeg2->p1);
	}
	float totalLength = 0.0f;
	for (int i = 0; i < count - 1; ++i)
		totalLength += (segs[i].p2 - segs[i].p1).GetLength();
	float len = 0.0f;
	for (int i = 0; i < count - 1; ++i)
	{
		segs[i].h1 = h1 + (len / totalLength) * (h2 - h1);
		len += (segs[i].p2 - segs[i].p1).GetLength();
		segs[i].h2 = h1 + (len / totalLength) * (h2 - h1);
	}
}

void InterpolateRamp(float* pHeight, float* pDist, Vec2 pos, const std::vector<RampSegment>& segs, float innerRadius, bool smoothHeight)
{
	float bestDistSquared = FLT_MAX;
	int segmentIndex = 0;
	float segmentParam = 0;

	for (int seg = 0; seg < segs.size(); ++seg)
	{
		const RampSegment* pSeg = &segs[seg];

		Vec2 p1 = pSeg->p1;
		Vec2 p2 = pSeg->p2;
		Vec2 d = p2 - p1;
		float distSquared = 0;
		float param = d.Dot(pos - p1) / d.Dot(d);

		if (param < 0)
		{
			distSquared = (pos - p1).GetLength2();
			param = 0.0f;
		}
		else if (param > 1)
		{
			distSquared = (pos - p2).GetLength2();
			param = 1.0f;
		}
		else
		{
			distSquared = (pos - p1 - param * d).GetLength2();
		}
		if (distSquared < bestDistSquared)
		{
			bestDistSquared = distSquared;
			segmentIndex = seg;
			segmentParam = param;
		}
	}

	const RampSegment* pSeg = &segs[segmentIndex];
	if (segmentIndex > 0 && segmentParam == 0.0f)
		*pDist = fabsf((pSeg->offsetP1 - pos).GetLength() - innerRadius);
	else if (segmentIndex < segs.size() - 1 && segmentParam == 1.0f)
		*pDist = fabsf((pSeg->offsetP2 - pos).GetLength() - innerRadius);
	else
		*pDist = sqrtf(bestDistSquared);

	float t = segmentParam;
	if (smoothHeight)
		t = t * t * (3.0f - 2.0f * t);
	*pHeight = pSeg->h1 + t * (pSeg->h2 - pSeg->h1);
}

void CHeightmap::Hold()
{
	////////////////////////////////////////////////////////////////////////
	// Save a backup copy of the heightmap
	////////////////////////////////////////////////////////////////////////

	if (!IsAllocated())
		return;

	FILE* hFile = NULL;

	CLogFile::WriteLine("Saving temporary copy of the heightmap");

	AfxGetMainWnd()->BeginWaitCursor();

	// Open the hold / fetch file
	hFile = fopen(HEIGHTMAP_HOLD_FETCH_FILE, "wb");
	ASSERT(hFile);
	if (hFile)
	{
		// Write the dimensions
		VERIFY(fwrite(&m_iWidth, sizeof(m_iWidth), 1, hFile));
		VERIFY(fwrite(&m_iHeight, sizeof(m_iHeight), 1, hFile));

		// Write the data
		VERIFY(fwrite(m_pHeightmap, sizeof(t_hmap), m_iWidth * m_iHeight, hFile));

		//! Write the info.
		VERIFY(fwrite(m_LayerIdBitmap.GetData(), sizeof(unsigned char), m_LayerIdBitmap.GetSize(), hFile));

		fclose(hFile);
	}

	AfxGetMainWnd()->EndWaitCursor();
}

void CHeightmap::Fetch()
{
	////////////////////////////////////////////////////////////////////////
	// Read a backup copy of the heightmap
	////////////////////////////////////////////////////////////////////////

	CLogFile::WriteLine("Loading temporary copy of the heightmap");

	AfxGetMainWnd()->BeginWaitCursor();

	if (!Read(HEIGHTMAP_HOLD_FETCH_FILE))
	{
		CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("You need to use 'Hold' before 'Fetch' !"));
		return;
	}

	AfxGetMainWnd()->EndWaitCursor();
	GetIEditorImpl()->GetTerrainManager()->SetModified();
}

bool CHeightmap::Read(string strFileName)
{
	////////////////////////////////////////////////////////////////////////
	// Load a heightmap from a file
	////////////////////////////////////////////////////////////////////////

	FILE* hFile = NULL;
	uint64 iWidth, iHeight;

	if (strFileName.IsEmpty())
		return false;

	// Open the hold / fetch file
	hFile = fopen(strFileName.GetString(), "rb");

	if (!hFile)
		return false;

	// Read the dimensions
	VERIFY(fread(&iWidth, sizeof(iWidth), 1, hFile));
	VERIFY(fread(&iHeight, sizeof(iHeight), 1, hFile));

	// Resize the heightmap
	Resize(iWidth, iHeight, m_unitSize, true, true);

	// Load the data
	VERIFY(fread(m_pHeightmap, sizeof(t_hmap), m_iWidth * m_iHeight, hFile));

	//! Write the info.
	m_LayerIdBitmap.Allocate(m_iWidth, m_iHeight);
	VERIFY(fread(m_LayerIdBitmap.GetData(), sizeof(unsigned char), m_LayerIdBitmap.GetSize(), hFile));

	fclose(hFile);

	return true;
}

void CHeightmap::InitNoise()
{
	////////////////////////////////////////////////////////////////////////
	// Initialize the noise array
	////////////////////////////////////////////////////////////////////////
	if (m_pNoise)
		return;

	CNoise cNoise;
	static bool bFirstQuery = true;
	float fFrequency = 6.0f;
	float fFrequencyStep = 2.0f;
	float fYScale = 1.0f;
	float fFade = 0.46f;
	float fLowestPoint = 256000.0f, fHighestPoint = -256000.0f;
	float fValueRange;
	unsigned int i, j;

	// Allocate a new array class to
	m_pNoise = new CDynamicArray2D(NOISE_ARRAY_SIZE, NOISE_ARRAY_SIZE);

	// Process layers
	for (i = 0; i < 8; i++)
	{
		// Apply the fractal noise function to the array
		cNoise.FracSynthPass(m_pNoise, fFrequency, fYScale, NOISE_ARRAY_SIZE, NOISE_ARRAY_SIZE, TRUE);

		// Modify noise generation parameters
		fFrequency *= fFrequencyStep;
		fYScale *= fFade;
	}

	// Find the value range
	for (i = 0; i < NOISE_ARRAY_SIZE; i++)
		for (j = 0; j < NOISE_ARRAY_SIZE; j++)
		{
			fLowestPoint = __min(fLowestPoint, m_pNoise->m_Array[i][j]);
			fHighestPoint = __max(fHighestPoint, m_pNoise->m_Array[i][j]);
		}

	// Storing the value range in this way saves us a division and a multiplication
	fValueRange = NOISE_RANGE / (fHighestPoint - fLowestPoint);

	// Normalize the heightmap
	for (i = 0; i < NOISE_ARRAY_SIZE; i++)
		for (j = 0; j < NOISE_ARRAY_SIZE; j++)
		{
			m_pNoise->m_Array[i][j] -= fLowestPoint;
			m_pNoise->m_Array[i][j] *= fValueRange;

			// Keep signed / unsigned balance
			//m_pNoise->m_Array[i][j] += 2500.0f/255.0f;
		}
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::CalcSurfaceTypes(const CRect* rect)
{
	int i;
	bool first = true;

	CRect rc;

	CFloatImage hmap;
	hmap.Attach(m_pHeightmap, m_iWidth, m_iHeight);

	if (rect)
		rc = *rect;
	else
		rc.SetRect(0, 0, m_iWidth, m_iHeight);

	// Generate the masks
	int numLayers = GetIEditorImpl()->GetTerrainManager()->GetLayerCount();
	for (i = 0; i < numLayers; i++)
	{
		CLayer* pLayer = GetIEditorImpl()->GetTerrainManager()->GetLayer(i);

		if (!pLayer->IsInUse())
			continue;

		uint32 dwLayerId = pLayer->GetOrRequestLayerId();

		if (first)
		{
			first = false;

			// For every pixel of layer update surface type.
			for (uint32 y = rc.top; y < rc.bottom; y++)
			{
				int yp = y * m_iWidth;
				for (uint32 x = rc.left; x < rc.right; x++)
				{
					SSurfaceTypeItem st;
					st = dwLayerId;
					SetLayerIdAt(x, y, st);
				}
			}
		}
		else
		{
			// Assume that layer mask is at size of full resolution texture.
			CByteImage& layerMask = pLayer->GetMask();
			if (!layerMask.IsValid())
				continue;

			int layerWidth = layerMask.GetWidth();
			int layerHeight = layerMask.GetHeight();
			float xScale = (float)layerWidth / m_iWidth;
			float yScale = (float)layerHeight / m_iHeight;

			uint8* pLayerMask = layerMask.GetData();

			// For every pixel of layer update surface type.
			for (uint32 y = rc.top; y < rc.bottom; y++)
			{
				for (uint32 x = rc.left; x < rc.right; x++)
				{
					uint8 a = pLayerMask[int(x * xScale + layerWidth * y * yScale)];
					if (a > OVVERIDE_LAYER_SURFACETYPE_FROM)
					{
						SSurfaceTypeItem st;
						st = dwLayerId;
						SetLayerIdAt(x, y, st);
					}
				}
			}
		}
	}
}

void CHeightmap::UpdateEngineTerrain(bool bOnlyElevation, bool boUpdateReloadSurfacertypes)
{
	LOADING_TIME_PROFILE_SECTION(gEnv->pSystem);

	if (boUpdateReloadSurfacertypes)
	{
		GetIEditorImpl()->GetTerrainManager()->ReloadSurfaceTypes();
	}

	UpdateEngineTerrain(0, 0, m_iWidth, m_iHeight, ETerrainUpdateType::Elevation | (bOnlyElevation ? 0 : ETerrainUpdateType::InfoBits));
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::UpdateEngineTerrain(int x1, int y1, int areaSize, int _height, TerrainUpdateFlags updateFlags)
{
	LOADING_TIME_PROFILE_SECTION(gEnv->pSystem);

	I3DEngine* p3DEngine = GetIEditorImpl()->Get3DEngine();
	const int nHeightMapUnitSize = p3DEngine->GetHeightMapUnitSize();

	// update terrain by square blocks aligned to terrain sector size
	int nSecSize = p3DEngine->GetTerrainSectorSize() / p3DEngine->GetHeightMapUnitSize();
	if (nSecSize == 0)
		return;

	const int originalInputAreaSize = areaSize;
	const int originalInputX1 = x1;
	const int originalInputY1 = y1;

	int nTerrSize = m_iWidth;

	areaSize = ceil(areaSize / (float)nSecSize + 1) * nSecSize;
	areaSize = CLAMP(areaSize, 0, nTerrSize);

	x1 = floor(x1 / (float)nSecSize) * nSecSize;
	y1 = floor(y1 / (float)nSecSize) * nSecSize;
	x1 = CLAMP(x1, 0, (nTerrSize - areaSize));
	y1 = CLAMP(y1, 0, (nTerrSize - areaSize));

	int x2 = x1 + areaSize;
	int y2 = y1 + areaSize;
	x2 = CLAMP(x2, areaSize, nTerrSize);
	y2 = CLAMP(y2, areaSize, nTerrSize);

	int w, h;
	w = x2 - x1;
	h = y2 - y1;

	if (w <= 0 || h <= 0)
		return;

	CSurfTypeImage image;
	image.Allocate(w, h);
	SSurfaceTypeItem* terrainBlock = (SSurfaceTypeItem*)image.GetData();

	// fill image with surface type info
	{
		uint8 LayerIdToDetailId[256];

		// to speed up the following loops
		for (uint32 dwI = 0; dwI < CLayer::e_hole; ++dwI)
			LayerIdToDetailId[dwI] = GetIEditorImpl()->GetTerrainManager()->GetDetailIdLayerFromLayerId(dwI);

		for (int y = y1; y < y2; y++)
		{
			for (int x = x1; x < x2; x++)
			{
				int ncell = (y - y1) * w + (x - x1);
				assert(ncell >= 0 && ncell < w * h);
				SSurfaceTypeItem* p = &terrainBlock[ncell];

				*p = m_LayerIdBitmap.ValueAt(x, y);

				for (int s = 0; s < SSurfaceTypeItem::kMaxSurfaceTypesNum; s++)
				{
					// convert to engine side global surface type id
					p->ty[s] = LayerIdToDetailId[p->ty[s]];
				}
			}
		}
	}

	if (updateFlags != 0)
	{
		uint32 nSizeX = m_TerrainRGBTexture.GetTileCountX();
		uint32 nSizeY = m_TerrainRGBTexture.GetTileCountY();
		uint32* pResolutions = new uint32[nSizeX * nSizeY];
		for (int x = 0; x < nSizeX; x++)
			for (int y = 0; y < nSizeY; y++)
				pResolutions[x + y * nSizeX] = m_TerrainRGBTexture.GetTileResolution(x, y);

		p3DEngine->SetHeightMapMaxHeight(m_fMaxHeight);
		p3DEngine->GetITerrain()->SetTerrainElevation(y1, x1, w, h, m_pHeightmap, terrainBlock,
		                                              y1, x1, w, h,
		                                              nSizeX ? pResolutions : NULL, nSizeX, nSizeY);

		delete[] pResolutions;
	}

	const Vec2 worldModPosition(originalInputY1 * nHeightMapUnitSize + originalInputAreaSize * nHeightMapUnitSize / 2,
		originalInputX1 * nHeightMapUnitSize + originalInputAreaSize * nHeightMapUnitSize / 2);
	const float areaRadius = originalInputAreaSize * nHeightMapUnitSize / 2;

	GetIEditorImpl()->GetGameEngine()->OnTerrainModified(worldModPosition, areaRadius, (originalInputAreaSize == m_iWidth), updateFlags & (ETerrainUpdateType::Elevation | ETerrainUpdateType::InfoBits));
}

void CHeightmap::Serialize(CXmlArchive& xmlAr)
{
	if (xmlAr.bLoading)
	{
		// Loading
		XmlNodeRef heightmap = xmlAr.root;

		if (stricmp(heightmap->getTag(), "Heightmap"))
		{
			heightmap = xmlAr.root->findChild("Heightmap"); // load old version
			if (!heightmap)
				return;
		}

		uint32 nWidth(m_iWidth);
		uint32 nHeight(m_iHeight);

		// To remain compatible.
		if (heightmap->getAttr("Width", nWidth))
		{
			m_iWidth = nWidth;
		}

		// To remain compatible.
		if (heightmap->getAttr("Height", nHeight))
		{
			m_iHeight = nHeight;
		}

		heightmap->getAttr("WaterLevel", m_fWaterLevel);
		heightmap->getAttr("UnitSize", m_unitSize);
		heightmap->getAttr("MaxHeight", m_fMaxHeight);

		int textureSize;
		if (heightmap->getAttr("TextureSize", textureSize))
		{
			SetSurfaceTextureSize(textureSize, textureSize);
		}

		void* pData;
		int size1, size2;

		ClearModSectors();

		if (xmlAr.pNamedData->GetDataBlock("HeightmapModSectors", pData, size1))
		{
			int nSize = size1 / (sizeof(int) * 2);
			int* data = (int*)pData;
			for (int i = 0; i < nSize; i++)
				AddModSector(data[i * 2], data[i * 2 + 1]);
			m_updateModSectors = true;
		}

		// Allocate new memory
		Resize(m_iWidth, m_iHeight, m_unitSize);

		// Load heightmap data.
		if (xmlAr.pNamedData->GetDataBlock("HeightmapDataW", pData, size1))
		{
			const int dataSize = m_iWidth * m_iHeight * sizeof(uint16);
			if (size1 != dataSize)
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "ERROR: Unexpected size of HeightmapDataW: %d (expected %d)", size1, dataSize);
			else
			{
				float fInvPrecision = 1.0f / GetShortPrecisionScale();
				uint16* pSrc = (uint16*)pData;
				for (int i = 0; i < m_iWidth * m_iHeight; i++)
				{
					m_pHeightmap[i] = (float)pSrc[i] * fInvPrecision;
				}
			}
		}
		else if (xmlAr.pNamedData->GetDataBlock("HeightmapData", pData, size1))
		{
			// Backward compatability for float heigthmap data.
			const int dataSize = m_iWidth * m_iHeight * sizeof(t_hmap);
			if (size1 != dataSize)
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "ERROR: Unexpected size of HeightmapData: %d (expected %d)", size1, dataSize);
			else
				memcpy(m_pHeightmap, pData, dataSize);
		}

		if (xmlAr.pNamedData->GetDataBlock(HEIGHTMAP_LAYERINFO_BLOCK_NAME, pData, size2))
		{
			const int dataSize = m_LayerIdBitmap.GetSize();
			// new version
			if (size2 != dataSize)
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "ERROR: Unexpected size of %s: %d (expected %d)", HEIGHTMAP_LAYERINFO_BLOCK_NAME, size2, dataSize);
			else
				memcpy(m_LayerIdBitmap.GetData(), pData, dataSize);
		}
		else
		{
			// old version - to be backward compatible
			if (xmlAr.pNamedData->GetDataBlock(HEIGHTMAP_LAYERINFO_BLOCK_NAME_OLD, pData, size2)) // older version are not supported any longer
			{
				std::vector<byte> oldLayerIdBitmap;
				const size_t dataSize = m_iWidth * m_iHeight;
				oldLayerIdBitmap.resize(dataSize);
				if (size2 != dataSize)
					CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "ERROR: Unexpected size of %s: %d (expected %d)", HEIGHTMAP_LAYERINFO_BLOCK_NAME_OLD, size2, dataSize);
				else
					memcpy(&oldLayerIdBitmap[0], pData, dataSize);

				CLogFile::WriteLine("Converting heightmap LayerID (new format) ...");

				for (uint32 dwY = 0; dwY < m_iHeight; ++dwY)
				{
					for (uint32 dwX = 0; dwX < m_iWidth; ++dwX)
					{
						unsigned char ucVal = oldLayerIdBitmap[dwX + dwY * m_iWidth];

						SSurfaceTypeItem st;
						st = ucVal & CLayer::e_undefined;
						SetLayerIdAt(dwX, dwY, st);
						SetHoleAt(dwX, dwY, ucVal & CLayer::e_hole);
					}
				}
			}
		}
	}
	else
	{
		xmlAr.root = XmlHelpers::CreateXmlNode("Heightmap");
		XmlNodeRef heightmap = xmlAr.root;

		heightmap->setAttr("Width", (uint32)m_iWidth);
		heightmap->setAttr("Height", (uint32)m_iHeight);
		heightmap->setAttr("WaterLevel", m_fWaterLevel);
		heightmap->setAttr("UnitSize", m_unitSize);
		heightmap->setAttr("TextureSize", m_textureSize);
		heightmap->setAttr("MaxHeight", m_fMaxHeight);

		if (m_modSectors.size())
		{
			int* data = new int[m_modSectors.size() * 2];
			for (int i = 0; i < m_modSectors.size(); i++)
			{
				data[i * 2] = m_modSectors[i].x;
				data[i * 2 + 1] = m_modSectors[i].y;
			}
			xmlAr.pNamedData->AddDataBlock("HeightmapModSectors", data, sizeof(int) * m_modSectors.size() * 2);
			delete[] data;
		}

		// Save heightmap data as words.
		//xmlAr.pNamedData->AddDataBlock( "HeightmapData",m_pHeightmap,m_iWidth * m_iHeight * sizeof(t_hmap) );
		{
			CWordImage hdata;
			hdata.Allocate(m_iWidth, m_iHeight);
			uint16* pTrg = hdata.GetData();
			float fPrecisionScale = GetShortPrecisionScale();
			for (int i = 0; i < m_iWidth * m_iHeight; i++)
			{
				float val = m_pHeightmap[i];
				unsigned int h = int(val * fPrecisionScale + 0.5f);
				if (h > 0xFFFF) h = 0xFFFF;
				pTrg[i] = h;
			}
			xmlAr.pNamedData->AddDataBlock("HeightmapDataW", hdata.GetData(), hdata.GetSize());
		}

		// Save surface type info
		xmlAr.pNamedData->AddDataBlock(HEIGHTMAP_LAYERINFO_BLOCK_NAME, m_LayerIdBitmap.GetData(), m_LayerIdBitmap.GetSize());

		// Save surface type also in old format (for back compatibility, only dominating type is saved, weighted blending is lost)
		{
			CByteImage oldFormatLayerIds;
			oldFormatLayerIds.Allocate(m_iWidth, m_iHeight);

			for (int y = 0; y < m_LayerIdBitmap.GetHeight(); y++)
			{
				for (int x = 0; x < m_LayerIdBitmap.GetWidth(); x++)
				{
					oldFormatLayerIds.ValueAt(x, y) = m_LayerIdBitmap.ValueAt(x, y).GetDominatingSurfaceType();

					if (m_LayerIdBitmap.ValueAt(x, y).GetHole())
					{
						oldFormatLayerIds.ValueAt(x, y) |= CLayer::e_hole;
					}
				}
			}

			xmlAr.pNamedData->AddDataBlock(HEIGHTMAP_LAYERINFO_BLOCK_NAME_OLD, oldFormatLayerIds.GetData(), oldFormatLayerIds.GetSize());
		}
	}
}

void CHeightmap::SerializeTerrain(CXmlArchive& xmlAr)
{
	LOADING_TIME_PROFILE_SECTION(gEnv->pSystem);

	if (xmlAr.bLoading)
	{
		// Loading
		void* pData = NULL;
		int nSize = 0;

		{
			if (xmlAr.pNamedData->GetDataBlock("TerrainCompiledData", pData, nSize))
			{
				STerrainChunkHeader* pHeader = (STerrainChunkHeader*)pData;
				/*if ((pHeader->nVersion == TERRAIN_CHUNK_VERSION) && (pHeader->TerrainInfo.sectorSize_InMeters == pHeader->TerrainInfo.unitSize_InMeters * SECTOR_SIZE_IN_UNITS))
				{
					SSectorInfo si;
					GetSectorsInfo(si);

					ITerrain* pTerrain = GetIEditorImpl()->Get3DEngine()->CreateTerrain(pHeader->TerrainInfo);
					// check if size of terrain in compile data match
					if (pHeader->TerrainInfo.unitSize_InMeters)
						if (!pTerrain->SetCompiledData((uint8*)pData, nSize, NULL, NULL))
							GetIEditorImpl()->Get3DEngine()->DeleteTerrain();
				}*/
			}
		}
	}
	else
	{
		if (ITerrain* pTerrain = GetIEditorImpl()->Get3DEngine()->GetITerrain())
		{
			int nSize = pTerrain->GetCompiledDataSize();
			if (nSize > 0)
			{
				// Storing
				uint8* pData = new uint8[nSize];
				GetIEditorImpl()->Get3DEngine()->GetITerrain()->GetCompiledData(pData, nSize, NULL, NULL, NULL, GetPlatformEndian());
				xmlAr.pNamedData->AddDataBlock("TerrainCompiledData", pData, nSize, true);
				delete[] pData;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::SetWaterLevel(float waterLevel)
{
	// We modified the heightmap.
	GetIEditorImpl()->GetTerrainManager()->SetModified();

	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
		GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CUndoWaterLevel(this));

	m_fWaterLevel = waterLevel;

	if (GetIEditorImpl()->GetSystem()->GetI3DEngine())
		if (GetIEditorImpl()->GetSystem()->GetI3DEngine()->GetITerrain())
			GetIEditorImpl()->GetSystem()->GetI3DEngine()->GetITerrain()->SetOceanWaterLevel(waterLevel);

	signalWaterLevelChanged();
};

void CHeightmap::SetHoleAt(const int x, const int y, const bool bHole)
{
  m_LayerIdBitmap.ValueAt(x, y).SetHole(bHole);
}

//////////////////////////////////////////////////////////////////////////
// Make hole.
void CHeightmap::MakeHole(int x1, int y1, int width, int height, bool bMake)
{
	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
		GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CUndoHeightmapInfo(x1, y1, width + 1, height + 1, this));

	I3DEngine* engine = GetIEditorImpl()->Get3DEngine();
	int x2 = x1 + width;
	int y2 = y1 + height;
	for (int x = x1; x <= x2; x++)
	{
		for (int y = y1; y <= y2; y++)
		{
			SetHoleAt(x, y, bMake);
		}
	}

	UpdateEngineTerrain(x1, y1, x2 - x1, y2 - y1, ETerrainUpdateType::InfoBits);
}

//////////////////////////////////////////////////////////////////////////
bool CHeightmap::IsHoleAt(const int x, const int y) const
{
	return m_LayerIdBitmap.ValueAt(x, y).GetHole();
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::SetLayerIdAt(const int x, const int y, const SSurfaceTypeItem& st)
{
	if (0 <= x && x < m_LayerIdBitmap.GetWidth() && 0 <= y && y < m_LayerIdBitmap.GetHeight())
	{
		SSurfaceTypeItem& ucRef = (SSurfaceTypeItem&)m_LayerIdBitmap.ValueAt(x, y);

		bool hole = ucRef.GetHole();
		
		assert(!st.GetHole());

		ucRef = st;

		ucRef.SetHole(hole);
	}
}

//////////////////////////////////////////////////////////////////////////
uint32 CHeightmap::GetDominatingSurfaceTypeIdAtPosition(const int x, const int y) const
{
	uint32 dwLayerId = m_LayerIdBitmap.ValueAt(x, y).GetDominatingSurfaceType();

	return GetIEditorImpl()->GetTerrainManager()->GetDetailIdLayerFromLayerId(dwLayerId);
}

//////////////////////////////////////////////////////////////////////////
SSurfaceTypeItem CHeightmap::GetLayerInfoAt(const int x, const int y) const
{
	SSurfaceTypeItem ucValue;

	if (x >= 0 && y >= 0 && x < m_LayerIdBitmap.GetWidth() && y < m_LayerIdBitmap.GetHeight())
		ucValue = m_LayerIdBitmap.ValueAt(x, y);

	return ucValue;
}

//////////////////////////////////////////////////////////////////////////
uint32 CHeightmap::GetLayerIdAt(const int x, const int y) const
{
	uint32 ucValue;

	if (x >= 0 && y >= 0 && x < m_LayerIdBitmap.GetWidth() && y < m_LayerIdBitmap.GetHeight())
		ucValue = m_LayerIdBitmap.ValueAt(x, y).GetDominatingSurfaceType();

	return ucValue;
}

//////////////////////////////////////////////////////////////////////////
ColorB CHeightmap::GetColorAtPosition(const float x, const float y, ColorB* colors, const int colorsNum, const float xStep)
{
	float fLevelSize = GetIEditorImpl()->GetSystem()->GetI3DEngine()->GetTerrainSize();

	float cx = CLAMP(x, 0, fLevelSize - 1.f) / fLevelSize;
	float cy = CLAMP(y, 0, fLevelSize - 1.f) / fLevelSize;

	ColorB res = Col_Black;

	if (colors)
	{
		float texelSize = 1.f / m_TerrainRGBTexture.CalcMaxLocalResolution(cx, cy, cx + FLT_EPSILON, cy + FLT_EPSILON);
		float cs = xStep / fLevelSize;

		uint32* ptr = (uint32*)colors;
		uint32* ptrEnd = (uint32*)colors + colorsNum;

		// use bilinear filtering if sampling density is higher than texel density
		if (cs < texelSize * 0.75f)
		{
			while (ptr < ptrEnd)
			{
				*ptr++ = m_TerrainRGBTexture.GetFilteredValueAt(cx, cy);
				cx += cs;
			}
		}
		else
		{
			while (ptr < ptrEnd)
			{
				*ptr++ = m_TerrainRGBTexture.GetValueAt(cx, cy);
				cx += cs;
			}
		}
	}
	else
	{
		res = (ColorB)m_TerrainRGBTexture.GetFilteredValueAt(cx, cy);
	}

	return res;
}

float CHeightmap::GetRGBMultiplier()
{
	return m_kfBrMultiplier;
}

//////////////////////////////////////////////////////////////////////////
float CHeightmap::GetZInterpolated(const float x, const float y)
{
	if (x <= 0 || y <= 0 || x >= m_iWidth - 1 || y >= m_iHeight - 1)
		return 0;

	int nX = fastftol_positive(x);
	int nY = fastftol_positive(y);

	float dx1 = x - nX;
	float dy1 = y - nY;

	float dDownLandZ0 =
	  (1.f - dx1) * (m_pHeightmap[nX + nY * m_iWidth]) +
	  (dx1) * (m_pHeightmap[(nX + 1) + nY * m_iWidth]);

	float dDownLandZ1 =
	  (1.f - dx1) * (m_pHeightmap[nX + (nY + 1) * m_iWidth]) +
	  (dx1) * (m_pHeightmap[(nX + 1) + (nY + 1) * m_iWidth]);

	float dDownLandZ = (1 - dy1) * dDownLandZ0 + (dy1) * dDownLandZ1;

	if (dDownLandZ < 0)
		dDownLandZ = 0;

	return dDownLandZ;
}

//////////////////////////////////////////////////////////////////////////
float CHeightmap::GetAccurateSlope(const float x, const float y)
{
	uint32 iHeightmapWidth = GetWidth();

	if (x <= 0 || y <= 0 || x >= m_iWidth - 1 || y >= m_iHeight - 1)
		return 0;

	// Calculate the slope for this point

	float arrEvelvations[8];
	int nId = 0;

	float d = 0.7f;
	arrEvelvations[nId++] = GetZInterpolated(x + 1, y);
	arrEvelvations[nId++] = GetZInterpolated(x - 1, y);
	arrEvelvations[nId++] = GetZInterpolated(x, y + 1);
	arrEvelvations[nId++] = GetZInterpolated(x, y - 1);
	arrEvelvations[nId++] = GetZInterpolated(x + d, y + d);
	arrEvelvations[nId++] = GetZInterpolated(x - d, y - d);
	arrEvelvations[nId++] = GetZInterpolated(x + d, y - d);
	arrEvelvations[nId++] = GetZInterpolated(x - d, y + d);

	float fMin = arrEvelvations[0];
	float fMax = arrEvelvations[0];

	for (int i = 0; i < 8; i++)
	{
		if (arrEvelvations[i] > fMax)
			fMax = arrEvelvations[i];
		if (arrEvelvations[i] < fMin)
			fMin = arrEvelvations[i];
	}

	// Compensate the smaller slope for bigger height fields
	return (fMax - fMin) * 0.5f / GetUnitSize();
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::UpdateEngineHole(int x1, int y1, int width, int height)
{
	UpdateEngineTerrain(x1, y1, width, height, ETerrainUpdateType::InfoBits);
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::GetSectorsInfo(SSectorInfo& si)
{
	ZeroStruct(si);
	si.unitSize = m_unitSize;
	si.sectorSize = m_unitSize * SECTOR_SIZE_IN_UNITS;
	si.numSectors = m_numSectors;
	if (si.numSectors > 0)
		si.sectorTexSize = m_textureSize / si.numSectors;
	si.surfaceTextureSize = m_textureSize;
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::SetSurfaceTextureSize(int width, int height)
{
	assert(width == height);

	if (width != 0)
	{
		m_textureSize = width;
	}
	m_terrainGrid->SetResolution(m_textureSize);
}

/*
   //////////////////////////////////////////////////////////////////////////
   void CHeightmap::MoveBlock( const CRect &rc,CPoint offset )
   {
   CRect hrc(0,0,m_iWidth,m_iHeight );
   CRect subRc = rc & hrc;
   CRect trgRc = rc;
   trgRc.OffsetRect(offset);
   trgRc &= hrc;

   if (subRc.IsRectEmpty() || trgRc.IsRectEmpty())
    return;

   if (CUndo::IsRecording())
   {
    // Must be square.
    int size = (trgRc.Width() > trgRc.Height()) ? trgRc.Width() : trgRc.Height();
    CUndo::Record( new CUndoHeightmapModify(trgRc.left,trgRc.top,size,size,this) );
   }

   CFloatImage hmap;
   CFloatImage hmapSubImage;
   hmap.Attach( m_pHeightmap,m_iWidth,m_iHeight );
   hmapSubImage.Allocate( subRc.Width(),subRc.Height() );

   hmap.GetSubImage( subRc.left,subRc.top,subRc.Width(),subRc.Height(),hmapSubImage );
   hmap.SetSubImage( trgRc.left,trgRc.top,hmapSubImage );

   CByteImage infoMap;
   CByteImage infoSubImage;
   infoMap.Attach( &m_info[0],m_iWidth,m_iHeight );
   infoSubImage.Allocate( subRc.Width(),subRc.Height() );

   infoMap.GetSubImage( subRc.left,subRc.top,subRc.Width(),subRc.Height(),infoSubImage );
   infoMap.SetSubImage( trgRc.left,trgRc.top,infoSubImage );

   // Move Vegetation.
   if (m_vegetationMap)
   {
    Vec3 p1 = HmapToWorld(CPoint(subRc.left,subRc.top));
    Vec3 p2 = HmapToWorld(CPoint(subRc.right,subRc.bottom));
    Vec3 ofs = HmapToWorld(offset);
    CRect worldRC( p1.x,p1.y,p2.x,p2.y );
    // Export and import to block.
    CXmlArchive ar("Root");
    ar.bLoading = false;
    m_vegetationMap->ExportBlock( worldRC,ar );
    ar.bLoading = true;
    m_vegetationMap->ImportBlock( ar,CPoint(ofs.x,ofs.y) );
   }
   }
 */

//////////////////////////////////////////////////////////////////////////
int CHeightmap::LogLayerSizes()
{
	int totalSize = 0;
	CCryEditDoc* doc = GetIEditorImpl()->GetDocument();
	int numLayers = GetIEditorImpl()->GetTerrainManager()->GetLayerCount();
	for (int i = 0; i < numLayers; i++)
	{
		CLayer* pLayer = GetIEditorImpl()->GetTerrainManager()->GetLayer(i);
		int layerSize = pLayer->GetSize();
		totalSize += layerSize;
		CryLog("Layer %s: %dM", (const char*)pLayer->GetLayerName(), layerSize / (1024 * 1024));
	}
	CryLog("Total Layers Size: %dM", totalSize / (1024 * 1024));
	return totalSize;
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::ExportBlock(const CRect& inrect, CXmlArchive& xmlAr, bool exportVegetation, bool exportRgbLayer)
{
	using namespace Private_Heightmap;

	// Storing
	CLogFile::WriteLine("Exporting Heightmap settings...");

	XmlNodeRef heightmap = xmlAr.root->newChild("Heightmap");

	CRect subRc(inrect);

	heightmap->setAttr("Width", (uint32)m_iWidth);
	heightmap->setAttr("Height", (uint32)m_iHeight);

	// Save rectangle dimensions to root of terrain block.
	xmlAr.root->setAttr("X1", subRc.left);
	xmlAr.root->setAttr("Y1", subRc.top);
	xmlAr.root->setAttr("X2", subRc.right);
	xmlAr.root->setAttr("Y2", subRc.bottom);

	// Rectangle.
	heightmap->setAttr("X1", subRc.left);
	heightmap->setAttr("Y1", subRc.top);
	heightmap->setAttr("X2", subRc.right);
	heightmap->setAttr("Y2", subRc.bottom);

	heightmap->setAttr("UnitSize", m_unitSize);

	SRgbRegion rgbRegion{};
	if (exportRgbLayer)
	{
		rgbRegion = MapToRgbRegion(subRc, *this);
		if (!rgbRegion.IsEmpty())
		{
			XmlNodeRef rgbLayer = xmlAr.root->newChild("rgbLayer");
			rgbLayer->setAttr("X1", rgbRegion.left);
			rgbLayer->setAttr("Y1", rgbRegion.top);
			rgbLayer->setAttr("X2", rgbRegion.right);
			rgbLayer->setAttr("Y2", rgbRegion.bottom);
			rgbLayer->setAttr("Width", rgbRegion.width);
			rgbLayer->setAttr("Height", rgbRegion.height);
		}
	}

	CFloatImage hmap;
	CFloatImage hmapSubImage;
	hmap.Attach(m_pHeightmap, m_iWidth, m_iHeight);
	hmapSubImage.Allocate(subRc.Width(), subRc.Height());

	hmap.GetSubImage(subRc.left, subRc.top, subRc.Width(), subRc.Height(), hmapSubImage);

	CSurfTypeImage LayerIdBitmapImage;
	LayerIdBitmapImage.Allocate(subRc.Width(), subRc.Height());

	m_LayerIdBitmap.GetSubImage(subRc.left, subRc.top, subRc.Width(), subRc.Height(), LayerIdBitmapImage);

	// Save heightmap.
	xmlAr.pNamedData->AddDataBlock("HeightmapData", hmapSubImage.GetData(), hmapSubImage.GetSize());
	xmlAr.pNamedData->AddDataBlock(HEIGHTMAP_LAYERINFO_BLOCK_NAME, LayerIdBitmapImage.GetData(), LayerIdBitmapImage.GetSize());

	// Save rgb map.
	if (!rgbRegion.IsEmpty())
	{
		CImageEx image;
		if (image.Allocate(rgbRegion.width, rgbRegion.height))
		{
			m_TerrainRGBTexture.GetSubImageStretched(rgbRegion.left, rgbRegion.top, rgbRegion.right, rgbRegion.bottom, image);
			xmlAr.pNamedData->AddDataBlock(HEIGHTMAP_RGB_BLOCK_NAME, image.GetData(), image.GetSize());
		}
	}

	if (exportVegetation)
	{
		Vec3 p1 = HmapToWorld(CPoint(subRc.left, subRc.top));
		Vec3 p2 = HmapToWorld(CPoint(subRc.right, subRc.bottom));
		if (GetIEditorImpl()->GetVegetationMap())
		{
			CRect worldRC(p1.x, p1.y, p2.x, p2.y);
			GetIEditorImpl()->GetVegetationMap()->ExportBlock(worldRC, xmlAr);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CPoint CHeightmap::ImportBlock(CXmlArchive& xmlAr, CPoint newPos, bool useNewPos, float heightOffset, bool importOnlyVegetation, int nRot)
{
	using namespace Private_Heightmap;

	CLogFile::WriteLine("Importing Heightmap settings...");

	XmlNodeRef heightmap = xmlAr.root->findChild("Heightmap");
	if (!heightmap)
		return CPoint(0, 0);

	uint32 width, height;

	heightmap->getAttr("Width", width);
	heightmap->getAttr("Height", height);

	CPoint offset(0, 0);

	if (width != m_iWidth || height != m_iHeight)
	{
		CryMessageBox(_T("Terrain Block dimensions differ from current terrain."), _T("Warning"), eMB_Error);
		return CPoint(0, 0);
	}

	CRect subRc;
	int srcWidth, srcHeight;
	if (nRot == 1 || nRot == 3)
	{
		heightmap->getAttr("Y1", subRc.left);
		heightmap->getAttr("X1", subRc.top);
		heightmap->getAttr("Y2", subRc.right);
		heightmap->getAttr("X2", subRc.bottom);
		srcWidth = subRc.bottom - subRc.top;
		srcHeight = subRc.right - subRc.left;
	}
	else
	{
		heightmap->getAttr("X1", subRc.left);
		heightmap->getAttr("Y1", subRc.top);
		heightmap->getAttr("X2", subRc.right);
		heightmap->getAttr("Y2", subRc.bottom);
		srcWidth = subRc.right - subRc.left;
		srcHeight = subRc.bottom - subRc.top;
	}

	if (useNewPos)
	{
		offset = CPoint(newPos.x - subRc.left, newPos.y - subRc.top);
		subRc.OffsetRect(offset);
	}

	if (!importOnlyVegetation)
	{
		void* pData;
		int size;

		// Load heightmap data.
		if (xmlAr.pNamedData->GetDataBlock("HeightmapData", pData, size))
		{
			// Backward compatability for float heigthmap data.
			CFloatImage hmap;
			CFloatImage hmapSubImage;
			hmap.Attach(m_pHeightmap, m_iWidth, m_iHeight);
			hmapSubImage.Attach((float*)pData, srcWidth, srcHeight);

			if (nRot)
			{
				CFloatImage hmapSubImageRot;
				hmapSubImageRot.RotateOrt(hmapSubImage, nRot);
				hmap.SetSubImage(subRc.left, subRc.top, hmapSubImageRot, heightOffset, m_fMaxHeight);
			}
			else
				hmap.SetSubImage(subRc.left, subRc.top, hmapSubImage, heightOffset, m_fMaxHeight);
		}

		if (xmlAr.pNamedData->GetDataBlock(HEIGHTMAP_LAYERINFO_BLOCK_NAME, pData, size))
		{
			CSurfTypeImage LayerIdBitmapImage;
			LayerIdBitmapImage.Attach((SSurfaceTypeItem*)pData, srcWidth, srcHeight);

			if (nRot)
			{
				CSurfTypeImage LayerIdBitmapImageRot;
				LayerIdBitmapImageRot.RotateOrt(LayerIdBitmapImage, nRot);
				m_LayerIdBitmap.SetSubImage(subRc.left, subRc.top, LayerIdBitmapImageRot);
			}
			else
			{
				m_LayerIdBitmap.SetSubImage(subRc.left, subRc.top, LayerIdBitmapImage);
			}
		}

		XmlNodeRef rgbLayer = xmlAr.root->findChild("rgbLayer");
		if (rgbLayer && xmlAr.pNamedData->GetDataBlock(HEIGHTMAP_RGB_BLOCK_NAME, pData, size))
		{
			CRY_ASSERT_MESSAGE(!useNewPos, "Not implemented");

			SRgbRegion rgbRegion{};
			rgbLayer->getAttr("X1", rgbRegion.left);
			rgbLayer->getAttr("Y1", rgbRegion.top);
			rgbLayer->getAttr("X2", rgbRegion.right);
			rgbLayer->getAttr("Y2", rgbRegion.bottom);
			rgbLayer->getAttr("Width", rgbRegion.width);
			rgbLayer->getAttr("Height", rgbRegion.height);
			if (!rgbRegion.IsEmpty())
			{
				CImageEx image;
				image.Attach((uint32*)pData, rgbRegion.width, rgbRegion.height);

				if (nRot)
				{
					CImageEx imageRot;
					imageRot.RotateOrt(image, nRot);
					m_TerrainRGBTexture.SetSubImageStretched(rgbRegion.left, rgbRegion.top, rgbRegion.right, rgbRegion.bottom, imageRot);
				}
				else
				{
					m_TerrainRGBTexture.SetSubImageStretched(rgbRegion.left, rgbRegion.top, rgbRegion.right, rgbRegion.bottom, image);
				}

				const int32 sectorCount = m_terrainGrid->GetNumSectors();
				const int32 minSecX = (int32)floor(rgbRegion.left * sectorCount);
				const int32 minSecY = (int32)floor(rgbRegion.top * sectorCount);
				const int32 maxSecX = (int32)ceil(rgbRegion.right * sectorCount);
				const int32 maxSecY = (int32)ceil(rgbRegion.bottom * sectorCount);
				for (int y = minSecY; y < maxSecY; ++y)
				{
					for (int x = minSecX; x < maxSecX; ++x)
					{
						UpdateSectorTexture(CPoint(x, y), rgbRegion.left, rgbRegion.top, rgbRegion.right, rgbRegion.bottom);
					}
				}
			}
		}

		// After heightmap serialization, update terrain in 3D Engine.
		int wid = subRc.Width() + 2;
		if (wid < subRc.Height() + 2)
		{
			wid = subRc.Height() + 2;
		}

		auto updateFlags = (rgbLayer != nullptr)
			? ETerrainUpdateType::Elevation | ETerrainUpdateType::InfoBits | ETerrainUpdateType::Paint
			: ETerrainUpdateType::Elevation | ETerrainUpdateType::InfoBits;
		UpdateEngineTerrain(subRc.left - 1, subRc.top - 1, wid, wid, updateFlags);
	}

	if (GetIEditorImpl()->GetVegetationMap())
	{
		Vec3 ofs = HmapToWorld(offset);
		GetIEditorImpl()->GetVegetationMap()->ImportBlock(xmlAr, CPoint(ofs.x, ofs.y));
	}

	if (!importOnlyVegetation)
	{
		// Import layers.
		XmlNodeRef layers = xmlAr.root->findChild("Layers");
		if (layers)
		{
			SSectorInfo si;
			GetSectorsInfo(si);
			float pixelsPerMeter = ((float)si.sectorTexSize) / si.sectorSize;

			CPoint layerOffset;
			layerOffset.x = offset.x * si.unitSize * pixelsPerMeter;
			layerOffset.y = offset.y * si.unitSize * pixelsPerMeter;

			// Export layers settings.
			for (int i = 0; i < layers->getChildCount(); i++)
			{
				CXmlArchive ar(xmlAr);
				ar.root = layers->getChild(i);
				string layerName;
				if (!ar.root->getAttr("Name", layerName))
					continue;
				CLayer* layer = GetIEditorImpl()->GetTerrainManager()->FindLayer(layerName);
				if (!layer)
					continue;

				layer->ImportBlock(ar, layerOffset, nRot);
			}
		}
	}

	return offset;
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::ClearSegmentHeights(const CRect& rc)
{
	int w = rc.Width() * sizeof(t_hmap);
	for (int y = rc.top; y < rc.bottom; ++y)
	{
		memset(m_pHeightmap + y * m_iWidth + rc.left, 0, w);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::ImportSegmentHeights(CMemoryBlock& mem, const CRect& rc)
{
	CFloatImage hmapSubImage;
	hmapSubImage.Attach((float*)mem.GetBuffer(), rc.Width(), rc.Height());

	CFloatImage hmap;
	hmap.Attach(m_pHeightmap, m_iWidth, m_iHeight);

	hmap.SetSubImage(rc.left, rc.top, hmapSubImage);
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::ExportSegmentHeights(CMemoryBlock& mem, const CRect& rc)
{
	CFloatImage hmap;
	hmap.Attach(m_pHeightmap, m_iWidth, m_iHeight);

	CFloatImage hmapSubImage;
	hmap.GetSubImage(rc.left, rc.top, rc.Width(), rc.Height(), hmapSubImage);

	hmapSubImage.Compress(mem);
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::ClearSegmentLayerIDs(const CRect& rc)
{
	int w = rc.Width() * sizeof(SSurfaceTypeItem);
	SSurfaceTypeItem* pLayerIDs = m_LayerIdBitmap.GetData();
	for (int y = rc.top; y < rc.bottom; ++y)
	{
		memset(pLayerIDs + y * m_iWidth + rc.left, 0, w);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::ImportSegmentLayerIDs(CMemoryBlock& mem, const CRect& rc)
{
	//TODO: load layers info, update terrain layers, remap indicies
	typedef std::map<uint32, CryGUID>    TUIntGuidMap;
	typedef std::pair<uint32, CryGUID>   TUIntGuidPair;

	typedef std::map<uint32, uint32>  TUIntUIntMap;
	typedef std::pair<uint32, uint32> TUIntUIntPair;

	int nImageMemSize = sizeof(SSurfaceTypeItem) * rc.Width() * rc.Height();
	bool isLayerIDsInfoStored = (mem.GetSize() != nImageMemSize);

	SSurfaceTypeItem* p = (SSurfaceTypeItem*)mem.GetBuffer();
	int curr = 0;

	TUIntUIntMap tLayerIDMap;

	bool isNeedToReindex = false;

	if (isLayerIDsInfoStored)
	{

		TUIntGuidMap tMap;

		int nSavedLayersCount;
		memcpy(&nSavedLayersCount, &p[curr], sizeof(int));
		curr += sizeof(int); // layers count

		for (int i = 0; i < nSavedLayersCount; i++)
		{
			TUIntGuidPair tPair;
			memcpy(&tPair.first, &p[curr], sizeof(uint32));
			curr += sizeof(uint32); //layer id

			memcpy(&tPair.second, &p[curr], sizeof(GUID));
			curr += sizeof(GUID); // layer GUID

			tMap.insert(tPair);
		}

		int nLayersCount = GetIEditorImpl()->GetTerrainManager()->GetLayerCount();
		if (nLayersCount != tMap.size())
		{
			isNeedToReindex = true;
		}

		TUIntGuidMap::const_iterator it;
		for (it = tMap.begin(); it != tMap.end(); ++it)
		{
			TUIntUIntPair tUIntPair;
			tUIntPair.first = it->first;
			tUIntPair.second = 0;
			bool bFound = false;
			for (int i = 0; i < nLayersCount; i++)
			{
				CLayer* pLayer = GetIEditorImpl()->GetTerrainManager()->GetLayer(i);

				if (it->second == pLayer->GetGUID())
				{
					tUIntPair.second = pLayer->GetCurrentLayerId();
					if (tUIntPair.first != tUIntPair.second)
						isNeedToReindex = true;
					bFound = true;
					//tLayerIDMap.insert(tUIntPair);
					break;
				}
			}

			if (!bFound)
				isNeedToReindex = true;

			tLayerIDMap.insert(tUIntPair);
		}
	}

	CRY_ASSERT((curr + nImageMemSize) == mem.GetSize());

	CMemoryBlock memImgData;
	memImgData.Allocate(nImageMemSize);
	memcpy(memImgData.GetBuffer(), &p[curr], nImageMemSize);

	CSurfTypeImage LayerIdBitmapImage;
	LayerIdBitmapImage.Attach((SSurfaceTypeItem*) memImgData.GetBuffer(), rc.Width(), rc.Height());

	if (isLayerIDsInfoStored)
	{

		// remap ids if needed
		if (isNeedToReindex)
		{
			for (int y = 0; y < LayerIdBitmapImage.GetHeight(); ++y)
			{
				for (int x = 0; x < LayerIdBitmapImage.GetWidth(); ++x)
				{
					SSurfaceTypeItem& rValue = (SSurfaceTypeItem&)LayerIdBitmapImage.ValueAt(x, y);
					int lidOld = rValue.GetDominatingSurfaceType() & CLayer::e_undefined;
					TUIntUIntMap::const_iterator itNew = tLayerIDMap.find(lidOld);
					if (itNew == tLayerIDMap.end())
						continue;
					int lidNew = itNew->second;
					rValue = (rValue.GetDominatingSurfaceType()& CLayer::e_hole) | (lidNew & CLayer::e_undefined);
				}
			}
		}
	}

	m_LayerIdBitmap.SetSubImage(rc.left, rc.top, LayerIdBitmapImage);
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::ExportSegmentLayerIDs(CMemoryBlock& mem, const CRect& rc)
{
	//TODO: save layers info with number of "instances" per layer

	int sizeOfMem = 0;
	typedef std::map<uint32, CryGUID>  TUIntGuidMap;
	typedef std::pair<uint32, CryGUID> TUIntGuidPair;

	TUIntGuidMap tMap;
	int nLayersCount = GetIEditorImpl()->GetTerrainManager()->GetLayerCount();
	sizeOfMem += sizeof(int); // num layers

	for (int i = 0; i < nLayersCount; i++)
	{
		CLayer* pLayer = GetIEditorImpl()->GetTerrainManager()->GetLayer(i);
		TUIntGuidPair tPair;
		tPair.first = pLayer->GetCurrentLayerId();
		sizeOfMem += sizeof(uint32); // layer id

		tPair.second = pLayer->GetGUID();

		sizeOfMem += sizeof(CryGUID); // layer GUID

		tMap.insert(tPair);
	}

	//size of image
	sizeOfMem += (sizeof(unsigned char) * rc.Width() * rc.Height());

	CMemoryBlock memAggregated;
	memAggregated.Allocate(sizeOfMem);

	unsigned char* p = (unsigned char*)memAggregated.GetBuffer();

	int curr = 0;

	// saving of layer info
	//-----------------------------

	memcpy(&p[curr], &nLayersCount, sizeof(int));
	curr += sizeof(int); // layers count

	TUIntGuidMap::iterator it;

	for (it = tMap.begin(); it != tMap.end(); ++it)
	{
		memcpy(&p[curr], &it->first, sizeof(uint32));
		curr += sizeof(uint32); //layer id

		memcpy(&p[curr], &it->second, sizeof(CryGUID));
		curr += sizeof(CryGUID); // layer GUID
	}

	CMemoryBlock memImgData;
	CMemoryBlock tempMem;

	CSurfTypeImage LayerIdBitmapImage;
	m_LayerIdBitmap.GetSubImage(rc.left, rc.top, rc.Width(), rc.Height(), LayerIdBitmapImage);
	LayerIdBitmapImage.Compress(tempMem);
	tempMem.Uncompress(memImgData);

	// saving LayerIds
	memcpy(&p[curr], memImgData.GetBuffer(), memImgData.GetSize());

	memAggregated.Compress(mem);
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::CopyFrom(t_hmap* pHmap, unsigned char* pLayerIdBitmap, int resolution)
{
	int x, y;
	int res = min(resolution, (int)m_iWidth);
	for (y = 0; y < res; y++)
	{
		for (x = 0; x < res; x++)
		{
			SetXY(x, y, pHmap[x + y * resolution]);
			m_LayerIdBitmap.ValueAt(x, y) = pLayerIdBitmap[x + y * resolution];
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::CopyFromInterpolate(t_hmap* pHmap, unsigned char* pLayerIdBitmap, int resolution, int prevUnitSize)
{
	float fKof = float(prevUnitSize) / m_unitSize;
	if (fKof > 1)
	{
		int iWidth = m_iWidth;
		int iHeight = m_iHeight;
		float ratio = m_iWidth / (fKof * resolution);
		if (ratio > 1)
		{
			iWidth = iHeight = resolution * prevUnitSize;
		}

		int kof = (int) fKof;
		for (int y = 0, y2 = 0; y < iHeight; y += kof, y2++)
			for (int x = 0, x2 = 0; x < iWidth; x += kof, x2++)
			{
				for (int y1 = 0; y1 < kof; y1++)
					for (int x1 = 0; x1 < kof; x1++)
					{
						if (x + x1 < iWidth && y + y1 < iHeight && x2 < resolution && y2 < resolution)
						{
							float kofx = (float) x1 / kof;
							float kofy = (float) y1 / kof;

							int x3 = x2 + 1;
							int y3 = y2 + 1;
							/*
							            if(x2>0 && y2>0 && x3+1<resolution && y3+1<resolution)
							            {
							              t_hmap p1xy2 = pHmap[x2 + y2*resolution] + (pHmap[x2 + y2*resolution] - pHmap[x2-1 + y2*resolution])/3;
							              t_hmap p2xy2 = pHmap[x3 + y2*resolution] + (pHmap[x3 + y2*resolution] - pHmap[x3+1 + y2*resolution])/3;

							              t_hmap p1xy3 = pHmap[x2 + y3*resolution] + (pHmap[x2 + y3*resolution] - pHmap[x2-1 + y3*resolution])/3;
							              t_hmap p2xy3 = pHmap[x3 + y3*resolution] + (pHmap[x3 + y3*resolution] - pHmap[x3+1 + y3*resolution])/3;


							              t_hmap pxy2 = p1xy2
							            }
							            else
							            {
							 */
							if (x3 >= resolution)
								x3 = x2;
							if (y3 >= resolution)
								y3 = y2;

							SetXY(x + x1, y + y1,
							      (1.0f - kofy) * ((1.0f - kofx) * pHmap[x2 + y2 * resolution] + kofx * pHmap[x3 + y2 * resolution]) +
							      kofy * ((1.0f - kofx) * pHmap[x2 + y3 * resolution] + kofx * pHmap[x3 + y3 * resolution])
							      );
							//						}

							m_LayerIdBitmap.ValueAt(x + x1, y + y1) = pLayerIdBitmap[x2 + y2 * resolution];
						}
					}
				unsigned char val = pLayerIdBitmap[x2 + y2 * resolution];

				if (y2 < resolution - 1)
				{
					unsigned char val1 = pLayerIdBitmap[x2 + (y2 + 1) * resolution];
					if (val1 > val)
						m_LayerIdBitmap.ValueAt(x, y + kof - 1) = val1;
				}

				if (x2 < resolution - 1)
				{
					unsigned char val1 = pLayerIdBitmap[x2 + 1 + y2 * resolution];
					if (val1 > val)
						m_LayerIdBitmap.ValueAt(x + kof - 1, y) = val1;
				}

				if (x2 < resolution - 1 && y2 < resolution - 1)
				{
					// choose max occured value or max value between several max occured.
					unsigned char valu[4];
					int bal[4];
					valu[0] = val;
					valu[1] = pLayerIdBitmap[x2 + 1 + y2 * resolution];
					valu[2] = pLayerIdBitmap[x2 + (y2 + 1) * resolution];
					valu[3] = pLayerIdBitmap[x2 + 1 + (y2 + 1) * resolution];

					int max = 0;

					int k = 0;
					for (k = 0; k < 4; k++)
					{
						bal[k] = 1000 + valu[k];
						if (bal[k] > max)
						{
							val = valu[k];
							max = bal[k];
						}
						if (k > 0)
						{
							for (int kj = 0; kj < k; kj++)
							{
								if (valu[kj] == valu[k])
								{
									valu[k] = 0;
									bal[kj] += bal[k];
									bal[k] = 0;
									if (bal[kj] > max)
									{
										val = valu[kj];
										max = bal[kj];
									}
									break;
								}
							}
						}
					}

					m_LayerIdBitmap.ValueAt(x + kof - 1, y + kof - 1) = val;
				}
			}
	}
	else if (0.1f < fKof && fKof < 1.0f)
	{
		int kof = int(1.0f / fKof + 0.5f);
		for (int y = 0; y < m_iHeight; y++)
			for (int x = 0; x < m_iWidth; x++)
			{
				int x2 = x * kof;
				int y2 = y * kof;
				if (x2 < resolution && y2 < resolution)
				{
					SetXY(x, y, pHmap[x2 + y2 * resolution]);
					m_LayerIdBitmap.ValueAt(x, y) = pLayerIdBitmap[x2 + y2 * resolution];
				}
			}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::InitSectorGrid()
{
	SSectorInfo si;
	GetSectorsInfo(si);
	GetTerrainGrid()->InitSectorGrid(si.numSectors);
}

void CHeightmap::GetMemoryUsage(ICrySizer* pSizer)
{
	pSizer->Add(*this);

	if (m_pHeightmap)
	{
		SIZER_COMPONENT_NAME(pSizer, "Heightmap 2D Array");
		pSizer->Add(m_pHeightmap, m_iWidth * m_iHeight);
	}

	if (m_pNoise)
	{
		m_pNoise->GetMemoryUsage(pSizer);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "LayerId 2D Array");
		pSizer->Add((char*)m_LayerIdBitmap.GetData(), m_LayerIdBitmap.GetSize());
	}

	if (m_terrainGrid)
	{
		m_terrainGrid->GetMemoryUsage(pSizer);
	}
}

t_hmap CHeightmap::GetSafeXY(const uint32 dwX, const uint32 dwY) const
{
	if (dwX <= 0 || dwY <= 0 || dwX >= (uint32)m_iWidth || dwY >= (uint32)m_iHeight)
	{
		return 0;
	}

	return m_pHeightmap[dwX + dwY * m_iWidth];
}

void CHeightmap::RecordUndo(int x1, int y1, int width, int height, bool bInfo)
{
	if (GetIEditorImpl()->GetIUndoManager()->IsUndoRecording())
	{
		if (bInfo)
		{
			GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CUndoHeightmapInfo(x1, y1, width, height, this));
		}
		else
		{
			GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CUndoHeightmapModify(x1, y1, width, height, this));
		}
	}
}

void CHeightmap::UpdateLayerTexture(const CRect& rect)
{
	I3DEngine* p3DEngine = GetIEditorImpl()->Get3DEngine();
	if (p3DEngine->GetTerrainTextureMultiplier() == 0.0f)
	{
		return; // texture is not generated
	}

	float fBrMultiplier = 1.f / p3DEngine->GetTerrainTextureMultiplier();

	float x1 = HmapToWorld(CPoint(rect.left, rect.top)).x;
	float y1 = HmapToWorld(CPoint(rect.left, rect.top)).y;
	float x2 = HmapToWorld(CPoint(rect.right, rect.bottom)).x;
	float y2 = HmapToWorld(CPoint(rect.right, rect.bottom)).y;

	int iTexSectorSize = GetIEditorImpl()->Get3DEngine()->GetTerrainTextureNodeSizeMeters();
	if (iTexSectorSize == 0)
	{
		return;
	}

	int iTerrainSize = GetIEditorImpl()->Get3DEngine()->GetTerrainSize();
	int iTexSectorsNum = iTerrainSize / iTexSectorSize;

	uint32 dwFullResolution = m_TerrainRGBTexture.CalcMaxLocalResolution((float)rect.left / GetWidth(), (float)rect.top / GetHeight(), (float)rect.right / GetWidth(), (float)rect.bottom / GetHeight());
	uint32 dwNeededResolution = dwFullResolution / iTexSectorsNum;

	int iSectMinX = (int)floor(y1 / iTexSectorSize);
	int iSectMinY = (int)floor(x1 / iTexSectorSize);
	int iSectMaxX = (int)floor(y2 / iTexSectorSize);
	int iSectMaxY = (int)floor(x2 / iTexSectorSize);

	CWaitProgress progress("Updating Terrain Layers");

	int nTotalSectors = (iSectMaxX - iSectMinX + 1) * (iSectMaxY - iSectMinY + 1);
	int nCurrSectorNum = 0;
	for (int iSectX = iSectMinX; iSectX <= iSectMaxX; iSectX++)
	{
		for (int iSectY = iSectMinY; iSectY <= iSectMaxY; iSectY++)
		{
			progress.Step((100 * nCurrSectorNum) / nTotalSectors);
			nCurrSectorNum++;
			int iLocalOutMinX = y1 * dwFullResolution / iTerrainSize - iSectX * dwNeededResolution;
			int iLocalOutMinY = x1 * dwFullResolution / iTerrainSize - iSectY * dwNeededResolution;
			int iLocalOutMaxX = y2 * dwFullResolution / iTerrainSize - iSectX * dwNeededResolution;
			int iLocalOutMaxY = x2 * dwFullResolution / iTerrainSize - iSectY * dwNeededResolution;

			if (iLocalOutMinX < 0) iLocalOutMinX = 0;
			if (iLocalOutMinY < 0) iLocalOutMinY = 0;
			if (iLocalOutMaxX > dwNeededResolution) iLocalOutMaxX = dwNeededResolution;
			if (iLocalOutMaxY > dwNeededResolution) iLocalOutMaxY = dwNeededResolution;

			if (iLocalOutMinX != iLocalOutMaxX && iLocalOutMinY != iLocalOutMaxY)
			{
				bool bFullRefreshRequired = GetTerrainGrid()->GetSector(CPoint(iSectX, iSectY))->textureId == 0;
				bool bRecreated;
				int texId = GetTerrainGrid()->LockSectorTexture(CPoint(iSectX, iSectY), dwFullResolution / iTexSectorsNum, bRecreated);
				if (bFullRefreshRequired || bRecreated)
				{
					iLocalOutMinX = 0;
					iLocalOutMinY = 0;
					iLocalOutMaxX = dwNeededResolution;
					iLocalOutMaxY = dwNeededResolution;
				}

				CImageEx image;
				image.Allocate(iLocalOutMaxX - iLocalOutMinX, iLocalOutMaxY - iLocalOutMinY);

				m_TerrainRGBTexture.GetSubImageStretched(
					(iSectX + (float)iLocalOutMinX / dwNeededResolution) / iTexSectorsNum,
					(iSectY + (float)iLocalOutMinY / dwNeededResolution) / iTexSectorsNum,
					(iSectX + (float)iLocalOutMaxX / dwNeededResolution) / iTexSectorsNum,
					(iSectY + (float)iLocalOutMaxY / dwNeededResolution) / iTexSectorsNum,
					image);

				// convert RGB colour into format that has less compression artefacts for brightness variations
				{
					bool bRGBA = GetIEditorImpl()->GetRenderer()->GetRenderType() == ERenderType::Direct3D11;      // i would be cleaner if renderer would ensure the behave the same but that can be slower

					uint32 dwWidth = image.GetWidth();
					uint32 dwHeight = image.GetHeight();

					uint32* pMem = &image.ValueAt(0, 0);

					for (uint32 dwY = 0; dwY < dwHeight; ++dwY)
					{
						for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
						{
							float fR = ((*pMem >> 16) & 0xff) * (1.0f / 255.0f);
							float fG = ((*pMem >> 8) & 0xff) * (1.0f / 255.0f);
							float fB = ((*pMem) & 0xff) * (1.0f / 255.0f);

							ColorF cCol = ColorF(fR, fG, fB);

							// Convert to linear space
							cCol.srgb2rgb();

							cCol *= fBrMultiplier;
							cCol.Clamp();

#if TERRAIN_USE_CIE_COLORSPACE
							cCol = cCol.RGB2mCIE();
#endif

							// Convert to gamma 2.2 space
							cCol.rgb2srgb();

							uint32 dwR = (uint32)(cCol.r * 255.0f + 0.5f);
							uint32 dwG = (uint32)(cCol.g * 255.0f + 0.5f);
							uint32 dwB = (uint32)(cCol.b * 255.0f + 0.5f);

							if (bRGBA)
								*pMem++ = 0xff000000 | (dwB << 16) | (dwG << 8) | (dwR);    // shader requires alpha channel = 1
							else
								*pMem++ = 0xff000000 | (dwR << 16) | (dwG << 8) | (dwB); // shader requires alpha channel = 1
						}
					}
				}

				GetIEditorImpl()->GetRenderer()->UpdateTextureInVideoMemory(texId, (unsigned char*)image.GetData(),
					iLocalOutMinX, iLocalOutMinY, image.GetWidth(), image.GetHeight(), eTF_R8G8B8A8);
				AddModSector(iSectX, iSectY);
			}
		}
	}
}

/////////////////////////////////////////////////////////
void CHeightmap::GetLayerInfoBlock(int x, int y, int width, int height, CSurfTypeImage& map)
{
	if (m_LayerIdBitmap.IsValid())
	{
		m_LayerIdBitmap.GetSubImage(x, y, width, height, map);
	}
}

/////////////////////////////////////////////////////////
// return array of dominating layer id
void CHeightmap::GetLayerIdBlock(int x, int y, int width, int height, CByteImage& map)
{
	CSurfTypeImage surfTypeImage;
	surfTypeImage.Allocate(width, height);

	if (m_LayerIdBitmap.IsValid())
	{
		m_LayerIdBitmap.GetSubImage(x, y, width, height, surfTypeImage);
	}

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			map.ValueAt(x, y) = surfTypeImage.ValueAt(x, y).GetDominatingSurfaceType();
		}
	}
}

/////////////////////////////////////////////////////////
void CHeightmap::SetLayerIdBlock(int x, int y, const CSurfTypeImage& map)
{
	if (m_LayerIdBitmap.IsValid())
	{
		m_LayerIdBitmap.SetSubImage(x, y, map);
	}
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::UpdateSectorTexture(CPoint texsector,
                                     const float fGlobalMinX, const float fGlobalMinY, const float fGlobalMaxX, const float fGlobalMaxY)
{
	I3DEngine* p3DEngine = GetIEditorImpl()->Get3DEngine();

	int texSectorSize = p3DEngine->GetTerrainTextureNodeSizeMeters();
	if (texSectorSize == 0)
	{
		return;
	}

	int texSectorCount = p3DEngine->GetTerrainSize() / texSectorSize;

	if (texSectorCount == 0 || texsector.x >= texSectorCount || texsector.y >= texSectorCount)
	{
		return;
	}

	float fInvSectorCnt = 1.0f / (float)texSectorCount;
	float fMinX = texsector.x * fInvSectorCnt;
	float fMinY = texsector.y * fInvSectorCnt;

	uint32 dwFullResolution = m_TerrainRGBTexture.CalcMaxLocalResolution(fMinX, fMinY, fMinX + fInvSectorCnt, fMinY + fInvSectorCnt);

	GetIEditorImpl()->Get3DEngine()->SetEditorHeightmapCallback(this);

	if (dwFullResolution)
	{
		uint32 dwNeededResolution = dwFullResolution / texSectorCount;

		CTerrainSector* st = m_terrainGrid->GetSector(texsector);

		bool bFullRefreshRequired = true;

		int iLocalOutMinX = 0, iLocalOutMinY = 0, iLocalOutMaxX = dwNeededResolution, iLocalOutMaxY = dwNeededResolution;    // in pixel

		bool bRecreated;

		int texId = m_terrainGrid->LockSectorTexture(texsector, dwNeededResolution, bRecreated);

		if (bRecreated)
		{
			bFullRefreshRequired = true;
		}

		if (!bFullRefreshRequired)
		{
			iLocalOutMinX = floor((fGlobalMinX - fMinX) * dwFullResolution);
			iLocalOutMinY = floor((fGlobalMinY - fMinY) * dwFullResolution);
			iLocalOutMaxX = ceil((fGlobalMaxX - fMinX) * dwFullResolution);
			iLocalOutMaxY = ceil((fGlobalMaxY - fMinY) * dwFullResolution);

			iLocalOutMinX = CLAMP(iLocalOutMinX, 0, dwNeededResolution);
			iLocalOutMinY = CLAMP(iLocalOutMinY, 0, dwNeededResolution);
			iLocalOutMaxX = CLAMP(iLocalOutMaxX, 0, dwNeededResolution);
			iLocalOutMaxY = CLAMP(iLocalOutMaxY, 0, dwNeededResolution);
		}

		if (iLocalOutMaxX != iLocalOutMinX && iLocalOutMaxY != iLocalOutMinY)
		{
			CImageEx image;

			image.Allocate(iLocalOutMaxX - iLocalOutMinX, iLocalOutMaxY - iLocalOutMinY);

			m_TerrainRGBTexture.GetSubImageStretched(
			  fMinX + fInvSectorCnt / dwNeededResolution * iLocalOutMinX,
			  fMinY + fInvSectorCnt / dwNeededResolution * iLocalOutMinY,
			  fMinX + fInvSectorCnt / dwNeededResolution * iLocalOutMaxX,
			  fMinY + fInvSectorCnt / dwNeededResolution * iLocalOutMaxY,
			  image);

			// convert RGB colour into format that has less compression artifacts for brightness variations
			{
				bool bRGBA = GetIEditorImpl()->GetRenderer()->GetRenderType() == ERenderType::Direct3D11;      // i would be cleaner if renderer would ensure the behave the same but that can be slower

				uint32 dwWidth = image.GetWidth();
				uint32 dwHeight = image.GetHeight();

				uint32* pMem = &image.ValueAt(0, 0);
				if (!pMem)
				{
					CryLog("Can't get surface terrain texture. May be it was not generated.");
					return;
				}

				float fBrMultiplier = p3DEngine->GetTerrainTextureMultiplier();
				if (!fBrMultiplier)
				{
					fBrMultiplier = 1.0f;
				}

				fBrMultiplier = 1.f / fBrMultiplier;

				for (uint32 dwY = 0; dwY < dwHeight; ++dwY)
				{
					for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
					{
						float fR = ((((*pMem) & 0x00ff0000) >> 16) & 0xff) * (1.0f / 255.0f);
						float fG = ((((*pMem) & 0x0000ff00) >> 8) & 0xff) * (1.0f / 255.0f);
						float fB = ((((*pMem) & 0x000000ff)) & 0xff) * (1.0f / 255.0f);

						ColorF cCol = ColorF(fR, fG, fB);

						// Convert to linear space
						cCol.srgb2rgb();

						cCol *= fBrMultiplier;
						cCol.Clamp();

#if TERRAIN_USE_CIE_COLORSPACE
						cCol = cCol.RGB2mCIE();
#endif

						// Convert to gamma 2.2 space
						cCol.rgb2srgb();

						uint32 dwR = (uint32)(cCol.r * 255.0f + 0.5f);
						uint32 dwG = (uint32)(cCol.g * 255.0f + 0.5f);
						uint32 dwB = (uint32)(cCol.b * 255.0f + 0.5f);

						if (bRGBA)
						{
							*pMem++ = 0xff000000 | (dwB << 16) | (dwG << 8) | (dwR);    // shader requires alpha channel = 1
						}
						else
						{
							*pMem++ = 0xff000000 | (dwR << 16) | (dwG << 8) | (dwB);    // shader requires alpha channel = 1
						}
					}
				}
			}

			GetIEditorImpl()->GetRenderer()->UpdateTextureInVideoMemory(texId, (unsigned char*)image.GetData(), iLocalOutMinX, iLocalOutMinY, iLocalOutMaxX - iLocalOutMinX, iLocalOutMaxY - iLocalOutMinY, eTF_R8G8B8A8);
			AddModSector(texsector.x, texsector.y);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CHeightmap::InitTerrain()
{
	InitSectorGrid();

	// construct terrain in 3dengine if was not loaded during SerializeTerrain call
	SSectorInfo si;
	GetSectorsInfo(si);

	STerrainInfo TerrainInfo;
	TerrainInfo.heightMapSize_InUnits = si.sectorSize * si.numSectors / si.unitSize;
	TerrainInfo.unitSize_InMeters = si.unitSize;
	TerrainInfo.sectorSize_InMeters = si.sectorSize;
	TerrainInfo.sectorsTableSize_InSectors = si.numSectors;
	TerrainInfo.heightmapZRatio = 1.f / (256.f * 256.f) * GetMaxHeight();
	TerrainInfo.oceanWaterLevel = GetWaterLevel();

	bool bCreateTerrain = false;
	ITerrain* pTerrain = gEnv->p3DEngine->GetITerrain();
	if (pTerrain)
	{
		if ((gEnv->p3DEngine->GetTerrainSize() != (si.sectorSize * si.numSectors)) ||
		    (gEnv->p3DEngine->GetHeightMapUnitSize() != si.unitSize))
		{
			bCreateTerrain = true;
		}
	}
	else
	{
		bCreateTerrain = true;
	}

	if (bCreateTerrain)
	{
		pTerrain = gEnv->p3DEngine->CreateTerrain(TerrainInfo);

		// pass heightmap data to the 3dengine
		UpdateEngineTerrain(false);
	}
}

/////////////////////////////////////////////////////////
void CHeightmap::AddModSector(int x, int y)
{
	//	std::vector<Vec2i>::iterator linkIt;

	bool isExist = false;

	for (std::vector<Vec2i>::iterator it = m_modSectors.begin(); it != m_modSectors.end(); ++it)
	{
		if (it->x == x && it->y == y)
		{
			isExist = true;
			break;
		}
	}

	if (!isExist)
	{
		m_modSectors.push_back(Vec2(x, y));
	}
}

/////////////////////////////////////////////////////////
void CHeightmap::ClearModSectors()
{
	m_modSectors.clear();
}

/////////////////////////////////////////////////////////
void CHeightmap::UnlockSectorsTexture(const CRect& rc)
{
	if (m_modSectors.size())
	{
		for (std::vector<Vec2i>::iterator it = m_modSectors.begin(); it != m_modSectors.end(); )
		{
			CPoint pointSector(it->x, it->y);
			if (rc.PtInRect(pointSector))
			{
				GetTerrainGrid()->UnlockSectorTexture(CPoint(pointSector));
				it = m_modSectors.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

}

/////////////////////////////////////////////////////////
void CHeightmap::ImportSegmentModSectors(CMemoryBlock& mem, const CRect& rc)
{
	unsigned char* p = (unsigned char*)mem.GetBuffer();
	int curr = 0;

	int nModSectorsCount;
	memcpy(&nModSectorsCount, &p[curr], sizeof(int));
	curr += sizeof(int); // number of mod_sectors

	for (int i = 0; i < nModSectorsCount; i++)
	{
		int x;
		int y;
		memcpy(&x, &p[curr], sizeof(int));
		curr += sizeof(int); // x

		memcpy(&y, &p[curr], sizeof(int));
		curr += sizeof(int); // y

		AddModSector(x + rc.left, y + rc.top);
	}
	m_updateModSectors = true;
}

/////////////////////////////////////////////////////////
void CHeightmap::ExportSegmentModSectors(CMemoryBlock& mem, const CRect& rc)
{
	std::vector<CPoint> modSectorsInRect;
	int nSizeOfMem = 0;
	nSizeOfMem += sizeof(int); // number of mod_sectors in rect
	if (m_modSectors.size())
	{
		for (std::vector<Vec2i>::iterator it = m_modSectors.begin(); it != m_modSectors.end(); ++it)
		{
			CPoint pointSector(it->x, it->y);
			if (rc.PtInRect(pointSector))
			{
				modSectorsInRect.push_back(CPoint(pointSector.x - rc.left, pointSector.y - rc.top));
				nSizeOfMem += sizeof(int); // x
				nSizeOfMem += sizeof(int); // y
			}
		}
	}

	CMemoryBlock tempMem;
	tempMem.Allocate(nSizeOfMem);
	unsigned char* p = (unsigned char*)tempMem.GetBuffer();
	int curr = 0;

	int nModSectorsCount = modSectorsInRect.size();

	memcpy(&p[curr], &nModSectorsCount, sizeof(int));
	curr += sizeof(int); // number of mod_sectors in rect

	for (std::vector<CPoint>::iterator it = modSectorsInRect.begin(); it != modSectorsInRect.end(); ++it)
	{
		memcpy(&p[curr], &it->x, sizeof(int));
		curr += sizeof(int); // x

		memcpy(&p[curr], &it->y, sizeof(int));
		curr += sizeof(int); // y
	}

	tempMem.Compress(mem);
}

/////////////////////////////////////////////////////////
void CHeightmap::UpdateModSectors()
{
	if (!m_updateModSectors)
	{
		return;
	}

	I3DEngine* p3DEngine = GetIEditorImpl()->Get3DEngine();
	int iTerrainSize = p3DEngine->GetTerrainSize();
	if (!iTerrainSize)
	{
		assert(false && "[CHeightmap::UpdateModSectors] Zero sized terrain.");     // maybe we should visualized this to the user
		return;
	}

	if (m_modSectors.size())
	{
		for (std::vector<Vec2i>::iterator it = m_modSectors.begin(); it != m_modSectors.end(); ++it)
		{
			UpdateSectorTexture(CPoint(it->x, it->y), 0, 0, 1, 1);
		}
	}
	m_updateModSectors = false;
}

/////////////////////////////////////////////////////////
bool CHeightmap::IsAllocated()
{
	return m_pHeightmap ? true : false;
}

void CHeightmap::OffsetModSectors(int ox, int oy)
{
	int nSectorsX = GetTerrainGrid()->GetNumSectors();
	int nSectorsY = GetTerrainGrid()->GetNumSectors();
	int yStart, yEnd, yStep;
	if (oy < 0)
	{
		yStart = 0;
		yEnd = nSectorsY;
		yStep = 1;
	}
	else
	{
		yStart = nSectorsY - 1;
		yEnd = -1;
		yStep = -1;
	}

	int xStart, xEnd, xStep;
	if (ox < 0)
	{
		xStart = 0;
		xEnd = nSectorsX;
		xStep = 1;
	}
	else
	{
		xStart = nSectorsX - 1;
		xEnd = -1;
		xStep = -1;
	}

	for (int y = yStart; y != yEnd; y += yStep)
	{
		int sy = y - oy;
		for (int x = xStart; x != xEnd; x += xStep)
		{
			int sx = x - ox;
			CTerrainSector* pSector = GetTerrainGrid()->GetSector(CPoint(x, y));
			unsigned int iOldTex = pSector->textureId;
			if (iOldTex)
			{
				GetIEditorImpl()->GetRenderer()->RemoveTexture(iOldTex);
				pSector->textureId = 0;
			}

			int iNewTex;
			if (sy >= 0 && sy < nSectorsY && sx >= 0 && sx < nSectorsX)
			{
				CTerrainSector* pSrc = GetTerrainGrid()->GetSector(CPoint(sx, sy));
				iNewTex = pSrc->textureId;
				if (iNewTex)
				{
					GetIEditorImpl()->Get3DEngine()->SetTerrainSectorTexture(sy, sx, 0);
					pSrc->textureId = 0;
				}
			}
			else
			{
				iNewTex = 0;
			}

			if (iNewTex == iOldTex)
			{
				continue;
			}

			GetIEditorImpl()->Get3DEngine()->SetTerrainSectorTexture(y, x, iNewTex);
			pSector->textureId = iNewTex;
		}
	}

	int iModSect = 0;
	while (iModSect < m_modSectors.size())
	{
		Vec2i& v = m_modSectors[iModSect];
		v.x += ox;
		v.y += oy;
		if (v.x < 0 || v.x > nSectorsX || v.y < 0 || v.y > nSectorsY)
		{
			m_modSectors.erase(m_modSectors.begin() + iModSect);
			continue;
		}
		++iModSect;
	}
}

int CHeightmap::GetNoiseSize() const
{
	return NOISE_ARRAY_SIZE;
}

float CHeightmap::GetNoise(int x, int y) const
{
	return m_pNoise->m_Array[x][y];
}

float SampleImage(const CFloatImage* pImage, float x, float y)
{
	float w = (float)pImage->GetWidth();
	float h = (float)pImage->GetHeight();
	float px = (w - 1) * x;
	float py = (h - 1) * y;
	float tx = px - floorf(px);
	float ty = py - floorf(py);
	int x1 = clamp_tpl<int>((int)px, 0, w - 1);
	int x2 = clamp_tpl<int>(x1 + 1, 0, w - 1);
	int y1 = clamp_tpl<int>((int)py, 0, h - 1);
	int y2 = clamp_tpl<int>(y1 + 1, 0, h - 1);
	float v1 = pImage->ValueAt(x1, y1);
	float v2 = pImage->ValueAt(x2, y1);
	float v3 = pImage->ValueAt(x1, y2);
	float v4 = pImage->ValueAt(x2, y2);
	return (v1 * (1 - tx) + v2 * tx) * (1 - ty) + (v3 * (1 - tx) + v2 * tx) * ty;
}

SSurfaceTypeItem SampleImage(const CSurfTypeImage* pImage, float x, float y)
{
	int w = pImage->GetWidth();
	int h = pImage->GetHeight();
	int ix = clamp_tpl<int>((int)((w - 1) * x + 0.5f), 0, w - 1);
	int iy = clamp_tpl<int>((int)((h - 1) * y + 0.5f), 0, h - 1);
	return pImage->ValueAt(ix, iy);
}

ColorB BilinearColorInterpolation(const ColorB& c11, const ColorB& c12, const ColorB& c21, const ColorB& c22, float tx, float ty)
{
	uint8 r = (uint8)((c11.r * (1 - tx) + c12.r * tx) * (1 - ty) + (c21.r * (1 - tx) + c22.r * tx) * ty);
	uint8 g = (uint8)((c11.g * (1 - tx) + c12.g * tx) * (1 - ty) + (c21.g * (1 - tx) + c22.g * tx) * ty);
	uint8 b = (uint8)((c11.b * (1 - tx) + c12.b * tx) * (1 - ty) + (c21.b * (1 - tx) + c22.b * tx) * ty);
	uint8 a = (uint8)((c11.a * (1 - tx) + c12.a * tx) * (1 - ty) + (c21.a * (1 - tx) + c22.a * tx) * ty);
	return ColorB(r, g, b, a);
}

unsigned int SampleImage(const CImageEx* pImage, float x, float y)
{
	float w = (float)pImage->GetWidth();
	float h = (float)pImage->GetHeight();
	float px = (w - 1) * x;
	float py = (h - 1) * y;
	float tx = px - floorf(px);
	float ty = py - floorf(py);
	int x1 = clamp_tpl<int>((int)px, 0, w - 1);
	int x2 = clamp_tpl<int>(x1 + 1, 0, w - 1);
	int y1 = clamp_tpl<int>((int)py, 0, h - 1);
	int y2 = clamp_tpl<int>(y1 + 1, 0, h - 1);
	ColorB v1 = pImage->ValueAt(x1, y1);
	ColorB v2 = pImage->ValueAt(x2, y1);
	ColorB v3 = pImage->ValueAt(x1, y2);
	ColorB v4 = pImage->ValueAt(x2, y2);

	return BilinearColorInterpolation(v1, v2, v3, v4, tx, ty).pack_abgr8888();
}

CRect GetBounds(const Matrix34& mat, int w, int h)
{
	Vec3 p0 = mat.TransformPoint(Vec3(0, 0, 0));
	Vec3 p1 = mat.TransformPoint(Vec3(1, 0, 0));
	Vec3 p2 = mat.TransformPoint(Vec3(0, 1, 0));
	Vec3 p3 = mat.TransformPoint(Vec3(1, 1, 0));
	CRect rect;
	rect.left = clamp_tpl<int>((int)floorf(min(min(p0.x, p1.x), min(p2.x, p3.x))), 0, w);
	rect.right = clamp_tpl<int>((int) ceilf(max(max(p0.x, p1.x), max(p2.x, p3.x))) + 1, 0, h);
	rect.top = clamp_tpl<int>((int)floorf(min(min(p0.y, p1.y), min(p2.y, p3.y))), 0, w);
	rect.bottom = clamp_tpl<int>((int) ceilf(max(max(p0.y, p1.y), max(p2.y, p3.y))) + 1, 0, h);
	return rect;
}

template<typename T>
void BlitImage(TImage<T>* pDest, TImage<T>* pSrc, const Matrix34& mat, T delta)
{
	Matrix34 invMat = mat.GetInverted();
	CRect bounds = GetBounds(mat, pDest->GetWidth(), pDest->GetHeight());
	float minX = -1.0f / (2.0f * (pSrc->GetWidth() - 1));
	float minY = -1.0f / (2.0f * (pSrc->GetHeight() - 1));
	float maxX = 1.0f + 1.0f / (2.0f * (pSrc->GetWidth() - 1));
	float maxY = 1.0f + 1.0f / (2.0f * (pSrc->GetHeight() - 1));
	for (int y = bounds.top; y < bounds.bottom; ++y)
	{
		for (int x = bounds.left; x < bounds.right; ++x)
		{
			Vec3 uvw = invMat.TransformPoint(Vec3((float)x, (float)y, 0));
			if (uvw.x < minX || uvw.y < minY || uvw.x > maxX || uvw.y > maxY)
			{
				continue;
			}
			pDest->ValueAt(x, y) = delta + SampleImage(pSrc, uvw.x, uvw.y);
		}
	}
}

