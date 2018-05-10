// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   MatMan.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: Material Manager Implementation
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "MatMan.h"
#include "3dEngine.h"
#include "ObjMan.h"
#include <CryRenderer/IRenderer.h>
#include "SurfaceTypeManager.h"
#include <Cry3DEngine/CGF/CGFContent.h>
#include <CrySystem/File/IResourceManager.h>

#define MATERIAL_EXT                    ".mtl"
#define MATERIAL_NODRAW                 "nodraw"

#define MATERIAL_DECALS_FOLDER          "Materials/Decals"
#define MATERIAL_DECALS_SEARCH_WILDCARD "*.mtl"
#define MTL_LEVEL_CACHE_PAK             "mtl.pak"

//////////////////////////////////////////////////////////////////////////
struct MaterialHelpers CMatMan::s_materialHelpers;

#if !defined(_RELEASE)
static const char* szReplaceMe = "%ENGINE%/EngineAssets/TextureMsg/ReplaceMe.tif";
static const char* szGeomNotBreakable = "%ENGINE%/EngineAssets/TextureMsg/GeomNotBreakable.tif";
#else
static const char* szReplaceMe = "%ENGINE%/EngineAssets/TextureMsg/ReplaceMeRelease.tif";
static const char* szGeomNotBreakable = "%ENGINE%/EngineAssets/TextureMsg/ReplaceMeRelease.tif";
#endif

//////////////////////////////////////////////////////////////////////////
CMatMan::CMatMan()
{
	m_bInitialized = false;
	m_bLoadSurfaceTypesInInit = true;
	m_pListener = NULL;
	m_pDefaultMtl = NULL;
	m_pDefaultTerrainLayersMtl = NULL;
	m_pDefaultLayersMtl = NULL;
	m_pDefaultHelperMtl = NULL;
	m_pNoDrawMtl = NULL;
	m_nDelayedDeleteID = 0;

	m_pSurfaceTypeManager = new CSurfaceTypeManager();

	m_pXmlParser = GetISystem()->GetXmlUtils()->CreateXmlParser();

#if defined(ENABLE_CONSOLE_MTL_VIZ)
	CMatInfo::RegisterConsoleMatCVar();
#endif

}

//////////////////////////////////////////////////////////////////////////
CMatMan::~CMatMan()
{
	delete m_pSurfaceTypeManager;
	int nNotUsed = 0, nNotUsedParents = 0;
	m_pDefaultMtl = NULL;
	m_pDefaultTerrainLayersMtl = NULL;
	m_pDefaultLayersMtl = NULL;
	m_pDefaultHelperMtl = NULL;

	/*
	   for (MtlSet::iterator it = m_mtlSet.begin(); it != m_mtlSet.end(); ++it)
	   {
	   IMaterial *pMtl = *it;
	   SShaderItem Sh = pMtl->GetShaderItem();
	   if(Sh.m_pShader)
	   {
	    Sh.m_pShader->Release();
	    Sh.m_pShader = 0;
	   }
	   if(Sh.m_pShaderResources)
	   {
	    Sh.m_pShaderResources->Release();
	    Sh.m_pShaderResources = 0;
	   }
	   pMtl->SetShaderItem( Sh );

	   if(!(pMtl->GetFlags()&MIF_CHILD))
	   if(!(pMtl->GetFlags()&MIF_WASUSED))
	   {
	    PrintMessage("Warning: CMatMan::~CMatMan: Material was loaded but never used: %s", pMtl->GetName());
	    nNotUsed += (pMtl->GetSubMtlCount()+1);
	    nNotUsedParents++;
	   }
	   if (pMtl->GetNumRefs() > 1)
	   {
	    //
	    PrintMessage("Warning: CMatMan::~CMatMan: Material %s is being referenced", pMtl->GetName());
	   }
	   }
	 */

	if (nNotUsed)
		PrintMessage("Warning: CMatMan::~CMatMan: %d(%d) of %" PRISIZE_T " materials was not used in level",
		             nNotUsedParents, nNotUsed, m_mtlNameMap.size());
}

//////////////////////////////////////////////////////////////////////////
const char* CMatMan::UnifyName(const char* sMtlName)
{
	static char name[260];
	int n = strlen(sMtlName);

	//TODO: change this code to not use a statically allocated buffer and return it ...
	//TODO: provide a general name unification function, which can be used in other places as well and remove this thing
	if (n >= 260)
		Error("Static buffer size exceeded by material name!");

	for (int i = 0; i < n && i < sizeof(name); i++)
	{
		name[i] = CryStringUtils::toLowerAscii(sMtlName[i]);
		if (sMtlName[i] == '.')
		{
			n = i;
			break;
		}
		else if (sMtlName[i] == '\\')
		{
			name[i] = '/';
		}
	}
	PREFAST_ASSUME(n >= 0 && n < 260);
	name[n] = 0;
	return name;
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CMatMan::CreateMaterial(const char* sMtlName, int nMtlFlags)
{
	AUTO_LOCK(m_AccessLock);

	CMatInfo* pMat = new CMatInfo;

	//m_mtlSet.insert( pMat );
	pMat->SetName(sMtlName);
	pMat->SetFlags(nMtlFlags | pMat->GetFlags());
	if (!(nMtlFlags & MTL_FLAG_PURE_CHILD))
	{
		m_mtlNameMap[UnifyName(sMtlName)] = pMat;
	}

	if (nMtlFlags & MTL_FLAG_NON_REMOVABLE)
	{
		// Add reference to this material to prevent its deletion.
		m_nonRemovables.push_back(pMat);
	}

	return pMat;
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::NotifyCreateMaterial(IMaterial* pMtl)
{
	if (m_pListener)
		m_pListener->OnCreateMaterial(pMtl);
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::DelayedMaterialDeletion()
{
	AUTO_LOCK(m_AccessLock);

	uint32 nID = (m_nDelayedDeleteID + 1) % MATERIAL_DELETION_DELAY;

	while (!m_DelayedDeletionMtls[nID].empty())
	{
		CMatInfo* ptr = m_DelayedDeletionMtls[nID].back();
		ptr->ShutDown();
		Unregister(ptr);
		// TODO: CMatInfo pointer leaks
		m_DelayedDeletionMtls[nID].pop_back();
	}

	m_nDelayedDeleteID = nID;
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::ForceDelayedMaterialDeletion()
{
	CRY_ASSERT(m_AccessLock.IsLocked());

	// make sure nothing is in flight on RT if we force delete materials
	if (GetRenderer())
	{
		GetRenderer()->StopLoadtimeFlashPlayback();
		GetRenderer()->FlushRTCommands(true, true, true);
	}

	for (uint32 i = 0; i < MATERIAL_DELETION_DELAY; i++)
	{
		// clear list m_nDelayedDeleteID last because sub materials can still be added there when clearing the other lists
		const int nListIndex = (m_nDelayedDeleteID + 1 + i) % MATERIAL_DELETION_DELAY;
		while (!m_DelayedDeletionMtls[nListIndex].empty())
		{
			CMatInfo* ptr = m_DelayedDeletionMtls[nListIndex].back();
			ptr->ShutDown();
			Unregister(ptr);
			m_DelayedDeletionMtls[nListIndex].pop_back();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::DelayedDelete(CMatInfo* pMat)
{
	AUTO_LOCK(m_AccessLock);

	CRY_ASSERT(m_bInitialized);
	if (!pMat->m_bDeletePending)
	{
		m_DelayedDeletionMtls[m_nDelayedDeleteID].push_back(pMat);
		pMat->m_bDeletePending = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::Unregister(CMatInfo* pMat)
{
	AUTO_LOCK(m_AccessLock);

	assert(pMat);

	if (!(pMat->m_Flags & MTL_FLAG_PURE_CHILD))
		m_mtlNameMap.erase(CONST_TEMP_STRING(UnifyName(pMat->GetName())));

	if (m_pListener)
		m_pListener->OnDeleteMaterial(pMat);
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::RenameMaterial(IMaterial* pMtl, const char* sNewName)
{
	AUTO_LOCK(m_AccessLock);

	assert(pMtl);
	const char* sName = pMtl->GetName();
	if (*sName != '\0')
	{
		m_mtlNameMap.erase(CONST_TEMP_STRING(UnifyName(pMtl->GetName())));
	}
	pMtl->SetName(sNewName);
	m_mtlNameMap[UnifyName(sNewName)] = pMtl;
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CMatMan::FindMaterial(const char* sMtlName) const
{
	AUTO_LOCK(m_AccessLock);

	const char* name = UnifyName(sMtlName);

	MtlNameMap::const_iterator it = m_mtlNameMap.find(CONST_TEMP_STRING(name));

	if (it == m_mtlNameMap.end())
		return 0;

	if (!static_cast<CMatInfo*>(it->second)->IsValid())
		return 0;

	return it->second;
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CMatMan::LoadMaterial(const char* sMtlName, bool bMakeIfNotFound, bool bNonremovable, unsigned long nLoadingFlags)
{
	CRY_ASSERT(!m_AccessLock.IsLocked());

	if (!m_bInitialized)
		InitDefaults();

	if (m_pDefaultMtl && GetCVars()->e_StatObjPreload == 2)
		return m_pDefaultMtl;

	if (IMaterial* pFound = FindMaterial(sMtlName))
		return pFound;

	// TODO: To make LoadMaterial threadsafe the CreateMaterial()-call in MakeMaterialFromXml()
	//       needs to be moved into here (and elsewhere respectively) and put under the m_AccessLock
	//       together with FindMaterial()

	const char* name = UnifyName(sMtlName);
	IMaterial* pMtl = 0;

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Materials");
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_MTL, EMemStatContextFlags::MSF_Instance, "%s", name);
	LOADING_TIME_PROFILE_SECTION_ARGS(sMtlName); // Only profile actually loading of the material.

	CRY_DEFINE_ASSET_SCOPE("Material", sMtlName);

	if (!pMtl)
	{
		if (!(nLoadingFlags & ELoadingFlagsPreviewMode))
		{
			if (m_pListener)
			{
				pMtl = m_pListener->OnLoadMaterial(sMtlName, bMakeIfNotFound, nLoadingFlags);
				if (pMtl)
				{
					AUTO_LOCK(m_AccessLock);

					if (bNonremovable)
						m_nonRemovables.push_back(static_cast<CMatInfo*>(pMtl));

					if (pMtl->GetFlags() & MTL_FLAG_TRACEABLE_TEXTURE)
						pMtl->SetKeepLowResSysCopyForDiffTex();

					return pMtl;
				}
			}
		}

		// Try to load material from file.
		stack_string filename = name;
		stack_string::size_type extPos = filename.find('.');

		// If we've got an alternate material suffix, try appending that on the end of the
		// requested material, and if a material exists with that name use it instead.
		//
		// Note that we still register the material under the original name, so subsequent
		// lookups will get the alternate.  That does mean that if all affected materials
		// aren't released after switching or turning off the alt name the wrong materials
		// will be returned, but since we use the same suffix for an entire level it
		// doesn't seem to be an issue..
		if (!m_altSuffix.empty())
		{
			if (extPos == stack_string::npos)
			{
				filename += m_altSuffix;
				filename += MATERIAL_EXT;
			}
			else
			{
				filename.insert(extPos, m_altSuffix);
			}

			CCryFile testFile;

			// If we find an alt material use it, otherwise switch back to the normal one
			if (testFile.Open(filename.c_str(), "rb"))
			{
				extPos = filename.find('.');
			}
			else
			{
				filename = name;
			}
		}

		if (extPos == stack_string::npos)
			filename += MATERIAL_EXT;

		static int nRecursionCounter = 0;

		bool bCanCleanPools = nRecursionCounter == 0; // Only clean pool, when we are called not recursively.
		nRecursionCounter++;

		XmlNodeRef mtlNode;
		if (m_pXmlParser)
		{
			// WARNING!!!
			// Shared XML parser does not support recursion!!!
			// On parsing a new XML file all previous XmlNodes are invalidated.
			mtlNode = m_pXmlParser->ParseFile(filename.c_str(), bCanCleanPools);
		}
		else
		{
			mtlNode = GetSystem()->LoadXmlFromFile(filename.c_str());
		}

		if (mtlNode)
		{
			pMtl = MakeMaterialFromXml(name, name, mtlNode, false, 0, 0, nLoadingFlags);
		}

		nRecursionCounter--;
	}

#if defined(ENABLE_CONSOLE_MTL_VIZ)
	if (pMtl && CMatInfo::IsConsoleMatModeEnabled())
	{
		assert(gEnv->IsEditor());
		pMtl->LoadConsoleMaterial();
	}
#endif

	if (!pMtl && bMakeIfNotFound)
	{
		pMtl = GetDefaultMaterial();
	}

	if (pMtl)
	{
		AUTO_LOCK(m_AccessLock);

		if (bNonremovable)
			m_nonRemovables.push_back(static_cast<CMatInfo*>(pMtl));

		if (pMtl->GetFlags() & MTL_FLAG_TRACEABLE_TEXTURE)
			pMtl->SetKeepLowResSysCopyForDiffTex();

		return pMtl;
	}

	return pMtl;
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CMatMan::MakeMaterialFromXml(const char* sMtlName, const char* sMtlFilename, XmlNodeRef node, bool bForcePureChild, uint16 sortPrio, IMaterial* pExistingMtl, unsigned long nLoadingFlags, IMaterial* pParentMtl)
{
	CMatMan* pMatMan = static_cast<CMatMan*>(gEnv->p3DEngine->GetMaterialManager());

	int mtlFlags = 0;
	CryFixedStringT<128> shaderName;
	uint64 nShaderGenMask = 0;

	assert(node != 0);

	SInputShaderResourcesPtr sr = GetRenderer() ?
	                              GetRenderer()->EF_CreateInputShaderResource() : nullptr;

	if (sr)
	{
		sr->m_SortPrio = sortPrio;
	}

	// Loading
	node->getAttr("MtlFlags", mtlFlags);
	mtlFlags &= (MTL_FLAGS_SAVE_MASK); // Clean flags that are not supposed to be save/loaded.
	if (bForcePureChild)
		mtlFlags |= MTL_FLAG_PURE_CHILD;

	IMaterial* pMtl = pExistingMtl;
	if (!pMtl)
	{
		pMtl = pMatMan->CreateMaterial(sMtlName, mtlFlags);
	}
	else
	{
		pMtl->SetFlags(mtlFlags | pMtl->GetFlags());
	}

	const char* matTemplate = NULL;

	if (!(mtlFlags & MTL_FLAG_MULTI_SUBMTL))
	{
		matTemplate = node->getAttr("MatTemplate");
		pMtl->SetMatTemplate(matTemplate);
		//
		shaderName = node->getAttr("Shader");

		if (!(mtlFlags & MTL_64BIT_SHADERGENMASK))
		{
			uint32 nShaderGenMask32 = 0;
			node->getAttr("GenMask", nShaderGenMask32);
			nShaderGenMask = nShaderGenMask32;

			// Remap 32bit flags to 64 bit version
			nShaderGenMask = GetRenderer() ?
			                 GetRenderer()->EF_GetRemapedShaderMaskGen((const char*) shaderName, nShaderGenMask) : 0;
			mtlFlags |= MTL_64BIT_SHADERGENMASK;
		}
		else
			node->getAttr("GenMask", nShaderGenMask);

		if (GetRenderer())
		{
			if (node->haveAttr("StringGenMask"))
			{
				const char* pszShaderGenMask = node->getAttr("StringGenMask");
				nShaderGenMask = GetRenderer()->EF_GetShaderGlobalMaskGenFromString((const char*)shaderName, pszShaderGenMask, nShaderGenMask);  // get common mask gen
			}
			else
			{
				// version doens't has string gen mask yet ? Remap flags if needed
				nShaderGenMask = GetRenderer()->EF_GetRemapedShaderMaskGen((const char*)shaderName, nShaderGenMask, ((mtlFlags & MTL_64BIT_SHADERGENMASK) != 0));
			}
		}
		mtlFlags |= MTL_64BIT_SHADERGENMASK;

		const char* surfaceType = node->getAttr("SurfaceType");
		pMtl->SetSurfaceType(surfaceType);

		if (stricmp(shaderName, "nodraw") == 0)
			mtlFlags |= MTL_FLAG_NODRAW;

		pMtl->SetFlags(mtlFlags | pMtl->GetFlags());

		if (sr)
		{
			s_materialHelpers.SetLightingFromXml(*sr, node);
			s_materialHelpers.SetTexturesFromXml(*sr, node, sMtlFilename);
			s_materialHelpers.MigrateXmlLegacyData(*sr, node);

			for (EEfResTextures texId = EFTT_DIFFUSE; texId < EFTT_MAX; texId = EEfResTextures(texId + 1))
			{
				// Ignore textures with drive letters in them
				const char* name = sr->m_Textures[texId].m_Name;
				if (name && (strchr(name, ':') != NULL))
				{
					CryLog("Invalid texture '%s' found in material '%s'", name, sMtlName);
				}
			}
		}

	}

	//////////////////////////////////////////////////////////////////////////
	// Check if we have a link name
	//////////////////////////////////////////////////////////////////////////
	XmlNodeRef pLinkName = node->findChild("MaterialLinkName");
	if (pLinkName)
	{
		const char* szLinkName = pLinkName->getAttr("name");
		pMtl->SetMaterialLinkName(szLinkName);
	}

	if (sr)
	{
		//////////////////////////////////////////////////////////////////////////
		// Check if we have vertex deform.
		//////////////////////////////////////////////////////////////////////////
		s_materialHelpers.SetVertexDeformFromXml(*sr, node);

		//////////////////////////////////////////////////////////////////////////
		// Check for detail decal
		//////////////////////////////////////////////////////////////////////////
		if (mtlFlags & MTL_FLAG_DETAIL_DECAL)
			s_materialHelpers.SetDetailDecalFromXml(*sr, node);
		else
			sr->m_DetailDecalInfo.Reset();

		//////////////////////////////////////////////////////////////////////////
		// Load public parameters.
		XmlNodeRef publicVarsNode = node->findChild("PublicParams");

		//////////////////////////////////////////////////////////////////////////
		// Reload shader item with new resources and shader.
		if (!(mtlFlags & MTL_FLAG_MULTI_SUBMTL))
		{
			sr->m_szMaterialName = sMtlName;
			LoadMaterialShader(pMtl, pParentMtl, shaderName.c_str(), nShaderGenMask, *sr, publicVarsNode, nLoadingFlags);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Load material layers data
	//////////////////////////////////////////////////////////////////////////

	if (pMtl && pMtl->GetShaderItem().m_pShader && pMtl->GetShaderItem().m_pShaderResources)
	{
		XmlNodeRef pMtlLayersNode = node->findChild("MaterialLayers");
		if (pMtlLayersNode)
		{
			int nLayerCount = min((int) MTL_LAYER_MAX_SLOTS, (int) pMtlLayersNode->getChildCount());
			if (nLayerCount)
			{
				uint8 nMaterialLayerFlags = 0;

				pMtl->SetLayerCount(nLayerCount);
				for (int l(0); l < nLayerCount; ++l)
				{
					XmlNodeRef pLayerNode = pMtlLayersNode->getChild(l);
					if (pLayerNode)
					{
						if (const char* pszShaderName = pLayerNode->getAttr("Name"))
						{
							bool bNoDraw = false;
							pLayerNode->getAttr("NoDraw", bNoDraw);

							uint8 nLayerFlags = 0;
							if (bNoDraw)
							{
								nLayerFlags |= MTL_LAYER_USAGE_NODRAW;

								if (!strcmpi(pszShaderName, "frozenlayerwip"))
									nMaterialLayerFlags |= MTL_LAYER_FROZEN;
								else if (!strcmpi(pszShaderName, "cloaklayer"))
									nMaterialLayerFlags |= MTL_LAYER_CLOAK;
							}
							else
								nLayerFlags &= ~MTL_LAYER_USAGE_NODRAW;

							bool bFadeOut = false;
							pLayerNode->getAttr("FadeOut", bFadeOut);
							if (bFadeOut)
							{
								nLayerFlags |= MTL_LAYER_USAGE_FADEOUT;
							}
							else
							{
								nLayerFlags &= ~MTL_LAYER_USAGE_FADEOUT;
							}

							if (sr)
							{
								XmlNodeRef pPublicsParamsNode = pLayerNode->findChild("PublicParams");
								sr->m_szMaterialName = sMtlName;
								LoadMaterialLayerSlot(l, pMtl, pszShaderName, *sr, pPublicsParamsNode, nLayerFlags);
							}
						}
					}
				}

				SShaderItem pShaderItemBase = pMtl->GetShaderItem();
				if (pShaderItemBase.m_pShaderResources)
					pShaderItemBase.m_pShaderResources->SetMtlLayerNoDrawFlags(nMaterialLayerFlags);
			}
		}
	}

	// Serialize sub materials.
	XmlNodeRef childsNode = node->findChild("SubMaterials");
	if (childsNode)
	{
		int nSubMtls = childsNode->getChildCount();
		pMtl->SetSubMtlCount(nSubMtls);
		for (int i = 0; i < nSubMtls; i++)
		{
			XmlNodeRef mtlNode = childsNode->getChild(i);
			if (mtlNode->isTag("Material"))
			{
				const char* name = mtlNode->getAttr("Name");
				IMaterial* pChildMtl = MakeMaterialFromXml(name, sMtlName, mtlNode, true, nSubMtls - i - 1, 0, nLoadingFlags, pMtl);
				if (pChildMtl)
					pMtl->SetSubMtl(i, pChildMtl);
				else
					pMtl->SetSubMtl(i, pMatMan->GetDefaultMaterial());
			}
			else
			{
				const char* name = mtlNode->getAttr("Name");
				if (name[0])
				{
					IMaterial* pChildMtl = pMatMan->LoadMaterial(name, true, false, nLoadingFlags);
					if (pChildMtl)
						pMtl->SetSubMtl(i, pChildMtl);
				}
			}
		}
	}
	//
	if (matTemplate != NULL && strlen(matTemplate) != 0 && strcmp(matTemplate, sMtlName) != 0)
	{
		CMatInfo* pMtlTmpl = NULL;
		pMtlTmpl = static_cast<CMatInfo*>(pMatMan->LoadMaterial(matTemplate, false));
		if (pMtlTmpl)
		{
			pMtlTmpl->Copy(static_cast<CMatInfo*>(pMtl), MTL_COPY_DEFAULT);
			return pMtl;
		}
	}
	//
	return pMtl;
}

//////////////////////////////////////////////////////////////////////////
bool CMatMan::LoadMaterialShader(IMaterial* pMtl, IMaterial* pParentMtl, const char* sShader, uint64 nShaderGenMask, SInputShaderResources& sr, XmlNodeRef& publicsNode, unsigned long nLoadingFlags)
{
	// Mark material invalid by default.
	sr.m_ResFlags = pMtl->GetFlags();

	// Set public params.
	if (publicsNode)
	{
		// Copy public params from the shader.
		//sr.m_ShaderParams = shaderItem.m_pShader->GetPublicParams();
		// Parse public parameters, and assign them to source shader resources.
		ParsePublicParams(sr, publicsNode);
		//shaderItem.m_pShaderResources->SetShaderParams(&sr, shaderItem.m_pShader);
	}

	IRenderer::SLoadShaderItemArgs args(pMtl, pParentMtl);
	SShaderItem shaderItem = gEnv->pRenderer->EF_LoadShaderItem(sShader, false, 0, &sr, nShaderGenMask, &args);
	if (!shaderItem.m_pShader || (shaderItem.m_pShader->GetFlags() & EF_NOTFOUND) != 0)
	{
		Warning("Failed to load shader \"%s\" in material \"%s\"", sShader, pMtl->GetName());
		if (!shaderItem.m_pShader)
			return false;
	}
	pMtl->AssignShaderItem(shaderItem);

	return true;
}

bool CMatMan::LoadMaterialLayerSlot(uint32 nSlot, IMaterial* pMtl, const char* szShaderName, SInputShaderResources& pBaseResources, XmlNodeRef& pPublicsNode, uint8 nLayerFlags)
{
	if (!pMtl || pMtl->GetLayer(nSlot) || !pPublicsNode)
	{
		return false;
	}

	// need to handle no draw case
	if (stricmp(szShaderName, "nodraw") == 0)
	{
		// no shader = skip layer
		return false;
	}

	// Get base material/shaderItem info
	SInputShaderResourcesPtr pInputResources = GetRenderer()->EF_CreateInputShaderResource();
	SShaderItem pShaderItemBase = pMtl->GetShaderItem();

	uint32 nMaskGenBase = (uint32)pShaderItemBase.m_pShader->GetGenerationMask();
	SShaderGen* pShaderGenBase = pShaderItemBase.m_pShader->GetGenerationParams();

	// copy diffuse and bump textures names

	pInputResources->m_szMaterialName = pBaseResources.m_szMaterialName;
	pInputResources->m_Textures[EFTT_DIFFUSE].m_Name = pBaseResources.m_Textures[EFTT_DIFFUSE].m_Name;
	pInputResources->m_Textures[EFTT_NORMALS].m_Name = pBaseResources.m_Textures[EFTT_NORMALS].m_Name;

	// Check if names are valid - else replace with default textures

	if (pInputResources->m_Textures[EFTT_DIFFUSE].m_Name.empty())
	{
		pInputResources->m_Textures[EFTT_DIFFUSE].m_Name = szReplaceMe;
		//		pInputResources->m_Textures[EFTT_DIFFUSE].m_Name = "<default>";
	}

	if (pInputResources->m_Textures[EFTT_NORMALS].m_Name.empty())
	{
		pInputResources->m_Textures[EFTT_NORMALS].m_Name = "%ENGINE%/EngineAssets/Textures/white_ddn.dds";
	}

	// Load layer shader item
	IShader* pNewShader = gEnv->pRenderer->EF_LoadShader(szShaderName, 0);
	if (!pNewShader)
	{
		Warning("Failed to load material layer shader %s in Material %s", szShaderName, pMtl->GetName());
		return false;
	}

	// mask generation for base material shader
	uint32 nMaskGenLayer = 0;
	SShaderGen* pShaderGenLayer = pNewShader->GetGenerationParams();
	if (pShaderGenBase && pShaderGenLayer)
	{
		for (unsigned nLayerBit(0); nLayerBit < pShaderGenLayer->m_BitMask.size(); ++nLayerBit)
		{
			SShaderGenBit* pLayerBit = pShaderGenLayer->m_BitMask[nLayerBit];

			for (unsigned nBaseBit(0); nBaseBit < pShaderGenBase->m_BitMask.size(); ++nBaseBit)
			{
				SShaderGenBit* pBaseBit = pShaderGenBase->m_BitMask[nBaseBit];

				// Need to check if flag name is common to both shaders (since flags values can be different), if so activate it on this layer
				if (nMaskGenBase & pBaseBit->m_Mask)
				{
					if (!pLayerBit->m_ParamName.empty() && !pBaseBit->m_ParamName.empty())
					{
						if (pLayerBit->m_ParamName == pBaseBit->m_ParamName)
						{
							nMaskGenLayer |= pLayerBit->m_Mask;
							break;
						}
					}
				}
			}
		}
	}

	// Reload with proper flags
	IShader* pShader = gEnv->pRenderer->EF_LoadShader(szShaderName, 0, nMaskGenLayer);
	if (!pShader)
	{
		Warning("Failed to load material layer shader %s in Material %s", szShaderName, pMtl->GetName());
		SAFE_RELEASE(pNewShader);
		return false;
	}
	SAFE_RELEASE(pNewShader);

	// Copy public params from the shader.
	//pInputResources->m_ShaderParams = pShaderItem.m_pShader->GetPublicParams();

	// Copy resources from base material
	SShaderItem ShaderItem(pShader, pShaderItemBase.m_pShaderResources->Clone());

	ParsePublicParams(*pInputResources, pPublicsNode);

	// Parse public parameters, and assign them to source shader resources.
	ShaderItem.m_pShaderResources->SetShaderParams(pInputResources, ShaderItem.m_pShader);

	IMaterialLayer* pCurrMtlLayer = pMtl->CreateLayer();

	pCurrMtlLayer->SetFlags(nLayerFlags);
	pCurrMtlLayer->SetShaderItem(pMtl, ShaderItem);

	// Clone returns an instance with a refcount of 1, and SetShaderItem increments it, so
	// we need to release the cloned ref.
	SAFE_RELEASE(ShaderItem.m_pShaderResources);
	SAFE_RELEASE(ShaderItem.m_pShader);

	pMtl->SetLayer(nSlot, pCurrMtlLayer);

	return true;
}

//////////////////////////////////////////////////////////////////////////

static void shGetVector4(const char* buf, float v[4])
{
	if (!buf)
		return;
	int res = sscanf(buf, "%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3]);
	assert(res);
}

void CMatMan::ParsePublicParams(SInputShaderResources& sr, XmlNodeRef paramsNode)
{
	sr.m_ShaderParams.clear();

	int nA = paramsNode->getNumAttributes();
	if (!nA)
		return;

	for (int i = 0; i < nA; i++)
	{
		const char* key = NULL, * val = NULL;
		paramsNode->getAttributeByIndex(i, &key, &val);
		SShaderParam Param;
		assert(val && key);
		cry_strcpy(Param.m_Name, key);
		Param.m_Value.m_Color[0] = Param.m_Value.m_Color[1] = Param.m_Value.m_Color[2] = Param.m_Value.m_Color[3] = 0;
		shGetVector4(val, Param.m_Value.m_Color);
		sr.m_ShaderParams.push_back(Param);
	}
}

//////////////////////////////////////////////////////////////////////////
ISurfaceType* CMatMan::GetSurfaceTypeByName(const char* sSurfaceTypeName, const char* sWhy)
{
	return m_pSurfaceTypeManager->GetSurfaceTypeByName(sSurfaceTypeName, sWhy);
};

//////////////////////////////////////////////////////////////////////////
int CMatMan::GetSurfaceTypeIdByName(const char* sSurfaceTypeName, const char* sWhy)
{
	ISurfaceType* pSurfaceType = m_pSurfaceTypeManager->GetSurfaceTypeByName(sSurfaceTypeName, sWhy);
	if (pSurfaceType)
		return pSurfaceType->GetId();
	return 0;
};

//////////////////////////////////////////////////////////////////////////
IMaterial* CMatMan::GetDefaultLayersMaterial()
{
	if (!m_bInitialized)
		InitDefaults();

	return m_pDefaultLayersMtl;
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CMatMan::GetDefaultHelperMaterial()
{
	if (!m_bInitialized)
		InitDefaults();

	return m_pDefaultHelperMtl;
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::GetLoadedMaterials(IMaterial** pData, uint32& nObjCount) const
{
	AUTO_LOCK(m_AccessLock);

	nObjCount = 0;
	for (const auto& it : m_mtlNameMap)
	{
		if (static_cast<CMatInfo*>(it.second)->IsValid())
		{
			++nObjCount;

			if (pData)
			{
				*pData++ = it.second;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::SetAltMaterialSuffix(const char* pSuffix)
{
	AUTO_LOCK(m_AccessLock);

	if (!pSuffix)
		m_altSuffix.clear();
	else
		m_altSuffix = pSuffix;
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CMatMan::CloneMaterial(IMaterial* pSrcMtl, int nSubMtl)
{
	if (pSrcMtl->GetFlags() & MTL_FLAG_MULTI_SUBMTL)
	{
		IMaterial* pMultiMat = new CMatInfo;

		//m_mtlSet.insert( pMat );
		pMultiMat->SetName(pSrcMtl->GetName());
		pMultiMat->SetFlags(pMultiMat->GetFlags() | MTL_FLAG_MULTI_SUBMTL);

		bool bCloneAllSubMtls = nSubMtl < 0;

		int nSubMtls = pSrcMtl->GetSubMtlCount();
		pMultiMat->SetSubMtlCount(nSubMtls);
		for (int i = 0; i < nSubMtls; i++)
		{
			CMatInfo* pChildSrcMtl = (CMatInfo*)pSrcMtl->GetSubMtl(i);
			if (!pChildSrcMtl)
				continue;
			if (bCloneAllSubMtls)
			{
				pMultiMat->SetSubMtl(i, pChildSrcMtl->Clone((CMatInfo*)pMultiMat));
			}
			else
			{
				pMultiMat->SetSubMtl(i, pChildSrcMtl);
				if (i == nSubMtls)
				{
					// Clone this slot.
					pMultiMat->SetSubMtl(i, pChildSrcMtl->Clone((CMatInfo*)pMultiMat));
				}
			}
		}
		return pMultiMat;
	}
	else
	{
		return ((CMatInfo*)pSrcMtl)->Clone(0);
	}
}

void CMatMan::CopyMaterial(IMaterial* pMtlSrc, IMaterial* pMtlDest, EMaterialCopyFlags flags)
{
	return ((CMatInfo*)pMtlSrc)->Copy(pMtlDest, flags);
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CMatMan::CloneMultiMaterial(IMaterial* pSrcMtl, const char* sSubMtlName)
{
	if (pSrcMtl->GetFlags() & MTL_FLAG_MULTI_SUBMTL)
	{
		IMaterial* pMultiMat = new CMatInfo;

		//m_mtlSet.insert( pMat );
		pMultiMat->SetName(pSrcMtl->GetName());
		pMultiMat->SetFlags(pMultiMat->GetFlags() | MTL_FLAG_MULTI_SUBMTL);

		bool bCloneAllSubMtls = sSubMtlName == 0;

		int nSubMtls = pSrcMtl->GetSubMtlCount();
		pMultiMat->SetSubMtlCount(nSubMtls);
		for (int i = 0; i < nSubMtls; i++)
		{
			CMatInfo* pChildSrcMtl = (CMatInfo*)pSrcMtl->GetSubMtl(i);
			if (!pChildSrcMtl)
				continue;
			if (bCloneAllSubMtls)
			{
				pMultiMat->SetSubMtl(i, pChildSrcMtl->Clone((CMatInfo*)pMultiMat));
			}
			else
			{
				pMultiMat->SetSubMtl(i, pChildSrcMtl);
				if (stricmp(pChildSrcMtl->GetName(), sSubMtlName) == 0)
				{
					// Clone this slot.
					pMultiMat->SetSubMtl(i, pChildSrcMtl->Clone((CMatInfo*)pMultiMat));
				}
			}
		}
		return pMultiMat;
	}
	else
	{
		return ((CMatInfo*)pSrcMtl)->Clone(0);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::DoLoadSurfaceTypesInInit(bool doLoadSurfaceTypesInInit)
{
	m_bLoadSurfaceTypesInInit = doLoadSurfaceTypesInInit;
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::InitDefaults()
{
	if (m_bInitialized)
		return;
	m_bInitialized = true;

	LOADING_TIME_PROFILE_SECTION;

	SYNCHRONOUS_LOADING_TICK();

	if (m_bLoadSurfaceTypesInInit)
	{
		m_pSurfaceTypeManager->LoadSurfaceTypes();
	}

	auto CreateDefaultMaterialWithShader = [](const char* mat_name, const char* shader_name)
	{
		_smart_ptr<CMatInfo> mi = new CMatInfo();
		mi->SetName(mat_name);
		if (GetRenderer())
		{
			SInputShaderResourcesPtr pIsr = GetRenderer()->EF_CreateInputShaderResource();
			pIsr->m_LMaterial.m_Opacity = 1;
			pIsr->m_LMaterial.m_Diffuse.set(1, 1, 1, 1);
			pIsr->m_Textures[EFTT_DIFFUSE].m_Name = szReplaceMe;
			SShaderItem si = GetRenderer()->EF_LoadShaderItem(shader_name, true, 0, pIsr);
			if (si.m_pShaderResources)
				si.m_pShaderResources->SetMaterialName(mat_name);
			mi->AssignShaderItem(si);
		}
		return mi;
	};

	if (!m_pDefaultMtl)
	{
		// This line is REQUIRED by the buildbot testing framework to determine when tests have formally started. Please inform WillW or Morgan before changing this.
		CryLogAlways("Initializing default materials...");
		m_pDefaultMtl = CreateDefaultMaterialWithShader("Default", "Illum");
	}

	if (!m_pDefaultTerrainLayersMtl)
	{
		m_pDefaultTerrainLayersMtl = CreateDefaultMaterialWithShader("DefaultTerrainLayer", "Terrain.Layer");
	}

	if (!m_pDefaultLayersMtl)
	{
		m_pDefaultLayersMtl = LoadMaterial("Materials/material_layers_default", false);
	}

	if (!m_pNoDrawMtl)
	{
		m_pNoDrawMtl = new CMatInfo;
		m_pNoDrawMtl->SetFlags(MTL_FLAG_NODRAW);
		m_pNoDrawMtl->SetName(MATERIAL_NODRAW);
		if (GetRenderer())
		{
			SShaderItem si;
			si.m_pShader = GetRenderer()->EF_LoadShader(MATERIAL_NODRAW, 0);
			m_pNoDrawMtl->AssignShaderItem(si);
		}
		m_mtlNameMap[UnifyName(m_pNoDrawMtl->GetName())] = m_pNoDrawMtl;
	}

	if (!m_pDefaultHelperMtl)
	{
		m_pDefaultHelperMtl = CreateDefaultMaterialWithShader("DefaultHelper", "Helper");
	}

	SLICE_AND_SLEEP();
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CMatMan::LoadCGFMaterial(const char* szMaterialName, const char* szCgfFilename, unsigned long nLoadingFlags)
{
	FUNCTION_PROFILER_3DENGINE;

	CryPathString sMtlName = szMaterialName;
	if (sMtlName.find('/') == stack_string::npos)
	{
		// If no slashes in the name assume it is in same folder as a cgf.
		sMtlName = PathUtil::AddSlash(PathUtil::GetPathWithoutFilename(stack_string(szCgfFilename))) + sMtlName;
	}
	else
	{
		sMtlName = PathUtil::MakeGamePath(sMtlName);
	}

	return LoadMaterial(sMtlName.c_str(), true, false, nLoadingFlags);
}

//////////////////////////////////////////////////////////////////////////

namespace
{
static bool IsPureChild(IMaterial* pMtl)
{
	return (pMtl->GetFlags() & MTL_FLAG_PURE_CHILD) ? true : false;
}
static bool IsMultiSubMaterial(IMaterial* pMtl)
{
	return (pMtl->GetFlags() & MTL_FLAG_MULTI_SUBMTL) ? true : false;
};
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CMatMan::LoadMaterialFromXml(const char* sMtlName, XmlNodeRef mtlNode)
{
	IMaterial* pMtl = FindMaterial(sMtlName);
	const char* name = UnifyName(sMtlName);

	if (pMtl)
	{
		pMtl = MakeMaterialFromXml(name, name, mtlNode, false, 0, pMtl);
	}
	else
	{
		pMtl = MakeMaterialFromXml(name, name, mtlNode, false);
	}

	return pMtl;
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::PreloadLevelMaterials()
{
	LOADING_TIME_PROFILE_SECTION;

	//bool bMtlfCacheExist = GetISystem()->GetIResourceManager()->LoadLevelCachePak( MTL_LEVEL_CACHE_PAK,"" );
	//if (!bMtlfCacheExist)
	//return;

	PrintMessage("==== Starting Loading Level Materials ====");
	float fStartTime = GetCurAsyncTimeSec();

	IResourceList* pResList = GetISystem()->GetIResourceManager()->GetLevelResourceList();

	if (!pResList)
	{
		Error("Error loading level Materials: resource list is NULL");
		return;
	}

	int nCounter = 0;
	int nInLevelCacheCount = 0;

	_smart_ptr<IXmlParser> pXmlParser = GetISystem()->GetXmlUtils()->CreateXmlParser();

	// Request objects loading from Streaming System.
	CryPathString mtlName;
	CryPathString mtlFilename;
	CryPathString mtlCacheFilename;
	for (const char* sName = pResList->GetFirst(); sName != NULL; sName = pResList->GetNext())
	{
		if (strstr(sName, ".mtl") == 0 && strstr(sName, ".binmtl") == 0)
			continue;

		mtlFilename = sName;

		mtlName = sName;
		PathUtil::RemoveExtension(mtlName);

		if (FindMaterial(mtlName))
			continue;

		// Load this material as un-removable.
		IMaterial* pMtl = LoadMaterial(mtlName, false, true);
		if (pMtl)
		{
			nCounter++;
		}

		//This loop can take a few seconds, so we should refresh the loading screen and call the loading tick functions to ensure that no big gaps in coverage occur.
		SYNCHRONOUS_LOADING_TICK();
	}

	//GetISystem()->GetIResourceManager()->UnloadLevelCachePak( MTL_LEVEL_CACHE_PAK );
	PrintMessage("==== Finished loading level Materials: %d  mtls loaded (%d from LevelCache) in %.1f sec ====", nCounter, nInLevelCacheCount, GetCurAsyncTimeSec() - fStartTime);
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::PreloadDecalMaterials()
{
	LOADING_TIME_PROFILE_SECTION;

	float fStartTime = GetCurAsyncTimeSec();

	bool bVerboseLogging = GetCVars()->e_StatObjPreload > 1;
	int nCounter = 0;

	// Wildcards load.
	CryPathString sPath = PathUtil::Make(CryPathString(MATERIAL_DECALS_FOLDER), CryPathString(MATERIAL_DECALS_SEARCH_WILDCARD));
	PrintMessage("===== Loading all Decal materials from a folder: %s =====", sPath.c_str());

	std::vector<string> mtlFiles;
	SDirectoryEnumeratorHelper dirHelper;
	dirHelper.ScanDirectoryRecursive("", MATERIAL_DECALS_FOLDER, MATERIAL_DECALS_SEARCH_WILDCARD, mtlFiles);

	for (int i = 0, num = (int)mtlFiles.size(); i < num; i++)
	{
		CryPathString sMtlName = mtlFiles[i];
		PathUtil::RemoveExtension(sMtlName);

		if (bVerboseLogging)
		{
			CryLog("Preloading Decal Material: %s", sMtlName.c_str());
		}

		IMaterial* pMtl = LoadMaterial(sMtlName.c_str(), false, true); // Load material as non-removable
		if (pMtl)
		{
			nCounter++;
		}
	}
	PrintMessage("==== Finished Loading Decal Materials: %d  mtls loaded in %.1f sec ====", nCounter, GetCurAsyncTimeSec() - fStartTime);
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::ShutDown()
{
	AUTO_LOCK(m_AccessLock);

	m_pXmlParser = 0;
	stl::free_container(m_nonRemovables);
	m_mtlNameMap.clear();

	// Free default materials
	if (m_pDefaultMtl)
	{
		m_pDefaultMtl->ShutDown();
		m_pDefaultMtl = 0;
	}

	if (m_pDefaultTerrainLayersMtl)
	{
		m_pDefaultTerrainLayersMtl->ShutDown();
		m_pDefaultTerrainLayersMtl = 0;
	}

	if (m_pNoDrawMtl)
	{
		m_pNoDrawMtl->ShutDown();
		m_pNoDrawMtl = 0;
	}

	if (m_pDefaultHelperMtl)
	{
		m_pDefaultHelperMtl->ShutDown();
		m_pDefaultHelperMtl = 0;
	}

	m_pDefaultLayersMtl = 0; // Created using LoadMaterial func, so will be freed in FreeAllMaterials

	FreeAllMaterials();

	m_pSurfaceTypeManager->RemoveAll();
	m_bInitialized = false;
}

//////////////////////////////////////////////////////////////////////////
void CMatMan::FreeAllMaterials()
{
	CRY_ASSERT(m_AccessLock.IsLocked());

#ifndef _RELEASE
	{
		std::vector<IMaterial*> Materials;
		uint32 nObjCount = 0;
		GetLoadedMaterials(0, nObjCount);
		Materials.resize(nObjCount);
		if (nObjCount > 0)
		{
			GetLoadedMaterials(&Materials[0], nObjCount);
			Warning("CObjManager::CheckMaterialLeaks: %d materials(s) found in memory", nObjCount);
			for (uint32 i = 0; i < nObjCount; i++)
			{
				Warning("Material not deleted: %s (RefCount: %d)", Materials[i]->GetName(), Materials[i]->GetNumRefs());
	#ifdef TRACE_MATERIAL_LEAKS
				Warning("      RefCount: %d, callstack : %s,", Materials[i]->GetNumRefs(), Materials[i]->GetLoadingCallstack());
	#endif
			}
		}
	}
#endif //_RELEASE

	//////////////////////////////////////////////////////////////////////////

	ForceDelayedMaterialDeletion();
	CMatInfo* pMtl = CMatInfo::get_intrusive_list_root();

	// Shut down all materials
	pMtl = CMatInfo::get_intrusive_list_root();
	while (pMtl)
	{
		pMtl->ShutDown();
		pMtl = pMtl->get_next_intrusive();
	}
}

void CMatMan::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, sizeof(*this));
	pSizer->AddObject(m_pDefaultMtl);
	pSizer->AddObject(m_pDefaultLayersMtl);
	pSizer->AddObject(m_pDefaultTerrainLayersMtl);
	pSizer->AddObject(m_pNoDrawMtl);
	pSizer->AddObject(m_pDefaultHelperMtl);
	pSizer->AddObject(m_pSurfaceTypeManager);
	pSizer->AddObject(m_pXmlParser);

	pSizer->AddObject(m_mtlNameMap);
	pSizer->AddObject(m_nonRemovables);
}

void CMatMan::UpdateShaderItems()
{
	AUTO_LOCK(m_AccessLock);

	for (MtlNameMap::iterator iter = m_mtlNameMap.begin(); iter != m_mtlNameMap.end(); ++iter)
	{
		CMatInfo* pMaterial = static_cast<CMatInfo*>(iter->second);
		pMaterial->UpdateShaderItems();
	}
}

void CMatMan::RefreshMaterialRuntime()
{
	RefreshShaderResourceConstants();
}

void CMatMan::RefreshShaderResourceConstants()
{
	AUTO_LOCK(m_AccessLock);

	for (MtlNameMap::iterator iter = m_mtlNameMap.begin(); iter != m_mtlNameMap.end(); ++iter)
	{
		CMatInfo* pMaterial = static_cast<CMatInfo*>(iter->second);
		pMaterial->RefreshShaderResourceConstants();
	}
}
