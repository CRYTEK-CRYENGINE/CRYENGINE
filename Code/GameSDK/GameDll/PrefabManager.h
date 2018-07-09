// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: 
  
 -------------------------------------------------------------------------
  History:
  - Created by Marco Corbetta

*************************************************************************/
#ifndef __GAME_PREFAB_MANAGER_H__
#define __GAME_PREFAB_MANAGER_H__

#if _MSC_VER > 1000
# pragma once
#endif

namespace CryGame 
{
	class CRuntimePrefab;

	typedef std::map<EntityId,CRuntimePrefab*>	lstRuntimePrefab;
	typedef lstRuntimePrefab::iterator			lstRuntimePrefabIt;

	//////////////////////////////////////////////////////////////////////////
	typedef struct tBrushParams
	{
		tBrushParams()
			: dwRenderNodeFlags(0)
			, nLodRatio(100)
			, nViewDistRatio(100)		
			, bIsDecal(false)
			, nProjectionType(0)
			, bDeferredDecal(false)
			, nSortPrio(16)
			, fDepth(1.0f)
			, nBinaryDesignerObjectVersion(0)
		{
		}

		string				sFilename;
		string				sMaterial;
		uint32				dwRenderNodeFlags;
		int					nLodRatio;
		int					nViewDistRatio;
		Matrix34			mat;

		bool				bIsDecal;
		int					nProjectionType;		
		bool				bDeferredDecal;
		int					nSortPrio;
		float				fDepth;
		int					nBinaryDesignerObjectVersion;		
		std::vector<char>	vBinaryDesignerObject;

	}tBrushParams;

	//////////////////////////////////////////////////////////////////////////
	typedef struct  
	{
		SEntitySpawnParams	pSpawnParams;
		XmlNodeRef					pEntityNode;		
		Matrix34						mat;
	}tEntityParams;

	class CPrefab;
	//////////////////////////////////////////////////////////////////////////
	typedef struct  
	{
		Matrix34	mat;
		string		sFullName;
		CPrefab		*pPrefab; // recursive prefabs				
	}tPrefabParams;

	//////////////////////////////////////////////////////////////////////////
	class CPrefab
	{
	public:

		CPrefab(const string &sName) { cry_strcpy(m_szName,sName.c_str()); }
		CPrefab() { m_szName[0]=0; }		
		~CPrefab() {}		

		void	Load(XmlNodeRef &itemNode);		

		char	m_szName[128];

		std::vector<tEntityParams>	m_lstEntities;
		std::vector<tBrushParams>	m_lstBrushes;		
		std::vector<tPrefabParams>	m_lstPrefabs;			

	protected:				

		bool ExtractArcheTypeLoadParams(XmlNodeRef &objNode, SEntitySpawnParams& loadParams);
		void	ExtractTransform(XmlNodeRef &objNode,Matrix34 &mat);
		bool	ExtractBrushLoadParams(XmlNodeRef &objNode,tBrushParams	&loadParams);
		bool	ExtractPrefabLoadParams(XmlNodeRef &objNode,tPrefabParams &loadParams);
		bool	ExtractDecalLoadParams(XmlNodeRef &objNode,tBrushParams	&loadParams);
		bool	ExtractEntityLoadParams(XmlNodeRef &objNode,SEntitySpawnParams &loadParams);
		bool	ExtractDesignerLoadParams(XmlNodeRef &objName,tBrushParams &loadParams);

	private:
	};

	typedef std::map<string,CPrefab*>	lstPrefab;
	typedef lstPrefab::iterator		lstPrefabIt;

	//////////////////////////////////////////////////////////////////////////
	class CPrefabLib
	{
	public:				
		CPrefabLib(const string &sFilename,const string &sName) 
		{ 
			m_sName=sName;
			m_sFilename=sFilename;
		}
		~CPrefabLib();
		
		void		AddPrefab(CPrefab &pPrefab);
		CPrefab	*GetPrefab(const string &sName);		

		string	m_sName;
		string	m_sFilename;

		lstPrefab	m_lstPrefabs;

	protected:		
		
	};

	typedef std::map<string,CPrefabLib*>	lstPrefabLib;
	typedef lstPrefabLib::iterator		lstPrefabLibIt;

	//////////////////////////////////////////////////////////////////////////
	class CPrefabManager
	{
	public:
		CPrefabManager();
		virtual ~CPrefabManager(); 
				
		bool	LoadPrefabLibrary(const string &sFilename);			
		void	SpawnPrefab(const string &sLibraryFilename,const string &sFullName,EntityId id,uint32 nSeed,uint32 nMaxSpawn);
		void	MovePrefab(EntityId id);
		void	DeletePrefab(EntityId id);		
		void	HidePrefab(EntityId id, bool bHide);		
		void	Clear(bool bDeleteLibs=true);

		// called during level generation process
		void	CallOnSpawn(IEntity *pEntity,int nSeed);

		CPrefab	*FindPrefab(CPrefabLib *pLib,const string &sFullName,lstPrefabIt &i,bool bLoadIfNotFound=false);

		void	SetPrefabGroup(const string &sGroupName);
		
	protected:					
		
		CPrefab	*GetPrefab(const string &sLibrary,const string &sName);				

		lstPrefabLib			m_lstPrefabLibs;
		lstRuntimePrefab	m_lstRuntimePrefabs;

	private:

		void		StripLibraryName(const string &sLibraryName,const string &sFullName,string &sResult,bool bApplyGroup);
		CPrefab	*FindPrefabInGroup(CPrefabLib *pLib,const string &sLibraryName,const string &sFullName,lstPrefabIt &i);

		// random generation functions
		CPrefab	*GetRandomPrefab(CPrefabLib *pLib,const lstPrefabIt &it,uint32 nSeed);

		std::map<CPrefab*,int>	m_lstOccurrences;
		string	m_sLastPrefab;

		string	m_sCurrentGroup;
		
	};
}

#endif //__GAME_PREFAB_MANAGER_H__
