// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   MatMan.h
//  Version:     v1.00
//  Created:     23/8/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __MatMan_h__
#define __MatMan_h__
#pragma once

#include "Cry3DEngineBase.h"
#include "SurfaceTypeManager.h"
#include <CryThreading/CryThreadSafeRendererContainer.h>
#include "MaterialHelpers.h"

#define MATERIAL_DELETION_DELAY 3 // Number of frames delay + 1

// forward declarations.
struct IMaterial;
struct ISurfaceType;
struct ISurfaceTypeManager;
class CMatInfo;

//////////////////////////////////////////////////////////////////////////
//
// CMatMan is a material manager class.
//
//////////////////////////////////////////////////////////////////////////
class CMatMan : public IMaterialManager, public Cry3DEngineBase
{
public:
	CMatMan();
	virtual ~CMatMan();

	void ShutDown();

	// interface IMaterialManager --------------------------------------------------------

	virtual IMaterial* CreateMaterial(const char* sMtlName, int nMtlFlags = 0);
	virtual IMaterial* FindMaterial(const char* sMtlName) const;
	virtual IMaterial* LoadMaterial(const char* sMtlName, bool bMakeIfNotFound = true, bool bNonremovable = false, unsigned long nLoadingFlags = 0);
	virtual IMaterial* LoadMaterialFromXml(const char* sMtlName, XmlNodeRef mtlNode);
	virtual void       SetListener(IMaterialManagerListener* pListener) { m_pListener = pListener; };
	virtual IMaterial* GetDefaultMaterial();
	virtual IMaterial* GetDefaultTerrainLayerMaterial()
	{
		if (!m_bInitialized)
		{
			InitDefaults();
		}
		return m_pDefaultTerrainLayersMtl;
	}
	virtual IMaterial*    GetDefaultLayersMaterial();
	virtual IMaterial*    GetDefaultHelperMaterial();
	virtual ISurfaceType* GetSurfaceTypeByName(const char* sSurfaceTypeName, const char* sWhy = NULL);
	virtual int           GetSurfaceTypeIdByName(const char* sSurfaceTypeName, const char* sWhy = NULL);
	virtual ISurfaceType* GetSurfaceType(int nSurfaceTypeId, const char* sWhy = NULL)
	{
		return m_pSurfaceTypeManager->GetSurfaceTypeFast(nSurfaceTypeId, sWhy);
	}
	virtual ISurfaceTypeManager* GetSurfaceTypeManager() { return m_pSurfaceTypeManager; }
	virtual IMaterial*           LoadCGFMaterial(const char* szMaterialName, const char* szCgfFilename, unsigned long nLoadingFlags = 0);
	virtual IMaterial*           CloneMaterial(IMaterial* pMtl, int nSubMtl = -1);
	virtual IMaterial*           CloneMultiMaterial(IMaterial* pMtl, const char* sSubMtlName = 0);
	virtual void                 GetLoadedMaterials(IMaterial** pData, uint32& nObjCount) const;
	virtual void                 SetAltMaterialSuffix(const char* pSuffix);

	virtual void                 CopyMaterial(IMaterial* pMtlSrc, IMaterial* pMtlDest, EMaterialCopyFlags flags);

	virtual void                 RenameMaterial(IMaterial* pMtl, const char* sNewName);
	virtual void                 RefreshMaterialRuntime();
	// ------------------------------------------------------------------------------------

	void InitDefaults();

	void PreloadLevelMaterials();
	void DoLoadSurfaceTypesInInit(bool doLoadSurfaceTypesInInit);

	void UpdateShaderItems();
	void RefreshShaderResourceConstants();

	// Load all known game decal materials.
	void PreloadDecalMaterials();

	//////////////////////////////////////////////////////////////////////////
	ISurfaceType* GetSurfaceTypeFast(int nSurfaceTypeId, const char* sWhy = NULL) { return m_pSurfaceTypeManager->GetSurfaceTypeFast(nSurfaceTypeId, sWhy); }

	virtual void  GetMemoryUsage(ICrySizer* pSizer) const;

	void          DelayedMaterialDeletion();

private: // -----------------------------------------------------------------------------
	friend class CMatInfo;
	void        Unregister(CMatInfo* pMat);

	void        DelayedDelete(CMatInfo* pMat);
	void        ForceDelayedMaterialDeletion();

	static bool LoadMaterialShader(IMaterial* pMtl, IMaterial* pParentMtl, const char* sShader, uint64 nShaderGenMask, SInputShaderResources& sr, XmlNodeRef& publicsNode, unsigned long nLoadingFlags = 0);
	static bool LoadMaterialLayerSlot(uint32 nSlot, IMaterial* pMtl, const char* szShaderName, SInputShaderResources& pBaseResources, XmlNodeRef& pPublicsNode, uint8 nLayerFlags);
	static IMaterial* MakeMaterialFromXml(const char* sMtlName, const char* sMtlFilename, XmlNodeRef node, bool bForcePureChild, uint16 sortPrio = 0, IMaterial* pExistingMtl = 0, unsigned long nLoadingFlags = 0, IMaterial* pParentMtl = 0);

	static void ParsePublicParams(SInputShaderResources& sr, XmlNodeRef paramsNode);
	static const char* UnifyName(const char* sMtlName);

	// Can be called after material creation and initialization, to inform editor that new material in engine exist.
	// Only used internally.
	void       NotifyCreateMaterial(IMaterial* pMtl);
	// Make a valid material from the XML node.

	void       FreeAllMaterials();

private:
	typedef std::map<string, IMaterial*> MtlNameMap;

	CryCriticalSection                m_AccessLock;

	MtlNameMap                        m_mtlNameMap;                   //

	IMaterialManagerListener*         m_pListener;                    //
	_smart_ptr<CMatInfo>              m_pDefaultMtl;                  //
	_smart_ptr<IMaterial>             m_pDefaultLayersMtl;            //
	_smart_ptr<CMatInfo>              m_pDefaultTerrainLayersMtl;     //
	_smart_ptr<CMatInfo>              m_pNoDrawMtl;                   //
	_smart_ptr<CMatInfo>              m_pDefaultHelperMtl;

	std::vector<_smart_ptr<CMatInfo>> m_nonRemovables;                //

	std::vector<CMatInfo*>            m_DelayedDeletionMtls[MATERIAL_DELETION_DELAY];

	CSurfaceTypeManager*              m_pSurfaceTypeManager;          //

	string                            m_altSuffix;

	//////////////////////////////////////////////////////////////////////////
	// Cached XML parser.
	_smart_ptr<IXmlParser> m_pXmlParser;

	bool                   m_bInitialized;
	bool                   m_bLoadSurfaceTypesInInit;
	uint8                  m_nDelayedDeleteID;

public:
	// Global namespace "instance", not a class "instance", no member-variables, only const functions;
	// Used to encapsulate the material-definition/io into Cry3DEngine (and make it plugable that way).
	static MaterialHelpers s_materialHelpers;
};

#endif // __MatMan_h__
