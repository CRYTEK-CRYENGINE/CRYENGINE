// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Mission.h"
#include "CryEditDoc.h"
#include "TerrainLighting.h"
#include "GameEngine.h"
#include "MissionScript.h"
#include <CryMovie/IMovieSystem.h>
#include "Vegetation/VegetationMap.h"
#include <Preferences/GeneralPreferences.h>

#include "Objects\EntityObject.h"
#include "Objects\EntityScript.h"

#include <Cry3DEngine/ITimeOfDay.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryAISystem/IAISystem.h>
#include <CryGame/IGameFramework.h>
#include <CryGame/IGame.h>


namespace
{
const char* kTimeOfDayFile = "TimeOfDay.xml";
const char* kTimeOfDayRoot = "TimeOfDay";
const char* kEnvironmentFile = "Environment.xml";
const char* kEnvironmentRoot = "Environment";
};

//////////////////////////////////////////////////////////////////////////
CMission::CMission(CCryEditDoc* doc)
{
	m_doc = doc;
	m_objects = XmlHelpers::CreateXmlNode("Objects");
	m_layers = XmlHelpers::CreateXmlNode("ObjectLayers");
	//m_exportData = XmlNodeRef( "ExportData" );
	m_weaponsAmmo = XmlHelpers::CreateXmlNode("Ammo");
	m_timeOfDay = XmlHelpers::CreateXmlNode("TimeOfDay");
	m_environment = XmlHelpers::CreateXmlNode("Environment");
	CXmlTemplate::SetValues(m_doc->GetEnvironmentTemplate(), m_environment);

	m_time = 12; // 12 PM by default.

	m_lighting = new LightingSettings();      // default values are set in the constructor

	m_pScript = new CMissionScript();
	m_numCGFObjects = 0;

	m_minimap.vCenter = Vec2(512, 512);
	m_minimap.vExtends = Vec2(512, 512);
	m_minimap.textureWidth = m_minimap.textureHeight = 1024;
	m_minimap.orientation = 1;
}

//////////////////////////////////////////////////////////////////////////
CMission::~CMission()
{
	delete m_lighting;
	delete m_pScript;
}

//////////////////////////////////////////////////////////////////////////
CMission* CMission::Clone()
{
	CMission* m = new CMission(m_doc);
	m->SetName(m_name);
	m->SetDescription(m_description);
	m->m_objects = m_objects->clone();
	m->m_layers = m_layers->clone();
	m->m_environment = m_environment->clone();
	m->m_time = m_time;
	return m;
}

//////////////////////////////////////////////////////////////////////////
void CMission::Serialize(CXmlArchive& ar, bool bParts)
{
	if (ar.bLoading)
	{
		// Load.
		ar.root->getAttr("Name", m_name);
		ar.root->getAttr("Description", m_description);

		//time_t time = 0;
		//ar.root->getAttr( "Time",time );
		//m_time = time;

		m_sPlayerEquipPack = "";
		ar.root->getAttr("PlayerEquipPack", m_sPlayerEquipPack);
		CString sScript = "";
		ar.root->getAttr("Script", sScript);
		m_pScript->SetScriptFile(sScript);

		XmlNodeRef objects = ar.root->findChild("Objects");
		if (objects)
			m_objects = objects;

		XmlNodeRef layers = ar.root->findChild("ObjectLayers");
		if (layers)
			m_layers = layers;

		SerializeTimeOfDay(ar);

		m_Animations = ar.root->findChild("MovieData");

		/*
		   XmlNodeRef expData = ar.root->findChild( "ExportData" );
		   if (expData)
		   {
		   m_exportData = expData;
		   }
		 */
		SerializeEnvironment(ar);

		m_lighting->Serialize(ar.root, ar.bLoading);

		m_usedWeapons.clear();

		XmlNodeRef weapons = ar.root->findChild("Weapons");
		if (weapons)
		{
			XmlNodeRef usedWeapons = weapons->findChild("Used");
			if (usedWeapons)
			{
				CString weaponName;
				for (int i = 0; i < usedWeapons->getChildCount(); i++)
				{
					XmlNodeRef weapon = usedWeapons->getChild(i);
					if (weapon->getAttr("Name", weaponName))
						m_usedWeapons.push_back(weaponName);
				}
			}
			XmlNodeRef ammo = weapons->findChild("Ammo");
			if (ammo)
				m_weaponsAmmo = ammo->clone();
		}

		XmlNodeRef minimapNode = ar.root->findChild("MiniMap");
		if (minimapNode)
		{
			minimapNode->getAttr("CenterX", m_minimap.vCenter.x);
			minimapNode->getAttr("CenterY", m_minimap.vCenter.y);
			minimapNode->getAttr("ExtendsX", m_minimap.vExtends.x);
			minimapNode->getAttr("ExtendsY", m_minimap.vExtends.y);
			//			minimapNode->getAttr( "CameraHeight",m_minimap.cameraHeight );
			minimapNode->getAttr("TexWidth", m_minimap.textureWidth);
			minimapNode->getAttr("TexHeight", m_minimap.textureHeight);
		}
	}
	else
	{
		ar.root->setAttr("Name", m_name);
		ar.root->setAttr("Description", m_description);

		//time_t time = m_time.GetTime();
		//ar.root->setAttr( "Time",time );

		ar.root->setAttr("PlayerEquipPack", m_sPlayerEquipPack);
		ar.root->setAttr("Script", m_pScript->GetFilename());

		CString timeStr;
		int nHour = floor(m_time);
		int nMins = (m_time - floor(m_time)) * 60.0f;
		timeStr.Format("%.2d:%.2d", nHour, nMins);
		ar.root->setAttr("MissionTime", timeStr);

		// Saving.
		XmlNodeRef layers = m_layers->clone();
		layers->setTag("ObjectLayers");
		ar.root->addChild(layers);

		///XmlNodeRef objects = m_objects->clone();
		m_objects->setTag("Objects");
		ar.root->addChild(m_objects);

		if (bParts)
		{
			SerializeTimeOfDay(ar);
			SerializeEnvironment(ar);
		}

		m_lighting->Serialize(ar.root, ar.bLoading);

		XmlNodeRef weapons = ar.root->newChild("Weapons");
		XmlNodeRef usedWeapons = weapons->newChild("Used");
		for (int i = 0; i < m_usedWeapons.size(); i++)
		{
			XmlNodeRef weapon = usedWeapons->newChild("Weapon");
			weapon->setAttr("Name", m_usedWeapons[i]);
		}
		weapons->addChild(m_weaponsAmmo->clone());

		XmlNodeRef minimapNode = ar.root->newChild("MiniMap");
		minimapNode->setAttr("CenterX", m_minimap.vCenter.x);
		minimapNode->setAttr("CenterY", m_minimap.vCenter.y);
		minimapNode->setAttr("ExtendsX", m_minimap.vExtends.x);
		minimapNode->setAttr("ExtendsY", m_minimap.vExtends.y);
		//		minimapNode->setAttr( "CameraHeight",m_minimap.cameraHeight );
		minimapNode->setAttr("TexWidth", m_minimap.textureWidth);
		minimapNode->setAttr("TexHeight", m_minimap.textureHeight);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMission::Export(XmlNodeRef& root, XmlNodeRef& objectsNode)
{
	// Also save exported objects data.
	root->setAttr("Name", m_name);
	root->setAttr("Description", m_description);

	CString timeStr;
	int nHour = floor(m_time);
	int nMins = (m_time - floor(m_time)) * 60.0f;
	timeStr.Format("%.2d:%.2d", nHour, nMins);
	root->setAttr("Time", timeStr);

	root->setAttr("PlayerEquipPack", m_sPlayerEquipPack);
	root->setAttr("Script", m_pScript->GetFilename());

	// Saving.
	//XmlNodeRef objects = m_exportData->clone();
	//objects->setTag( "Objects" );
	//root->addChild( objects );

	XmlNodeRef envNode = m_environment->clone();
	m_lighting->Serialize(envNode, false);
	root->addChild(envNode);

	m_timeOfDay->setAttr("Time", m_time);
	root->addChild(m_timeOfDay);

	XmlNodeRef minimapNode = root->newChild("MiniMap");
	minimapNode->setAttr("CenterX", m_minimap.vCenter.x);
	minimapNode->setAttr("CenterY", m_minimap.vCenter.y);
	minimapNode->setAttr("ExtendsX", m_minimap.vExtends.x);
	minimapNode->setAttr("ExtendsY", m_minimap.vExtends.y);
	//	minimapNode->setAttr( "CameraHeight",m_minimap.cameraHeight );
	minimapNode->setAttr("TexWidth", m_minimap.textureWidth);
	minimapNode->setAttr("TexHeight", m_minimap.textureHeight);

	XmlNodeRef weapons = root->newChild("Weapons");
	XmlNodeRef usedWeapons = weapons->newChild("Used");
	for (int i = 0; i < m_usedWeapons.size(); i++)
	{
		XmlNodeRef weapon = usedWeapons->newChild("Weapon");
		weapon->setAttr("Name", m_usedWeapons[i]);
		IEntity* pEnt = GetIEditorImpl()->GetSystem()->GetIEntitySystem()->FindEntityByName(m_usedWeapons[i]);
		if (pEnt)
			weapon->setAttr("id", pEnt->GetId());
	}
	weapons->addChild(m_weaponsAmmo->clone());

	IObjectManager* pObjMan = GetIEditorImpl()->GetObjectManager();

	//////////////////////////////////////////////////////////////////////////
	// Serialize objects.
	//////////////////////////////////////////////////////////////////////////
	char path[1024];
	cry_strcpy(path, m_doc->GetPathName());
	PathRemoveFileSpec(path);
	PathAddBackslash(path);

	objectsNode = root->newChild("Objects");
	pObjMan->Export(path, objectsNode, true);  // Export shared.
	pObjMan->Export(path, objectsNode, false); // Export not shared.
}

//////////////////////////////////////////////////////////////////////////
void CMission::ExportAnimations(XmlNodeRef& root)
{
	XmlNodeRef mission = XmlHelpers::CreateXmlNode("Mission");
	mission->setAttr("Name", m_name);

	XmlNodeRef AnimNode = XmlHelpers::CreateXmlNode("MovieData");
	GetIEditorImpl()->GetMovieSystem()->Serialize(AnimNode, false);

	for (int i = 0; i < AnimNode->getChildCount(); i++)
	{
		mission->addChild(AnimNode->getChild(i));
	}
	root->addChild(mission);
}

//////////////////////////////////////////////////////////////////////////
void CMission::SyncContent(bool bRetrieve, bool bIgnoreObjects, bool bSkipLoadingAI /* = false */)
{
	LOADING_TIME_PROFILE_SECTION;

	// Save data from current Document to Mission.
	IObjectManager* objMan = GetIEditorImpl()->GetObjectManager();
	if (bRetrieve)
	{
		// Activating this mission.
		CGameEngine* gameEngine = GetIEditorImpl()->GetGameEngine();

		// Load mission script.
		if (m_pScript)
			m_pScript->Load();

		// - have the AI system load its data from the .bai files *before* deserializing the objects
		// - if the .bai files were to be loaded *afterwards*, they would clear whatever objects were propagated to the AI system
		// - => as a result, things like AI shapes would show up in the editor nicely, but would not be known on the AI system side, until grabbing or changing those shapes (which is when they get re-registered in the AI system)
		if (!bSkipLoadingAI)
			gameEngine->LoadAI(gameEngine->GetLevelPath(), GetName().GetString());

		if (auto* pGame = gEnv->pGameFramework->GetIGame())
		{
			pGame->LoadExportedLevelDataInEditor(gameEngine->GetLevelPath(), GetName().GetString());
		}

		if (!bIgnoreObjects)
		{
			// Retrieve data from Mission and put to document.
			XmlNodeRef root = XmlHelpers::CreateXmlNode("Root");
			root->addChild(m_objects);
			root->addChild(m_layers);
			objMan->Serialize(root, true, SERIALIZE_ONLY_NOTSHARED);
		}

		m_doc->GetFogTemplate() = m_environment;

		CXmlTemplate::GetValues(m_doc->GetEnvironmentTemplate(), m_environment);

		UpdateUsedWeapons();
		gameEngine->SetPlayerEquipPack(m_sPlayerEquipPack);

		GetIEditorImpl()->GetSystem()->GetI3DEngine()->CompleteObjectsGeometry();

		// refresh positions of vegetation objects since voxel mesh is defined only now
		if (CVegetationMap* pVegetationMap = GetIEditorImpl()->GetVegetationMap())
			pVegetationMap->OnHeightMapChanged();

		if (m_Animations)
			GetIEditorImpl()->GetMovieSystem()->Serialize(m_Animations, true);

		objMan->SendEvent(EVENT_MISSION_CHANGE);
		m_doc->ChangeMission();

		if (!bSkipLoadingAI)
			gameEngine->FinishLoadAI();

		if (gEnv->pAISystem)
			gEnv->pAISystem->OnMissionLoaded();

		if (GetIEditorImpl()->Get3DEngine())
			m_numCGFObjects = GetIEditorImpl()->Get3DEngine()->GetLoadedObjectCount();

		// Load time of day.
		GetIEditorImpl()->Get3DEngine()->GetTimeOfDay()->Serialize(m_timeOfDay, true);
		gameEngine->ReloadEnvironment();
	}
	else
	{
		// Save time of day.
		m_timeOfDay = XmlHelpers::CreateXmlNode("TimeOfDay");
		GetIEditorImpl()->Get3DEngine()->GetTimeOfDay()->Serialize(m_timeOfDay, false);

		if (!bIgnoreObjects)
		{
			XmlNodeRef root = XmlHelpers::CreateXmlNode("Root");
			objMan->Serialize(root, false, SERIALIZE_ONLY_NOTSHARED);
			m_objects = root->findChild("Objects");
			XmlNodeRef layers = root->findChild("ObjectLayers");
			if (layers)
				m_layers = layers;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMission::OnEnvironmentChange()
{
	m_environment = XmlHelpers::CreateXmlNode("Environment");
	CXmlTemplate::SetValues(m_doc->GetEnvironmentTemplate(), m_environment);
}

//////////////////////////////////////////////////////////////////////////
void CMission::GetUsedWeapons(std::vector<CString>& weapons)
{
	weapons = m_usedWeapons;
}

//////////////////////////////////////////////////////////////////////////
void CMission::SetUsedWeapons(const std::vector<CString>& weapons)
{
	m_usedWeapons = weapons;
	UpdateUsedWeapons();
}

//////////////////////////////////////////////////////////////////////////
void CMission::UpdateUsedWeapons()
{

}

//////////////////////////////////////////////////////////////////////////
void CMission::ResetScript()
{
	if (m_pScript)
	{
		m_pScript->OnReset();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMission::AddObjectsNode(XmlNodeRef& node)
{
	for (int i = 0; i < node->getChildCount(); i++)
	{
		m_objects->addChild(node->getChild(i)->clone());
	}
}

//////////////////////////////////////////////////////////////////////////
void CMission::SetLayersNode(XmlNodeRef& node)
{
	m_layers = node->clone();
}

//////////////////////////////////////////////////////////////////////////
void CMission::SetMinimap(const SMinimapInfo& minimap)
{
	m_minimap = minimap;
}

//////////////////////////////////////////////////////////////////////////
void CMission::SaveParts()
{
	// Save Time of Day
	{
		CTempFileHelper helper(GetIEditorImpl()->GetLevelDataFolder() + kTimeOfDayFile);

		m_timeOfDay->saveToFile(helper.GetTempFilePath());

		if (!helper.UpdateFile(gEditorFilePreferences.filesBackup))
			return;
	}

	// Save Environment
	{
		CTempFileHelper helper(GetIEditorImpl()->GetLevelDataFolder() + kEnvironmentFile);

		XmlNodeRef root = m_environment->clone();
		root->setTag(kEnvironmentRoot);
		root->saveToFile(helper.GetTempFilePath());

		if (!helper.UpdateFile(gEditorFilePreferences.filesBackup))
			return;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMission::LoadParts()
{
	// Load Time of Day
	{
		CString filename = GetIEditorImpl()->GetLevelDataFolder() + kTimeOfDayFile;
		XmlNodeRef root = XmlHelpers::LoadXmlFromFile(filename);
		if (root && !stricmp(root->getTag(), kTimeOfDayRoot))
		{
			m_timeOfDay = root;
			m_timeOfDay->getAttr("Time", m_time);
		}
	}

	// Load Environment
	{
		CString filename = GetIEditorImpl()->GetLevelDataFolder() + kEnvironmentFile;
		XmlNodeRef root = XmlHelpers::LoadXmlFromFile(filename);
		if (root && !stricmp(root->getTag(), kEnvironmentRoot))
		{
			m_environment = root;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMission::SerializeTimeOfDay(CXmlArchive& ar)
{
	if (ar.bLoading)
	{
		XmlNodeRef todNode = ar.root->findChild("TimeOfDay");
		if (todNode)
		{
			m_timeOfDay = todNode;
			todNode->getAttr("Time", m_time);
		}
		else
			m_timeOfDay = XmlHelpers::CreateXmlNode("TimeOfDay");
	}
	else
	{
		m_timeOfDay->setAttr("Time", m_time);
		ar.root->addChild(m_timeOfDay);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMission::SerializeEnvironment(CXmlArchive& ar)
{
	if (ar.bLoading)
	{
		XmlNodeRef env = ar.root->findChild("Environment");
		if (env)
		{
			m_environment = env;
		}
	}
	else
	{
		XmlNodeRef env = m_environment->clone();
		env->setTag("Environment");
		ar.root->addChild(env);
	}
}

//////////////////////////////////////////////////////////////////////////
const SMinimapInfo& CMission::GetMinimap() const
{
	return m_minimap;
}

