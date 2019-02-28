// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef TerrainManager_h__
#define TerrainManager_h__

#pragma once

//////////////////////////////////////////////////////////////////////////
// Dependencies
#include "Terrain/Heightmap.h"
#include "DocMultiArchive.h"

//////////////////////////////////////////////////////////////////////////
// Forward declarations
class CLayer;
class CSurfaceType;
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Class
class SANDBOX_API CTerrainManager
{
public:
	CTerrainManager();
	virtual ~CTerrainManager();

	//////////////////////////////////////////////////////////////////////////
	// Surface Types.
	//////////////////////////////////////////////////////////////////////////
	// Returns:
	//   can be 0 if the id does not exit
	CSurfaceType* GetSurfaceTypePtr(int i) const { if (i >= 0 && i < m_surfaceTypes.size()) return m_surfaceTypes[i]; return 0; }
	int           GetSurfaceTypeCount() const    { return m_surfaceTypes.size(); }
	//! Find surface type by name, return -1 if name not found.
	int           FindSurfaceType(const string& name);
	void          RemoveSurfaceType(CSurfaceType* pSrfType);
	int           AddSurfaceType(CSurfaceType* srf);
	void          RemoveAllSurfaceTypes();
	void          SerializeSurfaceTypes(CXmlArchive& xmlAr, bool bUpdateEngineTerrain = true);
	void          ConsolidateSurfaceTypes();

	/** Put surface types from Editor to game.
	 */
	void ReloadSurfaceTypes(bool bUpdateEngineTerrain = true, bool bUpdateHeightmap = true);

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	// Layers
	int     GetLayerCount() const     { return m_layers.size(); };
	CLayer* GetLayer(int layer) const { return m_layers[layer]; };
	void    SelectLayer(int layerIndex);
	int     GetSelectedLayerIndex();
	CLayer* GetSelectedLayer();
	//! Find layer by name.
	CLayer* FindLayer(const char* sLayerName) const;
	CLayer* FindLayerByLayerId(const uint32 dwLayerId) const;
	void    SwapLayers(int layer1, int layer2);
	void    AddLayer(CLayer* layer);
	void    RemoveLayer(CLayer* layer);
	void    InvalidateLayers();
	void    ClearLayers();
	void    SerializeLayerSettings(CXmlArchive& xmlAr);
	void    MarkUsedLayerIds(bool bFree[256]) const;

	void    CreateDefaultLayer();

	// slow
	// Returns:
	//   0xffffffff if the method failed (internal error)
	uint32 GetDetailIdLayerFromLayerId(const uint32 dwLayerId);

	// needed to convert from old layer structure to the new one
	bool ConvertLayersToRGBLayer();

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	// Heightmap
	CHeightmap* GetHeightmap() { return &m_heightmap; };
	CRGBLayer*  GetRGBLayer();

	void        SetTerrainSize(int resolution, float unitSize);
	void        ResetHeightMap();
	bool        WouldHeightmapSaveSucceed();

	void        Save(bool bBackup = false);
	bool        Load();

	void        SaveTexture(bool bBackup = false);
	bool        LoadTexture();

	void        SerializeTerrain(TDocMultiArchive& arrXmlAr);
	void        SerializeTerrain(CXmlArchive& xmlAr);
	void        SerializeTexture(CXmlArchive& xmlAr);

	void        SetModified(int x1 = 0, int y1 = 0, int x2 = 0, int y2 = 0);

	void        GetTerrainMemoryUsage(ICrySizer* pSizer);
	//////////////////////////////////////////////////////////////////////////

	// Get the number of terrain data files in the level data folder.
	// \sa GetIEditorImpl()->GetLevelDataFolder(). 
	int         GetDataFilesCount() const;
	const char* GetDataFilename(int i) const;

	CCrySignal<void(void)>    signalLayersChanged;
	CCrySignal<void(CLayer*)> signalSelectedLayerChanged;

protected:
	std::vector<_smart_ptr<CSurfaceType>> m_surfaceTypes;
	std::vector<CLayer*>                  m_layers;
	CHeightmap                            m_heightmap;
};

#endif // TerrainManager_h__

