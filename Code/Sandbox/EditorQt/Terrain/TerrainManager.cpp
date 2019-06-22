// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>

#include "Terrain/SurfaceType.h"
#include "Terrain/TerrainLayerUndoObject.h"
#include "Terrain/TerrainManager.h"
#include "Terrain/TerrainTexGen.h"
#include "Util/AutoLogTime.h"
#include "CryEditDoc.h"
#include "LogFile.h"

#include "QT/Widgets/QWaitProgress.h"
#include <Controls/QuestionDialog.h>
#include <Util/TempFileHelper.h>
#include <Util/FileUtil.h>

namespace {
const char* kHeightmapFile = "Heightmap.dat";
const char* kTerrainTextureFile = "TerrainTexture.xml";
}

void CTerrainManager::RemoveSurfaceType(CSurfaceType* pSurfaceType)
{
	for (int i = 0; i < m_surfaceTypes.size(); i++)
	{
		if (m_surfaceTypes[i] == pSurfaceType)
		{
			m_surfaceTypes.erase(m_surfaceTypes.begin() + i);
			break;
		}
	}
	ConsolidateSurfaceTypes();
}

int CTerrainManager::AddSurfaceType(CSurfaceType* srf)
{
	GetIEditorImpl()->SetModifiedFlag(TRUE);
	m_surfaceTypes.push_back(srf);

	return m_surfaceTypes.size() - 1;
}

void CTerrainManager::RemoveAllSurfaceTypes()
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	GetIEditorImpl()->SetModifiedFlag(TRUE);
	m_surfaceTypes.clear();
}

void CTerrainManager::SerializeSurfaceTypes(CXmlArchive& xmlAr, bool bUpdateEngineTerrain)
{
	if (xmlAr.bLoading)
	{
		// Clear old layers
		RemoveAllSurfaceTypes();

		// Load the layer count
		XmlNodeRef node = xmlAr.root->findChild("SurfaceTypes");
		if (!node)
			return;

		// Read all node
		for (int i = 0; i < node->getChildCount(); i++)
		{
			CXmlArchive ar(xmlAr);
			ar.root = node->getChild(i);
			CSurfaceType* sf = new CSurfaceType;
			sf->Serialize(ar);
			AddSurfaceType(sf);

			if (sf->GetSurfaceTypeID() == e_layerIdUndefined)  // For older levels.
			{
				sf->AssignUnusedSurfaceTypeID();
			}
		}
		ReloadSurfaceTypes(bUpdateEngineTerrain);
	}
	else
	{
		CLogFile::WriteLine("Storing surface types...");

		XmlNodeRef node = xmlAr.root->newChild("SurfaceTypes");

		// Write all surface types.
		for (int i = 0; i < m_surfaceTypes.size(); i++)
		{
			CXmlArchive ar(xmlAr);
			ar.root = node->newChild("SurfaceType");
			m_surfaceTypes[i]->Serialize(ar);
		}
	}
}

void CTerrainManager::ConsolidateSurfaceTypes()
{
	// We must consolidate the IDs after removing
	for (size_t i = 0; i < m_surfaceTypes.size(); ++i)
	{
		int id = i < e_layerIdUndefined ? i : e_layerIdUndefined;
		m_surfaceTypes[i]->SetSurfaceTypeID(id);
	}
}

void CTerrainManager::ReloadSurfaceTypes(bool bUpdateEngineTerrain, bool bUpdateHeightmap)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY)(gEnv->pSystem);

	CXmlArchive ar;
	XmlNodeRef node = XmlHelpers::CreateXmlNode("SurfaceTypes_Editor");
	// Write all surface types.
	for (int i = 0; i < GetSurfaceTypeCount(); i++)
	{
		ar.root = node->newChild("SurfaceType");
		GetSurfaceTypePtr(i)->Serialize(ar);
	}

	gEnv->p3DEngine->LoadTerrainSurfacesFromXML(node, bUpdateEngineTerrain);

	if (bUpdateHeightmap && gEnv->p3DEngine->GetITerrain())
	{
		m_heightmap.UpdateEngineTerrain(false, false);
	}

	signalLayersChanged();
}

int CTerrainManager::FindSurfaceType(const string& name)
{
	for (int i = 0; i < m_surfaceTypes.size(); i++)
	{
		if (stricmp(m_surfaceTypes[i]->GetName(), name) == 0)
			return i;
	}
	return -1;
}

CLayer* CTerrainManager::FindLayer(const char* szLayerName) const
{
	for (int i = 0; i < GetLayerCount(); i++)
	{
		CLayer* pLayer = GetLayer(i);
		if (strcmp(pLayer->GetLayerName(), szLayerName) == 0)
			return pLayer;
	}
	return nullptr;
}

CLayer* CTerrainManager::FindLayerByLayerId(const uint32 dwLayerId) const
{
	for (int i = 0; i < GetLayerCount(); i++)
	{
		CLayer* layer = GetLayer(i);
		if (layer->GetCurrentLayerId() == dwLayerId)
			return layer;
	}
	return nullptr;
}

void CTerrainManager::InvalidateLayers()
{
	for (int i = 0; i < GetLayerCount(); ++i)
	{
		GetLayer(i)->InvalidateMask();
	}
}

void CTerrainManager::ClearLayers()
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	SelectLayer(s_invalidLayerIndex);

	for (CLayer* pLayer : m_layers)
	{
		delete pLayer;
	}

	m_layers.clear();

	ReloadSurfaceTypes();
}

void CTerrainManager::SerializeLayerSettings(CXmlArchive& xmlAr)
{
	if (xmlAr.bLoading)
	{
		// Loading
		int selectedLayer = GetSelectedLayerIndex();
		CLogFile::WriteLine("Loading layer settings...");
		CWaitProgress progress(_T("Loading Terrain Layers"));

		// Clear old layers
		ClearLayers();

		// Load the layer count
		XmlNodeRef layers = xmlAr.root->findChild("Layers");
		if (!layers)
			return;

		// Read all layers
		const int numLayers = layers->getChildCount();
		m_layers.reserve(numLayers);

		for (int i = 0; i < numLayers; i++)
		{
			progress.Step(100 * i / numLayers);

			CXmlArchive ar(xmlAr);
			ar.root = layers->getChild(i);

			CLayer* pLayer = new CLayer;
			pLayer->Serialize(ar);

			m_layers.push_back(pLayer);

			CryLog("  loaded editor layer %d  name='%s' LayerID=%d", i, pLayer->GetLayerName(), pLayer->GetCurrentLayerId());
		}

		// If surface type ids are unassigned, assign them.
		for (int i = 0; i < GetSurfaceTypeCount(); i++)
		{
			if (GetSurfaceTypePtr(i)->GetSurfaceTypeID() >= e_layerIdUndefined)
				GetSurfaceTypePtr(i)->AssignUnusedSurfaceTypeID();
		}

		ReloadSurfaceTypes();
		SelectLayer(selectedLayer);
	}
	else
	{
		CLogFile::WriteLine("Storing layer settings...");

		XmlNodeRef layers = xmlAr.root->newChild("Layers");

		// Write all layers
		for (CLayer* pLayer : m_layers)
		{
			CXmlArchive ar(xmlAr);
			ar.root = layers->newChild("Layer");
			pLayer->Serialize(ar);
		}
	}
}

uint32 CTerrainManager::GetDetailIdLayerFromLayerId(const uint32 dwLayerId)
{
	for (int i = 0, num = (int)m_layers.size(); i < num; i++)
	{
		CLayer& rLayer = *(m_layers[i]);

		if (dwLayerId == rLayer.GetOrRequestLayerId())
		{
			return rLayer.GetEngineSurfaceTypeId();
		}
	}

	// recreate referenced layer
	if (!GetIEditorImpl()->GetSystem()->GetI3DEngine()->GetITerrain())   // only if terrain loaded successfully
	{
		string no;

		no.Format("%d", dwLayerId);

		CLayer* pNewLayer = new CLayer;

		pNewLayer->SetLayerName(string("LAYER_") + no);
		pNewLayer->SetLayerId(dwLayerId);

		AddLayer(pNewLayer);
	}

	return e_layerIdUndefined;
}

void CTerrainManager::MarkUsedLayerIds(bool bFree[e_layerIdUndefined]) const
{
	for (const CLayer* pLayer : m_layers)
	{
		const uint32 id = pLayer->GetCurrentLayerId();

		if (id < e_layerIdUndefined)
			bFree[id] = false;
	}
}

void CTerrainManager::CreateDefaultLayer()
{
	// Create default layer.
	CLayer* layer = new CLayer;
	AddLayer(layer);
	layer->SetLayerName("Default");
	layer->LoadTexture("%ENGINE%/EngineAssets/Textures/grey.dds");
	layer->SetLayerId(0);

	// Create default surface type.
	CSurfaceType* sfType = new CSurfaceType;
	sfType->SetName("%ENGINE%/EngineAssets/Materials/material_terrain_default");
	AddSurfaceType(sfType);
	sfType->SetMaterial("%ENGINE%/EngineAssets/Materials/material_terrain_default");
	sfType->AssignUnusedSurfaceTypeID();

	layer->SetSurfaceType(sfType);
}

void CTerrainManager::SerializeTerrain(TDocMultiArchive& arrXmlAr)
{
	bool bLoading = IsLoadingXmlArArray(arrXmlAr);

	if (bLoading)
	{
		m_heightmap.Serialize((*arrXmlAr[DMAS_GENERAL]));

		// Surface Types ///////////////////////////////////////////////////////
		{
			CAutoLogTime logtime("Loading Surface Types");
			SerializeSurfaceTypes((*arrXmlAr[DMAS_TERRAIN_LAYERS]));
		}

		// Terrain texture /////////////////////////////////////////////////////
		{
			CAutoLogTime logtime("Loading Terrain Layers Info");
			SerializeLayerSettings((*arrXmlAr[DMAS_TERRAIN_LAYERS]));
		}

		{
			CAutoLogTime logtime("Load Terrain");

			m_heightmap.SerializeTerrain((*arrXmlAr[DMAS_GENERAL]));
		}

		if (!m_heightmap.IsAllocated())
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Heightmap data wasn't properly loaded. The file is missing or corrupted. Using this level is not recommended. Update level data from your backup.");
		}
		else
		{
			CAutoLogTime logtime("Process RGB Terrain Layers");
			ConvertLayersToRGBLayer();
		}
	}
	else
	{
		if (arrXmlAr[DMAS_GENERAL] != NULL)
		{
			// save terrain heightmap data
			m_heightmap.Serialize((*arrXmlAr[DMAS_GENERAL]));
			m_heightmap.SerializeTerrain((*arrXmlAr[DMAS_GENERAL]));
		}

		if (arrXmlAr[DMAS_TERRAIN_LAYERS] != NULL)
		{
			// Surface Types
			SerializeSurfaceTypes((*arrXmlAr[DMAS_TERRAIN_LAYERS]));
			SerializeLayerSettings((*arrXmlAr[DMAS_TERRAIN_LAYERS]));
		}
	}
}

void CTerrainManager::SerializeTerrain(CXmlArchive& xmlAr)
{
	if (xmlAr.bLoading)
	{
		m_heightmap.Serialize(xmlAr);

		// Surface Types ///////////////////////////////////////////////////////
		{
			CAutoLogTime logtime("Loading Surface Types");
			SerializeSurfaceTypes(xmlAr);
		}

		// Terrain texture /////////////////////////////////////////////////////
		{
			CAutoLogTime logtime("Loading Terrain Layers Info");
			SerializeLayerSettings(xmlAr);
		}

		{
			CAutoLogTime logtime("Load Terrain");
			m_heightmap.SerializeTerrain(xmlAr);
		}

		if (!m_heightmap.IsAllocated())
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Heightmap data wasn't properly loaded. The file is missing or corrupted. Using this level is not recommended. Update level data from your backup.");
		}
		else
		{
			CAutoLogTime logtime("Process RGB Terrain Layers");
			ConvertLayersToRGBLayer();
		}
	}
	else
	{
		// save terrain heightmap data
		m_heightmap.Serialize(xmlAr);

		m_heightmap.SerializeTerrain(xmlAr);

		// Surface Types
		SerializeSurfaceTypes(xmlAr);

		SerializeLayerSettings(xmlAr);
	}
}

void CTerrainManager::SerializeTexture(CXmlArchive& xmlAr)
{
	GetRGBLayer()->Serialize(xmlAr.root, xmlAr.bLoading);
}

void CTerrainManager::GetTerrainMemoryUsage(ICrySizer* pSizer)
{
	{
		SIZER_COMPONENT_NAME(pSizer, "Layers");
		for (const CLayer* pLayer : m_layers)
		{
			pLayer->GetMemoryUsage(pSizer);
		}
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "CHeightmap");
		m_heightmap.GetMemoryUsage(pSizer);
	}

	{
		SIZER_COMPONENT_NAME(pSizer, "RGBLayer");
		GetRGBLayer()->GetMemoryUsage(pSizer);
	}
}

int CTerrainManager::GetDataFilesCount() const
{
	return 2;
}

const char* CTerrainManager::GetDataFilename(int i) const
{
	switch (i)
	{
	case 0:
		return kHeightmapFile;
	case 1:
		return kTerrainTextureFile;
	default:
		return nullptr;
	}
}

bool CTerrainManager::ConvertLayersToRGBLayer()
{
	bool bConvertNeeded = !GetRGBLayer()->IsAllocated();

	if (!bConvertNeeded)
		return false;

	uint32 dwTexResolution = 0;

	for (const CLayer* pLayer : m_layers)
	{
		dwTexResolution = max(dwTexResolution, (uint32)pLayer->GetMaskResolution());
	}

	if (QDialogButtonBox::StandardButton::No == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Convert LayeredTerrainTexture to RGBTerrainTexture?"), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::No))
	{
		return false;
	}

	if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(""), QObject::tr("Double the resolution of the existing data?"), QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::No))
		dwTexResolution *= 2;

	// extract RGB texture
	{
		CTerrainLayerTexGen texGen;

		texGen.Init(dwTexResolution);

		SSectorInfo si;

		GetIEditorImpl()->GetHeightmap()->GetSectorsInfo(si);

		uint32 dwNumSectors = si.numSectors;

		uint32 dwSectorTexResolution = 512;
		uint32 dwSectorResolution = dwTexResolution / dwNumSectors;
		uint32 dwNumTexSectors = dwTexResolution / dwSectorTexResolution;

		// no error check because this is allocating only very few bytes
		GetRGBLayer()->AllocateTiles(dwNumTexSectors, dwNumTexSectors, dwSectorTexResolution);  // dwTexResolution x dwTexResolution;

		CImageEx sectorDiffuseImage;

		if (!sectorDiffuseImage.Allocate(dwSectorResolution, dwSectorResolution))
			return false;

		int flags = ETTG_QUIET | ETTG_NO_TERRAIN_SHADOWS | ETTG_ABGR;

		for (int y = 0; y < dwNumSectors; y++)
			for (int x = 0; x < dwNumSectors; x++)
			{
				CPoint sector(x, y);

				CRect rect(0, 0, sectorDiffuseImage.GetWidth(), sectorDiffuseImage.GetHeight());

				if (!texGen.GenerateSectorTexture(sector, rect, flags, sectorDiffuseImage))
					return false;     // maybe this should be handled better

				// M.M. to take a look at the result of the calculation
				/*char str[80];
				   cry_sprintf(str,"c:\\temp\\tile%d_%d.bmp",x,y);
				   CImageUtil::SaveBitmap(str,sectorDiffuseImage);
				 */
				GetRGBLayer()->SetSubImageRGBLayer(x * dwSectorResolution, y * dwSectorResolution, sectorDiffuseImage);
			}
	}

	// extract Detail LayerId info
	GetIEditorImpl()->GetHeightmap()->CalcSurfaceTypes();

	// we no longer need the layer mask data
	for (CLayer* pLayer : m_layers)
	{
		pLayer->ReleaseMask();
	}

	GetIEditorImpl()->GetHeightmap()->UpdateEngineTerrain(false);

	return true;
}

void CTerrainManager::SetTerrainSize(int resolution, float unitSize)
{
	m_heightmap.Resize(resolution, resolution, unitSize);
	GetRGBLayer()->SetDirty();
}

void CTerrainManager::ResetHeightMap()
{
	m_heightmap.ClearModSectors();
	m_heightmap.SetWaterLevel(16); // Default water level.
	SetTerrainSize(1024, 2);
	m_heightmap.SetMaxHeight(1024);
}

CRGBLayer* CTerrainManager::GetRGBLayer()
{
	return m_heightmap.GetRGBLayer();
}

void CTerrainManager::Save(bool bBackup)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	CTempFileHelper helper(GetIEditorImpl()->GetLevelDataFolder() + kHeightmapFile);

	CXmlArchive xmlAr;
	SerializeTerrain(xmlAr);
	xmlAr.Save(helper.GetTempFilePath());

	helper.UpdateFile(bBackup);
}

bool CTerrainManager::Load()
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	string filename = GetIEditorImpl()->GetLevelDataFolder() + kHeightmapFile;
	CXmlArchive xmlAr;
	xmlAr.bLoading = true;
	if (!xmlAr.Load(filename))
	{
		return false;
	}

	SerializeTerrain(xmlAr);
	return true;
}

void CTerrainManager::SaveTexture(bool bBackup)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	CTempFileHelper helper(GetIEditorImpl()->GetLevelDataFolder() + kTerrainTextureFile);

	XmlNodeRef root;
	GetRGBLayer()->Serialize(root, false);
	if (root)
		root->saveToFile(helper.GetTempFilePath());

	helper.UpdateFile(bBackup);
}

bool CTerrainManager::LoadTexture()
{
	string filename = GetIEditorImpl()->GetLevelDataFolder() + kTerrainTextureFile;
	XmlNodeRef root = XmlHelpers::LoadXmlFromFile(filename);
	if (root)
	{
		GetRGBLayer()->Serialize(root, true);
		return true;
	}
	return false;
}

void CTerrainManager::SetModified(int x1, int y1, int x2, int y2)
{
	if (!gEnv->p3DEngine->GetITerrain())
		return;

	AABB bounds;
	bounds.Reset();
	if (!(x1 == 0 && y1 == 0 && x2 == 0 && y2 == 0))
	{
		// Here we are making sure that we will update the whole sectors where the heightmap was changed.
		int nTerrainSectorSize(gEnv->p3DEngine->GetTerrainSectorSize());
		assert(nTerrainSectorSize > 0);

		x1 *= m_heightmap.GetUnitSize();
		y1 *= m_heightmap.GetUnitSize();

		x2 *= m_heightmap.GetUnitSize();
		y2 *= m_heightmap.GetUnitSize();

		x1 /= nTerrainSectorSize;
		y1 /= nTerrainSectorSize;
		x2 /= nTerrainSectorSize;
		y2 /= nTerrainSectorSize;

		// Y and X switched by historical reasons.
		bounds.Add(Vec3((y1 - 1) * nTerrainSectorSize, (x1 - 1) * nTerrainSectorSize, -32000.0f));
		bounds.Add(Vec3((y2 + 1) * nTerrainSectorSize, (x2 + 1) * nTerrainSectorSize, +32000.0f));
	}

	signalTerrainChanged();
}

void CTerrainManager::SelectLayer(int layerIndex)
{
	for (CLayer* pLayer : m_layers)
		pLayer->SetSelected(false);

	CLayer* pSelectedLayer = nullptr;
	if (0 <= layerIndex && m_layers.size() > layerIndex)
	{
		pSelectedLayer = m_layers[layerIndex];
		if (pSelectedLayer)
			pSelectedLayer->SetSelected(true);
	}

	signalSelectedLayerChanged(pSelectedLayer);
}

int CTerrainManager::GetSelectedLayerIndex()
{
	for (int i = 0; i < m_layers.size(); ++i)
	{
		CLayer* pLayer = m_layers[i];
		if (pLayer->IsSelected())
		{
			return i;
		}
	}

	return s_invalidLayerIndex;
}

CLayer* CTerrainManager::GetSelectedLayer()
{
	int idx = GetSelectedLayerIndex();
	if (0 <= idx && m_layers.size() > idx)
		return m_layers[idx];

	return nullptr;
}

void CTerrainManager::AddLayer(CLayer* pLayer, int index)
{
	std::vector<CLayer*>::iterator itPos;
	if (index == s_invalidLayerIndex || index > m_layers.size())
	{
		itPos = m_layers.end();
		index = m_layers.size();
	}
	else
	{
		itPos = m_layers.begin() + index;
	}

	m_layers.insert(itPos, pLayer);

	ReloadSurfaceTypes();
	SelectLayer(index);
}

void CTerrainManager::RemoveSelectedLayer()
{
	const int index = GetSelectedLayerIndex();
	if (index == s_invalidLayerIndex)
	{
		return;
	}

	CLayer* pLayer = m_layers[index];
	if (!pLayer)
	{
		return;
	}

	CUndo undo("Delete Terrain Layer");
	GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CTerrainLayersPropsUndoObject);

	if (pLayer->GetCurrentLayerId() != e_layerIdUndefined)
	{
		uint32 id = pLayer->GetCurrentLayerId();

		if (id != e_layerIdUndefined)
			m_heightmap.EraseLayerID(id);
	}

	delete pLayer;
	m_layers.erase(m_layers.begin() + index);

	ReloadSurfaceTypes();

	const int newSelection = (index != m_layers.size()) ? index : index - 1;
	SelectLayer(newSelection);
}

void CTerrainManager::DuplicateSelectedLayer()
{
	const int index = GetSelectedLayerIndex();
	if (index == s_invalidLayerIndex)
	{
		return;
	}

	CLayer* pLayer = m_layers[index];
	if (!pLayer)
	{
		return;
	}

	CUndo undo("Duplicate Terrain Layer");
	GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CTerrainLayersPropsUndoObject);

	CLayer* pNewLayer = pLayer->Duplicate();
	AddLayer(pNewLayer, index + 1);
}

void CTerrainManager::MoveLayer(int oldPos, int newPos)
{
	if (oldPos == newPos)
	{
		return;
	}

	CUndo undo("Move Terrain Layer");
	GetIEditorImpl()->GetIUndoManager()->RecordUndo(new CTerrainLayersPropsUndoObject);

	CRY_ASSERT(oldPos >= 0 && oldPos < m_layers.size());

	if (newPos == s_invalidLayerIndex)
	{
		newPos = m_layers.size();
	}

	if (newPos > oldPos)
	{
		// Adjust because of removed element
		--newPos;
	}

	CRY_ASSERT(newPos >= 0 && newPos < m_layers.size());

	CLayer* pMovedLayer = m_layers[oldPos];

	m_layers.erase(m_layers.begin() + oldPos);

	m_layers.insert(m_layers.begin() + newPos, pMovedLayer);

	signalLayersChanged();
}
