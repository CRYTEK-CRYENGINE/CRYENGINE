// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  
 -------------------------------------------------------------------------
  History:
  - Created by Marco Corbetta

*************************************************************************/
#include "StdAfx.h"
#include "PrefabManager.h"
#include "RuntimePrefab.h"
#include <CryCore/Base64.h>

using namespace CryGame;
 
//#pragma optimize("", off)
//#pragma inline_depth(0)

//////////////////////////////////////////////////////////////////////////
void	CPrefab::ExtractTransform(XmlNodeRef &objNode,Matrix34 &mat)
{
	// parse manually
	Vec3 pos(Vec3Constants<float>::fVec3_Zero);
	Quat rot(Quat::CreateIdentity());
	Vec3 scale(Vec3Constants<float>::fVec3_One);

	objNode->getAttr("Pos", pos);
	objNode->getAttr("Rotate", rot);
	objNode->getAttr("Scale", scale);

	// local transformation inside the prefab
	mat.Set(scale,rot,pos);		
}

bool CPrefab::ExtractArcheTypeLoadParams(XmlNodeRef &objNode, SEntitySpawnParams& loadParams)
{
	if (!gEnv->pEntitySystem->ExtractArcheTypeLoadParams(objNode, loadParams))
	{
		return false;
	}

	// this property is exported by Sandbox with different names in the Editor XML and in the game XML, so we need to check for it manually
	bool bNoStaticDecals = false;
	objNode->getAttr("NoStaticDecals", bNoStaticDecals);
	if (bNoStaticDecals)
	{
		loadParams.nFlags |= ENTITY_FLAG_NO_DECALNODE_DECALS;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CPrefab::ExtractBrushLoadParams(XmlNodeRef &objNode,tBrushParams &loadParams)
{	
	ExtractTransform(objNode,loadParams.mat);

	const char *szFilename = objNode->getAttr("Prefab");
	if (szFilename)
		loadParams.sFilename=string(szFilename);

	// flags already combined, can skip parsing lots of nodes	
	objNode->getAttr("RndFlags",loadParams.dwRenderNodeFlags);			
	objNode->getAttr("LodRatio",loadParams.nLodRatio);		
	objNode->getAttr("ViewDistRatio",loadParams.nViewDistRatio);	

	const char *szMat=objNode->getAttr( "Material");
	if (szMat && szMat[0])
		loadParams.sMaterial=string(szMat);

	// todo: parse collision filtering
	// Apply collision class filtering
	//m_collisionFiltering.ApplyToPhysicalEntity(m_pRenderNode ? m_pRenderNode->GetPhysics() : 0);

	return (true);
}

//////////////////////////////////////////////////////////////////////////
bool CPrefab::ExtractDecalLoadParams(XmlNodeRef &objNode,tBrushParams &loadParams)
{			
	ExtractTransform(objNode,loadParams.mat);

	const char *szMat=objNode->getAttr( "Material");
	if (szMat && szMat[0])
		loadParams.sMaterial=string(szMat);

	loadParams.bIsDecal=true;	

	int projectionType( SDecalProperties::ePlanar );
	objNode->getAttr( "ProjectionType", projectionType );
	loadParams.nProjectionType= projectionType;
	
	objNode->getAttr("RndFlags",loadParams.dwRenderNodeFlags);	
	objNode->getAttr("Deferred",loadParams.bDeferredDecal);
	objNode->getAttr("ViewDistRatio",loadParams.nViewDistRatio);		
	objNode->getAttr("LodRatio",loadParams.nLodRatio);		
	objNode->getAttr("SortPriority",loadParams.nSortPrio);		
	objNode->getAttr("ProjectionDepth",loadParams.fDepth);	

	return (true);
}

//////////////////////////////////////////////////////////////////////////
bool CPrefab::ExtractPrefabLoadParams(XmlNodeRef &objNode,tPrefabParams &loadParams)
{		
	ExtractTransform(objNode,loadParams.mat);

	const char *szFullName = objNode->getAttr("PrefabName");
	if (szFullName)
		loadParams.sFullName=string(szFullName);

	loadParams.pPrefab=NULL;

	return (true);
}

//////////////////////////////////////////////////////////////////////////
bool CPrefab::ExtractEntityLoadParams(XmlNodeRef &objNode,SEntitySpawnParams &loadParams)
{		
	if (!gEnv->pEntitySystem->ExtractEntityLoadParams(objNode, loadParams))
		return (false);

	// this property is exported by Sandbox with different names in the Editor XML and in the game XML, so we need to check for it manually
	bool bNoStaticDecals = false;
	objNode->getAttr("NoStaticDecals", bNoStaticDecals);
	if (bNoStaticDecals)
		loadParams.nFlags |= ENTITY_FLAG_NO_DECALNODE_DECALS;

	return (true);
}

//////////////////////////////////////////////////////////////////////////
bool CPrefab::ExtractDesignerLoadParams(XmlNodeRef &objNode, tBrushParams &loadParams)
{
	ExtractTransform(objNode,loadParams.mat);
	
	objNode->getAttr("RndFlags",loadParams.dwRenderNodeFlags);			
	objNode->getAttr("ViewDistRatio",loadParams.nViewDistRatio);	
	const char *szMat=objNode->getAttr( "Material");
	if (szMat && szMat[0])
		loadParams.sMaterial=string(szMat);

	XmlNodeRef meshNode = objNode->findChild("Mesh");
	if (meshNode)
	{
		loadParams.nBinaryDesignerObjectVersion = 0;
		meshNode->getAttr("Version",loadParams.nBinaryDesignerObjectVersion);
		const char* encodedStr;
		if (meshNode->getAttr("BinaryData",&encodedStr))
		{
			int nLength = strlen(encodedStr);
			if(nLength > 0)
			{
				unsigned int nDestBufferLen = Base64::decodedsize_base64(nLength);
				loadParams.vBinaryDesignerObject.resize(nDestBufferLen, 0);
				Base64::decode_base64(&loadParams.vBinaryDesignerObject[0], encodedStr, nLength, false);
				return true;
			}
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void CPrefab::Load(XmlNodeRef &itemNode)
{				
	cry_strcpy(m_szName, itemNode->getAttr("Name"));
	XmlNodeRef objects = itemNode->findChild( "Objects" );
	if (objects)
	{		
		int numObjects = objects->getChildCount();
		for (int i = 0; i < numObjects; i++)
		{
			XmlNodeRef objNode = objects->getChild(i);
			const char *szType = objNode->getAttr("Type");

			// try to parse entities first
			SEntitySpawnParams loadParams;
			if (ExtractEntityLoadParams(objNode, loadParams))
			{
				tEntityParams pParams;
				pParams.pSpawnParams=loadParams;
				pParams.pEntityNode=objNode->clone();

				// local entity transformation inside the prefab
				pParams.mat.Set(pParams.pSpawnParams.vScale,pParams.pSpawnParams.qRotation,pParams.pSpawnParams.vPosition);

				m_lstEntities.push_back(pParams);
			}
			else if (stricmp(szType, "EntityArchetype") == 0)
			{
				if (ExtractArcheTypeLoadParams(objNode, loadParams))
				{
					tEntityParams pParams;
					pParams.pSpawnParams=loadParams;
					pParams.pEntityNode=objNode->clone();

					// local entity transformation inside the prefab
					pParams.mat.Set(pParams.pSpawnParams.vScale,pParams.pSpawnParams.qRotation,pParams.pSpawnParams.vPosition);

					m_lstEntities.push_back(pParams);
				}
			}
			else if (stricmp(szType,"Brush") == 0)
			{
				// a brush? Parse manually
				tBrushParams pParams;
				if (ExtractBrushLoadParams(objNode,pParams))
					m_lstBrushes.push_back(pParams);
			}
			else if (stricmp(szType,"Decal") == 0)
			{
				tBrushParams pParams;
				if (ExtractDecalLoadParams(objNode,pParams))
					m_lstBrushes.push_back(pParams);
			}
			else if (stricmp(szType,"Prefab") == 0)
			{
				// Sometimes people are placing prefabs inside prefabs.
				// If that is the case, store a reference to the prefab and expand after the loading is done
				// (because sometimes the prefab definition is stored in the library after the declaration...)
				tPrefabParams pParams;
				if (ExtractPrefabLoadParams(objNode,pParams))
					m_lstPrefabs.push_back(pParams);
			}
			else if (stricmp(szType,"Designer") == 0)
			{
				tBrushParams pParams;
				if (ExtractDesignerLoadParams(objNode, pParams))
					m_lstBrushes.push_back(pParams);
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
CPrefabLib::~CPrefabLib()
{
	for (lstPrefabIt i=m_lstPrefabs.begin();i!=m_lstPrefabs.end();++i)
	{
		CPrefab *pPrefab=(*i).second;
		delete pPrefab;
	}	
}

//////////////////////////////////////////////////////////////////////////
void CPrefabLib::AddPrefab(CPrefab &pPrefab)
{
	m_lstPrefabs[pPrefab.m_szName]=&pPrefab;
}

//////////////////////////////////////////////////////////////////////////
CPrefab *CPrefabLib::GetPrefab(const string &sName)
{
	lstPrefabIt i=m_lstPrefabs.find(sName);
	if (i==m_lstPrefabs.end()) 
		return(NULL);	
	return((*i).second);	
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

using namespace CryGame;

//////////////////////////////////////////////////////////////////////////
CPrefabManager::CPrefabManager()
{			
}
 
//////////////////////////////////////////////////////////////////////////
CPrefabManager::~CPrefabManager()
{	
	Clear();
}

//////////////////////////////////////////////////////////////////////////
void CPrefabManager::Clear(bool bDeleteLibs)
{	
	if (bDeleteLibs)
	{
		for (lstPrefabLibIt i=m_lstPrefabLibs.begin();i!=m_lstPrefabLibs.end();++i)
		{
			CPrefabLib *pLib=(*i).second;
			delete pLib;
		}
		m_lstPrefabLibs.clear();
	}

	for (lstRuntimePrefabIt i=m_lstRuntimePrefabs.begin();i!=m_lstRuntimePrefabs.end();++i)
	{
		CRuntimePrefab *pPrefab=(*i).second;
		delete pPrefab;
	}
	m_lstRuntimePrefabs.clear();

	m_sLastPrefab.clear();
	m_lstOccurrences.clear();	
	m_sCurrentGroup.clear();
}

//////////////////////////////////////////////////////////////////////////
CPrefab	*CPrefabManager::GetPrefab(const string &sLibrary,const string &sName)
{
	lstPrefabLibIt i=m_lstPrefabLibs.find(sLibrary);
	if (i==m_lstPrefabLibs.end())
		return(NULL);

	CPrefabLib *pLib=(*i).second;
	return(pLib->GetPrefab(sName));
}

//////////////////////////////////////////////////////////////////////////
bool CPrefabManager::LoadPrefabLibrary(const string &sFilename)
{ 
	if (m_lstPrefabLibs.find(sFilename)!=m_lstPrefabLibs.end())
		return true;
	
	XmlNodeRef xmlLibRoot = gEnv->pSystem->LoadXmlFromFile( sFilename );
	if(xmlLibRoot==0)
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_SYSTEM,VALIDATOR_ERROR,0,0,"File %s failed to load",sFilename.c_str()); 
		return false;
	}
		
	const char *szName=xmlLibRoot->getAttr("Name");

	if (!szName)
	{
		gEnv->pSystem->Warning(VALIDATOR_MODULE_SYSTEM,VALIDATOR_ERROR,0,0,"Cannot find library name for %s",sFilename.c_str()); 
		return false;
	}
	
	CPrefabLib *pLib=new CPrefabLib(sFilename,szName);

	for (int i = 0; i < xmlLibRoot->getChildCount(); i++)
	{
		XmlNodeRef itemNode = xmlLibRoot->getChild(i);
		// Only accept nodes with correct name.
		if (stricmp(itemNode->getTag(),"Prefab") != 0)
			continue;

		CPrefab *pPrefab=new CPrefab();
		pPrefab->Load(itemNode);

		pLib->AddPrefab(*pPrefab);
	}

	m_lstPrefabLibs[pLib->m_sFilename]=pLib;		

	// verify embedded prefabs if any was found
	for (lstPrefabIt i=pLib->m_lstPrefabs.begin();i!=pLib->m_lstPrefabs.end();++i)
	{
		CPrefab *pPrefab=(*i).second;
		for (std::vector<tPrefabParams>::iterator i2=pPrefab->m_lstPrefabs.begin();i2!=pPrefab->m_lstPrefabs.end();++i2)
		{
			tPrefabParams &pParams=(*i2);
			// check if the prefab item exists inside the library and set the reference
			lstPrefabIt i3;
			pParams.pPrefab=FindPrefab(pLib,pParams.sFullName,i3,true);
		}		
	}			

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CPrefabManager::SetPrefabGroup(const string &sGroupName)
{	
	m_sCurrentGroup=sGroupName;
}

//////////////////////////////////////////////////////////////////////////
CPrefab *CPrefabManager::GetRandomPrefab(CPrefabLib *pLib,const lstPrefabIt &it,uint32 nSeed)
{
	return NULL;
}

//////////////////////////////////////////////////////////////////////////
void	CPrefabManager::StripLibraryName(const string &sLibraryName,const string &sFullName,string &sResult,bool bApplyGroup)
{
	// strip the library name if it was added (for example it happens when automatically converting from prefab to procedural object)	
	sResult=sFullName;

	int curPos = 0;
	string sLibName = sFullName.Tokenize(".",curPos);	
	if (sLibName==sLibraryName)
		sResult=string(&sFullName.c_str()[curPos]); 

	if (bApplyGroup && !m_sCurrentGroup.empty())
	{
		curPos = 0;
		string sGroup = sFullName.Tokenize(".",curPos);
		if (sGroup=="Default")
		{
			// if this is in the default group, adapt to the currently selected group, if any
			sResult=m_sCurrentGroup+(".")+string(&sFullName.c_str()[curPos]); 		
		}	
	}
}

//////////////////////////////////////////////////////////////////////////
CPrefab	*CPrefabManager::FindPrefabInGroup(CPrefabLib *pLib,const string &sLibraryName,const string &sFullName,lstPrefabIt &i)
{
	string sResult;

	// try with the group variation first (Voodoo etc.)
	StripLibraryName(pLib->m_sName,sFullName,sResult,true);		
	i=pLib->m_lstPrefabs.find(sResult);
	if (i!=pLib->m_lstPrefabs.end()) 		
		return ((*i).second);	

	// If not found try without the group variation, check if the default group has it
	StripLibraryName(pLib->m_sName,sFullName,sResult,false);		
	i=pLib->m_lstPrefabs.find(sResult);
	if (i!=pLib->m_lstPrefabs.end()) 		
		return ((*i).second);	

	return (NULL);
}

//////////////////////////////////////////////////////////////////////////
CPrefab *CPrefabManager::FindPrefab(CPrefabLib *pLib,const string &sFullName,lstPrefabIt &i,bool bLoadIfNotFound)
{	
	CPrefab *pRes=FindPrefabInGroup(pLib,pLib->m_sName,sFullName,i);
	
	// if the prefab was not found, try to see if the prefab is referencing a prefab from a completely different library.
	if (!pRes && bLoadIfNotFound)
	{		
		int curPos = 0;
		string sLibName = sFullName.Tokenize(".",curPos);
		if (!sLibName.empty())
		{
			// try to access this library.
			// check if the library exists
			string sLibFilename="Prefabs/"+sLibName+".xml";
			lstPrefabLibIt i1=m_lstPrefabLibs.find(sLibFilename);
			if (i1==m_lstPrefabLibs.end())
			{
				// try to load it
				if (!LoadPrefabLibrary(sLibFilename))
					return (NULL);				
				i1=m_lstPrefabLibs.find(sLibFilename);
				assert(i1!=m_lstPrefabLibs.end());					
			}

			// check if the item is there now in the new loaded library.
			CPrefabLib *pLib1=(*i1).second;	
			pRes=FindPrefabInGroup(pLib1,pLib1->m_sName,sFullName,i);
		}
	}	

	return pRes;		
}

//////////////////////////////////////////////////////////////////////////
void CPrefabManager::SpawnPrefab(const string &sLibraryFilename,const string &sFullName,EntityId id,uint32 nSeed,uint32 nMaxSpawn)
{
	// check if the library exists
	lstPrefabLibIt i1=m_lstPrefabLibs.find(sLibraryFilename);
	if (i1==m_lstPrefabLibs.end())
		return;
	CPrefabLib *pLib=(*i1).second;	
			
	// check if the prefab item exists inside the library
	lstPrefabIt i2;
	CPrefab *pPrefab=FindPrefab(pLib,sFullName,i2);		
	if (!pPrefab)
	{
		// cannot find this prefab, something went wrong (probably during editing a prefab was deleted and the xml not updated yet)
		return; 
	}

	// check if we have a limit on number of spawns
	if (nMaxSpawn>0)
	{
		std::map<CPrefab*,int>::iterator pOccCount=m_lstOccurrences.find(pPrefab);
		if (pOccCount!=m_lstOccurrences.end())
		{
			if (pOccCount->second>nMaxSpawn)
				return; 
			pOccCount->second=pOccCount->second+1;
		}
		else
			m_lstOccurrences[pPrefab]=1;
	}

	// find the runtime prefab associated to this entity id (the entity that spawned this prefab)
	CRuntimePrefab *pRuntimePrefab=NULL;

	lstRuntimePrefabIt i=m_lstRuntimePrefabs.find(id);
	if (i!=m_lstRuntimePrefabs.end())
		pRuntimePrefab=(*i).second;
	else
	{
		pRuntimePrefab=new CRuntimePrefab(id);	
		m_lstRuntimePrefabs[id]=pRuntimePrefab;
	}
			
	if (nSeed > 0)
	{
		if (CPrefab* const pTempPrefab = GetRandomPrefab(pLib, i2, nSeed))
		{
			pPrefab = pTempPrefab;
		}
	}

	pRuntimePrefab->Spawn(*pPrefab);
}

//////////////////////////////////////////////////////////////////////////
void CPrefabManager::MovePrefab(EntityId id)
{
	lstRuntimePrefabIt i=m_lstRuntimePrefabs.find(id);
	if (i!=m_lstRuntimePrefabs.end())
	{
		CRuntimePrefab *pRuntimePrefab=(*i).second;
		pRuntimePrefab->Move();	
	}		
}

//////////////////////////////////////////////////////////////////////////
void CPrefabManager::DeletePrefab(EntityId id)
{
	lstRuntimePrefabIt i=m_lstRuntimePrefabs.find(id);
	if (i!=m_lstRuntimePrefabs.end())
	{
		CRuntimePrefab *pRuntimePrefab=(*i).second;
		
		std::map<CPrefab*,int>::iterator pOccCount=m_lstOccurrences.find(pRuntimePrefab->GetSourcePrefab());
		if (pOccCount!=m_lstOccurrences.end())
		{
			pOccCount->second=max(0,pOccCount->second-1);
		}

		delete pRuntimePrefab;
		m_lstRuntimePrefabs.erase(i);
	}		
}

//////////////////////////////////////////////////////////////////////////
void CPrefabManager::HidePrefab(EntityId id,bool bHide)
{
	lstRuntimePrefabIt i=m_lstRuntimePrefabs.find(id);
	if (i!=m_lstRuntimePrefabs.end())
	{
		CRuntimePrefab *pRuntimePrefab=(*i).second;
		pRuntimePrefab->Hide(bHide);
	}		
}

//////////////////////////////////////////////////////////////////////////
void CPrefabManager::CallOnSpawn(IEntity *pEntity,int nSeed)
{
	IScriptTable *pScriptTable(pEntity->GetScriptTable());
	if (!pScriptTable)
		return;

	if ((pScriptTable->GetValueType("Spawn") == svtFunction) && pScriptTable->GetScriptSystem()->BeginCall(pScriptTable, "Spawn"))
	{
		pScriptTable->GetScriptSystem()->PushFuncParam(pScriptTable);
		pScriptTable->GetScriptSystem()->PushFuncParam(nSeed);
		pScriptTable->GetScriptSystem()->EndCall();
	} 

}
