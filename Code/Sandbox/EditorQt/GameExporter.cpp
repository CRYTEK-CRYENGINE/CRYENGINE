// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include <CryAISystem/IAISystem.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryGame/IGameFramework.h>
#include <CryGame/IGame.h>

#include "GameEngine.h"
#include "CryEditDoc.h"
#include "Mission.h"
#include "ShaderCache.h"
#include "CrySandbox/ScopedVariableSetter.h"
#include "Preferences/GeneralPreferences.h"
#include "Plugins/EditorCommon/FilePathUtil.h"

#include "ParticleExporter.h"

#include "AnimationSerializer.h"
#include "Material/MaterialManager.h"
#include "Material/MaterialLibrary.h"
#include "Particles/ParticleManager.h"
#include "GameTokens/GameTokenManager.h"

#include "AI/NavDataGeneration/Navigation.h"
#include "AI/AIManager.h"

#include "Vegetation/VegetationMap.h"
#include "Terrain/TextureCompression.h"
#include "Terrain/TerrainTexGen.h"
#include "Terrain/TerrainManager.h"
#include "TerrainLighting.h"
#include "Objects/ObjectLayerManager.h"
#include "Objects/ObjectPhysicsManager.h"
#include "Objects/EntityObject.h"
#include "LensFlareEditor/LensFlareManager.h"
#include "LensFlareEditor/LensFlareLibrary.h"
#include "LensFlareEditor/LensFlareItem.h"
#include "GameExporter.h"
#include "EntityPrototypeManager.h"
#include "Prefabs/PrefabManager.h"

#include <CryMath/Random.h>

//////////////////////////////////////////////////////////////////////////
#define GAMETOKENS_LEVEL_LIBRARY_FILE "LevelGameTokens.xml"
#define MATERIAL_LEVEL_LIBRARY_FILE   "Materials.xml"
#define RESOURCE_LIST_FILE            "ResourceList.txt"
#define PERLAYER_RESOURCE_LIST_FILE   "PerLayerResourceList.txt"
#define SHADER_LIST_FILE              "ShadersList.txt"

#define GetAValue(rgb) ((BYTE)((rgb) >> 24))

#pragma pack(push,1)
struct SAOInfo
{
	float fMax;
	float GetMax()           { return fMax; }
	void  SetMax(float fVal) { fMax = fVal; }
};
#pragma pack(pop)

const float gAOObjectsZRange = 32.f;
double gBuildSectorAODataTime = 0;

// low quality + nosky, high quality + nosky, low quality + sky, high quality + sky, TODO: completely remove low quality option (user can reduce resolution for more performance)
const ETEX_Format eTerrainPrimaryTextureFormat[2][2] = {
	{ eTF_BC3, eTF_BC3 }, { eTF_BC3, eTF_BC3 }
};
const ETEX_Format eTerrainSecondaryTextureFormat[2][2] = {
	{ eTF_BC3, eTF_BC3 }, { eTF_BC3, eTF_BC3 }
};

//////////////////////////////////////////////////////////////////////////
// SGameExporterSettings
//////////////////////////////////////////////////////////////////////////
SGameExporterSettings::SGameExporterSettings() :
	iExportTexWidth(4096),
	bHiQualityCompression(true),
	bUpdateIndirectLighting(false),
	nApplySS(1),
	fBrMultiplier(1.0f),
	eExportEndian(GetPlatformEndian())
{
}

//////////////////////////////////////////////////////////////////////////
void SGameExporterSettings::SetLowQuality()
{
	iExportTexWidth = 4096;
	bHiQualityCompression = false;
	bUpdateIndirectLighting = false;
	nApplySS = 0;
}

//////////////////////////////////////////////////////////////////////////
void SGameExporterSettings::SetHiQuality()
{
	iExportTexWidth = 16384;
	bHiQualityCompression = true;
	bUpdateIndirectLighting = true;
	nApplySS = 1;
}

CGameExporter* CGameExporter::m_pCurrentExporter = NULL;

//////////////////////////////////////////////////////////////////////////
// CGameExporter
//////////////////////////////////////////////////////////////////////////
CGameExporter::CGameExporter(const SGameExporterSettings& settings) :
	m_bAutoExportMode(false),
	m_settings(settings)
{
	m_pCurrentExporter = this;
}

CGameExporter::~CGameExporter()
{
	m_pCurrentExporter = NULL;
}

CGameExporter* CGameExporter::GetCurrentExporter()
{
	return m_pCurrentExporter;
}

//////////////////////////////////////////////////////////////////////////
bool CGameExporter::Export(unsigned int flags, const char* subdirectory)
{
	CGameEngine* pGameEngine = GetIEditorImpl()->GetGameEngine();
	if (pGameEngine->GetLevelPath().IsEmpty())
		return false;

	if (flags & eExp_AI_All)
	{
		if (!GetIEditorImpl()->GetAI()->IsReadyToGameExport(flags))
		{
			return false;
		}
	}

	GetIEditorImpl()->Notify(eNotify_OnBeginExportToGame);
	if (!DoExport(flags, subdirectory))
	{
		GetIEditorImpl()->Notify(eNotify_OnExportToGameFailed);
		return false;
	}

	GetIEditorImpl()->Notify(eNotify_OnExportToGame);
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CGameExporter::DoExport(unsigned int flags, const char* subdirectory)
{
	CAutoDocNotReady autoDocNotReady;
	CObjectManagerLevelIsExporting levelIsExportingFlag;
	CWaitCursor waitCursor;

	CGameEngine* pGameEngine = GetIEditorImpl()->GetGameEngine();
	CObjectManager* pObjectManager = static_cast<CObjectManager*>(GetIEditorImpl()->GetObjectManager());
	CObjectLayerManager* pObjectLayerManager = pObjectManager->GetLayersManager();

	SetCurrentDirectory(GetIEditorImpl()->GetMasterCDFolder());

	// Close all Editor tools
	GetIEditorImpl()->SetEditTool(0);

	string sLevelPath = PathUtil::AddSlash(pGameEngine->GetLevelPath().GetString());
	if (subdirectory && subdirectory[0] && strcmp(subdirectory, ".") != 0)
	{
		sLevelPath = PathUtil::AddSlash((sLevelPath + subdirectory).GetString());
		CreateDirectory(sLevelPath.GetString(), 0);
	}

	m_levelPak.m_sPath = string(sLevelPath) + GetLevelPakFilename();

	m_levelPath = PathUtil::RemoveSlash(sLevelPath.GetString()).c_str();
	string rootLevelPath = PathUtil::AddSlash(pGameEngine->GetLevelPath().GetString());

	if (pObjectLayerManager->InitLayerSwitches())
		pObjectLayerManager->SetupLayerSwitches(false, true);

	// Exclude objects from layers without export flag
	pObjectManager->UnregisterNoExported();

	CCryEditDoc* pDocument = GetIEditorImpl()->GetDocument();

	if (flags & eExp_Fast)
	{
		m_settings.SetLowQuality();
	}
	else if (m_bAutoExportMode)
	{
		m_settings.SetHiQuality();
	}
	else if (flags & eExp_SurfaceTexture)
	{
		LightingSettings* pSettings = pDocument->GetLighting();
		m_settings.nApplySS = pSettings->iILApplySS;
	}

	CryAutoLock<CryMutex> autoLock(CGameEngine::GetPakModifyMutex());

	// Close this pak file.
	if (!CloseLevelPack(m_levelPak, true))
	{
		Error("Cannot close Pak file " + m_levelPak.m_sPath);
		return false;
	}

	if (m_bAutoExportMode)
	{
		// Remove read-only flags.
		CrySetFileAttributes(m_levelPak.m_sPath, FILE_ATTRIBUTE_NORMAL);
	}

	//////////////////////////////////////////////////////////////////////////
	if (!CFileUtil::OverwriteFile(m_levelPak.m_sPath))
	{
		Error("Cannot overwrite Pak file " + m_levelPak.m_sPath);
		return false;
	}

	if (!OpenLevelPack(m_levelPak, false))
	{
		Error("Cannot open Pak file " + m_levelPak.m_sPath + " for writing.");
		return false;
	}

	////////////////////////////////////////////////////////////////////////
	// Inform all objects that an export is about to begin
	////////////////////////////////////////////////////////////////////////
	GetIEditorImpl()->GetObjectManager()->GetPhysicsManager()->PrepareForExport();
	GetIEditorImpl()->GetObjectManager()->SendEvent(EVENT_PRE_EXPORT);

	////////////////////////////////////////////////////////////////////////
	// Export all data to the game
	////////////////////////////////////////////////////////////////////////
	if (!ExportMap(sLevelPath, flags & eExp_SurfaceTexture, m_settings.eExportEndian))
		return false;

	////////////////////////////////////////////////////////////////////////
	// Export the heightmap, store shadow informations in it
	////////////////////////////////////////////////////////////////////////
	ExportHeightMap(sLevelPath, m_settings.eExportEndian);

	//////////////////////////////////////////////////////////////////////////
	ExportDynTexSrcLayerInfo(sLevelPath);

	////////////////////////////////////////////////////////////////////////
	// Exporting map setttings
	////////////////////////////////////////////////////////////////////////
	ExportOcclusionMesh(sLevelPath);

	////////////////////////////////////////////////////////////////////////
	// Exporting map setttings
	////////////////////////////////////////////////////////////////////////
	//ExportMapIni( sLevelPath );

	//////////////////////////////////////////////////////////////////////////
	// Export Particles.
	{
		CParticlesExporter partExporter;
		partExporter.ExportParticles(sLevelPath, m_levelPath, m_levelPak.m_pakFile);
	}

	// Export prototypes/archetypes
	{
		GetIEditorImpl()->GetEntityProtManager()->ExportPrototypes(sLevelPath, m_levelPath, m_levelPak.m_pakFile);
	}

	//! Export Level data.
	CLogFile::WriteLine("Exporting LevelData.xml");
	ExportLevelData(sLevelPath);
	CLogFile::WriteLine("Exporting LevelData.xml done.");

	ExportLevelInfo(sLevelPath);

	// (MATT) Function copies bai files into PAK (and attempts to do other things) {2008/08/11}
	//if(bAI) bAI was always true
	{
		if (!(flags & eExp_Fast))
		{
			CLogFile::WriteLine("Regenerating AI data!");
			pGameEngine->GenerateAiAll(flags & eExp_AI_All);
		}
		ExportAI(sLevelPath, flags & eExp_AI_All);
	}

	CLogFile::WriteLine("Exporting Game Data...");
	ExportGameData(sLevelPath);
	CLogFile::WriteLine("Exporting Game Data done");

	//////////////////////////////////////////////////////////////////////////
	// Start Movie System animations.
	//////////////////////////////////////////////////////////////////////////
	ExportAnimations(sLevelPath);

	//////////////////////////////////////////////////////////////////////////
	// Export Brushes.
	//////////////////////////////////////////////////////////////////////////
	ExportBrushes(sLevelPath);

	ExportLevelLensFlares(sLevelPath);
	ExportLevelResourceList(sLevelPath);
	ExportLevelPerLayerResourceList(sLevelPath);
	ExportLevelShaderCache(sLevelPath);
	//////////////////////////////////////////////////////////////////////////
	// Export list of entities to save/load during gameplay
	//////////////////////////////////////////////////////////////////////////
	CLogFile::WriteLine("Exporting serialization list");
	XmlNodeRef entityList = XmlHelpers::CreateXmlNode("EntitySerialization");
	pGameEngine->BuildEntitySerializationList(entityList);
	string levelDataFile = sLevelPath + "Serialize.xml";
	string pakFilename = PathUtil::RemoveSlash(sLevelPath.GetString()) + "Level.pak";
	XmlString xmlData = entityList->getXML();
	CCryMemFile file;
	file.Write(xmlData.c_str(), xmlData.length());
	m_levelPak.m_pakFile.UpdateFile(levelDataFile, file);

	//////////////////////////////////////////////////////////////////////////
	// End Exporting Game data.
	//////////////////////////////////////////////////////////////////////////

#if defined(IS_COMMERCIAL)
	MAKE_ENCRYPTED_COMMERCIAL_LEVEL_PAK(m_levelPak.m_pakFile.GetArchive());
#endif

	// Close all packs.
	CloseLevelPack(m_levelPak, false);
	//	m_texturePakFile.Close();

	pObjectManager->RegisterNoExported();

	pObjectLayerManager->InitLayerSwitches(true);

	////////////////////////////////////////////////////////////////////////
	// Reload the level in the engine
	////////////////////////////////////////////////////////////////////////
	if (flags & eExp_ReloadTerrain)
	{
		pGameEngine->ReloadLevel();
	}

	// Disabled, for now. (inside EncryptPakFile)
	EncryptPakFile(pakFilename);

	// Reopen this pak file.
	if (!OpenLevelPack(m_levelPak, true))
	{
		Error("Cannot open Pak file " + m_levelPak.m_sPath);
		return false;
	}

	// Commit changes to the disk.
	_flushall();

	// finally create filelist.xml
	string levelName = PathUtil::GetFileName(pGameEngine->GetLevelPath());
	ExportFileList(sLevelPath, levelName);

	pDocument->SetLevelExported(true);

	CLogFile::WriteLine("Exporting was successful.");

	return true;
}

bool CGameExporter::ExportMap(const char* pszGamePath, bool bSurfaceTexture, EEndian eExportEndian)
{
	string ctcFilename;
	ctcFilename.Format("%s%s", pszGamePath, COMPILED_TERRAIN_TEXTURE_FILE_NAME);

	// console and PC uses same terrain texture file
	if (eExportEndian != GetPlatformEndian())
	{
		m_levelPak.m_pakFile.RemoveFile(ctcFilename);
		Error("Export Endian is different from the platform Endian (incompatible platforms?)");
		return false;
	}

	// No need to generate texture if there are no layers or the caller does
	// not demand the texture to be generated
	if (GetIEditorImpl()->GetTerrainManager()->GetLayerCount() == 0 || !bSurfaceTexture)
	{
		return true;
	}

	CryLog("Exporting data to game (%s)...", pszGamePath);

	////////////////////////////////////////////////////////////////////////
	// Export the terrain texture
	////////////////////////////////////////////////////////////////////////

	CLogFile::WriteLine("Exporting terrain texture.");

	// Check dimensions
	CHeightmap* pHeightMap = GetIEditorImpl()->GetHeightmap();
	if (pHeightMap->GetWidth() != pHeightMap->GetHeight() || pHeightMap->GetWidth() % 128)
	{
		ASSERT(pHeightMap->GetWidth() % 128 == 0);
		CLogFile::WriteLine("Can't export a heightmap");
		Error("Can't export a heightmap with dimensions that can't be evenly divided by 128 or that are not square!");
		return false;
	}

	DWORD startTime = GetTickCount();

	if (!ExportSurfaceTexture(m_levelPak.m_pakFile, ctcFilename))
	{
		return "Can't export surface texture " + ctcFilename;
	}

	{
		CLogFile::WriteLine("Update terrain texture file...");

		GetIEditorImpl()->GetTerrainManager()->GetRGBLayer()->CleanupCache();   // clean up memory

		// close writing, open for reading, compute file CRC, open for writing, update file CRC
		CloseLevelPack(m_levelPak, false);

		OpenLevelPack(m_levelPak, true);
		uint32 dwCRC = gEnv->pCryPak->ComputeCRC(ctcFilename);
		CloseLevelPack(m_levelPak, true);

		if (!OpenLevelPack(m_levelPak, false))
		{
			Error("Level PAK file cannot be opened, make sure it is not locked by other process (or threads)");
			return false;
		}

		m_levelPak.m_pakFile.GetArchive()->UpdateFileCRC(ctcFilename, dwCRC);
		CloseLevelPack(m_levelPak, false);

		// Reopen the file again, to restore previous open state.
		OpenLevelPack(m_levelPak, false);
	}

	pHeightMap->SetSurfaceTextureSize(m_settings.iExportTexWidth, m_settings.iExportTexWidth);
	pHeightMap->ClearModSectors();

	CryLog("Terrain Texture Exported in %u seconds.", (GetTickCount() - startTime) / 1000);
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportHeightMap(const char* pszGamePath, EEndian eExportEndian)
{
	char szFileOutputPath[_MAX_PATH];

	// export compiled terrain
	CLogFile::WriteLine("Exporting terrain...");

	// remove old files
	cry_sprintf(szFileOutputPath, "%s%s", pszGamePath, COMPILED_HEIGHT_MAP_FILE_NAME);
	m_levelPak.m_pakFile.RemoveFile(szFileOutputPath);
	cry_sprintf(szFileOutputPath, "%s%s", pszGamePath, COMPILED_VISAREA_MAP_FILE_NAME);
	m_levelPak.m_pakFile.RemoveFile(szFileOutputPath);

	SHotUpdateInfo* pExportInfo = NULL;

	SHotUpdateInfo exportInfo;
	I3DEngine* p3DEngine = GetIEditorImpl()->Get3DEngine();

	if (eExportEndian == GetPlatformEndian()) // skip second export, this data is common for PC and consoles
	{
		std::vector<struct IStatObj*>* pTempBrushTable = NULL;
		std::vector<struct IMaterial*>* pTempMatsTable = NULL;
		std::vector<struct IStatInstGroup*>* pTempVegGroupTable = NULL;

		if (ITerrain* pTerrain = p3DEngine->GetITerrain())
		{
			if (int nSize = pTerrain->GetCompiledDataSize(pExportInfo))
			{
				// get terrain data from 3dengine and save it into file
				uint8* pData = new uint8[nSize];
				pTerrain->GetCompiledData(pData, nSize, &pTempBrushTable, &pTempMatsTable, &pTempVegGroupTable, eExportEndian, pExportInfo);
				cry_sprintf(szFileOutputPath, "%s%s", pszGamePath, COMPILED_HEIGHT_MAP_FILE_NAME);
				CCryMemFile hmapCompiledFile;
				hmapCompiledFile.Write(pData, nSize);
				m_levelPak.m_pakFile.UpdateFile(szFileOutputPath, hmapCompiledFile);
				delete[] pData;
			}
		}

		////////////////////////////////////////////////////////////////////////
		// Export instance lists of pre-mergedmeshes
		////////////////////////////////////////////////////////////////////////
		ExportMergedMeshInstanceSectors(pszGamePath, eExportEndian, pTempVegGroupTable);

		// export visareas
		CLogFile::WriteLine("Exporting indoors...");
		if (IVisAreaManager* pVisAreaManager = p3DEngine->GetIVisAreaManager())
		{
			if (int nSize = pVisAreaManager->GetCompiledDataSize())
			{
				// get visareas data from 3dengine and save it into file
				uint8* pData = new uint8[nSize];
				pVisAreaManager->GetCompiledData(pData, nSize, &pTempBrushTable, &pTempMatsTable, &pTempVegGroupTable, eExportEndian);
				cry_sprintf(szFileOutputPath, "%s%s", pszGamePath, COMPILED_VISAREA_MAP_FILE_NAME);
				CCryMemFile visareasCompiledFile;
				visareasCompiledFile.Write(pData, nSize);
				m_levelPak.m_pakFile.UpdateFile(szFileOutputPath, visareasCompiledFile);
				delete[] pData;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportMergedMeshInstanceSectors(const char* pszGamePath, EEndian eExportEndian, std::vector<struct IStatInstGroup*>* pVegGroupTable)
{
	char szFileOutputPath[_MAX_PATH];
	DynArray<string> usedMeshes;
	const char* status = "Exporting merged meshes sectors...";

	I3DEngine* p3DEngine = GetIEditorImpl()->Get3DEngine();
	IMergedMeshesManager* mergedMeshes = p3DEngine->GetIMergedMeshesManager();

	CLogFile::WriteLine(status);

	// Nuke old directory if present to make sure that it's not present anymore
	cry_sprintf(szFileOutputPath, "%s%s", pszGamePath, COMPILED_MERGED_MESHES_BASE_NAME);
	m_levelPak.m_pakFile.RemoveDir(szFileOutputPath);

	mergedMeshes->CompileSectors(pVegGroupTable);
	const size_t nCompiledSectorCount = mergedMeshes->GetInstanceSectorCount();

	// Save out the compiled sector files into separate files
	for (size_t i = 0; i < nCompiledSectorCount; ++i)
	{
		const IMergedMeshesManager::SInstanceSector& instanceSector = mergedMeshes->GetInstanceSector(i);
		cry_sprintf(szFileOutputPath, "%s%s\\%s.dat", pszGamePath, COMPILED_MERGED_MESHES_BASE_NAME, instanceSector.id.c_str());
		m_levelPak.m_pakFile.UpdateFile(szFileOutputPath, const_cast<uint8*>(&instanceSector.data[0]), instanceSector.data.size());
	}
	mergedMeshes->ClearInstanceSectors();

	status = "Exporting merged meshes list...";
	CLogFile::WriteLine(status);

	mergedMeshes->GetUsedMeshes(usedMeshes);

	std::vector<char> byteArray;
	for (int i = 0; i < usedMeshes.size(); ++i)
	{
		byteArray.reserve(usedMeshes[i].size() + byteArray.size() + 1);
		byteArray.insert(byteArray.end(), usedMeshes[i].begin(), usedMeshes[i].end());
		byteArray.insert(byteArray.end(), '\n');
	}

	cry_sprintf(szFileOutputPath, "%s%s\\%s", pszGamePath, COMPILED_MERGED_MESHES_BASE_NAME, COMPILED_MERGED_MESHES_LIST);
	m_levelPak.m_pakFile.UpdateFile(szFileOutputPath, (byteArray.size() ? &byteArray[0] : NULL), byteArray.size());
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportDynTexSrcLayerInfo(const char* pszGamePath)
{
	XmlNodeRef root = GetIEditorImpl()->GetObjectManager()->GetLayersManager()->GenerateDynTexSrcLayerInfo();

	string levelDataFile = string(pszGamePath) + "DynTexSrcLayerAct.xml";
	XmlString xmlData = root->getXML();
	CCryMemFile file;
	file.Write(xmlData.c_str(), xmlData.length());
	m_levelPak.m_pakFile.UpdateFile(levelDataFile, file);
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportOcclusionMesh(const char* pszGamePath)
{
	XmlNodeRef root = GetIEditorImpl()->GetObjectManager()->GetLayersManager()->GenerateDynTexSrcLayerInfo();

	string levelDataFile = string(pszGamePath) + "occluder.ocm";
	CFile FileIn;
	if (FileIn.Open(levelDataFile, CFile::modeRead))
	{
		CMemoryBlock Temp;
		const size_t Size = FileIn.GetLength();
		Temp.Allocate(Size);
		FileIn.Read(Temp.GetBuffer(), Size);
		FileIn.Close();
		CCryMemFile FileOut;
		FileOut.Write(Temp.GetBuffer(), Size);
		m_levelPak.m_pakFile.UpdateFile(levelDataFile, FileOut);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CGameExporter::ExportSvogiData()
{
	CScopedVariableSetter<bool> autoBackupEnabledChange(gEditorFilePreferences.autoSaveEnabled, false);

	if (!GetIEditorImpl()->GetGameEngine()->IsLevelLoaded())
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Please load a level before attempting to export.");
		return false;
	}
	if (!GetIEditorImpl()->GetDocument()->IsDocumentReady())
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Please wait until previous operation will be finished.");
		return false;
	}

	CGameEngine* pGameEngine = GetIEditorImpl()->GetGameEngine();
	if (pGameEngine->GetLevelPath().IsEmpty())
		return false;

	CObjectManager* pObjectManager = static_cast<CObjectManager*>(GetIEditorImpl()->GetObjectManager());
	CObjectLayerManager* pObjectLayerManager = pObjectManager->GetLayersManager();
	if (pObjectLayerManager->InitLayerSwitches())
		pObjectLayerManager->SetupLayerSwitches(false, true);

	string sLevelPath = PathUtil::AddSlash(pGameEngine->GetLevelPath().GetString());

	m_SvogiDataPak.m_sPath = string(sLevelPath) + GetSvogiDataPakFilename();

	// Close this pak file.
	if (!CloseLevelPack(m_SvogiDataPak, true))
	{
		Error("Cannot close Pak file " + m_SvogiDataPak.m_sPath);
		return false;
	}

	if (!CFileUtil::OverwriteFile(m_SvogiDataPak.m_sPath))
	{
		Error("Cannot overwrite Pak file " + m_SvogiDataPak.m_sPath);
		return false;
	}

	// completely remove old pak. TODO: support incremental update - update only modified level segments
	PathUtil::RemoveFile(m_SvogiDataPak.m_sPath);

	if (!OpenLevelPack(m_SvogiDataPak, false))
	{
		Error("Cannot open Pak file " + m_SvogiDataPak.m_sPath + " for writing.");
		return false;
	}

	CLogFile::WriteLine("Exporting SVO...");

	I3DEngine* p3DEngine = GetIEditorImpl()->Get3DEngine();

	if (p3DEngine->GetSvoCompiledData(m_SvogiDataPak.m_pakFile.GetArchive()))
	{
		CLogFile::WriteLine("SVO exported successfully");
	}
	else
	{
		CLogFile::WriteLine("SVO export disabled or nothing to export");
	}

	CloseLevelPack(m_SvogiDataPak, false);

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportAnimations(const string& path)
{
	CLogFile::WriteLine("Export animation sequences...");
	CAnimationSerializer animSaver;
	animSaver.SaveAllSequences(path, m_levelPak.m_pakFile);
	CLogFile::WriteString("Done.");
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportLevelData(const string& path, bool bExportMission)
{
	XmlNodeRef root = XmlHelpers::CreateXmlNode("LevelData");
	root->setAttr("SandboxVersion", (const char*)GetIEditorImpl()->GetFileVersion().ToFullString());
	XmlNodeRef rootAction = XmlHelpers::CreateXmlNode("LevelDataAction");
	rootAction->setAttr("SandboxVersion", (const char*)GetIEditorImpl()->GetFileVersion().ToFullString());

	ExportMapInfo(root);

	//////////////////////////////////////////////////////////////////////////
	/// Export vegetation objects.
	XmlNodeRef vegetationNode = root->newChild("Vegetation");
	CVegetationMap* pVegetationMap = GetIEditorImpl()->GetVegetationMap();
	if (pVegetationMap)
		pVegetationMap->SerializeObjects(vegetationNode);

	//////////////////////////////////////////////////////////////////////////
	// Export materials.
	ExportMaterials(root, path);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Export particle manager.
	GetIEditorImpl()->GetParticleManager()->Export(root);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Export Level GameTokens.
	ExportGameTokens(root, path);
	//////////////////////////////////////////////////////////////////////////

	CCryEditDoc* pDocument = GetIEditorImpl()->GetDocument();
	CMission* pCurrentMission = 0;

	if (bExportMission)
	{
		pCurrentMission = pDocument->GetCurrentMission();
		// Save contents of current mission.
	}

	//////////////////////////////////////////////////////////////////////////
	// Export missions tag.
	//////////////////////////////////////////////////////////////////////////
	XmlNodeRef missionsNode = rootAction->newChild("Missions");
	string missionFileName;
	string currentMissionFileName;
	I3DEngine* p3DEngine = GetIEditorImpl()->Get3DEngine();
	for (int i = 0; i < pDocument->GetMissionCount(); i++)
	{
		CMission* pMission = pDocument->GetMission(i);

		string name = pMission->GetName();
		name.Replace(' ', '_');
		missionFileName.Format("Mission_%s.xml", (const char*)name);

		XmlNodeRef missionDescNode = missionsNode->newChild("Mission");
		missionDescNode->setAttr("Name", pMission->GetName());
		missionDescNode->setAttr("File", missionFileName);
		missionDescNode->setAttr("CGFCount", p3DEngine->GetLoadedObjectCount());

		int nProgressBarRange = m_numExportedMaterials / 10 + p3DEngine->GetLoadedObjectCount();
		missionDescNode->setAttr("ProgressBarRange", nProgressBarRange);

		if (pMission == pCurrentMission)
		{
			currentMissionFileName = missionFileName;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Save Level Data XML
	//////////////////////////////////////////////////////////////////////////
	string levelDataFile = path + "LevelData.xml";
	XmlString xmlData = root->getXML();
	CCryMemFile file;
	file.Write(xmlData.c_str(), xmlData.length());
	m_levelPak.m_pakFile.UpdateFile(levelDataFile, file);

	string levelDataActionFile = path + "LevelDataAction.xml";
	XmlString xmlDataAction = rootAction->getXML();
	CCryMemFile fileAction;
	fileAction.Write(xmlDataAction.c_str(), xmlDataAction.length());
	m_levelPak.m_pakFile.UpdateFile(levelDataActionFile, fileAction);

	if (bExportMission)
	{
		//////////////////////////////////////////////////////////////////////////
		// Export current mission file.
		//////////////////////////////////////////////////////////////////////////
		XmlNodeRef objectsNode = NULL;
		XmlNodeRef missionNode = rootAction->createNode("Mission");
		pCurrentMission->Export(missionNode, objectsNode);

		missionNode->setAttr("CGFCount", p3DEngine->GetLoadedObjectCount());

		//if (!CFileUtil::OverwriteFile( path+currentMissionFileName ))
		//			return;

		_smart_ptr<IXmlStringData> pXmlStrData = missionNode->getXMLData(5000000);

		CCryMemFile fileMission;
		fileMission.Write(pXmlStrData->GetString(), pXmlStrData->GetStringLength());
		m_levelPak.m_pakFile.UpdateFile(path + currentMissionFileName, fileMission);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportLevelInfo(const string& path)
{
	//////////////////////////////////////////////////////////////////////////
	// Export short level info xml.
	//////////////////////////////////////////////////////////////////////////
	XmlNodeRef root = XmlHelpers::CreateXmlNode("LevelInfo");
	root->setAttr("SandboxVersion", (const char*)GetIEditorImpl()->GetFileVersion().ToFullString());

	string levelName = GetIEditorImpl()->GetGameEngine()->GetLevelPath();
	root->setAttr("Name", levelName);
	root->setAttr("HeightmapSize", GetIEditorImpl()->GetHeightmap()->GetWidth());

	if (gEnv->p3DEngine->GetITerrain())
	{
		int compiledDataSize = gEnv->p3DEngine->GetITerrain()->GetCompiledDataSize();
		byte* pInfo = new byte[compiledDataSize];
		gEnv->p3DEngine->GetITerrain()->GetCompiledData(pInfo, compiledDataSize, 0, 0, 0, false);
		STerrainChunkHeader* pHeader = (STerrainChunkHeader*)pInfo;
		XmlNodeRef terrainInfo = root->newChild("TerrainInfo");
		int heightmapSize = GetIEditorImpl()->GetHeightmap()->GetWidth();
		terrainInfo->setAttr("HeightmapSize", heightmapSize);
		terrainInfo->setAttr("UnitSize", pHeader->TerrainInfo.unitSize_InMeters);
		terrainInfo->setAttr("SectorSize", pHeader->TerrainInfo.sectorSize_InMeters);
		terrainInfo->setAttr("SectorsTableSize", pHeader->TerrainInfo.sectorsTableSize_InSectors);
		terrainInfo->setAttr("HeightmapZRatio", pHeader->TerrainInfo.heightmapZRatio);
		terrainInfo->setAttr("OceanWaterLevel", pHeader->TerrainInfo.oceanWaterLevel);

		delete[] pInfo;
	}

	// Save all missions in this level.
	XmlNodeRef missionsNode = root->newChild("Missions");
	int numMissions = GetIEditorImpl()->GetDocument()->GetMissionCount();
	for (int i = 0; i < numMissions; i++)
	{
		CMission* pMission = GetIEditorImpl()->GetDocument()->GetMission(i);
		XmlNodeRef missionNode = missionsNode->newChild("Mission");
		missionNode->setAttr("Name", pMission->GetName());
		missionNode->setAttr("Description", pMission->GetDescription());
	}

	//////////////////////////////////////////////////////////////////////////
	// Save LevelInfo file.
	//////////////////////////////////////////////////////////////////////////
	string filename = path + "LevelInfo.xml";
	XmlString xmlData = root->getXML();

	CCryMemFile file;
	file.Write(xmlData.c_str(), xmlData.length());
	m_levelPak.m_pakFile.UpdateFile(filename, file);
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportMapInfo(XmlNodeRef& node)
{
	XmlNodeRef info = node->newChild("LevelInfo");

	// Write the creation time of the file
	char szBuffer[1024];
	_strdate(szBuffer);
	info->setAttr("CreationDate", szBuffer);

	cry_strcpy(szBuffer, LPCTSTR(GetIEditorImpl()->GetDocument()->GetTitle()));
	PathRemoveExtension(szBuffer);
	info->setAttr("Name", szBuffer);

	CHeightmap* heightmap = GetIEditorImpl()->GetHeightmap();
	if (heightmap)
	{
		info->setAttr("HeightmapSize", heightmap->GetWidth());
		info->setAttr("HeightmapUnitSize", heightmap->GetUnitSize());
		info->setAttr("HeightmapMaxHeight", heightmap->GetMaxHeight());
		info->setAttr("WaterLevel", heightmap->GetWaterLevel());

		SSectorInfo sectorInfo;
		heightmap->GetSectorsInfo(sectorInfo);
		int nTerrainSectorSizeInMeters = sectorInfo.sectorSize;
		info->setAttr("TerrainSectorSizeInMeters", nTerrainSectorSizeInMeters);
	}

	// Serialize surface types.
	CXmlArchive xmlAr;
	xmlAr.bLoading = false;
	xmlAr.root = node;
	GetIEditorImpl()->GetTerrainManager()->SerializeSurfaceTypes(xmlAr);

	GetIEditorImpl()->GetObjectManager()->GetPhysicsManager()->SerializeCollisionClasses(xmlAr);

	GetIEditorImpl()->GetObjectManager()->GetLayersManager()->ExportLayerSwitches(node);
}

// "locally higher texture resolution"

//////////////////////////////////////////////////////////////////////////
bool CGameExporter::ExportSurfaceTexture(CPakFile& levelPakFile, const char* szFilePathName, float fLeft, float fTop, float fWidth, float fHeight)
{
	CHeightmap* pHeightmap = GetIEditorImpl()->GetHeightmap();

	int nTerrainNodeSizeMeters(gEnv->p3DEngine->GetTerrainTextureNodeSizeMeters());
	int nMaxTilesNum(32);

	// Make sure 3D engine closes texture handle.
	GetIEditorImpl()->Get3DEngine()->CloseTerrainTextureFile();

	if (nTerrainNodeSizeMeters)
	{
		nMaxTilesNum = gEnv->p3DEngine->GetTerrainSize() / nTerrainNodeSizeMeters;
	}

	pHeightmap->GetTerrainGrid()->InitSectorGrid(nMaxTilesNum);   // release the editor textures on the terrain

	SSectorInfo sectorInfo;
	pHeightmap->GetSectorsInfo(sectorInfo);
	int lSectorDimensions[2]; // [0]=layer 0, [1]=layer 1

	lSectorDimensions[0] = 256;                                         // good texture size for streaming
	lSectorDimensions[1] = lSectorDimensions[0] / OCCMAP_DOWNSCALE_FACTOR;

	CRGBLayer* pRGBLayer = GetIEditorImpl()->GetTerrainManager()->GetRGBLayer();
	uint32 dwMinRequiredTextureExtend = pRGBLayer->CalcMinRequiredTextureExtend();

	// if the requested texture resolution is higher then the painted one, stick to this resolution
	//	if(lSectorDimensions[0]*numSectors>dwMinRequiredTextureExtend)
	//		lSectorDimensions[0] = dwMinRequiredTextureExtend/numSectors;

	CLogFile::WriteLine("Generating terrain texture...");

	CCryMemFile ctcFile;

	// write header
	{
		SCommonFileHeader header;
		ZeroStruct(header);

		cry_strcpy(header.signature, "CRY");
		header.file_type = eTerrainTextureFile;
		header.flags = 0;
		header.version = FILEVERSION_TERRAIN_TEXTURE_FILE;

		ctcFile.Write(&header, sizeof(header));
	}

	// write sub header
	{
		STerrainTextureFileHeader subHeader;
		subHeader.nLayerCount = 2; // RGB and Occlusion texture
		subHeader.dwFlags = 0;
		if (m_settings.bUpdateIndirectLighting)
			subHeader.dwFlags |= TTFHF_AO_DATA_IS_VALID;
		subHeader.fBrMultiplier = 1.0f / m_settings.fBrMultiplier;
		ctcFile.Write(&subHeader, sizeof(subHeader));
	}

	STerrainTextureLayerFileHeader layerHeader0, layerHeader1;
	ISystem* pSystem = GetIEditorImpl()->GetSystem();

	// layer 0
	layerHeader0.eTexFormat = eTerrainPrimaryTextureFormat[m_settings.bUpdateIndirectLighting][m_settings.bHiQualityCompression];
	layerHeader0.nSectorSizePixels = lSectorDimensions[0];
	layerHeader0.nSectorSizeBytes = pSystem->GetIRenderer()->GetTextureFormatDataSize(lSectorDimensions[0], lSectorDimensions[0], 1, 1, layerHeader0.eTexFormat);

	// layer 1
	layerHeader1.eTexFormat = eTerrainSecondaryTextureFormat[m_settings.bUpdateIndirectLighting][m_settings.bHiQualityCompression];
	layerHeader1.nSectorSizePixels = lSectorDimensions[1];
	layerHeader1.nSectorSizeBytes = pSystem->GetIRenderer()->GetTextureFormatDataSize(lSectorDimensions[1], lSectorDimensions[1], 1, 1, layerHeader1.eTexFormat);

	ctcFile.Write(&layerHeader0, sizeof(STerrainTextureLayerFileHeader));
	ctcFile.Write(&layerHeader1, sizeof(STerrainTextureLayerFileHeader));

	// build index block - needed for locally higher/lower resolution
	uint32 dwUsedTextureIds = 0;
	std::vector<int16> IndexBlock;    // >=0 means x is texture index, -1 is used as terminator
	{
		uint32 dwMaxTextureRes = pRGBLayer->CalcMinRequiredTextureExtend();
		_BuildIndexBlockRecursive(lSectorDimensions[0], IndexBlock, dwUsedTextureIds, fLeft, fTop, fWidth, fHeight);

		uint16 dwSize = (uint16)IndexBlock.size();
		ctcFile.Write(&dwSize, sizeof(uint16));
		ctcFile.Write(&IndexBlock[0], sizeof(uint16) * dwSize);
	}

	ULONGLONG DataSeekPos = ctcFile.GetPosition();    // pos in file after header
	ULONGLONG ElementFileSize = layerHeader0.nSectorSizeBytes + layerHeader1.nSectorSizeBytes;

	CryLog("Generation stats: %d tiles(base: %dx%d) = %.2f MB", dwUsedTextureIds, lSectorDimensions[0], lSectorDimensions[0], (double)(ElementFileSize * dwUsedTextureIds) / (1024.0 * 1024.0));

	// reserve file size -
	levelPakFile.GetArchive()->StartContinuousFileUpdate(szFilePathName, DataSeekPos + ElementFileSize * dwUsedTextureIds);

	// write header
	levelPakFile.GetArchive()->UpdateFileContinuousSegment(szFilePathName, DataSeekPos + ElementFileSize * dwUsedTextureIds, ctcFile.GetMemPtr(), ctcFile.GetLength());

	// release header memory
	ctcFile.Close();

	{
		CTerrainLayerTexGen LayerTexGen;        // to generate the terrain texture
		CTerrainLightGen LightGen(m_settings.nApplySS, &levelPakFile, m_settings.bUpdateIndirectLighting);// to generate the surface occlusion/lighting data

		LightGen.Init(pHeightmap->GetWidth(), false);

		CWaitProgress progress("Generating Terrain Texture");

		int nSectorId = 0;

		SProgressHelper phelper(progress, dwUsedTextureIds, LightGen, ctcFile, IndexBlock, DataSeekPos, ElementFileSize, szFilePathName);

		phelper.m_dwLayerExtends[0] = lSectorDimensions[0];
		phelper.m_dwLayerExtends[1] = lSectorDimensions[1];

		int iIndexBlockValue = phelper.m_rIndexBlock[phelper.m_IndexBlockPos++];

		gBuildSectorAODataTime = 0;

		CCryMemFile fileTemp;

		if (!_ExportSurfaceTextureRecursive(levelPakFile, fileTemp, phelper, iIndexBlockValue, phelper.m_TempMem[0].m_Quarter[0], fLeft, fTop, fWidth, fHeight))
		{
			Error("Error exporting terrain texture recursive");
			return false;
		}

		pSystem->GetILog()->Log("* Building AO data took %d seconds *", int(gBuildSectorAODataTime));
	}

	return true;
}

bool CGameExporter::_BuildIndexBlockRecursive(const uint32 dwTileSize, std::vector<int16>& rIndexBlock,
                                              uint32& dwUsedTextureIds, const float fMinX, const float fMinY, const float fWidth, const float fHeight,
                                              const uint32 dwRecursionDepth)
{
	CRGBLayer* pRGBLayer = GetIEditorImpl()->GetTerrainManager()->GetRGBLayer();

	uint32 dwRes = pRGBLayer->CalcMaxLocalResolution(fMinX, fMinY, fMinX + fWidth, fMinY + fHeight);

	bool bDescent = true;

	if (dwRecursionDepth)
	{
		dwRes = (uint32) (dwRes * max(fWidth, fHeight));

		if (dwRes < dwTileSize)
			bDescent = false;                                 // data in input does not have more resolution

		if ((dwTileSize << dwRecursionDepth) > m_settings.iExportTexWidth)
			bDescent = false;                                 // user defined
	}

	//	char str[256];

	//	cry_sprintf(str,"_BuildIndexBlockRecursive %d. bDescent=%c %d>%d, dwRes=%d dwRecursionDepth=%d (%.2f %.2f)-(%.2f,%.2f)\n",dwUsedTextureIds,bDescent?'1':'0',((dwRes*2)>>dwRecursionDepth),dwTileSize,dwRes,dwRecursionDepth,fMinX,fMinY,fMinX+fInvSectorCnt,fMinY+fInvSectorCnt);
	//	OutputDebugString(str);

	if (bDescent)
	{
		rIndexBlock.push_back(dwUsedTextureIds++);

		// split in 4 parts, recursively proceed
		float fHalfWidth = fWidth * 0.5f;
		float fHalfHeight = fHeight * 0.5f;
		for (uint32 dwX = 0; dwX < 2; ++dwX)
			for (uint32 dwY = 0; dwY < 2; ++dwY)
			{
				// recursion
				if (!_BuildIndexBlockRecursive(dwTileSize, rIndexBlock, dwUsedTextureIds, fMinX + dwX * fHalfWidth, fMinY + dwY * fHalfHeight, fHalfWidth, fHalfHeight, dwRecursionDepth + 1))
					return false;
			}
	}
	else rIndexBlock.push_back(-1);

	return true;
}

float GetFilteredNoiseVal(int X, int Y)
{
	static float arrNoise2D[256][256];

	static int nBuildNoiseMap = true;
	if (nBuildNoiseMap)
	{
		srand(0);

		// generate
		for (int x = 0; x < 256; x++)
		{
			for (int y = 0; y < 256; y++)
			{
				arrNoise2D[x][y] = cry_random(-0.5f, 0.5f);
			}
		}

		// blur
		static float arrNoise2DTmp[256][256];
		for (int pass = 0; pass < 8; pass++)
		{
			memcpy(arrNoise2DTmp, arrNoise2D, sizeof(arrNoise2DTmp));

			for (int x = 0; x < 256; x++)
			{
				for (int y = 0; y < 256; y++)
				{
					float fVal = 0;

					fVal += arrNoise2DTmp[(x) & 255][(y + 1) & 255] * 0.6f;
					fVal += arrNoise2DTmp[(x) & 255][(y - 1) & 255] * 0.6f;
					fVal += arrNoise2DTmp[(x + 1) & 255][(y) & 255] * 0.6f;
					fVal += arrNoise2DTmp[(x - 1) & 255][(y) & 255] * 0.6f;

					fVal += arrNoise2DTmp[(x) & 255][(y) & 255];
					arrNoise2D[x][y] = fVal / (0.6f * 4.f + 1.f);
				}
			}
		}

		nBuildNoiseMap = false;
	}

	return arrNoise2D[X & 255][Y & 255];
}

bool CGameExporter::_ExportSurfaceTextureRecursive(CPakFile& levelPakFile, CCryMemFile& fileTemp, SProgressHelper& phelper, const int iParentIndex, SRecursionHelperQuarter& rDestPiece,
                                                   const float fMinX, const float fMinY, const float fWidth, const float fHeight, const uint32 dwRecursionDepth)
{
	// to jump over one mip level
	uint32 dwMipSectorCount = 1 << (dwRecursionDepth + 1);

	SSectorInfo sectorInfo;
	CHeightmap* pHeightmap = GetIEditorImpl()->GetHeightmap();
	pHeightmap->GetSectorsInfo(sectorInfo);

	//	char str[256];

	//	cry_sprintf(str,"_ExportSurfaceTextureRecursive iParentIndex=%d dwRecursionDepth=%d\n",iParentIndex,dwRecursionDepth);
	//	OutputDebugString(str);

	//SRecursionHelper &rHelper = phelper.m_TempMem[dwRecursionDepth+1];

	bool bLeafNode = true;

	// split in 4 parts, recursively proceed
	if (iParentIndex != -1)
	{
		for (uint32 dwI = 0; dwI < 4; ++dwI)
		{
			int iIndexBlockValue = phelper.m_rIndexBlock[phelper.m_IndexBlockPos + dwI];

			if (iIndexBlockValue >= 0)
			{
				bLeafNode = false;
				break;
			}
		}
	}

	if (bLeafNode)
	{
		//		OutputDebugString("Leaf\n");

		// layer 1 -----------------
		{
			CImageEx& ref = rDestPiece.m_Layer[1];

			if (!ref.IsValid())
				ref.Allocate(phelper.m_dwLayerExtends[1], phelper.m_dwLayerExtends[1]);

			phelper.m_rLightGen.GetSubImageStretched(fMinX, fMinY, fMinX + fWidth, fMinY + fHeight, ref,
			                                         ETTG_QUIET | ETTG_LIGHTING | ETTG_STATOBJ_SHADOWS | ETTG_STATOBJ_PAINTBRIGHTNESS | ETTG_ABGR);
		}

		// layer 0 -----------------
		{
			CImageEx& ref = rDestPiece.m_Layer[0];

			if (!ref.IsValid())
				ref.Allocate(phelper.m_dwLayerExtends[0], phelper.m_dwLayerExtends[0]);

			CRGBLayer* pRGBLayer = GetIEditorImpl()->GetTerrainManager()->GetRGBLayer();

			pRGBLayer->GetSubImageStretched(fMinX, fMinY, fMinX + fWidth, fMinY + fHeight, ref, true);
		}

		// convert RGB colour into format that has less compression artefacts for brightness variations
		{
			uint32 dwWidth = rDestPiece.m_Layer[0].GetWidth();
			uint32 dwHeight = rDestPiece.m_Layer[0].GetHeight();

			uint32* pMem = &rDestPiece.m_Layer[0].ValueAt(0, 0);
			uint32* pMemAO = &rDestPiece.m_Layer[1].ValueAt(0, 0);

			I3DEngine* p3DEngine = GetIEditorImpl()->Get3DEngine();
			int nTerrainSize = p3DEngine->GetTerrainSize();
			float fWSMinX = fMinX * nTerrainSize;
			float fWSMinY = fMinY * nTerrainSize;
			float fWSSectorSizeMeters = nTerrainSize * fWidth;

			// find terrain min/max
			float fTerrainMinZ = 1000000;
			float fTerrainMaxZ = 0;
			for (uint32 dwY = 0; dwY < dwHeight; ++dwY)
			{
				for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
				{
					float fWSDX = ((((float)dwX)) * (1.f + 1.f / 256.f) - 0.5f) / (float)dwWidth * (float)fWSSectorSizeMeters;
					float fWSDY = ((((float)dwY)) * (1.f + 1.f / 256.f) - 0.5f) / (float)dwHeight * (float)fWSSectorSizeMeters;

					Vec3 vWSPos; // pos on terrain
					vWSPos.x = (fWSMinY + fWSDY);
					vWSPos.y = (fWSMinX + fWSDX);
					vWSPos.z = p3DEngine->GetTerrainElevation(int(vWSPos.x), int(vWSPos.y));

					fTerrainMaxZ = max(fTerrainMaxZ, vWSPos.z);
					fTerrainMinZ = min(fTerrainMinZ, vWSPos.z);
				}
			}

			fTerrainMinZ = max(0.f, fTerrainMinZ - TERRAIN_DEFORMATION_MAX_DEPTH);

			// build sector AO data
			int nAODataSize = 256;
			SAOInfo* pAOData = NULL;
			if (m_settings.bUpdateIndirectLighting)
			{
				pAOData = new SAOInfo[nAODataSize * nAODataSize];
				BuildSectorAOData(fWSMinX, fWSMinY, fWSSectorSizeMeters, pAOData, nAODataSize, fTerrainMinZ, fTerrainMaxZ);
			}

			for (uint32 dwY = 0; dwY < dwHeight; ++dwY)
			{
				for (uint32 dwX = 0; dwX < dwWidth; ++dwX)
				{
					// tex0
					uint32 dwR, dwG, dwB;

					{
						float fR = ((*pMem >> 16) & 0xff) * (1.0f / 255.0f);
						float fG = ((*pMem >> 8) & 0xff) * (1.0f / 255.0f);
						float fB = ((*pMem >> 0) & 0xff) * (1.0f / 255.0f);

						ColorF cCol = ColorF(fR, fG, fB);

						// Convert to linear space
						cCol.srgb2rgb();

						// increase color range in order to minimize DXT compression artifacts
						cCol *= m_settings.fBrMultiplier;
						cCol.Clamp();
#if TERRAIN_USE_CIE_COLORSPACE
						cCol = cCol.RGB2mCIE();
#endif

						// Convert to gamma 2.2 space
						cCol.rgb2srgb();

						dwR = (uint32)(cCol.r * 255.0f + 0.5f);
						dwG = (uint32)(cCol.g * 255.0f + 0.5f);
						dwB = (uint32)(cCol.b * 255.0f + 0.5f);
					}

					// get world pos
					float fWSDX = ((((float)dwX)) * (1.f + 1.f / 256.f) - 0.5f) / (float)dwWidth * (float)fWSSectorSizeMeters;
					float fWSDY = ((((float)dwY)) * (1.f + 1.f / 256.f) - 0.5f) / (float)dwHeight * (float)fWSSectorSizeMeters;
					Vec3 vWSPos; // pos on terrain
					vWSPos.x = (fWSMinY + fWSDY);
					vWSPos.y = (fWSMinX + fWSDX);
					vWSPos.z = p3DEngine->GetTerrainElevation(int(vWSPos.x), int(vWSPos.y));

					// store ground level sky access
					float fSkyAmount = (((*pMemAO) >> 8) & 0xff) / 255.f;
					// add some filtered noise
					fSkyAmount += GetFilteredNoiseVal(vWSPos.x * 4.f, vWSPos.y * 4.f) * SATURATE(1.f - fSkyAmount) * 0.5f;
					uint32 dwA = SATURATEB(fSkyAmount * 255.5f);

					*pMem = (dwA << 24) | (dwB << 16) | (dwG << 8) | (dwR);

					dwR = dwG = dwB = dwA = 255;

					if (m_settings.bUpdateIndirectLighting)
					{
						// tex1
						// store objects elevation z
						if (dwY >= 0 && dwY < nAODataSize && dwX >= 0 && dwX < nAODataSize)
						{
							float fObjZ = pAOData[dwY + dwX * nAODataSize].GetMax();
							dwG = SATURATEB((fObjZ - vWSPos.z) / gAOObjectsZRange * 255.f);
						}

						// store terrain z
						int nH(0); // The default values could be 0 or 1.
						           // The limit to left would lead us into 1.
						           // But still, if fTerrainMaxZ==fTerrainMinZ,
						           // must vWSPos.z==fTerrainMinZ, also be true.
						           // So, the numerator would be 0...which leads
						           // us into a full indetermination, as fTerrainMinZ
						           // and fTerrainMinZ can be considered constant
						           // in our expression.
						if (fTerrainMaxZ > fTerrainMinZ)
						{
							nH = SATURATE((vWSPos.z - fTerrainMinZ) / (fTerrainMaxZ - fTerrainMinZ)) * 256.f * 256.f; // from 0 to 255
						}
						dwA = SATURATEB(nH / 256);
					}

					// store normal in R and B
					Vec3 vNormal = p3DEngine->GetTerrainSurfaceNormal(vWSPos);
					dwR = SATURATEB(vNormal.x * 127.5f + 127.5f);
					dwB = SATURATEB(vNormal.y * 127.5f + 127.5f);

					*pMemAO = (dwA << 24) | (dwB << 16) | (dwG << 8) | (dwR);

					pMemAO++;
					pMem++;
				}
			}

			delete[] pAOData;
			pAOData = NULL;

			rDestPiece.fTerrainMinZ = fTerrainMinZ;
			rDestPiece.fTerrainMaxZ = fTerrainMaxZ;
		}

		if (iParentIndex != -1)
			phelper.m_IndexBlockPos += 4;
	}
	else
	{
		// OutputDebugString("Nodes:\n");

		// split in 4 parts, recursively proceed
		float fHalfWidth = fWidth / 2;
		float fHalfHeight = fHeight / 2;
		for (uint32 dwX = 0; dwX < 2; ++dwX)
		{
			for (uint32 dwY = 0; dwY < 2; ++dwY)
			{
				uint32 dwPart = dwX + dwY * 2; // 0=lefttop,1=righttop,2=leftbottom,3=rightbottom

				int iIndexBlockValue = phelper.m_rIndexBlock[phelper.m_IndexBlockPos++];

				// char str[256];
				// cry_sprintf(str,"   Node iIndexBlockValue = m_rIndexBlock[%d/%d] = %d\n",phelper.m_IndexBlockPos-1,(uint32)phelper.m_rIndexBlock.size(),iIndexBlockValue);
				// OutputDebugString(str);

				// recursion
				if (!_ExportSurfaceTextureRecursive(levelPakFile, fileTemp, phelper, iIndexBlockValue, phelper.m_TempMem[dwRecursionDepth + 1].m_Quarter[dwPart],
				                                    fMinX + dwX * fHalfWidth, fMinY + dwY * fHalfHeight, fHalfWidth, fHalfHeight, dwRecursionDepth + 1))
					return false;

				assert(phelper.m_TempMem[dwRecursionDepth + 1].m_Quarter[dwPart].m_Layer[0].IsValid());
				assert(phelper.m_TempMem[dwRecursionDepth + 1].m_Quarter[dwPart].m_Layer[1].IsValid());
			}
		}

		//SRecursionHelper &rHelper = phelper.m_TempMem[dwRecursionDepth];

		for (uint32 dwLayer = 0; dwLayer < 2; ++dwLayer)
		{
			CImageEx InputQuarters[4];
			float arrTerrainMinZ[4];
			float arrTerrainMaxZ[4];

			// Attach only copies pointers, not the data itself
			for (uint32 dwPart = 0; dwPart < 4; ++dwPart)
			{
				InputQuarters[dwPart].Attach(phelper.m_TempMem[dwRecursionDepth + 1].m_Quarter[dwPart].m_Layer[dwLayer]);

				arrTerrainMinZ[dwPart] = phelper.m_TempMem[dwRecursionDepth + 1].m_Quarter[dwPart].fTerrainMinZ;
				arrTerrainMaxZ[dwPart] = phelper.m_TempMem[dwRecursionDepth + 1].m_Quarter[dwPart].fTerrainMaxZ;
				/*
				        if(m_bDebugTexture)
				        if(dwLayer==0)
				        {
				          char str[80];
				          cry_sprintf(str,"C:\\temp\\input%d.bmp",dwPart);
				          CImageUtil::SaveBitmap(str,InputQuarters[dwPart]);
				        }
				 */
			}

			if (!rDestPiece.m_Layer[dwLayer].IsValid())
				rDestPiece.m_Layer[dwLayer].Allocate(phelper.m_dwLayerExtends[dwLayer], phelper.m_dwLayerExtends[dwLayer]);

			DownSampleWithBordersPreserved(InputQuarters, rDestPiece.m_Layer[dwLayer]);

			rDestPiece.fTerrainMaxZ = max(max(arrTerrainMaxZ[0], arrTerrainMaxZ[1]), max(arrTerrainMaxZ[2], arrTerrainMaxZ[3]));
			rDestPiece.fTerrainMinZ = min(min(arrTerrainMinZ[0], arrTerrainMinZ[1]), min(arrTerrainMinZ[2], arrTerrainMinZ[3]));
		}
	}

	if (iParentIndex != -1)
	{
		assert(iParentIndex >= 0);

		// char str[256];
		// cry_sprintf(str,"Store %d\n",iParentIndex);
		// OutputDebugString(str);

		// if(iParentIndex==0)
		// {
		//	OutputDebugString("Root\n");
		// }

		if (!SeekToSectorAndStore(levelPakFile, fileTemp, phelper, iParentIndex, dwRecursionDepth, rDestPiece))
			return false;
	}
	fileTemp.SetLength(0);

	return true;
}

bool CGameExporter::SeekToSectorAndStore(CPakFile& levelPakFile, CCryMemFile& fileTemp, SProgressHelper& phelper, const int iParentIndex,
                                         const uint32 dwLevel, SRecursionHelperQuarter& rQuarter)
{
	assert(iParentIndex >= 0);

	// debug border diffuse (black edges)
	/*{
	   for(uint32 dwY=0;dwY<rQuarter.m_Layer[0].GetHeight();++dwY)
	   for(uint32 dwX=0;dwX<rQuarter.m_Layer[0].GetWidth();++dwX)
	   {
	    bool bBorder = dwX==0 || dwX==rQuarter.m_Layer[0].GetWidth()-1 || dwY==0 || dwY==rQuarter.m_Layer[0].GetHeight()-1;

	    if(bBorder)
	      rQuarter.m_Layer[0].ValueAt(dwX,dwY) = 0xffffffff;
	   }
	   }

	   // debug border occ (black edges)
	   {
	   for(uint32 dwY=0;dwY<rQuarter.m_Layer[1].GetHeight();++dwY)
	    for(uint32 dwX=0;dwX<rQuarter.m_Layer[1].GetWidth();++dwX)
	    {
	      bool bBorder = dwX==0 || dwX==rQuarter.m_Layer[1].GetWidth()-1 || dwY==0 || dwY==rQuarter.m_Layer[1].GetHeight()-1;

	      if(bBorder)
	        rQuarter.m_Layer[1].ValueAt(dwX,dwY) = 0xffffffff;
	    }
	   }*/

	//	phelper.m_toFile.Seek(phelper.m_FileTileOffset + phelper.m_ElementFileSize*iParentIndex,CFile::begin);

	CTextureCompression texcomp(m_settings.bHiQualityCompression);

	//	char str[256];
	//	cry_sprintf(str,"CGameExporter::SeekToSectorAndStore (%dx%d) parent:%d level:%d [%d+%d*%d]\n",rQuarter.m_Layer[0].GetWidth(),rQuarter.m_Layer[0].GetHeight(),iParentIndex,dwLevel,(uint32)phelper.m_FileTileOffset,(uint32)phelper.m_ElementFileSize,iParentIndex);
	//	OutputDebugString(str);

	// layer 0 -----------------
	texcomp.WriteDDSToFile(fileTemp, (uint8*)rQuarter.m_Layer[0].GetData(), rQuarter.m_Layer[0].GetWidth(), rQuarter.m_Layer[0].GetHeight(), rQuarter.m_Layer[0].GetSize(), eTF_R8G8B8A8, eTerrainPrimaryTextureFormat[m_settings.bUpdateIndirectLighting][m_settings.bHiQualityCompression], 0, false, false);

	// layer 1 -----------------
	// no dithering, convert from green channel 32 bit to 1 channel
	texcomp.WriteDDSToFile(fileTemp, (uint8*)rQuarter.m_Layer[1].GetData(), rQuarter.m_Layer[1].GetWidth(), rQuarter.m_Layer[1].GetHeight(), rQuarter.m_Layer[1].GetSize(), eTF_R8G8B8A8, eTerrainSecondaryTextureFormat[m_settings.bUpdateIndirectLighting][m_settings.bHiQualityCompression], 0, false, false);

	// seek to destination and update file there
	if (0 != levelPakFile.GetArchive()->UpdateFileContinuousSegment(phelper.m_szFilename, phelper.m_FileTileOffset + phelper.m_ElementFileSize * phelper.m_Max,
	                                                                fileTemp.GetMemPtr(), fileTemp.GetLength(), phelper.m_FileTileOffset + phelper.m_ElementFileSize * iParentIndex))
	{
		// error
		assert(0);
	}

	return phelper.Increase();
}

void CGameExporter::DownSampleWithBordersPreserved(const CImageEx rIn[4], CImageEx& rOut)
{
	uint32 dwOutputWidth = rOut.GetWidth();
	uint32 dwOutputHeight = rOut.GetHeight();

	assert(rIn[0].GetData() != rOut.GetData());
	assert(rIn[1].GetData() != rOut.GetData());
	assert(rIn[2].GetData() != rOut.GetData());
	assert(rIn[3].GetData() != rOut.GetData());

	// if needed this can be optimized a lot

	// lefttop
	{
		for (uint32 dwY = 0; dwY < dwOutputHeight; ++dwY)
			for (uint32 dwX = 0; dwX < dwOutputWidth; ++dwX)
			{
				uint32 dwPart = 0;
				uint32 dwLocalInX = dwX * 2, dwLocalInY = dwY * 2;

				if (dwLocalInX >= dwOutputWidth)
				{
					dwPart = 1;
					dwLocalInX -= dwOutputWidth;
				}

				if (dwLocalInY >= dwOutputHeight)
				{
					dwPart += 2;
					dwLocalInY -= dwOutputHeight;
				}

				if (dwX == 0)
				{
					if (dwY == 0)
						rOut.ValueAt(dwX, dwY) = rIn[dwPart].ValueAt(dwLocalInX, dwLocalInY);     // left top corner
					else if (dwY == dwOutputHeight - 1)
						rOut.ValueAt(dwX, dwY) = rIn[dwPart].ValueAt(dwLocalInX, dwLocalInY + 1); // left bottom corner
					else
					{
						// left side
						rOut.ValueAt(dwX, dwY) = ColorB::ComputeAvgCol_Fast(rIn[dwPart].ValueAt(dwLocalInX, dwLocalInY), rIn[dwPart].ValueAt(dwLocalInX, dwLocalInY + 1));
					}
				}
				else if (dwX == dwOutputWidth - 1)
				{
					if (dwY == 0)
						rOut.ValueAt(dwX, dwY) = rIn[dwPart].ValueAt(dwLocalInX + 1, dwLocalInY);   // right top corner
					else if (dwY == dwOutputHeight - 1)
						rOut.ValueAt(dwX, dwY) = rIn[dwPart].ValueAt(dwLocalInX + 1, dwLocalInY + 1); // right bottom corner
					else
					{
						// right side
						rOut.ValueAt(dwX, dwY) = ColorB::ComputeAvgCol_Fast(rIn[dwPart].ValueAt(dwLocalInX + 1, dwLocalInY), rIn[dwPart].ValueAt(dwLocalInX + 1, dwLocalInY + 1));
					}
				}
				else
				{
					if (dwY == 0)
					{
						rOut.ValueAt(dwX, dwY) = ColorB::ComputeAvgCol_Fast(rIn[dwPart].ValueAt(dwLocalInX, dwLocalInY), rIn[dwPart].ValueAt(dwLocalInX + 1, dwLocalInY));
					}
					else if (dwY == dwOutputHeight - 1)
					{
						rOut.ValueAt(dwX, dwY) = ColorB::ComputeAvgCol_Fast(rIn[dwPart].ValueAt(dwLocalInX, dwLocalInY + 1), rIn[dwPart].ValueAt(dwLocalInX + 1, dwLocalInY + 1));
					}
					else
					{
						// inner
						uint32 dwC[2];

						dwC[0] = ColorB::ComputeAvgCol_Fast(rIn[dwPart].ValueAt(dwLocalInX, dwLocalInY), rIn[dwPart].ValueAt(dwLocalInX + 1, dwLocalInY));
						dwC[1] = ColorB::ComputeAvgCol_Fast(rIn[dwPart].ValueAt(dwLocalInX, dwLocalInY + 1), rIn[dwPart].ValueAt(dwLocalInX + 1, dwLocalInY + 1));

						rOut.ValueAt(dwX, dwY) = ColorB::ComputeAvgCol_Fast(dwC[0], dwC[1]);
					}
				}
			}
	}
}

static uint32 ComputeAvgCol_AO(const uint32 dwCol0, const uint32 dwCol1,
                               float fTMZ_InMin, float fTMZ_InMax,
                               float fTMZ_OutMin, float fTMZ_OutMax)
{
	uint32 arrZOut[4];

	for (int i = 0; i < 4; i++)
	{
		if (i == 3) // terrain elevation needs to be range converted
		{
			float fZIn0 = ((dwCol0 >> (8 * i)) & 0xff) / 255.f * (fTMZ_InMax - fTMZ_InMin) + fTMZ_InMin;
			float fZIn1 = ((dwCol1 >> (8 * i)) & 0xff) / 255.f * (fTMZ_InMax - fTMZ_InMin) + fTMZ_InMin;
			float fZOut = (fZIn0 + fZIn1) * 0.5f;
			arrZOut[i] = SATURATEB(div_min(fZOut - fTMZ_OutMin, fTMZ_OutMax - fTMZ_OutMin, 1.f) * 255.f + 0.5f);
		}
		else
		{
			float fZIn0 = ((dwCol0 >> (8 * i)) & 0xff);
			float fZIn1 = ((dwCol1 >> (8 * i)) & 0xff);
			float fZOut = (fZIn0 + fZIn1) * 0.5f;
			arrZOut[i] = SATURATEB(fZOut);
		}
	}

	uint32 dwRes = (arrZOut[3] << 24) | (arrZOut[2] << 16) | (arrZOut[1] << 8) | (arrZOut[0]);

	return dwRes;
}

void CGameExporter::DownSampleWithBordersPreservedAO(const CImageEx rIn[4],
                                                     const float rInTerrainMinZ[4], const float rInTerrainMaxZ[4], CImageEx& rOut)
{
	uint32 dwOutputWidth = rOut.GetWidth();
	uint32 dwOutputHeight = rOut.GetHeight();

	assert(rIn[0].GetData() != rOut.GetData());
	assert(rIn[1].GetData() != rOut.GetData());
	assert(rIn[2].GetData() != rOut.GetData());
	assert(rIn[3].GetData() != rOut.GetData());

	// if needed this can be optimized a lot

	float fNewZMin = rInTerrainMinZ[0];
	float fNewZMax = rInTerrainMaxZ[0];

	for (int i = 0; i < 4; i++)
	{
		fNewZMin = min(fNewZMin, rInTerrainMinZ[i]);
		fNewZMax = max(fNewZMax, rInTerrainMaxZ[i]);
	}

	for (uint32 dwY = 0; dwY < dwOutputHeight; ++dwY)
	{
		for (uint32 dwX = 0; dwX < dwOutputWidth; ++dwX)
		{
			uint32 dwPart = 0;
			uint32 dwLocalInX = dwX * 2, dwLocalInY = dwY * 2;

			if (dwLocalInX >= dwOutputWidth)
			{
				dwPart = 1;
				dwLocalInX -= dwOutputWidth;
			}

			if (dwLocalInY >= dwOutputHeight)
			{
				dwPart += 2;
				dwLocalInY -= dwOutputHeight;
			}

			uint32 dwC[2];
			dwC[0] = ComputeAvgCol_AO(
			  rIn[dwPart].ValueAt(dwLocalInX, dwLocalInY),
			  rIn[dwPart].ValueAt(dwLocalInX + 1, dwLocalInY),
			  rInTerrainMinZ[dwPart], rInTerrainMaxZ[dwPart], fNewZMin, fNewZMax);
			dwC[1] = ComputeAvgCol_AO(
			  rIn[dwPart].ValueAt(dwLocalInX, dwLocalInY + 1), rIn[dwPart].ValueAt(dwLocalInX + 1, dwLocalInY + 1),
			  rInTerrainMinZ[dwPart], rInTerrainMaxZ[dwPart], fNewZMin, fNewZMax);

			rOut.ValueAt(dwX, dwY) = ComputeAvgCol_AO(dwC[0], dwC[1], fNewZMin, fNewZMax, fNewZMin, fNewZMax);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportAINavigationData(const string& path)
{
	if (!GetIEditorImpl()->GetSystem()->GetAISystem())
		return;

	char szLevel[1024];
	char szMission[1024];
	char fileNameNavigation[1024];

	cry_strcpy(szLevel, path);
	cry_strcpy(szMission, GetIEditorImpl()->GetDocument()->GetCurrentMission()->GetName());

	PathRemoveBackslash(szLevel);

	cry_sprintf(fileNameNavigation, "%s\\mnmnav%s.bai", szLevel, szMission);

	CFile file;
	if (file.Open(fileNameNavigation, CFile::modeRead))
	{
		CMemoryBlock mem;
		mem.Allocate(file.GetLength());
		file.Read(mem.GetBuffer(), file.GetLength());
		file.Close();

		if (!m_levelPak.m_pakFile.UpdateFile(fileNameNavigation, mem))
			Warning("Failed to update pak file with %s", fileNameNavigation);
		else
			DeleteFile(fileNameNavigation);
	}
	CLogFile::WriteLine("Exporting Navigation data done.");
}

void CGameExporter::ExportGameData(const string& path)
{
	const IGame* pGame = gEnv->pGameFramework->GetIGame();
	if (pGame == nullptr)
		return;

	CWaitProgress wait("Saving game level data");

	const string& missionName = GetIEditorImpl()->GetDocument()->GetCurrentMission()->GetName().GetString();

	const IGame::ExportFilesInfo exportedFiles = pGame->ExportLevelData(m_levelPath, missionName);

	// Update pak files for every exported file (we expect the same order on the game side!)
	char fileName[512];
	for (uint32 fileIdx = 0; fileIdx < exportedFiles.GetFileCount(); ++fileIdx)
	{
		IGame::ExportFilesInfo::GetNameForFile(exportedFiles.GetBaseFileName(), fileIdx, fileName, sizeof(fileName));

		CFile file;
		if (file.Open(fileName, CFile::modeRead))
		{
			CMemoryBlock mem;
			mem.Allocate(file.GetLength());
			file.Read(mem.GetBuffer(), file.GetLength());
			file.Close();

			if (!m_levelPak.m_pakFile.UpdateFile(fileName, mem))
				Warning("Failed to update pak file with %s", fileName);
			else
				DeleteFile(fileName);
		}

	}
}

//////////////////////////////////////////////////////////////////////////
string CGameExporter::ExportAIAreas(const string& path)
{
	// (MATT) Only updates _some_ files in the AI but is only place that copies all bai files present into the PAK! {2008/08/12}

	IAISystem* pAISystem = GetISystem()->GetAISystem();
	if (!pAISystem)
		return "No AI System available";

	char szLevel[1024];
	char szMission[1024];
	char fileNameAreas[1024];
	cry_strcpy(szLevel, path);
	cry_strcpy(szMission, GetIEditorImpl()->GetDocument()->GetCurrentMission()->GetName());
	PathRemoveBackslash(szLevel);
	cry_sprintf(fileNameAreas, "%s\\areas%s.bai", szLevel, szMission);

	string result;
	CFile file;

	// Read these files back and put them to Pak file.
	GetIEditorImpl()->GetGameEngine()->ExportAiData(0, fileNameAreas, 0, 0, 0, 0);
	if (file.Open(fileNameAreas, CFile::modeRead))
	{
		CMemoryBlock mem;
		mem.Allocate(file.GetLength());
		file.Read(mem.GetBuffer(), file.GetLength());
		file.Close();
		if (false == m_levelPak.m_pakFile.UpdateFile(fileNameAreas, mem))
			result += string("Failed to update pak file with ") + fileNameAreas + "\n";
		else
			DeleteFile(fileNameAreas);
	}

	CLogFile::WriteLine("Exporting AI done.");
	return result;
}

string CGameExporter::ExportAICoverSurfaces(const string& path)
{
	if (!GetISystem()->GetAISystem())
		return "No AI System available";

	char szLevel[1024];
	char szMission[1024];
	char fileNameCoverSurfaces[1024];

	cry_strcpy(szLevel, path);
	cry_strcpy(szMission, GetIEditorImpl()->GetDocument()->GetCurrentMission()->GetName());

	PathRemoveBackslash(szLevel);

	cry_sprintf(fileNameCoverSurfaces, "%s\\cover%s.bai", szLevel, szMission);

	string result;

	CFile file;
	if (file.Open(fileNameCoverSurfaces, CFile::modeRead))
	{
		CMemoryBlock mem;
		mem.Allocate(file.GetLength());
		file.Read(mem.GetBuffer(), file.GetLength());
		file.Close();

		if (!m_levelPak.m_pakFile.UpdateFile(fileNameCoverSurfaces, mem))
			result = string("Failed to update pak file with ") + fileNameCoverSurfaces + "\n";
		else
			DeleteFile(fileNameCoverSurfaces);
	}

	CLogFile::WriteLine("Exporting AI Cover Surfaces done.");
	return result;
}

//////////////////////////////////////////////////////////////////////////
string CGameExporter::ExportAI(const string& path, uint32 flags)
{
	if (!GetISystem()->GetAISystem())
		return "No AI System available!";

	string result = ExportAIAreas(path);

	if (flags & eExp_AI_CoverSurfaces)
		ExportAICoverSurfaces(path);

	if (flags & eExp_AI_MNM)
		ExportAINavigationData(path);

	return result;
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportBrushes(const string& path)
{
	GetIEditorImpl()->Notify(eNotify_OnExportBrushes);
}

void CGameExporter::ForceDeleteFile(const char* filename)
{
	SetFileAttributes(filename, FILE_ATTRIBUTE_NORMAL);
	DeleteFile(filename);
}

void CGameExporter::EncryptPakFile(string& rPakFilename)
{
	// Disabled, for now. (inside EncryptPakFile)
#if 0
	string args;

	args.Format("/zip_encrypt=1 \"%s\"", rPakFilename.GetBuffer());
	Log("Encrypting PAK: %s", rPakFilename.GetBuffer());
	::ShellExecute(AfxGetMainWnd()->GetSafeHwnd(), "open", "Bin32\\rc\\rc.exe", args, "", SW_HIDE);
#endif
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::DeleteOldFiles(const string& levelDir, bool bSurfaceTexture)
{
	ForceDeleteFile(levelDir + "brush.lst");
	ForceDeleteFile(levelDir + "particles.lst");
	ForceDeleteFile(levelDir + "LevelData.xml");
	ForceDeleteFile(levelDir + "MovieData.xml");
	ForceDeleteFile(levelDir + "objects.lst");
	ForceDeleteFile(levelDir + "objects.idx");
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportMaterials(XmlNodeRef& levelDataNode, const string& path)
{
	//////////////////////////////////////////////////////////////////////////
	// Export materials manager.
	CMaterialManager* pManager = GetIEditorImpl()->GetMaterialManager();
	pManager->Export(levelDataNode);

	string filename = PathUtil::Make(path, MATERIAL_LEVEL_LIBRARY_FILE);

	bool bHaveItems = true;

	int numMtls = 0;

	XmlNodeRef nodeMaterials = XmlHelpers::CreateXmlNode("MaterialsLibrary");
	// Export Materials local level library.
	for (int i = 0; i < pManager->GetLibraryCount(); i++)
	{
		XmlNodeRef nodeLib = nodeMaterials->newChild("Library");
		CMaterialLibrary* pLib = (CMaterialLibrary*)pManager->GetLibrary(i);
		if (pLib->GetItemCount() > 0)
		{
			bHaveItems = false;
			// Export this library.
			numMtls += pManager->ExportLib(pLib, nodeLib);
		}
	}
	if (!bHaveItems)
	{
		XmlString xmlData = nodeMaterials->getXML();

		CCryMemFile file;
		file.Write(xmlData.c_str(), xmlData.length());
		m_levelPak.m_pakFile.UpdateFile(filename, file);
	}
	else
	{
		m_levelPak.m_pakFile.RemoveFile(filename);
	}
	m_numExportedMaterials = numMtls;
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportLevelLensFlares(const string& path)
{
	std::vector<CBaseObject*> objects;
	GetIEditorImpl()->GetObjectManager()->FindObjectsOfType(RUNTIME_CLASS(CEntityObject), objects);
	std::set<string> flareNameSet;
	for (int i = 0, iObjectSize(objects.size()); i < iObjectSize; ++i)
	{
		CEntityObject* pEntity = (CEntityObject*)objects[i];
		if (!pEntity->IsLight())
			continue;
		string flareName = pEntity->GetEntityPropertyString(CEntityObject::s_LensFlarePropertyName);
		if (flareName.IsEmpty() || flareName == "@root")
			continue;
		flareNameSet.insert(flareName);
	}

	XmlNodeRef pRootNode = GetIEditorImpl()->GetSystem()->CreateXmlNode("LensFlareList");
	pRootNode->setAttr("Version", FLARE_EXPORT_FILE_VERSION);

	CLensFlareManager* pLensManager = GetIEditorImpl()->GetLensFlareManager();

	if (CLensFlareLibrary* pLevelLib = (CLensFlareLibrary*)pLensManager->GetLevelLibrary())
	{
		for (int i = 0; i < pLevelLib->GetItemCount(); i++)
		{
			CLensFlareItem* pItem = (CLensFlareItem*)pLevelLib->GetItem(i);

			if (flareNameSet.find(pItem->GetFullName()) == flareNameSet.end())
				continue;

			CBaseLibraryItem::SerializeContext ctx(pItem->CreateXmlData(), false);
			pRootNode->addChild(ctx.node);
			pItem->Serialize(ctx);
			flareNameSet.erase(pItem->GetFullName());
		}
	}

	std::set<string>::iterator iFlareNameSet = flareNameSet.begin();
	for (; iFlareNameSet != flareNameSet.end(); ++iFlareNameSet)
	{
		string flareName = *iFlareNameSet;
		XmlNodeRef pFlareNode = GetIEditorImpl()->GetSystem()->CreateXmlNode("LensFlare");
		pFlareNode->setAttr("name", flareName);
		pRootNode->addChild(pFlareNode);
	}

	CCryMemFile lensFlareNames;
	lensFlareNames.Write(pRootNode->getXMLData()->GetString(), pRootNode->getXMLData()->GetStringLength());

	string exportPathName = path + FLARE_EXPORT_FILE;
	m_levelPak.m_pakFile.UpdateFile(exportPathName, lensFlareNames);
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportLevelResourceList(const string& path)
{
	IResourceList* pResList = gEnv->pCryPak->GetResourceList(ICryPak::RFOM_Level);

	// Write resource list to file.
	CCryMemFile memFile;
	for (const char* filename = pResList->GetFirst(); filename; filename = pResList->GetNext())
	{
		memFile.Write(filename, strlen(filename));
		memFile.Write("\n", 1);
	}

	string resFile = PathUtil::Make(path, RESOURCE_LIST_FILE);

	m_levelPak.m_pakFile.UpdateFile(resFile, memFile, true);
}

//////////////////////////////////////////////////////////////////////////

void CGameExporter::GatherLayerResourceList_r(CObjectLayer* pLayer, CUsedResources& resources)
{
	GetIEditorImpl()->GetObjectManager()->GatherUsedResources(resources, pLayer);

	for (int i = 0; i < pLayer->GetChildCount(); i++)
	{
		CObjectLayer* pChildLayer = pLayer->GetChild(i);
		GatherLayerResourceList_r(pChildLayer, resources);
	}
}

void CGameExporter::ExportLevelPerLayerResourceList(const string& path)
{
	// Write per layer resource list to file.
	CCryMemFile memFile;

	const auto& layers = GetIEditorImpl()->GetObjectManager()->GetLayersManager()->GetLayers();
	for (size_t i = 0; i < layers.size(); ++i)
	{
		CObjectLayer* pLayer = layers[i];

		// Only export topmost layers, and make sure they are flagged for exporting
		if (pLayer->GetParent() || !pLayer->IsExporLayerPak())
			continue;

		CUsedResources resources;
		GatherLayerResourceList_r(pLayer, resources);

		const char* acLayerName = (const char*)pLayer->GetName();

		for (CUsedResources::TResourceFiles::const_iterator it = resources.files.begin(); it != resources.files.end(); it++)
		{
			string filePath = PathUtil::MakeGamePath((*it).GetString());
			filePath.MakeLower();

			const char* acFilename = (const char*) filePath;
			memFile.Write(acLayerName, strlen(acLayerName));
			memFile.Write(";", 1);
			memFile.Write(acFilename, strlen(acFilename));
			memFile.Write("\n", 1);
		}
	}

	string resFile = PathUtil::Make(path, PERLAYER_RESOURCE_LIST_FILE);

	m_levelPak.m_pakFile.UpdateFile(resFile, memFile, true);
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportLevelShaderCache(const string& path)
{
	string buf;
	GetIEditorImpl()->GetDocument()->GetShaderCache()->SaveBuffer(buf);
	CCryMemFile memFile;
	memFile.Write((const char*)buf, buf.GetLength());

	string filename = PathUtil::Make(path, SHADER_LIST_FILE);
	m_levelPak.m_pakFile.UpdateFile(filename, memFile, true);
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportGameTokens(XmlNodeRef& levelDataNode, const string& path)
{
	// Export game tokens
	CGameTokenManager* pGTM = GetIEditorImpl()->GetGameTokenManager();
	// write only needed libs for this levels
	pGTM->Export(levelDataNode);

	string gtPath = PathUtil::AddBackslash(path.GetString()) + "GameTokens";
	string filename = PathUtil::Make(gtPath, GAMETOKENS_LEVEL_LIBRARY_FILE);

	bool bEmptyLevelLib = true;
	XmlNodeRef nodeLib = XmlHelpers::CreateXmlNode("GameTokensLibrary");
	// Export GameTokens local level library.
	for (int i = 0; i < pGTM->GetLibraryCount(); i++)
	{
		IDataBaseLibrary* pLib = pGTM->GetLibrary(i);
		if (pLib->IsLevelLibrary())
		{
			if (pLib->GetItemCount() > 0)
			{
				bEmptyLevelLib = false;
				// Export this library.
				pLib->Serialize(nodeLib, false);
				nodeLib->setAttr("LevelName", GetIEditorImpl()->GetGameEngine()->GetLevelPath()); // we set the Name from "Level" to the realname
			}
		}
		else
		{
			// AlexL: Not sure if
			// (1) we should store all libs into the PAK file or
			// (2) just use references to the game global Game/Libs directory.
			// Currently we use (1)
			if (pLib->GetItemCount() > 0)
			{
				// Export this library to pak file.
				XmlNodeRef gtNodeLib = XmlHelpers::CreateXmlNode("GameTokensLibrary");
				pLib->Serialize(gtNodeLib, false);
				CCryMemFile file;
				XmlString xmlData = gtNodeLib->getXML();
				file.Write(xmlData.c_str(), xmlData.length());
				string gtfilename = PathUtil::Make(gtPath, PathUtil::GetFile(pLib->GetFilename()));
				m_levelPak.m_pakFile.UpdateFile(gtfilename, file);
			}
		}
	}
	if (!bEmptyLevelLib)
	{
		XmlString xmlData = nodeLib->getXML();

		CCryMemFile file;
		file.Write(xmlData.c_str(), xmlData.length());
		m_levelPak.m_pakFile.UpdateFile(filename, file);
	}
	else
	{
		m_levelPak.m_pakFile.RemoveFile(filename);
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameExporter::ExportFileList(const string& path, const string& levelName)
{
	// process the folder of the specified map name, producing a filelist.xml file
	//	that can later be used for map downloads
	struct _finddata_t fileinfo;
	intptr_t handle;
	string newpath;

	string filename = levelName;
	string mapname = filename + ".dds";
	string metaname = filename + ".xml";

	XmlNodeRef rootNode = gEnv->pSystem->CreateXmlNode("download");
	rootNode->setAttr("name", filename);
	rootNode->setAttr("type", "Map");
	XmlNodeRef indexNode = rootNode->newChild("index");
	if (indexNode)
	{
		indexNode->setAttr("src", "filelist.xml");
		indexNode->setAttr("dest", "filelist.xml");
	}
	XmlNodeRef filesNode = rootNode->newChild("files");
	if (filesNode)
	{
		newpath = GetIEditorImpl()->GetGameEngine()->GetLevelPath();
		newpath += "/*.*";
		handle = gEnv->pCryPak->FindFirst(newpath.c_str(), &fileinfo);
		if (handle == -1)
			return;
		do
		{
			// ignore "." and ".."
			if (fileinfo.name[0] == '.')
				continue;
			// do we need any files from sub directories?
			if (fileinfo.attrib & _A_SUBDIR)
			{
				continue;
			}

			// only need the following files for multiplayer downloads
			if (!stricmp(fileinfo.name, GetLevelPakFilename())
			    || !stricmp(fileinfo.name, mapname.c_str())
			    || !stricmp(fileinfo.name, metaname.c_str()))
			{
				XmlNodeRef newFileNode = filesNode->newChild("file");
				if (newFileNode)
				{
					// TEMP: this is just for testing. src probably needs to be blank.
					//		string src = "http://server41/updater/";
					//		src += m_levelName;
					//		src += "/";
					//		src += fileinfo.name;
					newFileNode->setAttr("src", fileinfo.name);
					newFileNode->setAttr("dest", fileinfo.name);
					newFileNode->setAttr("size", fileinfo.size);

					unsigned char md5[16];
					string filename = GetIEditorImpl()->GetGameEngine()->GetLevelPath();
					filename += "/";
					filename += fileinfo.name;
					if (gEnv->pCryPak->ComputeMD5(filename, md5))
					{
						char md5string[33];
						cry_sprintf(md5string, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
						            md5[0], md5[1], md5[2], md5[3],
						            md5[4], md5[5], md5[6], md5[7],
						            md5[8], md5[9], md5[10], md5[11],
						            md5[12], md5[13], md5[14], md5[15]);
						newFileNode->setAttr("md5", md5string);
					}
					else
						newFileNode->setAttr("md5", "");
				}
			}
		}
		while (gEnv->pCryPak->FindNext(handle, &fileinfo) != -1);

		gEnv->pCryPak->FindClose(handle);
	}

	// save filelist.xml
	newpath = path;
	newpath += "/filelist.xml";
	rootNode->saveToFile(newpath.c_str());
}

void CGameExporter::BuildSectorAOData(int nAreaY, int nAreaX, int nAreaSize, SAOInfo* pAOData, int nAODataSize, float fTerrainMinZ, float fTerrainMaxZ)
{
	ISystem* pSystem = GetISystem();
	ITimer* pTimer = pSystem->GetITimer();
	double dStartTime = pTimer->GetAsyncCurTime();

	ILog* pLog = pSystem->GetILog();
	pLog->Log("Building AO data for sector ( %d, %d ) x %d m ( z = %.1f-%.1f ) ... ", nAreaX, nAreaY, nAreaSize, fTerrainMinZ, fTerrainMaxZ);

	// TODO: include borders

	float fRatio = (float)nAreaSize / nAODataSize;

	bool bObjectsFound = false;

	I3DEngine* p3DEngine = GetIEditorImpl()->Get3DEngine();

	// generate
	for (int x = 0; x < nAODataSize; x++)
	{
		for (int y = 0; y < nAODataSize; y++)
		{
			Vec3 vWSPos(float(x * fRatio + nAreaX), float(y * fRatio + nAreaY), 0);
			vWSPos.z = p3DEngine->GetTerrainElevation(vWSPos.x, vWSPos.y);

			Vec3 vRayBot = vWSPos + Vec3(0.f, 0.f, 4.f);
			Vec3 vRayTop = vWSPos + Vec3(0.f, 0.f, 2048.f);

			pAOData[x + y * nAODataSize].SetMax(vWSPos.z);

			// store objects elevation relative to to sector min z
			Vec3 vHitPoint(0, 0, 0);
			if (p3DEngine->RayObjectsIntersection2D(vRayTop, vRayBot, vHitPoint, eERType_Vegetation))
			{
				pAOData[x + y * nAODataSize].SetMax(max(vWSPos.z, vHitPoint.z));
				bObjectsFound = true;
			}

			// do the same for brushes
			vHitPoint = Vec3(0, 0, 0);
			if (p3DEngine->RayObjectsIntersection2D(vRayTop, vRayBot, vHitPoint, eERType_Brush))
			{
				float fHeight = max(vWSPos.z, vHitPoint.z);
				if (bObjectsFound)
					fHeight = max(fHeight, pAOData[x + y * nAODataSize].GetMax());
				pAOData[x + y * nAODataSize].SetMax(fHeight);
				bObjectsFound = true;
			}
		}
	}

	// blur
	if (bObjectsFound)
		if (int nPassesNum = 8 / fRatio)
		{
			SAOInfo* pAODataTmp = new SAOInfo[nAODataSize * nAODataSize];

			for (int nPass = 0; nPass < nPassesNum; nPass++)
			{
				memcpy(pAODataTmp, pAOData, nAODataSize * nAODataSize * sizeof(pAOData[0]));

				for (int x = 1; x < nAODataSize - 1; x++)
				{
					for (int y = 1; y < nAODataSize - 1; y++)
					{
						//			float fRefMax = max(pAOData[((x)+(y)*nAODataSize)].GetMax(),
						//			m_I3DEngine->GetTerrainElevation(x,y));

						float fMax = 0;
						float fCount = 0;

						for (int _x = x - 1; _x <= x + 1; _x++)
						{
							for (int _y = y - 1; _y <= y + 1; _y++)
							{
								//							fMax += max(pAOData[((_x)+(_y)*nAODataSize)].GetMax(), fRefMax);
								fMax += pAOData[((_x) + (_y) * nAODataSize)].GetMax();
								fCount++;
							}
						}

						pAODataTmp[((x) + (y) * nAODataSize)].SetMax(clamp_tpl(fMax / fCount, 0.f, 64.f));
					}
				}

				memcpy(pAOData, pAODataTmp, nAODataSize * nAODataSize * sizeof(pAOData[0]));
			}
			delete[] pAODataTmp;
		}

	pLog->LogPlus(" done", nAreaX, nAreaY, nAreaSize, nAODataSize);

	gBuildSectorAODataTime += pTimer->GetAsyncCurTime() - dStartTime;
}

void CGameExporter::Error(const string& error)
{
	if (m_bAutoExportMode)
		CLogFile::WriteLine((string("Export failed! ") + error).GetString());
	else
		Warning((string("Export failed! ") + error).GetString());
}

bool CGameExporter::OpenLevelPack(SLevelPakHelper& lphelper, bool bCryPak)
{
	bool bRet = false;

	assert(lphelper.m_bPakOpened == false);
	assert(lphelper.m_bPakOpenedCryPak == false);
	assert(!lphelper.m_sPath.IsEmpty());

	if (bCryPak)
	{
		bRet = gEnv->pCryPak->OpenPack(lphelper.m_sPath);
		lphelper.m_bPakOpenedCryPak = bRet;
	}
	else
	{
		bRet = lphelper.m_pakFile.Open(lphelper.m_sPath);
		lphelper.m_bPakOpened = bRet;
	}
	return bRet;
}

bool CGameExporter::CloseLevelPack(SLevelPakHelper& lphelper, bool bCryPak)
{
	bool bRet = false;

	if (bCryPak)
	{
		assert(lphelper.m_bPakOpenedCryPak == true);
		assert(!lphelper.m_sPath.IsEmpty());
		bRet = gEnv->pCryPak->ClosePack(lphelper.m_sPath);
		assert(bRet);
		lphelper.m_bPakOpenedCryPak = false;
	}
	else
	{
		assert(lphelper.m_bPakOpened == true);
		lphelper.m_pakFile.Close();
		bRet = true;
		lphelper.m_bPakOpened = false;
	}

	assert(lphelper.m_bPakOpened == false);
	assert(lphelper.m_bPakOpenedCryPak == false);
	return bRet;
}

namespace Private_GameExporter
{
void ExportSvogiData()
{
	CGameExporter gameExporter;
	gameExporter.ExportSvogiData();
}
}

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(Private_GameExporter::ExportSvogiData, general, export_svogi_data,
                                     "Export SVOGI Data",
                                     "general.export_svogi_data()");

