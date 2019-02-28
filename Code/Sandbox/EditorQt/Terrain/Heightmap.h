// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "RGBLayer.h"
#include "Util/Image.h"

// Heightmap data type
typedef float t_hmap;
typedef TImage<SSurfaceTypeItem> CSurfTypeImage;

class CXmlArchive;
class CDynamicArray2D;
struct SNoiseParams;
class CTerrainGrid;
struct SEditorPaintBrush;

struct SANDBOX_API SSectorInfo
{
	float unitSize;           // Size of terrain unit.
	int sectorSize;         // Sector size in meters.
	int sectorTexSize;      // Size of texture for one sector in pixels.
	int numSectors;         // Number of sectors on one side of terrain.
	int surfaceTextureSize; // Size of whole terrain surface texture.
};

// Editor data structure to keep the heights, detail layer information/holes, terrain texture
class SANDBOX_API CHeightmap : public IEditorHeightmap
{
public:
	// constructor
	CHeightmap();
	// destructor
	virtual ~CHeightmap();

	// Member data access
	uint64 GetWidth() const     { return m_iWidth; };
	uint64 GetHeight() const    { return m_iHeight; };
	float  GetMaxHeight() const { return m_fMaxHeight; }

	//! Get size of every heightmap unit in meters.
	float  GetUnitSize() const { return m_unitSize; };

	void SetMaxHeight(float fMaxHeight);
	void SetUnitSize(float unitSize) { m_unitSize = unitSize; }

	//! Convert from world coordinates to heightmap coordinates.
	CPoint WorldToHmap(const Vec3& wp);
	//! Convert from heightmap coordinates to world coordinates.
	Vec3   HmapToWorld(CPoint hpos);

	// Maps world bounding box to the heightmap space rectangle.
	CRect WorldBoundsToRect(const AABB& worldBounds);

	//! Set size of current surface texture.
	void SetSurfaceTextureSize(int width, int height);

	//! Returns information about sectors on terrain.
	//! @param si Structure filled with queried data.
	void    GetSectorsInfo(SSectorInfo& si);

	t_hmap* GetData() { return m_pHeightmap; };
	bool    GetDataEx(t_hmap* pData, UINT iDestWidth, bool bSmooth = true, bool bNoise = true);

	// Fill image data
	// Arguments:
	//   resolution Resolution of needed heightmap.
	//   vTexOffset offset within hmap
	//   trgRect Target rectangle in scaled heightmap.
	bool GetData(const CRect& trgRect, const int resolution, const CPoint& vTexOffset, CFloatImage& hmap, bool bSmooth = true, bool bNoise = true);

	//////////////////////////////////////////////////////////////////////////
	// Terrain Grid functions.
	//////////////////////////////////////////////////////////////////////////
	void          InitSectorGrid();
	CTerrainGrid* GetTerrainGrid() const { return m_terrainGrid; };

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	void    SetXY(uint32 x, uint32 y, t_hmap iVal) { m_pHeightmap[x + y * m_iWidth] = iVal; };
	t_hmap& GetXY(uint32 x, uint32 y)              { return m_pHeightmap[x + y * m_iWidth]; };
	t_hmap  GetXY(uint32 x, uint32 y) const        { return m_pHeightmap[x + y * m_iWidth]; };
	t_hmap  GetSafeXY(const uint32 dwX, const uint32 dwY) const;

	//! Calculate heightmap slope at given point.
	//! @return clamped to 0-255 range slope value.
	float GetSlope(int x, int y);

	// Returns
	//   slope in height meters per meter
	float GetAccurateSlope(const float x, const float y);

	float GetZInterpolated(const float x1, const float y1);

	float CalcHeightScale() const;

	bool  GetPreviewBitmap(DWORD* pBitmapData, int width, bool bSmooth = true, bool bNoise = true, CRect* pUpdateRect = NULL, bool bShowWater = false);

	void  GetLayerInfoBlock(int x, int y, int width, int height, CSurfTypeImage& map);
	void  GetLayerIdBlock(int x, int y, int width, int height, CByteImage& map);
	void  SetLayerIdBlock(int x, int y, const CSurfTypeImage& map);

	// Arguments:
	//   fpx - 0..1 in the whole terrain
	//   fpy - 0..1 in the whole terrain
	//   dwLayerId - id from CLayer::GetLayerId()
	void PaintLayerId(const float fpx, const float fpy, const SEditorPaintBrush& brush, const uint32 dwLayerId);

	//
	bool         IsHoleAt(const int x, const int y) const;
	virtual bool GetHoleAtPosition(const int x, const int y) const { return IsHoleAt(x, y); }

	// Arguments
	//   bHole - true=hole, false=no hole
	void SetHoleAt(const int x, const int y, const bool bHole);
	// Arguments:
	//   x - 0..GetWidth()-1
	//   y - 0..GetHeight()-1
	// Returns:
	//   full surface type info
	SSurfaceTypeItem GetLayerInfoAt(const int x, const int y) const;
	// Arguments:
	//   x - 0..GetWidth()-1
	//   y - 0..GetHeight()-1
	// Returns:
	//   0xffffffff means there is not valid layer
	uint32 GetLayerIdAt(const int x, const int y) const;
	// Arguments:
	//   x - 0..GetWidth()-1
	//   y - 0..GetHeight()-1
	// Returns:
	//   0xffffffff means there is not valid layer
	virtual uint32 GetDominatingLayerIdAtPosition(const int x, const int y) const { return GetLayerIdAt(x, y); }
	// Arguments:
	//   x - 0..GetWidth()-1
	//   y - 0..GetHeight()-1
	// Returns:
	//   0xffffffff means there is not valid layer
	virtual ColorB GetColorAtPosition(const float x, const float y, ColorB* colors = nullptr, const int colorsNum = 0, const float xStep = 0);
	// Arguments:
	//   x - 0..GetWidth()-1
	//   y - 0..GetHeight()-1
	// Returns:
	//   0xffffffff means there is not valid detail layer
	uint32 GetDominatingSurfaceTypeIdAtPosition(const int x, const int y) const;

	float  GetElevationAtPosition(const float x, const float y) { return GetZInterpolated(x, y); }
	float  GetRGBMultiplier();

	// Arguments
	//   dwLayerId - from CLayer::GetLayerId()
	void SetLayerIdAt(const int x, const int y, const SSurfaceTypeItem& st);

	void EraseLayerID(uchar id);

	void MarkUsedLayerIds(bool bFree[256]) const;

	// Hold / fetch
	void Fetch();
	void Hold();
	bool Read(string strFileName);

	// (Re)Allocate / deallocate
	void Resize(int iWidth, int iHeight, float unitSize, bool bCleanOld = true, bool bForceKeepVegetation = false);
	void CleanUp();

	// Importing / exporting
	void Serialize(CXmlArchive& xmlAr);
	void SerializeTerrain(CXmlArchive& xmlAr);
	void SaveImage(LPCSTR pszFileName);
	void LoadImage(LPCSTR pszFileName);

	//! Save heightmap to PGM file format.
	void SavePGM(const string& pgmFile);
	//! Load heightmap from PGM file format.
	void LoadPGM(const string& pgmFile);

	//! Save heightmap in RAW format.
	void SaveRAW(const string& rawFile);
	//! Load heightmap from RAW format.
	void LoadRAW(const string& rawFile);

	// Actions
	void Smooth();
	void Smooth(CFloatImage& hmap, const CRect& rect);
	void Copy(const AABB& srcBox, int targetRot, const Vec3& targetPos, const Vec3& dym, const Vec3& sourcePos, bool bOnlyVegetation, bool bOnlyTerrain);
	void Noise();
	void Normalize();
	void Invert();
	void MakeIsle();
	void SmoothSlope();
	void Randomize();
	void MakeBeaches();
	void LowerRange(float fFactor);
	void Flatten(float fFactor);
	void GenerateTerrain(const SNoiseParams& sParam);
	void Clear(bool bClearLayerBitmap = true);
	void InitHeight(float fHeight);

	//! Calculate surface type in rectangle.
	//! @param rect if Rectangle is null surface type calculated for whole heightmap.
	void CalcSurfaceTypes(const CRect* rect = NULL);

	// Drawing
	void DrawSpot(int iX, int iY, int radius, float insideRadius, float fHeigh, float fHardness = 1.0f, CImageEx* pDisplacementMap = nullptr, float displacementScale = 1);
	void SmoothSpot(int iX, int iY, int radius, float fHeigh, float fHardness);
	void RiseLowerSpot(int iX, int iY, int radius, float insideRadius, float fHeigh, float fHardness = 1.0f, CImageEx* pDisplacementMap = nullptr, float displacementScale = 1);

	//! Make hole in the terrain.
	void MakeHole(int x1, int y1, int width, int height, bool bMake);

	//! Export terrain block.
	void   ExportBlock(const CRect& rect, CXmlArchive& ar, bool exportVegetation = true, bool exportRgbLayer = false);
	//! Import terrain block, return offset of block to new position.
	CPoint ImportBlock(CXmlArchive& ar, CPoint newPos, bool useNewPos = true, float heightOffset = 0.0f, bool importOnlyVegetation = false, int nRot = 0);

	//! Serialize segment in memory block
	void ClearSegmentHeights(const CRect& rc);
	void ImportSegmentHeights(CMemoryBlock& mem, const CRect& rc);
	void ExportSegmentHeights(CMemoryBlock& mem, const CRect& rc);
	void ClearSegmentLayerIDs(const CRect& rc);
	void ImportSegmentLayerIDs(CMemoryBlock& mem, const CRect& rc);
	void ExportSegmentLayerIDs(CMemoryBlock& mem, const CRect& rc);

	enum ETerrainUpdateType : uint8
	{
		Elevation = BIT(0),
		InfoBits  = BIT(1),
		Paint     = BIT(2),
	};
	typedef uint8 TerrainUpdateFlags;

	//! Update terrain block in engine terrain.
	void UpdateEngineTerrain(int x1, int y1, int width, int height, TerrainUpdateFlags updateFlags);
	//! Update all engine terrain.
	void UpdateEngineTerrain(bool bOnlyElevation = true, bool boUpdateReloadSurfacertypes = true);

	//! Update layer textures in video memory.
	void UpdateLayerTexture(const CRect& rect);

	//! Synchronize engine hole bit with bit stored in editor heightmap.
	void  UpdateEngineHole(int x1, int y1, int width, int height);

	void  SetWaterLevel(float waterLevel);
	float GetWaterLevel() const { return m_fWaterLevel; };

	void  CopyData(t_hmap* pDataOut)
	{
		memcpy(pDataOut, m_pHeightmap, sizeof(t_hmap) * m_iWidth * m_iHeight);
	}

	//! Copy from different heightmap data.
	void CopyFrom(t_hmap* pHmap, unsigned char* pLayerIdBitmap, int resolution);
	void CopyFromInterpolate(t_hmap* pHmap, unsigned char* pLayerIdBitmap, int resolution, int prevUnitSize);

	//! Dump to log sizes of all layers.
	//! @return Total size allocated for layers.
	int   LogLayerSizes();

	float GetShortPrecisionScale() const { return (256.f * 256.f - 1.0f) / m_fMaxHeight; }
	float GetBytePrecisionScale() const  { return 255.f / m_fMaxHeight; }

	void  GetMemoryUsage(ICrySizer* pSizer);

	void       RecordUndo(int x1, int y1, int width, int height, bool bInfo = false);

	CRGBLayer* GetRGBLayer() { return &m_TerrainRGBTexture; }

	// Arguments:
	//   texsector - make sure the values are in valid range
	void  UpdateSectorTexture(CPoint texsector, const float fGlobalMinX, const float fGlobalMinY, const float fGlobalMaxX, const float fGlobalMaxY);

	void  InitTerrain();

	void  AddModSector(int x, int y);
	void  ClearModSectors();
	void  UpdateModSectors();
	void  OffsetModSectors(int ox, int oy);
	void  UnlockSectorsTexture(const CRect& rc);
	void  ImportSegmentModSectors(CMemoryBlock& mem, const CRect& rc);
	void  ExportSegmentModSectors(CMemoryBlock& mem, const CRect& rc);

	int   GetNoiseSize() const;
	float GetNoise(int x, int y) const;

	bool  IsAllocated();

	static const int kInitHeight;

	CCrySignal<void()> signalWaterLevelChanged;

protected:
	void ClampHeight(float& h) { h = min(m_fMaxHeight, max(0.0f, h)); }

	// Helper functions
	__inline void  ClampToAverage(t_hmap* pValue, float fAverage);
	__inline float ExpCurve(float v, float fCover, float fSharpness);

	// Verify internal class state
	__inline void Verify()
	{
		ASSERT(m_iWidth && m_iHeight);
		ASSERT(!IsBadWritePtr(m_pHeightmap, sizeof(t_hmap) * m_iWidth * m_iHeight));
	};

	// Initialization
	void InitNoise();

private:
	const float        m_kfBrMultiplier;
	float              m_fWaterLevel;
	float              m_fMaxHeight;

	t_hmap*            m_pHeightmap;
	CDynamicArray2D*   m_pNoise;

	CSurfTypeImage     m_LayerIdBitmap; // (m_iWidth,m_iHeight) LayerId and EHeighmapInfo (was m_info, Detail LayerId per heightmap element)

	uint64             m_iWidth;
	uint64             m_iHeight;

	int                m_textureSize; // Size of surface texture.
	int                m_numSectors;  // Number of sectors per grid side.
	float              m_unitSize;

	CTerrainGrid*      m_terrainGrid;

	CRGBLayer          m_TerrainRGBTexture; // Terrain RGB texture

	std::vector<Vec2i> m_modSectors;
	bool               m_updateModSectors;

	friend class CUndoHeightmapInfo;
};

//////////////////////////////////////////////////////////////////////////
// Inlined implementation of get slope.
inline float CHeightmap::GetSlope(int x, int y)
{
	//assert( x >= 0 && x < m_iWidth && y >= 0 && y < m_iHeight );
	if (x <= 0 || y <= 0 || x >= m_iWidth - 1 || y >= m_iHeight - 1)
		return 0;

	t_hmap* p = &m_pHeightmap[x + y * m_iWidth];
	float h = *p;
	int w = m_iWidth;
	float fs = (fabs(*(p + 1) - h) +
	            fabs(*(p - 1) - h) +
	            fabs(*(p + w) - h) +
	            fabs(*(p - w) - h) +
	            fabs(*(p + w + 1) - h) +
	            fabs(*(p - w - 1) - h) +
	            fabs(*(p + w - 1) - h) +
	            fabs(*(p - w + 1) - h));
	fs = fs * 8;
	if (fs > 255.0f) fs = 255.0f;
	return fs;
};

