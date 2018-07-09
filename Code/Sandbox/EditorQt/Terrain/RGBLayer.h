// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct SEditorPaintBrush;

// RGB terrain texture layer
// internal structure (tiled based, loading tiles on demand) is hidden from outside
// texture does not have to be square
// cache uses "last recently used" strategy to remain within set memory bounds (during terrain texture generation)
class SANDBOX_API CRGBLayer
{
public:
	// constructor
	CRGBLayer(const char* szFilename);
	// destructor
	virtual ~CRGBLayer();

	// Serialization
	void Serialize(XmlNodeRef& node, bool bLoading);

	// might throw an exception if memory is low but we only need very few bytes
	// Arguments:
	//   dwTileCountX - must not be 0
	//   dwTileCountY - must not be 0
	//   dwTileResolution must be power of two
	void AllocateTiles(const uint32 dwTileCountX, const uint32 dwTileCountY, const uint32 dwTileResolution);

	//
	void FreeData();

	// calculated the texture resolution needed to capture all details in the texture
	uint32 CalcMinRequiredTextureExtend();

	uint32 GetTileCountX() const { return m_dwTileCountX; }
	uint32 GetTileCountY() const { return m_dwTileCountY; }

	// might need to load the tile once
	// Return:
	//   might be 0 if no tile exists at this point
	uint32 GetTileResolution(const uint32 dwTileX, const uint32 dwTileY);

	// Arguments:
	//   dwTileX -  0..GetTileCountX()-1
	//   dwTileY -  0..GetTileCountY()-1
	//   bRaise - true=raise, flase=lower
	bool ChangeTileResolution(const uint32 dwTileX, const uint32 dwTileY, uint32 dwNewSize);

	// 1:1
	void SetSubImageRGBLayer(const uint32 dwDstX, const uint32 dwDstY, const CImageEx& rTileImage);

	void SetSubImageStretched(const float fSrcLeft, const float fSrcTop, const float fSrcRight, const float fSrcBottom, CImageEx& rInImage, const bool bFiltered = false);

	void SetSubImageTransformed(const CImageEx* pImage, const Matrix34& transform);

	// stretched (dwSrcWidth,dwSrcHeight)->rOutImage
	// Arguments:
	//   fSrcLeft - 0..1
	//   fSrcTop - 0..1
	//   fSrcRight - 0..1, <fSrcLeft
	//   fSrcBottom - 0..1, <fSrcTop
	//	Notice: is build as follows: blue | green << 8 | red << 16, alpha unused
	void GetSubImageStretched(const float fSrcLeft, const float fSrcTop, const float fSrcRight, const float fSrcBottom, CImageEx& rOutImage, const bool bFiltered = false);

	// Extracts a stretched subimage from the layer to an image with the same pixel format. Faster, no filtering.
	// Arguments:
	//   fSrcLeft - 0..1
	//   fSrcTop - 0..1
	//   fSrcRight - 0..1, < fSrcLeft
	//   fSrcBottom - 0..1, < fSrcTop
	void GetSubimageFast(const float fSrcLeft, const float fSrcTop, const float fSrcRight, const float fSrcBottom, CImageEx& rOutImage);

	// Stretches an image with the same pixel format as the layer into a subrect of the layer. Faster, no filtering.
	// Arguments:
	//   fDestLeft - 0..1
	//   fDestTop - 0..1
	//   fDestRight - 0..1, < fSrcLeft
	//   fDestBottom - 0..1, < fSrcTop
	void SetSubimageFast(const float fDestLeft, const float fDestTop, const float fDestRight, const float fDestBottom, CImageEx& rSrcImage);

	// Paint spot with pattern (to an RGB image)
	// Arguments:
	//   fpx - 0..1 in the texture
	//   fpy - 0..1 in the texture
	void PaintBrushWithPatternTiled(const float fpx, const float fpy, SEditorPaintBrush& brush, const CImageEx& imgPattern);

	// needed for Layer Painting
	// Arguments:
	//   fSrcLeft - usual range: [0..1] but full range is valid
	//   fSrcTop - usual range: [0..1] but full range is valid
	//   fSrcRight - usual range: [0..1] but full range is valid
	//   fSrcBottom - usual range: [0..1] but full range is valid
	// Return:
	//   might be 0 if no tile was touched
	uint32 CalcMaxLocalResolution(const float fSrcLeft, const float fSrcTop, const float fSrcRight, const float fSrcBottom);

	bool   ImportExportBlock(const char* pFileName, int nSrcLeft, int nSrcTop, int nSrcRight, int nSrcBottom, uint32* outSquare, bool bIsImport = false);
	bool   ExportSegment(CMemoryBlock& mem, uint32 dwTileX, uint32 dwTileY, bool bCompress = true);

	//
	bool IsAllocated() const;

	void GetMemoryUsage(ICrySizer* pSizer);

	// useful to detect problems before saving (e.g. file is read-only)
	bool WouldSaveSucceed();

	// offsets texture by (x,y) tiles
	void Offset(int iTilesX, int iTilesY);

	// forces all tiles to be loaded into memory, violating max mem limit if necessary
	// used when converting normal level into segmented format
	void LoadAll();

	// similar to AllocTiles() but preserves the image
	// if dwTileCountX and dwTileCountY already match, will do nothing
	void Resize(uint32 dwTileCountX, uint32 dwTileCountY, uint32 dwTileResolution);

	// can be called from outside to save memory
	void CleanupCache();

	// writes out a debug images to the current directory
	void Debug();

	// rarely needed editor operation - n terrain texture tiles become 4*n
	// slow
	// Returns:
	//   success
	bool RefineTiles();

	// slow but convenient (will be filtered one day)
	// Arguments:
	//   fpx - 0..1
	//   fpx - 0..1
	uint32 GetFilteredValueAt(const float fpx, const float fpy);
	uint32 GetUnfilteredValueAt(const float fpx, const float fpy);

	// slow but convenient (will be filtered one day)
	// Arguments:
	//   fpx - 0..1
	//   fpy - 0..1
	uint32 GetValueAt(const float fpx, const float fpy);

	// Arguments:
	//   dwX - [0..GetTextureWidth()]
	//   dwY - [0..GetTextureHeight()]
	void SetValueAt(const uint32 dwX, const uint32 dwY, const uint32 dwValue);

	// slow but convenient (will be filtered one day)
	// Arguments:
	//   fpx - 0..1
	//   fpy - 0..1
	void      SetValueAt(const float fpx, const float fpy, const uint32 dwValue);

	CImageEx* GetTileImage(int tileX, int tileY, bool setDirtyFlag = true);
	void      UnloadTile(int tileX, int tileY);

private:
	class CTerrainTextureTiles
	{
	public:
		// default constructor
		CTerrainTextureTiles() : m_pTileImage(0), m_bDirty(false), m_dwSize(0)
		{
		}

		CImageEx*  m_pTileImage;          // 0 if not loaded, otherwise pointer to the image (m_dwTileResolution,m_dwTileResolution)
		CTimeValue m_timeLastUsed;        // needed for "last recently used" strategy
		bool       m_bDirty;              // true=tile needs to be written to disk, needed for "last recently used" strategy
		uint32     m_dwSize;              // only valid if m_dwSize!=0, if not valid you need to call LoadTileIfNeeded()
	};

private:
	bool OpenPakForLoading();
	bool ClosePakForLoading();
	bool SaveAndFreeMemory(const bool bForceFileCreation = false);

	bool GetValueAt(const float fpx, const float fpy, uint32*& value);

	// Arguments:
	//   dwTileX - 0..m_dwTileCountX
	//   dwTileY - 0..m_dwTileCountY
	//   bNoGarbageCollection - do not garbage collect (used by LoadAll())
	// Return:
	//   might be 0 if no tile exists at this point
	CTerrainTextureTiles* LoadTileIfNeeded(const uint32 dwTileX, const uint32 dwTileY, bool bNoGarbageCollection = false);

	// Arguments:
	//   dwTileX - 0..m_dwTileCountX
	//   dwTileY - 0..m_dwTileCountY
	// Return:
	//   might be 0 if no tile exists at this point
	CTerrainTextureTiles* GetTilePtr(const uint32 dwTileX, const uint32 dwTileY);

	void                  FreeTile(CTerrainTextureTiles& rTile);

	// Return:
	//   might be 0
	CTerrainTextureTiles* FindOldestTileToFree();

	// removed tiles till we reach the limit
	void ConsiderGarbageCollection();

	void ClipTileRect(CRect& inoutRect) const;

	// Return:
	//   true = save needed
	bool   IsDirty() const;

	string GetFullFileName();

private:
	std::vector<CTerrainTextureTiles> m_TerrainTextureTiles;            // [x+y*m_dwTileCountX]

	uint32                            m_dwTileCountX;                   //
	uint32                            m_dwTileCountY;                   //
	uint32                            m_dwTileResolution;               // must be power of two, tiles are square in size
	uint32                            m_dwCurrentTileMemory;            // to detect if GarbageCollect is needed
	bool                              m_bPakOpened;                     // to optimize redundant OpenPak an ClosePak
	bool                              m_bInfoDirty;                     // true=save needed e.g. internal properties changed
	bool                              m_bNextSerializeForceSizeSave;    //

	static const uint32               m_dwMaxTileMemory = 1024 * 1024 * 1024; // Stall free support for up to 16k x 16k terrain texture
	string                            m_TerrainRGBFileName;
};

