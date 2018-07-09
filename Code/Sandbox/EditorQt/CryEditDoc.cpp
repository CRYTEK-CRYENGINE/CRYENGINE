// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryEditDoc.h"
#include "ICommandManager.h"
#include "PluginManager.h"
#include "Mission.h"
#include "Vegetation/VegetationMap.h"
#include "ViewManager.h"
#include "GameEngine.h"
#include "MissionSelectDialog.h"
#include "Terrain/SurfaceType.h"
#include "Terrain/TerrainManager.h"
#include "Util/PakFile.h"
#include "Util/FileUtil.h"
#include "Objects/BaseObject.h"
#include "EntityPrototypeManager.h"
#include "Material/MaterialManager.h"
#include "Particles/ParticleManager.h"
#include "Prefabs/PrefabManager.h"
#include "LevelIndependentFileMan.h"
#include "GameTokens/GameTokenManager.h"
#include "LensFlareEditor/LensFlareManager.h"
#include "IUndoManager.h"
#include "SurfaceTypeValidator.h"
#include "ShaderCache.h"
#include "Util/AutoLogTime.h"
#include "Util/BoostPythonHelpers.h"
#include "Objects/ObjectLayerManager.h"
#include <CrySystem/File/ICryPak.h>
#include "Objects/BrushObject.h"
#include "Dialogs/CheckOutDialog.h"
#include <CryGame/IGameFramework.h>
#include "IEditorImpl.h"
#include <CrySandbox/IEditorGame.h>
#include <CrySandbox/ScopedVariableSetter.h>
#include <Cry3DEngine/ITimeOfDay.h>
#include "CryEdit.h"
#include "LevelEditor/LevelEditor.h"
#include "LevelEditor/LevelAssetType.h"
#include "FilePathUtil.h"
#include <Preferences/LightingPreferences.h>
#include <Preferences/GeneralPreferences.h>
#include "Util/MFCUtil.h"
#include "Controls/QuestionDialog.h"
#include "LevelEditor/LevelFileUtils.h"

#include <CryCore/Platform/CryLibrary.h>

//#define PROFILE_LOADING_WITH_VTUNE

// profilers api.
//#include "pure.h"
#ifdef PROFILE_LOADING_WITH_VTUNE
	#include "C:\Program Files\Intel\Vtune\Analyzer\Include\VTuneApi.h"
	#pragma comment(lib,"C:\\Program Files\\Intel\\Vtune\\Analyzer\\Lib\\VTuneApi.lib")
#endif

#if defined(IS_COMMERCIAL)
	#include "CryDevProjectSelectionDialog.cpp"
#endif

static const char* kAutoBackupFolder = "_autobackup";
static const char* kLastLoadPathFilename = "lastLoadPath.preset";

// List of files that need to be copied when saving to a different folder
static const char* kLevelSaveAsCopyList[] =
{
	"level.pak",
	"terraintexture.pak",
	"filelist.xml",
	"levelshadercache.pak"
	"tags.txt"
};

namespace Private_CryEditDoc
{

void SetGlobalLevelName(const string& szFilename)
{
	// Set game g_levelname variable to the name of current level.
	string szGameLevelName = PathUtil::GetFileName(szFilename);
	ICVar* sv_map = gEnv->pConsole->GetCVar("sv_map");
	if (sv_map)
	{
		sv_map->Set((const char*)szGameLevelName);
	}
}

void DisablePhysicsAndAISimulation()
{
	// Do that BEFORE beginning scene open, since it might interfere with initialization of systems
	if (GetIEditorImpl()->GetGameEngine()->GetSimulationMode())
	{
		CCryEditApp::GetInstance()->OnSwitchPhysics();

		// TODO: A proper way to invoke state changes by also notifying a Sandbox button state is not yet implemented.
		QAction* pAction = GetIEditorImpl()->GetICommandManager()->GetAction("ui_action.actionEnable_Physics_AI");
		if (pAction)
		{
			pAction->setChecked(false);
		}
	}
}

void CallAudioSystemOnLoad(const string &szFilename)
{
	string fileName = PathUtil::GetFileName(szFilename.GetString());
	string const levelName = PathUtil::GetFileName(fileName.GetBuffer());

	if (!levelName.empty() && levelName.compareNoCase("Untitled") != 0)
	{
		gEnv->pAudioSystem->OnLoadLevel(levelName.c_str());
	}
}

void LoadTerrain(TDocMultiArchive& arrXmlAr)
{
	LOADING_TIME_PROFILE_SECTION_NAMED("Load Terrain");
	CAutoLogTime logtime("Load Terrain");
	if (!GetIEditorImpl()->GetTerrainManager()->Load())
		GetIEditorImpl()->GetTerrainManager()->SerializeTerrain(arrXmlAr); // load old version

	if (!GetIEditorImpl()->GetTerrainManager()->LoadTexture())
		GetIEditorImpl()->GetTerrainManager()->SerializeTexture((*arrXmlAr[DMAS_GENERAL]));  // load old version

	GetIEditorImpl()->GetHeightmap()->InitTerrain();
	GetIEditorImpl()->GetHeightmap()->UpdateEngineTerrain();
}

void LoadGameEngineLevel(const string& filename, string currentMissionName)
{
	HEAP_CHECK
	LOADING_TIME_PROFILE_SECTION_NAMED("Game Engine level load");
	CAutoLogTime logtime("Game Engine level load");
	string szLevelPath = PathUtil::GetPathWithoutFilename(filename);
	GetIEditorImpl()->GetGameEngine()->LoadLevel(szLevelPath, currentMissionName, true, true);
}

void LoadMaterials(TDocMultiArchive& arrXmlAr)
{
	LOADING_TIME_PROFILE_SECTION_NAMED("Load MaterialManager");
	CAutoLogTime logtime("Load MaterialManager");
	GetIEditorImpl()->GetMaterialManager()->Serialize((*arrXmlAr[DMAS_GENERAL]).root, (*arrXmlAr[DMAS_GENERAL]).bLoading);
}

void LoadParticles(TDocMultiArchive& arrXmlAr)
{
	LOADING_TIME_PROFILE_SECTION_NAMED("Load Particles");
	CAutoLogTime logtime("Load Particles");
	GetIEditorImpl()->GetParticleManager()->Serialize((*arrXmlAr[DMAS_GENERAL]).root, (*arrXmlAr[DMAS_GENERAL]).bLoading);
}

void LoadLensFlares(TDocMultiArchive& arrXmlAr)
{
	LOADING_TIME_PROFILE_SECTION_NAMED("Load Flares");
	CAutoLogTime logtime("Load Flares");
	GetIEditorImpl()->GetLensFlareManager()->Serialize((*arrXmlAr[DMAS_GENERAL]).root, (*arrXmlAr[DMAS_GENERAL]).bLoading);
}

void LoadGameTokens(TDocMultiArchive& arrXmlAr)
{
	LOADING_TIME_PROFILE_SECTION_NAMED("Load GameTokens");
	CAutoLogTime logtime("Load GameTokens");
	if (!GetIEditorImpl()->GetGameTokenManager()->Load())
	{
		GetIEditorImpl()->GetGameTokenManager()->Serialize((*arrXmlAr[DMAS_GENERAL]).root, (*arrXmlAr[DMAS_GENERAL]).bLoading);    // load old version
	}
}

void LoadVegetation(TDocMultiArchive& arrXmlAr)
{
	CVegetationMap* pVegetationMap = GetIEditorImpl()->GetVegetationMap();

	if (pVegetationMap)
	{
		LOADING_TIME_PROFILE_SECTION_NAMED("Load Vegetation");
		CAutoLogTime logtime("Load Vegetation");
		if (!pVegetationMap->Load())
			pVegetationMap->Serialize((*arrXmlAr[DMAS_VEGETATION])); // old version
	}
}

void UpdateSurfaceTypes()
{
	// update surf types because layers info only now is available in vegetation groups
	LOADING_TIME_PROFILE_SECTION_NAMED("Updating Surface Types");
	CAutoLogTime logtime("Updating Surface Types");
	GetIEditorImpl()->GetTerrainManager()->ReloadSurfaceTypes(false);
}

void LoadEntityPrototypeDatabase(TDocMultiArchive& arrXmlAr)
{
	LOADING_TIME_PROFILE_SECTION_NAMED("Load Entity Archetypes Database");
	CAutoLogTime logtime("Load Entity Archetypes Database");
	GetIEditorImpl()->GetEntityProtManager()->Serialize((*arrXmlAr[DMAS_GENERAL]).root, (*arrXmlAr[DMAS_GENERAL]).bLoading);
}

void LoadPrefabDatabase(TDocMultiArchive& arrXmlAr)
{
	LOADING_TIME_PROFILE_SECTION_NAMED("Load Prefabs Database");
	CAutoLogTime logtime("Load Prefabs Database");
	GetIEditorImpl()->GetPrefabManager()->Serialize((*arrXmlAr[DMAS_GENERAL]).root, (*arrXmlAr[DMAS_GENERAL]).bLoading);
}

void CreateMovieSystemSequenceObjects()
{
	LOADING_TIME_PROFILE_SECTION_NAMED("Movie System Compatibility Conversion");
	// support old version of sequences
	IMovieSystem* pMs = GetIEditorImpl()->GetMovieSystem();

	if (pMs)
	{
		for (int k = 0; k < pMs->GetNumSequences(); ++k)
		{
			IAnimSequence* seq = pMs->GetSequence(k);
			string fullname = seq->GetName();
			CBaseObject* pObj = GetIEditorImpl()->GetObjectManager()->FindObject(fullname);

			if (!pObj)
			{
				pObj = GetIEditorImpl()->GetObjectManager()->NewObject("SequenceObject", 0, fullname);
			}
		}
	}
}

void CreateLevelIfNeeded()
{
	// Add a layer if no layer was found for some reason
	auto* pObjectLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	if (!pObjectLayerManager->HasLayers())
	{
		LOADING_TIME_PROFILE_SECTION_NAMED("Creating Empty Layer");
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "The level doesn't have any layers. Creating a new layer as fallback. This is a critical error and may be a sign of data corruption.");

		pObjectLayerManager->CreateLayer("Main");
	}
}

}

/////////////////////////////////////////////////////////////////////////////
// CCryEditDoc construction/destruction

CCryEditDoc::CCryEditDoc()
	: doc_validate_surface_types(0)
	// It assumes loaded levels have already been exported. Can be a big fat lie, though.
	// The right way would require us to save to the level folder the export status of the
	// level.
	, m_boLevelExported(true)
	, m_mission(NULL)
	, m_bLevelBeingLoaded(false)
	, m_bIsClosing(false)
{
	////////////////////////////////////////////////////////////////////////
	// Set member variables to initial values
	////////////////////////////////////////////////////////////////////////
	m_bLoadFailed = false;
	m_waterColor = RGB(0, 0, 255);
	m_fogTemplate = GetIEditorImpl()->FindTemplate("Fog");
	m_environmentTemplate = GetIEditorImpl()->FindTemplate("Environment");

	if (m_environmentTemplate)
	{
		m_fogTemplate = m_environmentTemplate->findChild("Fog");
	}
	else
	{
		m_environmentTemplate = XmlHelpers::CreateXmlNode("Environment");
	}

	m_pLevelShaderCache = new CLevelShaderCache;
	m_bDocumentReady = false;
	GetIEditorImpl()->SetDocument(this);

	m_pTmpXmlArchHack = 0;
	RegisterConsoleVariables();
}

CCryEditDoc::~CCryEditDoc()
{
	ClearMissions();
	GetIEditorImpl()->GetTerrainManager()->ClearLayers();
	delete m_pLevelShaderCache;
}

bool CCryEditDoc::Save()
{
	return OnSaveDocument(GetPathName()) == TRUE;
}

void CCryEditDoc::ChangeMission()
{
	GetIEditorImpl()->Notify(eNotify_OnMissionChange);
}

void CCryEditDoc::DeleteContents()
{
	LOADING_TIME_PROFILE_SECTION;

	m_bIsClosing = true;

	SetDocumentReady(false);

	{
		LOADING_TIME_PROFILE_SECTION_NAMED("Notify Clear Level Contents")
		GetIEditorImpl()->Notify(eNotify_OnClearLevelContents);
	}

	GetIEditorImpl()->SetEditTool(0); // Turn off any active edit tools.
	GetIEditorImpl()->SetEditMode(eEditModeSelect);


	//////////////////////////////////////////////////////////////////////////
	// Clear all undo info.
	// TODO : this should only clear the relevant undo entries, not everything
	//////////////////////////////////////////////////////////////////////////
	GetIEditorImpl()->GetIUndoManager()->Flush();
	GetIEditorImpl()->GetIUndoManager()->Suspend();

	GetIEditorImpl()->GetVegetationMap()->ClearObjects();
	GetIEditorImpl()->GetTerrainManager()->ClearLayers();
	GetIEditorImpl()->ResetViews();

	// Delete all objects from Object Manager.
	GetIEditorImpl()->GetObjectManager()->DeleteAllObjects();
	GetIEditorImpl()->GetObjectManager()->GetLayersManager()->ClearLayers();
	GetIEditorImpl()->GetTerrainManager()->RemoveAllSurfaceTypes();
	ClearMissions();

	GetIEditorImpl()->GetGameEngine()->ResetResources();

	// Load scripts data
	SetModifiedFlag(FALSE);

	// Unload level specific audio binary data.
	gEnv->pAudioSystem->OnUnloadLevel();

	GetIEditorImpl()->GetIUndoManager()->Resume();

	m_bIsClosing = false;
}

void CCryEditDoc::Save(CXmlArchive& xmlAr)
{
	TDocMultiArchive arrXmlAr;
	FillXmlArArray(arrXmlAr, &xmlAr);
	Save(arrXmlAr);
}

void CCryEditDoc::Save(TDocMultiArchive& arrXmlAr)
{
	LOADING_TIME_PROFILE_SECTION;
	m_pTmpXmlArchHack = arrXmlAr[DMAS_GENERAL];
	CAutoDocNotReady autoDocNotReady;
	string currentMissionName;

	if (arrXmlAr[DMAS_GENERAL] != NULL)
	{
		(*arrXmlAr[DMAS_GENERAL]).root = XmlHelpers::CreateXmlNode("Level");
		(*arrXmlAr[DMAS_GENERAL]).root->setAttr("WaterColor", m_waterColor);
		(*arrXmlAr[DMAS_GENERAL]).root->setAttr("SandboxVersion", (const char*)GetIEditorImpl()->GetFileVersion().ToFullString());

		SerializeViewSettings((*arrXmlAr[DMAS_GENERAL]));
		// Fog settings  ///////////////////////////////////////////////////////
		SerializeFogSettings((*arrXmlAr[DMAS_GENERAL]));
		// Serialize Missions //////////////////////////////////////////////////
		SerializeMissions(arrXmlAr, currentMissionName, false);
		//! Serialize entity prototype manager.
		GetIEditorImpl()->GetEntityProtManager()->Serialize((*arrXmlAr[DMAS_GENERAL]).root, (*arrXmlAr[DMAS_GENERAL]).bLoading);
		//! Serialize prefabs manager.
		GetIEditorImpl()->GetPrefabManager()->Serialize((*arrXmlAr[DMAS_GENERAL]).root, (*arrXmlAr[DMAS_GENERAL]).bLoading);
		//! Serialize material manager.
		GetIEditorImpl()->GetMaterialManager()->Serialize((*arrXmlAr[DMAS_GENERAL]).root, (*arrXmlAr[DMAS_GENERAL]).bLoading);
		//! Serialize particles manager.
		GetIEditorImpl()->GetParticleManager()->Serialize((*arrXmlAr[DMAS_GENERAL]).root, (*arrXmlAr[DMAS_GENERAL]).bLoading);
		//! Serialize game tokens manager.
		GetIEditorImpl()->GetGameTokenManager()->Save(gEditorFilePreferences.filesBackup);
		//! Serialize LensFlare manager.
		GetIEditorImpl()->GetLensFlareManager()->Serialize((*arrXmlAr[DMAS_GENERAL]).root, (*arrXmlAr[DMAS_GENERAL]).bLoading);

		SerializeShaderCache((*arrXmlAr[DMAS_GENERAL_NAMED_DATA]));
	}
	m_pTmpXmlArchHack = 0;
}

void CCryEditDoc::Load(CXmlArchive& xmlAr, const string& szFilename)
{
	TDocMultiArchive arrXmlAr;
	FillXmlArArray(arrXmlAr, &xmlAr);
	CCryEditDoc::Load(arrXmlAr, szFilename);
}

void CCryEditDoc::Load(TDocMultiArchive& arrXmlAr, const string& filename)
{
	using namespace Private_CryEditDoc;
	CScopedVariableSetter<bool> loadingGuard(m_bLevelBeingLoaded, true);

	// Register a unique load event
	LOADING_TIME_PROFILE_SECTION(gEnv->pSystem);
	m_pTmpXmlArchHack = arrXmlAr[DMAS_GENERAL];
	CAutoDocNotReady autoDocNotReady;

	HEAP_CHECK

	CryLog("Loading from %s...", (const char*)filename);

	SetGlobalLevelName(filename);

	DisablePhysicsAndAISimulation();

	GetIEditorImpl()->Notify(eNotify_OnBeginSceneOpen);
	GetIEditorImpl()->GetMovieSystem()->RemoveAllSequences();

	int t0 = GetTickCount();

#ifdef PROFILE_LOADING_WITH_VTUNE
	VTResume();
#endif

	CallAudioSystemOnLoad(filename);

	HEAP_CHECK

	string currentMissionName = GetCurrentMissionName(arrXmlAr);

	HEAP_CHECK

	LoadTerrain(arrXmlAr);

	LoadGameEngineLevel(filename, currentMissionName);

	(*arrXmlAr[DMAS_GENERAL]).root->getAttr("WaterColor", m_waterColor);

	LoadMaterials(arrXmlAr);

	LoadParticles(arrXmlAr);

	LoadLensFlares(arrXmlAr);

	LoadGameTokens(arrXmlAr);

	SerializeViewSettings((*arrXmlAr[DMAS_GENERAL]));

	LoadVegetation(arrXmlAr);

	UpdateSurfaceTypes();

	SerializeFogSettings((*arrXmlAr[DMAS_GENERAL]));

	LoadEntityPrototypeDatabase(arrXmlAr);

	LoadPrefabDatabase(arrXmlAr);

	ActivateMission(currentMissionName);

	ForceSkyUpdate();

	LoadShaderCache(arrXmlAr);

	CreateMovieSystemSequenceObjects();

	CreateLevelIfNeeded();

	CSurfaceTypeValidator().Validate();

#ifdef PROFILE_LOADING_WITH_VTUNE
	VTPause();
#endif

	LogLoadTime(GetTickCount() - t0);
	m_pTmpXmlArchHack = 0;

	GetIEditorImpl()->Notify(eNotify_OnEndSceneOpen);
}

void CCryEditDoc::LoadShaderCache(TDocMultiArchive& arrXmlAr)
{
	LOADING_TIME_PROFILE_SECTION_NAMED("Load Level Shader Cache");
	CAutoLogTime logtime("Load Level Shader Cache");
	SerializeShaderCache((*arrXmlAr[DMAS_GENERAL_NAMED_DATA]));
}

void CCryEditDoc::ActivateMission(const string &currentMissionName)
{
	LOADING_TIME_PROFILE_SECTION_NAMED_ARGS("Activating Mission", currentMissionName.c_str());
	string str;
	str.Format("Activating Mission %s", (const char*)currentMissionName);
	CAutoLogTime logtime(str);

	// Select current mission.
	m_mission = FindMission(currentMissionName);

	if (m_mission)
	{
		SyncCurrentMissionContent(true);
	}
	else
	{
		GetCurrentMission();
	}

	// Check if there is a need to create a terrain layer (to support old levels created without the terrain layer).
	{
		auto getXMLAttribBool = [](XmlNodeRef pInputNode, const char* szLevel1, const char* szLevel2, bool bDefaultValue) -> bool
		{
			bool bResult = bDefaultValue;
			XmlNodeRef node1 = pInputNode->findChild(szLevel1);
			if (node1)
			{
				XmlNodeRef node2 = node1->findChild(szLevel2);
				if (node2)
				{
					node2->getAttr("value", bResult);
				}
			}
			return bResult;
		};

		XmlNodeRef pNode = GetEnvironmentTemplate();
		if (pNode)
		{
			const bool bShowTerrainSurface = getXMLAttribBool(pNode, "EnvState", "ShowTerrainSurface", true);
			CObjectLayerManager* const pObjectLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
			if (bShowTerrainSurface && !pObjectLayerManager->IsAnyLayerOfType(eObjectLayerType_Terrain))
			{
				pObjectLayerManager->CreateLayer("Terrain", eObjectLayerType_Terrain);
			}
		}
	}
}

string CCryEditDoc::GetCurrentMissionName(TDocMultiArchive& arrXmlAr)
{
	string currentMissionName;
	SerializeMissions(arrXmlAr, currentMissionName, false);

	// If multiple missions, select specific mission to load.
	if (GetMissionCount() > 1)
	{
		CMissionSelectDialog dlg;
		if (dlg.DoModal() == IDOK)
		{
			currentMissionName = dlg.GetSelected();
		}
	}	
	return currentMissionName;
}

void CCryEditDoc::SerializeViewSettings(CXmlArchive& xmlAr)
{
	// Load or restore the viewer settings from an XML
	if (xmlAr.bLoading)
	{
		// Loading
		CLogFile::WriteLine("Loading View settings...");

		Vec3 vp(0.0f, 0.0f, 256.0f);
		Ang3 va(ZERO);

		XmlNodeRef view = xmlAr.root->findChild("View");
		if (view)
		{
			view->getAttr("ViewerPos", vp);
			view->getAttr("ViewerAngles", va);
		}

		CViewport* pVP = GetIEditorImpl()->GetViewManager()->GetGameViewport();

		if (pVP)
		{
			Matrix34 tm = Matrix34::CreateRotationXYZ(va);
			tm.SetTranslation(vp);
			pVP->SetViewTM(tm);
		}
	}
	else
	{
		// Storing
		CLogFile::WriteLine("Storing View settings...");

		XmlNodeRef view = xmlAr.root->newChild("View");
		CViewport* pVP = GetIEditorImpl()->GetViewManager()->GetGameViewport();

		if (pVP)
		{
			Vec3 pos = pVP->GetViewTM().GetTranslation();
			Ang3 angles = Ang3::GetAnglesXYZ(Matrix33(pVP->GetViewTM()));
			view->setAttr("ViewerPos", pos);
			view->setAttr("ViewerAngles", angles);
		}
	}
}

void CCryEditDoc::SerializeFogSettings(CXmlArchive& xmlAr)
{
	if (xmlAr.bLoading)
	{
		CLogFile::WriteLine("Loading Fog settings...");

		XmlNodeRef fog = xmlAr.root->findChild("Fog");

		if (!fog)
			return;

		if (m_fogTemplate)
		{
			CXmlTemplate::GetValues(m_fogTemplate, fog);
		}
	}
	else
	{
		CLogFile::WriteLine("Storing Fog settings...");

		XmlNodeRef fog = xmlAr.root->newChild("Fog");

		if (m_fogTemplate)
		{
			CXmlTemplate::SetValues(m_fogTemplate, fog);
		}
	}
}

void CCryEditDoc::SerializeMissions(TDocMultiArchive& arrXmlAr, string& currentMissionName, bool bPartsInXml)
{
	bool bLoading = IsLoadingXmlArArray(arrXmlAr);

	if (bLoading)
	{
		// Loading
		CLogFile::WriteLine("Loading missions...");
		// Clear old layers
		ClearMissions();
		// Load shared objects and layers.
		XmlNodeRef objectsNode = arrXmlAr[DMAS_GENERAL]->root->findChild("Objects");
		XmlNodeRef objectLayersNode = arrXmlAr[DMAS_GENERAL]->root->findChild("ObjectLayers");
		// Load the layer count
		XmlNodeRef node = arrXmlAr[DMAS_GENERAL]->root->findChild("Missions");

		if (!node)
			return;

		string current;
		node->getAttr("Current", current);
		currentMissionName = current;

		// Read all node
		for (int i = 0; i < node->getChildCount(); i++)
		{
			CXmlArchive ar(*arrXmlAr[DMAS_GENERAL]);
			ar.root = node->getChild(i);
			CMission* mission = new CMission(this);
			mission->Serialize(ar);
			if (bPartsInXml)
			{
				mission->SerializeTimeOfDay(*arrXmlAr[DMAS_TIME_OF_DAY]);
				mission->SerializeEnvironment(*arrXmlAr[DMAS_ENVIRONMENT]);
			}
			else
				mission->LoadParts();

			// Timur[9/11/2002] For backward compatibility with shared objects
			if (objectsNode)
				mission->AddObjectsNode(objectsNode);
			if (objectLayersNode)
				mission->SetLayersNode(objectLayersNode);

			AddMission(mission);
		}
	}
	else
	{
		// Storing
		CLogFile::WriteLine("Storing missions...");
		// Save contents of current mission.
		SyncCurrentMissionContent(false);

		XmlNodeRef node = arrXmlAr[DMAS_GENERAL]->root->newChild("Missions");

		//! Store current mission name.
		currentMissionName = GetCurrentMission()->GetName();
		node->setAttr("Current", currentMissionName);

		// Write all surface types.
		for (int i = 0; i < m_missions.size(); i++)
		{
			CXmlArchive ar(*arrXmlAr[DMAS_GENERAL]);
			ar.root = node->newChild("Mission");
			m_missions[i]->Serialize(ar, false);
			if (bPartsInXml)
			{
				m_missions[i]->SerializeTimeOfDay(*arrXmlAr[DMAS_TIME_OF_DAY]);
				m_missions[i]->SerializeEnvironment(*arrXmlAr[DMAS_ENVIRONMENT]);
			}
			else
				m_missions[i]->SaveParts();
		}
		CLogFile::WriteString("Done");
	}
}

void CCryEditDoc::SerializeShaderCache(CXmlArchive& xmlAr)
{
	LOADING_TIME_PROFILE_SECTION;
	if (xmlAr.bLoading)
	{
		void* pData = 0;
		int nSize = 0;

		if (xmlAr.pNamedData->GetDataBlock("ShaderCache", pData, nSize))
		{
			if (nSize <= 0)
				return;

			CString buf;
			char* str = buf.GetBuffer(nSize + 1);
			memcpy(str, pData, nSize);
			str[nSize] = 0;
			m_pLevelShaderCache->LoadBuffer(str);
		}
	}
	else
	{
		string buf;
		m_pLevelShaderCache->SaveBuffer(buf);

		if (buf.GetLength() > 0)
		{
			xmlAr.pNamedData->AddDataBlock("ShaderCache", const_cast<char*>(buf.GetBuffer()), buf.GetLength());
		}
	}
}

bool CCryEditDoc::CanClose()
{
	if (!IsDocumentReady())
	{
		return true; // no level loaded
	}
	if (!IsModified())
	{
		return true; // nothing to save
	}

	auto levelName = QString::fromLocal8Bit(GetIEditorImpl()->GetGameEngine()->GetLevelName().GetString());
	auto titleText = CLevelEditor::tr("Save Level");
	auto messageText = CLevelEditor::tr("Level %1 has unsaved changes!\nSave Level?").arg(levelName);

	QDialogButtonBox::StandardButton result = CQuestionDialog::SQuestion(titleText, messageText, QDialogButtonBox::StandardButton::Yes | QDialogButtonBox::StandardButton::No | QDialogButtonBox::StandardButton::Cancel);

	switch (result)
	{
	case QDialogButtonBox::StandardButton::Yes:
		DoFileSave();
		return true;

	case QDialogButtonBox::StandardButton::No:
		return true;

	case QDialogButtonBox::StandardButton::Cancel:
	default:

		return false;
	}

	return true;
}

BOOL CCryEditDoc::DoSave(LPCTSTR lpszPathName, BOOL bReplace)
{
	LOADING_TIME_PROFILE_AUTO_SESSION("sandbox_save");

	BOOL bResult = OnSaveDocument(lpszPathName);
	// Restore current directory to root.
	SetCurrentDirectory(GetIEditorImpl()->GetMasterCDFolder());
	return bResult;
}

BOOL CCryEditDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
	TOpenDocContext context;
	if (!BeforeOpenDocument(lpszPathName, context))
		return FALSE;
	return DoOpenDocument(lpszPathName, context);
}

BOOL CCryEditDoc::BeforeOpenDocument(LPCTSTR lpszPathName, TOpenDocContext& context)
{
	CTimeValue loading_start_time = gEnv->pTimer->GetAsyncTime();
	//ensure we close any open packs
	if (!GetIEditorImpl()->GetLevelFolder().IsEmpty())
	{
		GetIEditorImpl()->GetSystem()->GetIPak()->ClosePacks(GetIEditorImpl()->GetLevelFolder() + string("\\*.*"));
	}

	// restore directory to root.
	SetCurrentDirectory(GetIEditorImpl()->GetMasterCDFolder());
	SetPathName(lpszPathName);
	string relativeLevelName = lpszPathName;
	CryLog("Opening document %s", (const char*)relativeLevelName);

	context.loading_start_time = loading_start_time;
	context.relativeLevelName = relativeLevelName;
	return TRUE;
}

BOOL CCryEditDoc::DoOpenDocument(LPCTSTR lpszPathName, TOpenDocContext& context)
{
	LOADING_TIME_PROFILE_AUTO_SESSION("sandbox_level_load");
	LOADING_TIME_PROFILE_SECTION;

	CTimeValue& loading_start_time = context.loading_start_time;
	string& relativeLevelName = context.relativeLevelName;

	// write the full filename and path to the log
	m_bLoadFailed = false;

	ICryPak* pIPak = GetIEditorImpl()->GetSystem()->GetIPak();
	string levelPath = PathUtil::GetPathWithoutFilename(relativeLevelName);

	TDocMultiArchive arrXmlAr = {};
	if (!LoadXmlArchiveArray(arrXmlAr, relativeLevelName, levelPath))
		return FALSE;

	if (gEnv->pGameFramework)
	{
		string level = relativeLevelName;
		gEnv->pGameFramework->SetEditorLevel(PathUtil::GetFileName(level).c_str(), PathUtil::GetPathWithoutFilename(level).c_str());
	}

	LoadLevel(arrXmlAr, relativeLevelName);
	ReleaseXmlArchiveArray(arrXmlAr);

	if (m_bLoadFailed)
		return FALSE;

	StartStreamingLoad();

	CTimeValue loading_end_time = gEnv->pTimer->GetAsyncTime();

	CryLog("-----------------------------------------------------------");
	CryLog("Successfully opened document %s", (const char*)levelPath);
	CryLog("Level loading time: %.2f seconds", (loading_end_time - loading_start_time).GetSeconds());
	CryLog("-----------------------------------------------------------");

	// It assumes loaded levels have already been exported. Can be a big fat lie, though.
	// The right way would require us to save to the level folder the export status of the
	// level.
	SetLevelExported(true);

	return TRUE;
}

BOOL CCryEditDoc::OnSaveDocument(LPCTSTR lpszPathName)
{
	TSaveDocContext context;
	if (!BeforeSaveDocument(lpszPathName, context))
		return FALSE;
	DoSaveDocument(lpszPathName, context);
	bool bShowPrompt = true;
	return AfterSaveDocument(lpszPathName, context, bShowPrompt);
}

BOOL CCryEditDoc::BeforeSaveDocument(LPCTSTR lpszPathName, TSaveDocContext& context)
{

#if defined(IS_COMMERCIAL)
	CCryDevProjectSelectionDialog ProjectSelection;
	INT_PTR iResult = ProjectSelection.DoModal();
	if (iResult == IDABORT || iResult == IDCANCEL)
	{
		CryLog("You cancelled saving of level file \"%s\"", (const char*)lpszPathName);
		return FALSE;
	}
#endif

	LOADING_TIME_PROFILE_SECTION;

	// Restore directory to root.
	SetCurrentDirectory(GetIEditorImpl()->GetMasterCDFolder());

	string levelPath = PathUtil::AbsolutePathToCryPakPath(lpszPathName).c_str();
	if (levelPath.empty())
		return FALSE;

	CryLog("Saving to %s...", (const char*)levelPath);
	GetIEditorImpl()->Notify(eNotify_OnBeginSceneSave);

	bool bSaved(true);

	context.bSaved = bSaved;
	return TRUE;
}

BOOL CCryEditDoc::DoSaveDocument(LPCTSTR filename, TSaveDocContext& context)
{
	LOADING_TIME_PROFILE_SECTION;

	bool& bSaved = context.bSaved;

	bSaved = GetIEditorImpl()->GetTerrainManager()->WouldHeightmapSaveSucceed();

	if (bSaved)
	{
		// Save Tag Point locations to file if auto save of tag points disabled
		CCryEditApp::GetInstance()->SaveTagLocations();

		bSaved = SaveLevel(PathUtil::AbsolutePathToCryPakPath(filename).c_str());

		// Changes filename for this document.
		SetPathName(filename);
	}

	return bSaved;
}

BOOL CCryEditDoc::AfterSaveDocument(LPCTSTR lpszPathName, TSaveDocContext& context, bool bShowPrompt)
{
	LOADING_TIME_PROFILE_SECTION;

	bool& bSaved = context.bSaved;

	GetIEditorImpl()->Notify(eNotify_OnEndSceneSave);

	if (!bSaved)
	{
		if (bShowPrompt)
		{
			CQuestionDialog::SCritical(QObject::tr(""), QObject::tr("Save Failed"));
		}
		CLogFile::WriteLine("$4Level saving has failed.");
	}
	else
	{
		CLogFile::WriteLine("$3Level successfully saved");
		SetModifiedFlag(FALSE);
		SetLastLoadedLevelName(lpszPathName);
	}

	return bSaved;
}

static void GetUserSettingsFile(const string& levelFolder, string& userSettings)
{
	const char* pUserName = GetISystem()->GetUserName();
	string fileName;
	fileName.Format("%s_usersettings.editor_xml", pUserName);
	userSettings = PathUtil::Make(levelFolder, fileName);
}

bool CCryEditDoc::SaveLevel(const string& filename)
{
	LOADING_TIME_PROFILE_SECTION;
	CWaitCursor wait;

	CAutoCheckOutDialogEnableForAll enableForAll;

	if (!CFileUtil::OverwriteFile(filename))
		return false;

	string levelFolder = PathUtil::GetPathWithoutFilename(filename);
	CFileUtil::CreateDirectory(levelFolder);
	GetIEditorImpl()->GetGameEngine()->SetLevelPath(levelFolder);

	// need to copy other level data before saving to different folder
	const string oldLevelPath = PathUtil::AbsolutePathToCryPakPath(GetPathName().GetString()).c_str();
	const string oldLevelFolder = PathUtil::GetPathWithoutFilename(oldLevelPath);

	if (oldLevelFolder != levelFolder)
	{
		LOADING_TIME_PROFILE_SECTION_NAMED("CCryEditDoc::SaveLevel() level folder changed");

		// make sure we stop streaming from level.pak
		gEnv->p3DEngine->CloseTerrainTextureFile();

		ICryPak* pIPak = GetIEditorImpl()->GetSystem()->GetIPak();
		pIPak->Lock();

		for (unsigned int i = 0; i < sizeof(kLevelSaveAsCopyList) / sizeof(char*); ++i)
		{
			// if we're saving a pak file, ensure we do not have an existing version of that pak file open in memory
			// causes nasty problems when we reload the pak with different file contents and have old header info in mem
			if (strstr(kLevelSaveAsCopyList[i], ".pak"))
			{
				string pakPath = levelFolder + "/" + kLevelSaveAsCopyList[i];
				pIPak->ClosePack(pakPath);
			}

			CFileUtil::CopyFile(oldLevelFolder + "/" + kLevelSaveAsCopyList[i], levelFolder + "/" + kLevelSaveAsCopyList[i]);
		}

		pIPak->Unlock();
	}


	CryLog("Saving level file %s", filename.c_str());
	{
		// Make a backup of file.
		if (gEditorFilePreferences.filesBackup)
		{
			CFileUtil::BackupFile(filename.c_str());
		}

		CXmlArchive xmlAr;
		Save(xmlAr);
		if (!xmlAr.SaveToFile(filename))
		{
			return false;
		}
	}

	// Save Heightmap and terrain data
	GetIEditorImpl()->GetTerrainManager()->Save(gEditorFilePreferences.filesBackup);
	// Save TerrainTexture
	GetIEditorImpl()->GetTerrainManager()->SaveTexture(gEditorFilePreferences.filesBackup);

	// Save vegetation
	if (GetIEditorImpl()->GetVegetationMap())
		GetIEditorImpl()->GetVegetationMap()->Save(gEditorFilePreferences.filesBackup);

	if (GetIEditorImpl()->GetGameEngine()->GetIEditorGame())
		GetIEditorImpl()->GetGameEngine()->GetIEditorGame()->OnSaveLevel();

	// Commit changes to the disk.
	_flushall();
	return true;
}

bool CCryEditDoc::LoadLevel(TDocMultiArchive& arrXmlAr, const string& filename)
{
	LOADING_TIME_PROFILE_SECTION;
	GetISystem()->SetSystemGlobalState(ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_START_PREPARE);

	ICryPak* pIPak = GetIEditorImpl()->GetSystem()->GetIPak();

	string levelFolder = PathUtil::GetPathWithoutFilename(filename);

	string levelPath = levelFolder;
	string pszRelativePath = PathUtil::AbsolutePathToGamePath(levelPath.GetString());
	if (pszRelativePath[0] != '.') // if the level file is in the root path of mastercd.
		levelPath = PathUtil::GamePathToCryPakPath(pszRelativePath.GetString());

	GetIEditorImpl()->GetGameEngine()->SetLevelPath(levelPath);
	OnStartLevelResourceList();

	// Load next level resource list.
	pIPak->GetResourceList(ICryPak::RFOM_NextLevel)->Load(PathUtil::Make(levelFolder, "resourcelist.txt"));
	GetIEditorImpl()->Notify(eNotify_OnBeginLoad);
	DeleteContents();
	SetModifiedFlag(TRUE);  // dirty during de-serialize
	Load(arrXmlAr, filename);

	GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_END, 0, 0);
	// We don't need next level resource list anymore.
	pIPak->GetResourceList(ICryPak::RFOM_NextLevel)->Clear();
	SetModifiedFlag(FALSE); // start off with unmodified
	SetDocumentReady(true);
	GetIEditorImpl()->Notify(eNotify_OnEndLoad);

	GetISystem()->SetSystemGlobalState(ESYSTEM_GLOBAL_STATE_RUNNING);

	return true;
}

QString CCryEditDoc::GetLastLoadedLevelName()
{
	const string settingsPath = string(GetIEditorImpl()->GetUserFolder()) + kLastLoadPathFilename;
	if (!CFileUtil::FileExists(settingsPath))
		return "";

	XmlNodeRef lastUsedLevelPathNode = XmlHelpers::LoadXmlFromFile(settingsPath);
	if (lastUsedLevelPathNode == nullptr)
		return "";

	QString lastLoadedLevel = lastUsedLevelPathNode->getContent();

	// Validate that the last loaded level was used within this project
	LevelFileUtils::AbsolutePath basePath = LevelFileUtils::GetUserBasePath();
	if (!lastLoadedLevel.startsWith(basePath))
	{
		return "";
	}

	return lastLoadedLevel;
}

void CCryEditDoc::SetLastLoadedLevelName(const char* lastLoadedFileName)
{
	XmlNodeRef lastUsedLevelPathNode = XmlHelpers::CreateXmlNode("lastusedlevelpath");
	lastUsedLevelPathNode->setContent(lastLoadedFileName);

	const string settingsPath = string(GetIEditorImpl()->GetUserFolder()) + kLastLoadPathFilename;
	XmlHelpers::SaveXmlNode(lastUsedLevelPathNode, settingsPath);
}

//////////////////////////////////////////////////////////////////////////
namespace {
struct SFolderTime
{
	string folder;
	time_t  creationTime;
};

bool SortByCreationTime(SFolderTime& a, SFolderTime& b)
{
	return a.creationTime < b.creationTime;
}
}

void CCryEditDoc::SaveAutoBackup(bool bForce)
{
	if (!bForce && (!gEditorFilePreferences.autoSaveEnabled || GetIEditorImpl()->IsInGameMode()))
		return;

	string levelPath = GetIEditorImpl()->GetGameEngine()->GetLevelPath();
	if (levelPath.IsEmpty())
		return;

	static bool isInProgress = false;
	if (isInProgress)
		return;

	isInProgress = true;

	CWaitCursor wait;

	string autoBackupPath = levelPath + "/" + kAutoBackupFolder;

	// collect all subfolders
	std::vector<SFolderTime> folders;

	_finddata_t fileinfo;
	intptr_t handle = gEnv->pCryPak->FindFirst(autoBackupPath + "/*.*", &fileinfo);
	if (handle != -1)
	{
		do
		{
			if (fileinfo.name[0] == '.')
				continue;

			if (fileinfo.attrib & _A_SUBDIR)
			{
				SFolderTime ft;
				ft.folder = fileinfo.name;
				ft.creationTime = fileinfo.time_create;
				folders.push_back(ft);
			}

		}
		while (gEnv->pCryPak->FindNext(handle, &fileinfo) != -1);
	}

	// remove old subfolders
	std::sort(folders.begin(), folders.end(), SortByCreationTime);
	for (int i = int(folders.size()) - gEditorFilePreferences.autoSaveMaxCount; i >= 0; --i)
	{
		CFileUtil::Deltree(autoBackupPath + "/" + folders[i].folder + "/", true);
	}

	// save new backup
	CTime theTime = CTime::GetCurrentTime();
	string subFolder = theTime.Format("%Y.%m.%d [%H-%M.%S]");

	string levelName = GetIEditorImpl()->GetGameEngine()->GetLevelName();

	string filename = string().Format( "%s/%s/%s/%s.%s", 
		autoBackupPath.c_str(), 
		subFolder.c_str(), 
		levelName.c_str(), 
		levelName.c_str(), 
		CLevelType::GetFileExtensionStatic());

	SaveLevel(filename);
	GetIEditorImpl()->GetGameEngine()->SetLevelPath(levelPath);

	isInProgress = false;
}

CMission* CCryEditDoc::GetCurrentMission(bool bSkipLoadingAIWhenSyncingContent /* = false */)
{
	LOADING_TIME_PROFILE_SECTION;
	if (m_mission)
	{
		return m_mission;
	}

	if (!m_missions.empty())
	{
		// Choose first available mission.
		SetCurrentMission(m_missions[0]);
		return m_mission;
	}

	// Create initial mission.
	m_mission = new CMission(this);
	m_mission->SetName("Mission0");
	AddMission(m_mission);
	m_mission->SyncContent(true, true, bSkipLoadingAIWhenSyncingContent);
	return m_mission;
}

void CCryEditDoc::SetCurrentMission(CMission* mission)
{
	if (mission != m_mission)
	{
		CWaitCursor wait;

		if (m_mission)
			m_mission->SyncContent(false, false);

		m_mission = mission;
		m_mission->SyncContent(true, false);

		GetIEditorImpl()->GetGameEngine()->LoadMission(m_mission->GetName().GetString());
	}
}

void CCryEditDoc::ClearMissions()
{
	for (int i = 0; i < m_missions.size(); i++)
	{
		delete m_missions[i];
	}

	m_missions.clear();
	m_mission = 0;
}

bool CCryEditDoc::IsLevelExported() const
{
	return m_boLevelExported;
}

void CCryEditDoc::SetLevelExported(bool boExported)
{
	m_boLevelExported = boExported;
}

CMission* CCryEditDoc::FindMission(const string& name) const
{
	for (int i = 0; i < m_missions.size(); i++)
	{
		if (stricmp(name, m_missions[i]->GetName()) == 0)
			return m_missions[i];
	}
	return 0;
}

void CCryEditDoc::AddMission(CMission* mission)
{
	ASSERT(std::find(m_missions.begin(), m_missions.end(), mission) == m_missions.end());
	m_missions.push_back(mission);
	GetIEditorImpl()->Notify(eNotify_OnInvalidateControls);
}

void CCryEditDoc::RemoveMission(CMission* mission)
{
	// if deleting current mission.
	if (mission == m_mission)
	{
		m_mission = 0;
	}

	m_missions.erase(std::find(m_missions.begin(), m_missions.end(), mission));
	GetIEditorImpl()->Notify(eNotify_OnInvalidateControls);
}

LightingSettings* CCryEditDoc::GetLighting()
{
	return GetCurrentMission()->GetLighting();
}

void CCryEditDoc::LogLoadTime(int time)
{
	char szExeFileName[_MAX_PATH];
	GetModuleFileName(CryGetCurrentModule(), szExeFileName, sizeof(szExeFileName));
	string exePath = PathUtil::GetPathWithoutFilename(szExeFileName);
	string filename = PathUtil::Make(exePath, "LevelLoadTime.log");
	string level = GetIEditorImpl()->GetGameEngine()->GetLevelPath();

	CryLog("[LevelLoadTime] Level %s loaded in %d seconds", (const char*)level, time / 1000);
	SetFileAttributes(filename, FILE_ATTRIBUTE_ARCHIVE);

	FILE* file = fopen(filename, "at");

	if (file)
	{
		string version = GetIEditorImpl()->GetFileVersion().ToString();
		string text;

		time = time / 1000;
		text.Format("\n[%s] Level %s loaded in %d seconds", (const char*)version, (const char*)level, time);
		fwrite((const char*)text, text.GetLength(), 1, file);
		fclose(file);
	}
}

void CCryEditDoc::SetDocumentReady(bool bReady)
{
	m_bDocumentReady = bReady;
}

void CCryEditDoc::GetMemoryUsage(ICrySizer* pSizer)
{
	pSizer->Add(*this);
	GetIEditorImpl()->GetTerrainManager()->GetTerrainMemoryUsage(pSizer);
}

void CCryEditDoc::RegisterConsoleVariables()
{
	doc_validate_surface_types = gEnv->pConsole->GetCVar("doc_validate_surface_types");

	if (!doc_validate_surface_types)
	{
		doc_validate_surface_types = REGISTER_INT_CB("doc_validate_surface_types", 0, 0,
		                                             "Flag indicating whether icons are displayed on the animation graph.\n"
		                                             "Default is 1.\n",
		                                             OnValidateSurfaceTypesChanged);
	}
}

void CCryEditDoc::OnValidateSurfaceTypesChanged(ICVar*)
{
	CSurfaceTypeValidator().Validate();
}

void CCryEditDoc::OnStartLevelResourceList()
{
	// after loading another level we clear the RFOM_Level list, the first time the list should be empty
	static bool bFirstTime = true;

	if (bFirstTime)
	{
		const char* pResFilename = gEnv->pCryPak->GetResourceList(ICryPak::RFOM_Level)->GetFirst();

		while (pResFilename)
		{
			// This should be fixed because ExecuteCommandLine is executed right after engine init as we assume the
			// engine already has all data loaded an is initialized to process commands. Loading data afterwards means
			// some init was done later which can cause problems when running in the engine batch mode (executing console commands).
			gEnv->pLog->LogError("'%s' was loaded after engine init but before level load/new (should be fixed)", pResFilename);
			pResFilename = gEnv->pCryPak->GetResourceList(ICryPak::RFOM_Level)->GetNext();
		}

		bFirstTime = false;
	}

	gEnv->pCryPak->GetResourceList(ICryPak::RFOM_Level)->Clear();
}

void CCryEditDoc::ForceSkyUpdate()
{
	LOADING_TIME_PROFILE_SECTION;
	ITimeOfDay* pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
	CMission* pCurMission = GetIEditorImpl()->GetDocument()->GetCurrentMission();

	if (pTimeOfDay && pCurMission)
	{
		pTimeOfDay->SetTime(pCurMission->GetTime(), gLightingPreferences.bForceSkyUpdate);
		pCurMission->SetTime(pCurMission->GetTime());
		GetIEditorImpl()->Notify(eNotify_OnTimeOfDayChange);
	}
}

BOOL CCryEditDoc::DoFileSave()
{
	if (!IsDocumentReady())
		return FALSE;

	if (!GetIEditorImpl()->GetLevelIndependentFileMan()->PromptChangedFiles())
	{
		CLogFile::WriteLine("$8Level saving aborted due to user input. Please re-save your changes.");
		return FALSE;
	}

	if (GetIEditorImpl()->GetCommandManager()->Execute("general.save_level") == "true")
		return TRUE;
	else
		return FALSE;
}

void CCryEditDoc::InitEmptyLevel(int resolution, float unitSize, bool bUseTerrain)
{
	GetISystem()->SetSystemGlobalState(ESYSTEM_GLOBAL_STATE_LEVEL_LOAD_START_PREPARE);

	OnStartLevelResourceList();

	GetIEditorImpl()->Notify(eNotify_OnBeginNewScene);
	CLogFile::WriteLine("Preparing new document...");

	////////////////////////////////////////////////////////////////////////
	// Reset heightmap (water level, etc) to default
	////////////////////////////////////////////////////////////////////////
	GetIEditorImpl()->GetTerrainManager()->ResetHeightMap();

	// If possible set terrain to correct size here, this will help with initial camera placement in new levels
	if (bUseTerrain)
		GetIEditorImpl()->GetTerrainManager()->SetTerrainSize(resolution, unitSize);

	////////////////////////////////////////////////////////////////////////
	// Reset the terrain texture of the top render window
	////////////////////////////////////////////////////////////////////////

	//cleanup resources!
	GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_POST_UNLOAD, 0, 0);

	//////////////////////////////////////////////////////////////////////////
	// Initialize defaults.
	//////////////////////////////////////////////////////////////////////////
	if (!GetIEditorImpl()->IsInPreviewMode())
	{
		GetIEditorImpl()->GetTerrainManager()->CreateDefaultLayer();

		// Make new mission.
		GetIEditorImpl()->ReloadTemplates();
		m_environmentTemplate = GetIEditorImpl()->FindTemplate("Environment");

		GetCurrentMission(true);  // true = skip loading the AI in case the content needs to get synchronized (otherwise it would attempt to load AI stuff from the previously loaded level (!) which might give confusing warnings)
		GetIEditorImpl()->GetGameEngine()->SetMissionName(GetCurrentMission()->GetName().GetString());
		GetIEditorImpl()->GetGameEngine()->SetLevelCreated(true);
		GetIEditorImpl()->GetGameEngine()->ReloadEnvironment();
		GetIEditorImpl()->GetGameEngine()->SetLevelCreated(false);

		// Default time of day.
		XmlNodeRef root = GetISystem()->LoadXmlFromFile("%EDITOR%/default_time_of_day.xml");
		if (root)
		{
			ITimeOfDay* pTimeOfDay = gEnv->p3DEngine->GetTimeOfDay();
			pTimeOfDay->Serialize(root, true);
			pTimeOfDay->SetTime(12.0f, true);  // Set to 12:00.
		}
	}

	auto* pObjectLayerManager = GetIEditorImpl()->GetObjectManager()->GetLayersManager();
	CObjectLayer* pMainLayer = pObjectLayerManager->CreateLayer("Main");
	if (bUseTerrain)
	{
		CObjectLayer* pTerrainLayer = pObjectLayerManager->CreateLayer("Terrain", eObjectLayerType_Terrain);
	}

	GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_END, 0, 0);

	GetIEditorImpl()->Notify(eNotify_OnEndNewScene);
	SetModifiedFlag(FALSE);
	SetLevelExported(false);

	GetISystem()->SetSystemGlobalState(ESYSTEM_GLOBAL_STATE_RUNNING);
}

void CCryEditDoc::OnEnvironmentPropertyChanged(IVariable* pVar)
{
	if (pVar == NULL)
		return;

	XmlNodeRef node = GetEnvironmentTemplate();
	if (node == NULL)
		return;

	int nKey = (int)(intptr_t)pVar->GetUserData();

	int nGroup = (nKey & 0xFFFF0000) >> 16;
	int nChild = (nKey & 0x0000FFFF);

	if (nGroup < 0 || nGroup >= node->getChildCount())
		return;

	XmlNodeRef groupNode = node->getChild(nGroup);

	if (groupNode == NULL)
		return;

	if (nChild < 0 || nChild >= groupNode->getChildCount())
		return;

	XmlNodeRef childNode = groupNode->getChild(nChild);
	if (childNode == NULL)
		return;

	if (pVar->GetDataType() == IVariable::DT_COLOR)
	{
		Vec3 value;
		pVar->Get(value);
		string buff;
		COLORREF gammaColor = CMFCUtils::ColorLinearToGamma(ColorF(value.x, value.y, value.z));
		buff.Format("%d,%d,%d", GetRValue(gammaColor), GetGValue(gammaColor), GetBValue(gammaColor));
		childNode->setAttr("value", buff);
	}
	else
	{
		string value;
		pVar->Get(value);
		childNode->setAttr("value", value);
	}

	if (!IsLevelBeingLoaded())
	{
		GetIEditorImpl()->GetGameEngine()->ReloadEnvironment();
	}
}

string CCryEditDoc::GetCryIndexPath(const LPCTSTR levelFilePath)
{
	string levelPath = PathUtil::GetPathWithoutFilename(levelFilePath);
	string levelName = PathUtil::GetFileName(levelFilePath);
	return PathUtil::AddBackslash(levelPath + levelName + "_editor").c_str();
}

BOOL CCryEditDoc::LoadXmlArchiveArray(TDocMultiArchive& arrXmlAr, const string& relativeLevelName, const string& levelPath)
{
	ICryPak* pIPak = GetIEditorImpl()->GetSystem()->GetIPak();

	//if (m_pSWDoc->IsNull())
	{
		CXmlArchive* pXmlAr = new CXmlArchive();
		if (!pXmlAr)
			return FALSE;

		CXmlArchive& xmlAr = *pXmlAr;
		xmlAr.bLoading = true;
		if (!xmlAr.LoadFromFile(relativeLevelName))
		{
			return FALSE;
		}

		FillXmlArArray(arrXmlAr, &xmlAr);
	}

	return TRUE;
}

void CCryEditDoc::ReleaseXmlArchiveArray(TDocMultiArchive& arrXmlAr)
{
	SAFE_DELETE(arrXmlAr[0]);
}

void CCryEditDoc::SyncCurrentMissionContent(bool bRetrieve)
{
	GetCurrentMission()->SyncContent(bRetrieve, false);
}

void CCryEditDoc::SetModifiedFlag(bool bModified)
{
	m_bModified = bModified;
}

bool CCryEditDoc::IsModified() const
{
	return m_bModified;
}

string CCryEditDoc::GetTitle() const
{
	return PathUtil::GetFileName(m_pathName).c_str();
}

void CCryEditDoc::SetPathName(LPCTSTR lpszPathName, BOOL bAddToMRU /*= TRUE*/)
{
	m_pathName = lpszPathName;
}

namespace
{
bool PySaveLevel()
{
	if (!GetIEditorImpl()->GetDocument()->DoSave(GetIEditorImpl()->GetDocument()->GetPathName(), TRUE))
	{
		TRACE(traceAppMsg, 0, "Warning: File save failed.\n");
		return false;
	}

	return true;
}
}

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PySaveLevel, general, save_level,
                                     "Saves the current level.",
                                     "general.save_level()");

