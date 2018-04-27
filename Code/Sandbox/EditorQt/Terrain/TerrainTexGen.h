// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __terraintexgen_h__
#define __terraintexgen_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "TerrainGrid.h"

// forward declaration.
class CLayer;
struct LightingSettings;

enum ETerrainTexGenFlags
{
	ETTG_LIGHTING                = 0x0001,
	ETTG_SHOW_WATER              = 0x0002,
	ETTG_ABGR                    = 0x0008,  // specify different color channel order - this should be removed - the caller should do it if required
	ETTG_STATOBJ_SHADOWS         = 0x0010,
	ETTG_QUIET                   = 0x0020,  // Disable all logging and progress indication.
	ETTG_INVALIDATE_LAYERS       = 0x0040,  // Invalidate stored layers information.
	ETTG_NOTEXTURE               = 0x0080,  // Not generate texture information (Only lighting).
	ETTG_USE_LIGHTMAPS           = 0x0100,  // Use lightmaps for sectors.
	ETTG_STATOBJ_PAINTBRIGHTNESS = 0x0200,  // Paint brightness of vegetation objects.
	ETTG_FAST_LLIGHTING          = 0x0400,  // Use fastest possible lighting algorithm.
	ETTG_NO_TERRAIN_SHADOWS      = 0x0800,  // Do not calculate terrain shadows (Even if specified in lighting settings)
	ETTG_BAKELIGHTING            = 0x1000,  // enforce one result (e.g. eDynamicSun results usually in two textures)
	ETTG_PREVIEW                 = 0x2000,  // result is intended for the preview not rthe permanent storage
};

const uint32 OCCMAP_DOWNSCALE_FACTOR = 1;   // occlusionmap resolution factor to the diffuse texture, can be 1, 2, or 4

#include "TerrainLayerTexGen.h"         // CTerrainLayerTexGen
#include "TerrainLightGen.h"            // CTerrainLayerGen
//#include "IndirectLighting/TerrainGIGen.h"								// CTerrainGIGen (indirect lighting)

/** Class that generates terrain surface texture.
 */
class CTerrainTexGen
{
public:

	// default constructor with standard resolution
	CTerrainTexGen();

	/** Generate terrain surface texture.
	    @param surfaceTexture Output image where texture will be stored, it must already be allocated.
	      to the size of surfaceTexture.
	    @param sector Terrain sector to generate texture for.
	    @param rect Region on the terrain where texture must be generated within sector..
	    @param pOcclusionSurfaceTexture optional occlusion surface texture
	    @return true if texture was generated.
	 */
	bool GenerateSectorTexture(CPoint sector, const CRect& rect, int flags, CImageEx& surfaceTexture);

	// might be valid (!=0, depending on mode) after GenerateSectorTexture(), release with ReleaseOcclusionSurfaceTexture()
	const CImageEx* GetOcclusionSurfaceTexture() const;

	//
	void ReleaseOcclusionSurfaceTexture();

	//! Generate whole surface texture.
	bool GenerateSurfaceTexture(int flags, CImageEx& surfaceTexture);

	//! Query layer mask for pointer to layer.
	CByteImage* GetLayerMask(CLayer* layer);

	//! Invalidate layer valid flags for all sectors..
	void InvalidateLayers();

	//! Invalidate all lighting valid flags for all sectors..
	void InvalidateLighting();

	void Init(const int resolution, const bool bFullInit);

private: // ---------------------------------------------------------------------

	CTerrainLayerTexGen m_LayerTexGen;              //
	CTerrainLightGen    m_LightGen;                 //
};

#endif // __terraintexgen_h__

