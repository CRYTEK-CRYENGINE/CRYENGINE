// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __terrainlayertexgen_h__
#define __terrainlayertexgen_h__

#if _MSC_VER > 1000
	#pragma once
#endif

class CLayer;
struct LightingSettings;

// Class that generates terrain surface texture.
class CTerrainLayerTexGen
{
private:

	struct SLayerInfo
	{
		// constructor
		SLayerInfo()
		{
			m_pLayer = 0;
			m_pLayerMask = 0;
		}

		CLayer*     m_pLayer;                 // pointer to the layer - release needs to be done somewhere else
		CByteImage* m_pLayerMask;             // Layer mask for this layer.
	};

	// ------------------------

	class CTexSectorInfo
	{
	public:
		// constructor
		CTexSectorInfo() : m_Flags(0)
		{}

		int m_Flags;                        // eSectorLayersValid
	};

public: // -----------------------------------------------------------------------

	// default constructor
	CTerrainLayerTexGen();
	// constructor
	CTerrainLayerTexGen(const int resolution);
	// destructor
	~CTerrainLayerTexGen();

	// Generate terrain surface texture.
	// Arguments:
	//   surfaceTexture - Output image where texture will be stored, it must already be allocated to the size of surfaceTexture.
	//   sector - Terrain sector to generate texture for.
	//   rect - Region on the terrain where texture must be generated within sector..
	//   pOcclusionSurfaceTexture - optional occlusion surface texture
	// Return:
	//   true if texture was generated.
	bool GenerateSectorTexture(CPoint sector, const CRect& rect, int flags, CImageEx& surfaceTexture);

	// Query layer mask for pointer to layer.
	CByteImage* GetLayerMask(CLayer* layer);

	// Invalidate layer valid flags for all sectors..
	void InvalidateLayers();
	//
	void Init(const int resolution);

private: // ---------------------------------------------------------------------

	bool                        m_bLog;                           // true if ETTG_QUIET was not specified
	unsigned int                m_resolution;                     // target texture resolution

	int                         m_sectorResolution;               // Resolution of sector.
	int                         m_numSectors;                     // Number of sectors per side.

	std::vector<SLayerInfo>     m_layers;                         // Used Layers

	CLayer*                     m_waterLayer;                     // If have water layer

	std::vector<CTexSectorInfo> m_sectorGrid;                     // Sector grid.

	// ------------------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// Sectors.
	//////////////////////////////////////////////////////////////////////////
	// Get rectangle for this sector.
	void            GetSectorRect(CPoint sector, CRect& rect);
	// Get terrain sector info.
	CTexSectorInfo& GetCTexSectorInfo(CPoint sector);
	// Get terrain sector.
	int             GetSectorFlags(CPoint sector);
	// Add flags to sector.
	void            SetSectorFlags(CPoint sector, int flags);

	//////////////////////////////////////////////////////////////////////////
	// Layers.
	//////////////////////////////////////////////////////////////////////////

	//
	int     GetLayerCount() const;
	//
	CLayer* GetLayer(int id) const;
	//
	void    ClearLayers();

	// Prepare texture layers for usage.
	// Autogenerate layer masks that needed,
	// Rescale layer masks to requested size.
	bool UpdateSectorLayers(CPoint sector);

	// Log generation progress.
	void Log(const char* format, ...);

	friend class CTerrainTexGen;
};

#endif // __terrainlayertexgen_h__

