// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id: RealtimeRemoteUpdate.cpp,v 1.1 2009/01/03 10:45:15 Paulo Zaffari Exp wwwrun $
   $DateTime$
   Description:	This is the source file for the module Realtime remote
          update. The purpose of this module is to allow data update to happen
          remotely so that you can, for example, edit the terrain and see the changes
          in the console.
   -------------------------------------------------------------------------
   History:
   - 03:01:2009   10:45: Created by Paulo Zaffari
   - 23:09:2009   10:39: Merged c2 version to main and moved to the engine by Johnmichael Quinlan
*************************************************************************/

#include "StdAfx.h"
#include "RealtimeRemoteUpdate.h"
#include <CrySystem/ISystem.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CryEntitySystem/IEntitySystem.h>
#include "IViewSystem.h"
#include <CryEntitySystem/IEntitySystem.h>
#include <CryGame/IGameFramework.h>
#include "IGameRulesSystem.h"
#include <Cry3DEngine/ITimeOfDay.h>

// Should CERTAINLY be moved to CryCommon.
template<typename TObjectType, bool bArray = false>
class TScopedPointer
{
public:
	TScopedPointer(TObjectType* pPointer) : m_pPointer(pPointer){}

	~TScopedPointer()
	{
		if (bArray)
		{
			SAFE_DELETE_ARRAY(m_pPointer);
		}
		else
		{
			SAFE_DELETE(m_pPointer);
		}
	}

protected:

	TObjectType* m_pPointer;
};

//////////////////////////////////////////////////////////////////////////
CRealtimeRemoteUpdateListener& CRealtimeRemoteUpdateListener::GetRealtimeRemoteUpdateListener()
{
	static CRealtimeRemoteUpdateListener oRealtimeUpdateListener;
	return oRealtimeUpdateListener;
}
//////////////////////////////////////////////////////////////////////////
bool CRealtimeRemoteUpdateListener::Enable(bool boEnable)
{
	if (!gEnv)
	{
		return false;
	}

	if (!gEnv->pSystem)
	{
		return false;
	}

	INotificationNetwork* piNotificationNetwork = gEnv->pSystem->GetINotificationNetwork();
	if (!piNotificationNetwork)
	{
		return false;
	}

	if (boEnable)
	{
		m_boIsEnabled = piNotificationNetwork->ListenerBind("RealtimeUpdate", this);
	}
	else
	{
		piNotificationNetwork->ListenerRemove(this);
		m_boIsEnabled = false;
	}

	return m_boIsEnabled;
}
//////////////////////////////////////////////////////////////////////////
bool CRealtimeRemoteUpdateListener::IsEnabled()
{
	if (!gEnv)
	{
		return false;
	}

	if (!gEnv->pSystem)
	{
		return false;
	}

	INotificationNetwork* piNotificationNetwork = gEnv->pSystem->GetINotificationNetwork();
	if (!piNotificationNetwork)
	{
		return false;
	}

	// We should instead query the notification network here.
	return m_boIsEnabled;
}

//////////////////////////////////////////////////////////////////////////
void CRealtimeRemoteUpdateListener::AddGameHandler(IRealtimeUpdateGameHandler* handler)
{
	GameHandlerList::iterator item = m_gameHandlers.begin();
	GameHandlerList::iterator end = m_gameHandlers.end();

	for (; item != end; ++item)
	{
		if (handler == (*item))
		{
			return; //already present
		}
	}

	m_gameHandlers.push_back(handler);
}
//////////////////////////////////////////////////////////////////////////
void CRealtimeRemoteUpdateListener::RemoveGameHandler(IRealtimeUpdateGameHandler* handler)
{
	GameHandlerList::iterator item = m_gameHandlers.begin();
	GameHandlerList::iterator end = m_gameHandlers.end();
	for (; item != end; ++item)
	{
		if (handler == (*item))
		{
			break;
		}
	}

	m_gameHandlers.erase(item);
}

//////////////////////////////////////////////////////////////////////////
void CRealtimeRemoteUpdateListener::OnNotificationNetworkReceive(const void* pBuffer, size_t length)
{
	TDBuffer& rBuffer = *(new TDBuffer);

	rBuffer.resize(length, 0);
	memcpy(&rBuffer.front(), pBuffer, length);

	m_ProcessingQueue.push(&rBuffer);
}
//////////////////////////////////////////////////////////////////////////
CRealtimeRemoteUpdateListener::CRealtimeRemoteUpdateListener() :
	m_boIsEnabled(false), m_lastKeepAliveMessageTime((const int64)0)
{
}
//////////////////////////////////////////////////////////////////////////
CRealtimeRemoteUpdateListener::~CRealtimeRemoteUpdateListener()
{
}
//////////////////////////////////////////////////////////////////////////
void CRealtimeRemoteUpdateListener::LoadArchetypes(XmlNodeRef& root)
{
	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;

	// Remove Entities with ID`s from the list.
	for (int i = 0; i < root->getChildCount(); i++)
	{
		XmlNodeRef entityNode = root->getChild(i);
		if (entityNode->isTag("EntityPrototype"))
		{
			pEntitySystem->LoadEntityArchetype(entityNode);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CRealtimeRemoteUpdateListener::LoadTimeOfDay(XmlNodeRef& root)
{
	gEnv->p3DEngine->GetTimeOfDay()->Serialize(root, true);
	gEnv->p3DEngine->GetTimeOfDay()->Update(true, true);
}

//////////////////////////////////////////////////////////////////////////
void CRealtimeRemoteUpdateListener::LoadMaterials(XmlNodeRef& root)
{
	// Remove Entities with ID`s from the list.
	for (int i = 0; i < root->getChildCount(); i++)
	{
		XmlNodeRef mtlNode = root->getChild(i);
		if (mtlNode->isTag("Material"))
		{
			const char* mtlName = mtlNode->getAttr("name");
			gEnv->p3DEngine->GetMaterialManager()->LoadMaterialFromXml(mtlName, mtlNode);
		}
	}
}
//////////////////////////////////////////////////////////////////////////
void CRealtimeRemoteUpdateListener::LoadConsoleVariables(XmlNodeRef& root)
{
	IConsole* piConsole(NULL);
	char* szKey(NULL);
	char* szValue(NULL);
	ICVar* piCVar(NULL);

	piConsole = gEnv->pConsole;
	if (!piConsole)
	{
		return;
	}

	// Remove Entities with ID`s from the list.
	for (int i = 0; i < root->getNumAttributes(); ++i)
	{
		root->getAttributeByIndex(i, (const char**)&szKey, (const char**)&szValue);
		piCVar = piConsole->GetCVar(szKey);
		if (!piCVar)
		{
			continue;
		}
		piCVar->Set(szValue);
	}
}
//////////////////////////////////////////////////////////////////////////
void CRealtimeRemoteUpdateListener::LoadParticles(XmlNodeRef& root)
{
	XmlNodeRef oParticlesLibrary;
	XmlNodeRef oLibrary;
	XmlString strLibraryName;

	int nCurrentChild(0);
	int nNumberOfChildren(0);

	oParticlesLibrary = root->findChild("ParticlesLibrary");
	if (!oParticlesLibrary)
	{
		return;
	}

	nNumberOfChildren = oParticlesLibrary->getChildCount();
	for (nCurrentChild = 0; nCurrentChild < nNumberOfChildren; ++nCurrentChild)
	{
		oLibrary = oParticlesLibrary->getChild(nCurrentChild);
		if (oLibrary->isTag("Library"))
		{
			continue;
		}
		if (!oLibrary->getAttr("name", strLibraryName))
		{
			continue;
		}
		gEnv->pParticleManager->LoadLibrary((const char*)strLibraryName, oLibrary, true);
	}
}
//////////////////////////////////////////////////////////////////////////
void CRealtimeRemoteUpdateListener::LoadTerrainLayer(XmlNodeRef& root, unsigned char* uchData)
{
	int texId(0);
	int posx(0), posy(0);
	int w(0), h(0);
	int nSourceFormat(0);
	ETEX_Format eTFSrc(eTF_B8G8R8);

	if (!root->getAttr("Posx", posx))
	{
		return;
	}

	if (!root->getAttr("Posy", posy))
	{
		return;
	}

	if (!root->getAttr("w", w))
	{
		return;
	}

	if (!root->getAttr("h", h))
	{
		return;
	}

	if (!root->getAttr("ETEX_Format", nSourceFormat))
	{
		return;
	}

	eTFSrc = (ETEX_Format)nSourceFormat;

	if (gEnv->pRenderer && gEnv->p3DEngine)
	{
		texId = gEnv->pRenderer->UploadToVideoMemory(uchData, w, h, eTFSrc, eTFSrc, 0, false, FILTER_NONE, 0, NULL, FT_USAGE_ALLOWREADSRGB);
		// Swapped x & y for historical reasons.
		gEnv->p3DEngine->SetTerrainSectorTexture(posy, posx, texId);
	}
}

//////////////////////////////////////////////////////////////////////////
void CRealtimeRemoteUpdateListener::LoadEntities(XmlNodeRef& root)
{
	IEntitySystem* pEntitySystem = gEnv->pEntitySystem;

	bool bTransformOnly(false);
	bool bDeleteOnly = false;
	bool bRemoveAllOld = true;

	gEnv->pSystem->SetThreadState(ESubsys_Physics, false);

	if (root->haveAttr("PartialUpdate"))
	{
		bRemoveAllOld = false;
	}
	if (root->haveAttr("Delete"))
	{
		bDeleteOnly = true;
	}

	//////////////////////////////////////////////////////////////////////////
	// Delete all entities except the unremovable ones and the local player.
	if (bRemoveAllOld)
	{
		IEntityItPtr pIt = pEntitySystem->GetEntityIterator();
		IGameFramework* piGameFramework(gEnv->pGameFramework);
		IEntity* piRulesEntity(NULL);
		if (piGameFramework)
		{
			IGameRulesSystem* piGameRulesSystem(piGameFramework->GetIGameRulesSystem());
			if (piGameRulesSystem)
			{
				piRulesEntity = piGameRulesSystem->GetCurrentGameRulesEntity();
			}
		}

		pIt->MoveFirst();
		while (!pIt->IsEnd())
		{
			IEntity* pEntity = pIt->Next();
			IEntityClass* pEntityClass = pEntity->GetClass();
			uint32 nEntityFlags = pEntity->GetFlags();

			// Local player must not be deleted.
			if (nEntityFlags & ENTITY_FLAG_LOCAL_PLAYER)
				continue;

			// Rules should not be deleted as well.
			if (piRulesEntity)
			{
				if (pEntity->GetId() == piRulesEntity->GetId())
				{
					continue;
				}
			}

			pEntity->ClearFlags(ENTITY_FLAG_UNREMOVABLE);
			pEntitySystem->RemoveEntity(pEntity->GetId());
		}
		// Force deletion of removed entities.
		pEntitySystem->DeletePendingEntities();
		//////////////////////////////////////////////////////////////////////////
	}
	else
	{
		// Remove Entities with ID`s from the list.
		for (int i = 0; i < root->getChildCount(); i++)
		{
			XmlNodeRef objectNode = root->getChild(i);
			if (objectNode->isTag("Entity"))
			{
				// reserve the id
				EntityId id;
				if (objectNode->getAttr("EntityId", id))
				{
					IEntity* pEntity = pEntitySystem->GetEntity(id);

					if (!pEntity)
					{
						pEntitySystem->RemoveEntity(id, true);
						continue;
					}

					if (!objectNode->getAttr("TransformOnly", bTransformOnly))
					{
						pEntity->ClearFlags(ENTITY_FLAG_UNREMOVABLE);
						pEntitySystem->RemoveEntity(id, true);
						continue;
					}

					if (!bTransformOnly)
					{
						pEntity->ClearFlags(ENTITY_FLAG_UNREMOVABLE);
						pEntitySystem->RemoveEntity(id, true);
						continue;
					}

					Vec3 oPos(0.0f, 0.0f, 0.0f);
					Vec3 oScale(1.0f, 1.0f, 1.0f);
					Quat oRotate(1.0f, 0.0f, 0.0f, 0.0f);

					bool bHasPos = objectNode->getAttr("Pos", oPos);
					bool bHasRot = objectNode->getAttr("Rotate", oRotate);
					bool bHasScl = objectNode->getAttr("Scale", oScale);

					if (!bHasPos)
						oPos = pEntity->GetPos();

					if (!bHasRot)
						oRotate = pEntity->GetRotation();

					if (!bHasScl)
						oScale = pEntity->GetScale();

					pEntity->SetPosRotScale(oPos, oRotate, oScale);
				}
			}
		}
		// Force deletion of removed entities.
		pEntitySystem->DeletePendingEntities();
	}

	if (!bDeleteOnly)
	{
		pEntitySystem->LoadEntities(root, false);
	}

	// you can't pass temporaries to non-const references, so objects on the stack must be created
	SEntityEvent LevelLoaded(ENTITY_EVENT_LEVEL_LOADED);
	SEntityEvent StartGame(ENTITY_EVENT_START_GAME);

	pEntitySystem->SendEventToAll(LevelLoaded);
	pEntitySystem->SendEventToAll(StartGame);
	gEnv->pSystem->SetThreadState(ESubsys_Physics, true);
}
//////////////////////////////////////////////////////////////////////////
bool CRealtimeRemoteUpdateListener::IsSyncingWithEditor()
{
	CTimeValue oTimeValue(gEnv->pTimer->GetAsyncTime());
	oTimeValue -= m_lastKeepAliveMessageTime;

	return (fabs((oTimeValue).GetSeconds()) <= 30.0f);
}
//////////////////////////////////////////////////////////////////////////
void CRealtimeRemoteUpdateListener::Update()
{
	while (!m_ProcessingQueue.empty())
	{
		TDBuffer* pCurrentBuffer(m_ProcessingQueue.pop());

		if (!pCurrentBuffer)
		{
			continue;
		}

		TScopedPointer<TDBuffer> oScopedPointer(pCurrentBuffer);

		char* const szBuffer = (char*)&(pCurrentBuffer->front());
		const size_t nStringSize = strlen(szBuffer) + 1;
		unsigned char* const chBinaryBuffer = (unsigned char*)(szBuffer + nStringSize);
		const size_t nBinaryBufferSize = pCurrentBuffer->size() - nStringSize;

		XmlNodeRef oXmlNode = gEnv->pSystem->LoadXmlFromBuffer(szBuffer, nStringSize - 1);

		// Currently, if we have no XML node this is not a well formed message and
		// thus we stop processing.
		if (!oXmlNode)
		{
			continue;
		}

		if (strcmp(oXmlNode->getTag(), "SyncMessage") != 0)
		{
			continue;
		}

		string oSyncType = oXmlNode->getAttr("Type");
		if (oSyncType.empty())
		{
			continue;
		}

		size_t nBinaryDataSize = 0;
		if (!oXmlNode->getAttr("BinaryDataSize", nBinaryDataSize))
		{
			continue;
		}

		bool requiresFurtherProcessing = false;

		for (GameHandlerList::iterator item = m_gameHandlers.begin(), end = m_gameHandlers.end(); item != end; ++item)
		{
			if ((*item)->UpdateGameData(oXmlNode, chBinaryBuffer))
			{
				requiresFurtherProcessing = true;
			}
		}

		if (!requiresFurtherProcessing)
			continue;

		static std::vector<struct IStatObj*>* pStatObjTable = NULL;
		static std::vector<IMaterial*>* pMatTable = NULL;

		if (oSyncType.compare("EngineTerrainData") == 0)
		{
			gEnv->p3DEngine->LockCGFResources();

			if (nBinaryDataSize > 0)
			{
				if (ITerrain* piTerrain = gEnv->p3DEngine->GetITerrain())
				{
					size_t nUncompressedBinarySize(nBinaryDataSize);
					unsigned char* szData = new unsigned char[nBinaryDataSize];

					gEnv->pSystem->DecompressDataBlock(chBinaryBuffer, nBinaryBufferSize, szData, nUncompressedBinarySize);

					SHotUpdateInfo* pExportInfo = (SHotUpdateInfo*)szData;

					// As messages of oSyncType "EngineTerrainData" always come before
					// "EngineIndoorData" and are always paired together, and have
					// inter-dependencies amongst themselves, the locking is done here
					// and the unlocking is done when we receive a "EngineIndoorData".
					// Currently if we, for any reason, don't receive the second message,
					// we should expect horrible things to happen.
					gEnv->p3DEngine->LockCGFResources();

					pStatObjTable = NULL;
					pMatTable = NULL;

					piTerrain->SetCompiledData((uint8*)szData + sizeof(SHotUpdateInfo), nBinaryDataSize - sizeof(SHotUpdateInfo), &pStatObjTable, &pMatTable, true, pExportInfo);
					SAFE_DELETE_ARRAY(szData);
				}
			}
		}
		else if (oSyncType.compare("EngineIndoorData") == 0)
		{
			if (nBinaryDataSize > 0)
			{
				if (IVisAreaManager* piIVisAreaManager = gEnv->p3DEngine->GetIVisAreaManager())
				{
					size_t nUncompressedBinarySize(nBinaryDataSize);
					unsigned char* szData = new unsigned char[nBinaryDataSize];

					gEnv->pSystem->DecompressDataBlock(chBinaryBuffer, nBinaryBufferSize, szData, nUncompressedBinarySize);

					SHotUpdateInfo* pExportInfo = (SHotUpdateInfo*)szData;

					if (piIVisAreaManager)
						piIVisAreaManager->SetCompiledData((uint8*)szData + sizeof(SHotUpdateInfo), nBinaryDataSize - sizeof(SHotUpdateInfo), &pStatObjTable, &pMatTable, true, pExportInfo);

					SAFE_DELETE_ARRAY(szData);
				}
			}

			gEnv->p3DEngine->UnlockCGFResources();

			pStatObjTable = NULL;
			pMatTable = NULL;
		}
		else if (oSyncType.compare("Vegetation") == 0)
		{
			XmlNodeRef oCurrentNode = oXmlNode->findChild("Vegetation");
		}
		else if (oSyncType.compare("DetailLayers") == 0)
		{
			XmlNodeRef oChildRootNode = oXmlNode->findChild("SurfaceTypes");
			if (oChildRootNode)
			{
				gEnv->p3DEngine->LoadTerrainSurfacesFromXML(oChildRootNode, true);
			}
		}
		else if (oSyncType.compare("Environment") == 0)
		{
			XmlNodeRef oChildRootNode = oXmlNode->findChild("Environment");
			if (oChildRootNode)
			{
				gEnv->p3DEngine->LoadEnvironmentSettingsFromXML(oChildRootNode);
			}
		}
		else if (oSyncType.compare("TimeOfDay") == 0)
		{
			XmlNodeRef oChildRootNode = oXmlNode->findChild("TimeOfDay");
			if (oChildRootNode)
				LoadTimeOfDay(oChildRootNode);
		}
		else if (oSyncType.compare("Materials") == 0)
		{
			XmlNodeRef oChildRootNode = oXmlNode->findChild("Materials");
			if (oChildRootNode)
				LoadMaterials(oChildRootNode);
		}
		else if (oSyncType.compare("EntityArchetype") == 0)
		{
			XmlNodeRef oChildRootNode = oXmlNode->findChild("EntityPrototypes");
			if (oChildRootNode)
				LoadArchetypes(oChildRootNode);
		}
		else if (oSyncType.compare("ConsoleVariables") == 0)
		{
			LoadConsoleVariables(oXmlNode);
		}
		else if (oSyncType.compare("Particles") == 0)
		{
			LoadParticles(oXmlNode);
		}
		else if (oSyncType.compare("LayerTexture") == 0)
		{
			if (nBinaryDataSize > 0)
			{
				size_t nUncompressedBinarySize(nBinaryDataSize);
				unsigned char* szData = new unsigned char[nBinaryDataSize];

				if (!szData)
				{
					continue;
				}

				if (!gEnv->pSystem->DecompressDataBlock(chBinaryBuffer, nBinaryBufferSize, szData, nUncompressedBinarySize))
				{
					SAFE_DELETE_ARRAY(szData);
					continue;
				}

				LoadTerrainLayer(oXmlNode, szData);

				SAFE_DELETE_ARRAY(szData);
			}
		}
		else if (oSyncType.compare("Particle.Library") == 0)
		{
			XmlNodeRef oChildRootNode = oXmlNode->findChild("ParticleLibrary");
			if (oChildRootNode)
			{
				const char* szEffectName(NULL);

				oXmlNode->removeChild(oChildRootNode);

				if (!oChildRootNode->getAttr("Effect", &szEffectName))
				{
					continue;
				}

				XmlNodeRef oEffectNode = oChildRootNode->findChild("Effect");
				if (!oEffectNode)
				{
					continue;
				}

				gEnv->pParticleManager->LoadEffect(szEffectName, oEffectNode, true);
			}
		}
		else if (oSyncType.compare("ChangeLevel") == 0)
		{
			if (oXmlNode->haveAttr("LevelName"))
			{
				const char* szLevelName(NULL);
				string strMapCommand("map ");

				oXmlNode->getAttr("LevelName", &szLevelName);
				strMapCommand += szLevelName;

				gEnv->pConsole->ExecuteString(strMapCommand);
			}
		}
		else if (oSyncType.compare("Entities") == 0)
		{
			XmlNodeRef root = oXmlNode->findChild("Entities");

			if (!root)
				continue;

			LoadEntities(root);
		}
		else if (oSyncType.compare("GeometryList") == 0)
		{
			XmlNodeRef root = oXmlNode->findChild("Geometries");
			size_t nNumAttributes(root->getNumAttributes());
			size_t nCurrentAttribute(0);

			const char* szAttributeValue(NULL);
			const char* szAttributeName(NULL);

			for (nCurrentAttribute = 0; nCurrentAttribute < nNumAttributes; ++nCurrentAttribute)
			{
				root->getAttributeByIndex(nCurrentAttribute, &szAttributeName, &szAttributeValue);
				gEnv->p3DEngine->LoadStatObj(szAttributeName);
			}
		}
		else if (oSyncType.compare("KeepAlive") == 0)
		{
			// Here we reset the time counter for the keep alive message.
			m_lastKeepAliveMessageTime = gEnv->pTimer->GetAsyncTime();
		}
	}
}
//////////////////////////////////////////////////////////////////////////
