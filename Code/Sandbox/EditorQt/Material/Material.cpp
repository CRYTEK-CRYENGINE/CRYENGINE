// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Material.h"
#include "MaterialHelpers.h"
#include "MaterialManager.h"
#include "BaseLibrary.h"
#include "AssetSystem/Asset.h"

#include <Cry3DEngine/I3DEngine.h>
#include <CryCore/Containers/CryArray.h>
#include <Cry3DEngine/CGF/CryHeaders.h>
#include <CryAnimation/ICryAnimation.h>
#include <ISourceControl.h>
#include <CryRenderer/IShader.h>
#include <FilePathUtil.h>


SMaterialLayerResources::SMaterialLayerResources()
	: m_nFlags(MTL_LAYER_USAGE_REPLACEBASE), m_bRegetPublicParams(true), m_pMatLayer(0)
{
	m_shaderResources = gEnv->pRenderer->EF_CreateInputShaderResource();
}

void SMaterialLayerResources::SetFromUI(const char* shaderName, bool bNoDraw, bool bFadeOut)
{
	if (m_shaderName != shaderName)
	{
		m_shaderName = shaderName;
		m_bRegetPublicParams = true;
	}

	if (bNoDraw)
		m_nFlags |= MTL_LAYER_USAGE_NODRAW;
	else
		m_nFlags &= ~MTL_LAYER_USAGE_NODRAW;

	if (bFadeOut)
		m_nFlags |= MTL_LAYER_USAGE_FADEOUT;
	else
		m_nFlags &= ~MTL_LAYER_USAGE_FADEOUT;
}

//////////////////////////////////////////////////////////////////////////
CMaterial::CMaterial(const string& name, int nFlags)
	: m_highlightFlags(0)
{
	m_scFileAttributes = SCC_FILE_ATTRIBUTE_NORMAL;

	m_pParent = 0;

	m_shaderResources = gEnv->pRenderer->EF_CreateInputShaderResource();
	m_shaderResources->m_LMaterial.m_Opacity = 1;
	m_shaderResources->m_CloakAmount = 0;
	m_shaderResources->m_LMaterial.m_Diffuse.set(1.0f, 1.0f, 1.0f, 1.0f);
	m_shaderResources->m_LMaterial.m_Smoothness = 10.0f;

	m_mtlFlags = nFlags;
	ZeroStruct(m_shaderItem);

	// Default shader.
	m_shaderName = "Illum";
	m_nShaderGenMask = 0;

	m_name = name;
	m_bRegetPublicParams = true;
	m_bKeepPublicParamsValues = false;
	m_bIgnoreNotifyChange = false;
	m_bDummyMaterial = false;
	m_bFromEngine = false;

	m_propagationFlags = 0;

	m_allowLayerActivation = true;
}

//////////////////////////////////////////////////////////////////////////
CMaterial::~CMaterial()
{
	if (IsModified())
	{
		Save(false);
	}

	// Release used shader.
	SAFE_RELEASE(m_shaderItem.m_pShader);
	SAFE_RELEASE(m_shaderItem.m_pShaderResources);

	if (m_pMatInfo)
	{
		m_pMatInfo->SetUserData(0);
		m_pMatInfo.reset();
	}

	if (!m_subMaterials.empty())
	{
		for (int i = 0; i < m_subMaterials.size(); i++)
		{
			if (m_subMaterials[i])
			{
				m_subMaterials[i]->m_pParent = NULL;
			}
		}

		m_subMaterials.clear();
	}

	if (!IsPureChild() && !(GetFlags() & MTL_FLAG_UIMATERIAL))
	{
		// Unregister this material from manager.
		// Don't use here local variable m_pManager. Manager can be destroyed.
		if (GetIEditorImpl()->GetMaterialManager())
		{
			GetIEditorImpl()->GetMaterialManager()->DeleteItem(this);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::SetName(const string& name)
{
	if (name != m_name)
	{
		string oldName = GetFullName();
		m_name = name;

		if (!IsPureChild())
		{
			if (GetIEditorImpl()->GetMaterialManager())
			{
				GetIEditorImpl()->GetMaterialManager()->OnRenameItem(this, oldName);
			}

			if (m_pMatInfo)
			{
				GetIEditorImpl()->Get3DEngine()->GetMaterialManager()->RenameMaterial(m_pMatInfo, GetName());
			}
		}
		else
		{
			if (m_pMatInfo)
			{
				m_pMatInfo->SetName(m_name);
			}
		}

		NotifyChanged();
	}

	if (m_shaderItem.m_pShaderResources)
	{
		// Only for correct warning message purposes.
		m_shaderItem.m_pShaderResources->SetMaterialName(m_name);
	}
}

void CMaterial::RenameSubMaterial(CMaterial* pSubMaterial, const string& newName)
{
	class RenameSubMaterialUndo : public IUndoObject
	{
	public:
		RenameSubMaterialUndo(CMaterial* pMaterial, CMaterial* pSubMaterial)
			: m_subMtlIndex(-1)
		{
			assert(pMaterial && pMaterial->IsMultiSubMaterial());
			m_mtlName = pMaterial->GetName();

			m_subMtlIndex = pMaterial->FindSubMaterial(pSubMaterial);

			assert(m_subMtlIndex != -1);
			m_undoName = pSubMaterial->GetName();
		}

	protected:
		virtual void Release() { delete this; };

		virtual const char* GetDescription() { return "Rename Sub-Material"; };

		virtual void        Undo(bool bUndo)
		{
			CMaterial* pMaterial = (CMaterial*)GetIEditorImpl()->GetMaterialManager()->FindItemByName(m_mtlName);
			assert(pMaterial);

			CMaterial* pSubMaterial = pMaterial->GetSubMaterial(m_subMtlIndex);
			assert(pSubMaterial);

			if (!pMaterial || !pSubMaterial)
				return;

			if (bUndo)
			{
				m_redoName = pSubMaterial->GetName();
			}

			pSubMaterial->SetName(m_undoName);

			if (bUndo)
			{
				GetIEditorImpl()->GetMaterialManager()->OnUpdateProperties(pMaterial);
			}
		}

		virtual void Redo()
		{
			CMaterial* pMaterial = (CMaterial*)GetIEditorImpl()->GetMaterialManager()->FindItemByName(m_mtlName);
			assert(pMaterial);

			CMaterial* pSubMaterial = pMaterial->GetSubMaterial(m_subMtlIndex);
			assert(pSubMaterial);

			if (!pMaterial || !pSubMaterial)
				return;

			pSubMaterial->SetName(m_redoName);
			GetIEditorImpl()->GetMaterialManager()->OnUpdateProperties(pMaterial);
		}

	private:
		string    m_mtlName;
		int		  m_subMtlIndex;
		string	  m_redoName;
		string    m_undoName;
	};

	CRY_ASSERT(pSubMaterial && pSubMaterial->IsPureChild() && pSubMaterial->GetParent() == this);

	CUndo undo("Rename Sub-Material");
	CUndo::Record(new RenameSubMaterialUndo(this, pSubMaterial));
	pSubMaterial->SetName(newName);
}

void CMaterial::RenameSubMaterial(int subMtlSlot, const string& newName)
{
	RenameSubMaterial(m_subMaterials[subMtlSlot], newName);
}

//////////////////////////////////////////////////////////////////////////
string CMaterial::GetFilename(bool bForWriting) const
{
	return GetIEditorImpl()->GetMaterialManager()->MaterialToFilename(IsPureChild() && m_pParent ? m_pParent->m_name : m_name, bForWriting);
}

//////////////////////////////////////////////////////////////////////////
int CMaterial::GetTextureFilenames(std::vector<string>& outFilenames) const
{
	for (int i = 0; i < EFTT_MAX; ++i)
	{
		CString name = m_shaderResources->m_Textures[i].m_Name;
		if (name.IsEmpty())
		{
			continue;
		}

		// Collect .tif filenames
		if (stricmp(PathUtil::GetExt(name), "tif") == 0 ||
		    stricmp(PathUtil::GetExt(name), "hdr") == 0)
		{
			stl::push_back_unique(outFilenames, PathUtil::GamePathToCryPakPath(name.GetString()).c_str());
		}

		// collect source files used in DCC tools
		CString dccFilename;
		if (CFileUtil::CalculateDccFilename(name, dccFilename))
		{
			stl::push_back_unique(outFilenames, PathUtil::GamePathToCryPakPath(dccFilename.GetString(), true).c_str());
		}
	}

	if (IsMultiSubMaterial())
	{
		for (int i = 0; i < GetSubMaterialCount(); ++i)
		{
			CMaterial* pSubMtl = GetSubMaterial(i);
			if (pSubMtl)
			{
				pSubMtl->GetTextureFilenames(outFilenames);
			}
		}
	}

	return outFilenames.size();
}

//////////////////////////////////////////////////////////////////////////
int CMaterial::GetAnyTextureFilenames(std::vector<string>& outFilenames) const
{
	for (int i = 0; i < EFTT_MAX; ++i)
	{
		string name = m_shaderResources->m_Textures[i].m_Name;
		if (name.IsEmpty())
		{
			continue;
		}

		// Collect any filenames
		stl::push_back_unique(outFilenames, PathUtil::GamePathToCryPakPath(name.GetString()).c_str());
	}

	if (IsMultiSubMaterial())
	{
		for (int i = 0; i < GetSubMaterialCount(); ++i)
		{
			CMaterial* pSubMtl = GetSubMaterial(i);
			if (pSubMtl)
			{
				pSubMtl->GetAnyTextureFilenames(outFilenames);
			}
		}
	}

	return outFilenames.size();
}

int CMaterial::GetTextureDependencies(std::vector<SAssetDependencyInfo>& outFilenames) const
{
	for (int i = 0; i < EFTT_MAX; ++i)
	{
		string name = m_shaderResources->m_Textures[i].m_Name;
		if (name.IsEmpty())
		{
			continue;
		}

		// Collect any filenames

		const string path = PathUtil::ToGamePath(name.GetString());
		auto it = std::find_if(outFilenames.begin(), outFilenames.end(), [&path](const SAssetDependencyInfo& x)
		{
			return x.path.CompareNoCase(path) == 0;
		});

		if (it == outFilenames.end())
		{
			outFilenames.emplace_back(path, 1);
		}
		else // increment instance count
		{
			++(it->usageCount);
		}
	}

	if (IsMultiSubMaterial())
	{
		for (int i = 0; i < GetSubMaterialCount(); ++i)
		{
			CMaterial* pSubMtl = GetSubMaterial(i);
			if (pSubMtl)
			{
				pSubMtl->GetTextureDependencies(outFilenames);
			}
		}
	}

	return outFilenames.size();
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::UpdateFileAttributes()
{
	string filename = GetFilename();
	if (filename.IsEmpty())
	{
		return;
	}

	m_scFileAttributes = CFileUtil::GetAttributes(filename);
}

//////////////////////////////////////////////////////////////////////////
uint32 CMaterial::GetFileAttributes()
{
	if (IsDummy())
	{
		return m_scFileAttributes;
	}

	if (IsPureChild() && m_pParent)
	{
		return m_pParent->GetFileAttributes();
	}

	UpdateFileAttributes();
	return m_scFileAttributes;
};

//////////////////////////////////////////////////////////////////////////
void CMaterial::SetShaderName(const string& shaderName)
{
	if (m_shaderName != shaderName)
	{
		m_bRegetPublicParams = true;
		m_bKeepPublicParamsValues = false;

		RecordUndo("Change Shader");
	}

	m_shaderName = shaderName;
	if (stricmp(m_shaderName, "nodraw") == 0)
		m_mtlFlags |= MTL_FLAG_NODRAW;
	else
		m_mtlFlags &= ~MTL_FLAG_NODRAW;
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::CheckSpecialConditions()
{
	if (stricmp(m_shaderName, "nodraw") == 0)
		m_mtlFlags |= MTL_FLAG_NODRAW;
	else
		m_mtlFlags &= ~MTL_FLAG_NODRAW;

	// If environment texture name have nearest cube-map in it, force material to use auto cube-map for it.
	if (!m_shaderResources->m_Textures[EFTT_ENV].m_Name.empty())
	{
		const char* sAtPos;

		sAtPos = strstr(m_shaderResources->m_Textures[EFTT_ENV].m_Name.c_str(), "auto_2d");
		if (sAtPos)
			m_shaderResources->m_Textures[EFTT_ENV].m_Sampler.m_eTexType = eTT_Auto2D;  // Force Auto-2D
	}

	// Force auto 2D map if user sets texture type
	if (m_shaderResources->m_Textures[EFTT_ENV].m_Sampler.m_eTexType == eTT_Auto2D)
	{
		m_shaderResources->m_Textures[EFTT_ENV].m_Name = "auto_2d";
	}

	// Force nearest cube map if user sets texture type
	if (m_shaderResources->m_Textures[EFTT_ENV].m_Sampler.m_eTexType == eTT_NearestCube)
	{
		m_shaderResources->m_Textures[EFTT_ENV].m_Name = "nearest_cubemap";
		m_mtlFlags |= MTL_FLAG_REQUIRE_NEAREST_CUBEMAP;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CMaterial::LoadShader()
{
	if (m_bDummyMaterial)
		return true;

	CheckSpecialConditions();

	//	if (shaderName.IsEmpty())
	//		return false;

	/*
	   if (m_mtlFlags & MTL_FLAG_LIGHTING)
	   m_shaderResources->m_LMaterial = &m_lightMaterial;
	   else
	   m_shaderResources->m_LMaterial = 0;
	 */

	m_shaderResources->m_ResFlags = m_mtlFlags;

	string sShader = m_shaderName;
	if (sShader.IsEmpty())
	{
		sShader = "<Default>";
	}

	m_shaderResources->m_szMaterialName = m_name;
	SShaderItem newShaderItem = GetIEditorImpl()->GetRenderer()->EF_LoadShaderItem(sShader, false, 0, m_shaderResources, m_nShaderGenMask);

	// Shader loading is an async RT command so we need to wait for the render thread
	GetIEditorImpl()->GetRenderer()->FlushRTCommands(true, true, true);

	// Shader not found
	if (newShaderItem.m_pShader && (newShaderItem.m_pShader->GetFlags() & EF_NOTFOUND) != 0)
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Failed to load shader \"%s\" in material \"%s\"", newShaderItem.m_pShader->GetName(), m_name);

	//if (newShaderItem.m_pShader)
	//{
	//  newShaderItem.m_pShader->AddRef();
	//}

	//if (newShaderItem.m_pShaderResources)
	//{
	//  newShaderItem.m_pShaderResources->AddRef();
	//}

	if (m_shaderItem.m_pShaderResources)
	{
		// Mark previous resource to be invalid
		m_shaderItem.m_pShaderResources->SetInvalid();
	}

	// Release previously used shader (Must be After new shader is loaded, for speed).
	SAFE_RELEASE(m_shaderItem.m_pShader);
	SAFE_RELEASE(m_shaderItem.m_pShaderResources);

	m_shaderItem = newShaderItem;
	if (!m_shaderItem.m_pShader)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Failed to Load Shader %s", (const char*)m_shaderName);
		return false;
	}

	IShader* pShader = m_shaderItem.m_pShader;
	m_nShaderGenMask = pShader->GetGenerationMask();
	if (pShader->GetFlags() & EF_NOPREVIEW)
		m_mtlFlags |= MTL_FLAG_NOPREVIEW;
	else
		m_mtlFlags &= ~MTL_FLAG_NOPREVIEW;

	CMaterial* pMtlTmpl = NULL;
	if (!m_matTemplate.IsEmpty())
	{
		pMtlTmpl = GetIEditorImpl()->GetMaterialManager()->LoadMaterial(m_matTemplate, false);
	}

	//////////////////////////////////////////////////////////////////////////
	// Reget shader parms.
	//////////////////////////////////////////////////////////////////////////
	if (m_bRegetPublicParams)
	{
		if (m_bKeepPublicParamsValues)
		{
			m_bKeepPublicParamsValues = false;
			m_publicVarsCache = XmlHelpers::CreateXmlNode("PublicParams");

			MaterialHelpers::SetXmlFromShaderParams(*m_shaderResources, m_publicVarsCache);
		}

		pShader->CopyPublicParamsTo(*m_shaderResources);
		m_bRegetPublicParams = false;
	}

	//////////////////////////////////////////////////////////////////////////
	// If we have XML node with public parameters loaded, apply it on shader parms.
	//////////////////////////////////////////////////////////////////////////
	if (m_publicVarsCache)
	{
		MaterialHelpers::SetShaderParamsFromXml(*m_shaderResources, m_publicVarsCache);
		GetIEditorImpl()->GetMaterialManager()->OnUpdateProperties(this);
		m_publicVarsCache = 0;
	}

	//////////////////////////////////////////////////////////////////////////
	// Set shader parms.
	if (m_shaderItem.m_pShaderResources)
	{
		m_shaderItem.m_pShaderResources->SetShaderParams(m_shaderResources, m_shaderItem.m_pShader);
	}
	//////////////////////////////////////////////////////////////////////////

	gEnv->pRenderer->UpdateShaderItem(&m_shaderItem, NULL);

	//////////////////////////////////////////////////////////////////////////
	// Set Shader Params for material layers
	//////////////////////////////////////////////////////////////////////////
	if (m_pMatInfo)
	{
		UpdateMatInfo();
	}

	GetIEditorImpl()->GetMaterialManager()->OnLoadShader(this);

	return true;
}

bool CMaterial::LoadMaterialLayers()
{
	if (!m_pMatInfo)
	{
		return false;
	}

	if (m_shaderItem.m_pShader && m_shaderItem.m_pShaderResources)
	{
		// mask generation for base material shader
		uint32 nMaskGenBase = m_shaderItem.m_pShader->GetGenerationMask();
		SShaderGen* pShaderGenBase = m_shaderItem.m_pShader->GetGenerationParams();

		for (uint32 l(0); l < MTL_LAYER_MAX_SLOTS; ++l)
		{
			SMaterialLayerResources* pCurrLayer = &m_pMtlLayerResources[l];
			pCurrLayer->m_nFlags |= MTL_FLAG_NODRAW;
			if (!pCurrLayer->m_shaderName.IsEmpty())
			{
				if (strcmpi(pCurrLayer->m_shaderName, "nodraw") == 0)
				{
					// no shader = skip layer
					pCurrLayer->m_shaderName.Empty();
					continue;
				}

				IShader* pNewShader = GetIEditorImpl()->GetRenderer()->EF_LoadShader(pCurrLayer->m_shaderName, 0);

				// Check if shader loaded
				if (!pNewShader || (pNewShader->GetFlags() & EF_NOTFOUND) != 0)
				{
					CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Failed to load material layer shader \"%s\" in material \"%s\"", pCurrLayer->m_shaderName, m_pMatInfo->GetName());
					if (!pNewShader)
						continue;
				}

				if (!pCurrLayer->m_pMatLayer)
				{
					pCurrLayer->m_pMatLayer = m_pMatInfo->CreateLayer();
				}

				// mask generation for base material shader
				uint64 nMaskGenLayer = 0;
				SShaderGen* pShaderGenLayer = pNewShader->GetGenerationParams();
				if (pShaderGenBase && pShaderGenLayer)
				{
					for (int nLayerBit(0); nLayerBit < pShaderGenLayer->m_BitMask.size(); ++nLayerBit)
					{
						SShaderGenBit* pLayerBit = pShaderGenLayer->m_BitMask[nLayerBit];

						for (int nBaseBit(0); nBaseBit < pShaderGenBase->m_BitMask.size(); ++nBaseBit)
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
				SShaderItem newShaderItem = GetIEditorImpl()->GetRenderer()->EF_LoadShaderItem(pCurrLayer->m_shaderName, false, 0, pCurrLayer->m_shaderResources, nMaskGenLayer);
				if (!newShaderItem.m_pShader || (newShaderItem.m_pShader->GetFlags() & EF_NOTFOUND) != 0)
				{
					CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Failed to load material layer shader \"%s\" in material \"%s\"", pCurrLayer->m_shaderName, m_pMatInfo->GetName());
					if (!newShaderItem.m_pShader)
						continue;
				}

				SShaderItem& pCurrShaderItem = pCurrLayer->m_pMatLayer->GetShaderItem();

				if (newShaderItem.m_pShader)
				{
					newShaderItem.m_pShader->AddRef();
				}

				// Release previously used shader (Must be After new shader is loaded, for speed).
				SAFE_RELEASE(pCurrShaderItem.m_pShader);
				SAFE_RELEASE(pCurrShaderItem.m_pShaderResources);
				SAFE_RELEASE(newShaderItem.m_pShaderResources);

				pCurrShaderItem.m_pShader = newShaderItem.m_pShader;
				// Copy resources from base material
				pCurrShaderItem.m_pShaderResources = m_shaderItem.m_pShaderResources->Clone();
				pCurrShaderItem.m_nTechnique = newShaderItem.m_nTechnique;
				pCurrShaderItem.m_nPreprocessFlags = newShaderItem.m_nPreprocessFlags;

				// set default params
				if (pCurrLayer->m_bRegetPublicParams)
				{
					pCurrShaderItem.m_pShader->CopyPublicParamsTo(*pCurrLayer->m_shaderResources);
				}

				pCurrLayer->m_bRegetPublicParams = false;

				if (pCurrLayer->m_publicVarsCache)
				{
					MaterialHelpers::SetShaderParamsFromXml(*pCurrLayer->m_shaderResources, pCurrLayer->m_publicVarsCache);
					pCurrLayer->m_publicVarsCache = 0;
				}

				if (pCurrShaderItem.m_pShaderResources)
				{
					pCurrShaderItem.m_pShaderResources->SetShaderParams(pCurrLayer->m_shaderResources, pCurrShaderItem.m_pShader);
				}

				// Activate layer
				pCurrLayer->m_nFlags &= ~MTL_FLAG_NODRAW;
			}

		}

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

void CMaterial::UpdateMaterialLayers()
{
	if (m_pMatInfo && m_shaderItem.m_pShaderResources)
	{
		m_pMatInfo->SetLayerCount(MTL_LAYER_MAX_SLOTS);

		uint8 nMaterialLayerFlags = 0;

		for (int l(0); l < MTL_LAYER_MAX_SLOTS; ++l)
		{
			SMaterialLayerResources* pCurrLayer = &m_pMtlLayerResources[l];
			if (pCurrLayer && !pCurrLayer->m_shaderName.IsEmpty() && pCurrLayer->m_pMatLayer)
			{
				pCurrLayer->m_pMatLayer->SetFlags(pCurrLayer->m_nFlags);
				m_pMatInfo->SetLayer(l, pCurrLayer->m_pMatLayer);

				if ((pCurrLayer->m_nFlags & MTL_LAYER_USAGE_NODRAW))
				{
					if (!strcmpi(pCurrLayer->m_shaderName, "frozenlayerwip"))
						nMaterialLayerFlags |= MTL_LAYER_FROZEN;
					else if (!strcmpi(pCurrLayer->m_shaderName, "cloaklayer"))
						nMaterialLayerFlags |= MTL_LAYER_CLOAK;
				}
			}
		}

		if (m_shaderItem.m_pShaderResources)
			m_shaderItem.m_pShaderResources->SetMtlLayerNoDrawFlags(nMaterialLayerFlags);
	}
}

void CMaterial::UpdateMatInfo()
{
	if (m_pMatInfo)
	{
		// Mark material invalid.
		m_pMatInfo->SetFlags(m_mtlFlags);
		m_pMatInfo->SetShaderItem(m_shaderItem);
		m_pMatInfo->SetSurfaceType(m_surfaceType);
		m_pMatInfo->SetMatTemplate(m_matTemplate);

		m_pMatInfo->LoadConsoleMaterial();

		LoadMaterialLayers();
		UpdateMaterialLayers();

		m_pMatInfo->SetMaterialLinkName(m_linkedMaterial);

		if (IsMultiSubMaterial())
		{
			m_pMatInfo->SetSubMtlCount(m_subMaterials.size());
			for (unsigned int i = 0; i < m_subMaterials.size(); i++)
			{
				if (m_subMaterials[i])
					m_pMatInfo->SetSubMtl(i, m_subMaterials[i]->GetMatInfo());
				else
					m_pMatInfo->SetSubMtl(i, NULL);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CVarBlock* CMaterial::GetPublicVars(SInputShaderResources& pShaderResources)
{
	return MaterialHelpers::GetPublicVars(pShaderResources);
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::SetPublicVars(CVarBlock* pPublicVars, CMaterial* pMtl)
{
	if (!pMtl->GetShaderResources().m_ShaderParams.empty())
		RecordUndo("Set Public Vars");

	MaterialHelpers::SetPublicVars(pPublicVars, pMtl->GetShaderResources(), pMtl->GetShaderItem().m_pShaderResources, pMtl->GetShaderItem().m_pShader);

	GetIEditorImpl()->GetMaterialManager()->OnUpdateProperties(this);
}

//////////////////////////////////////////////////////////////////////////
CVarBlock* CMaterial::GetShaderGenParamsVars()
{
	return MaterialHelpers::GetShaderGenParamsVars(GetShaderItem().m_pShader, m_nShaderGenMask);
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::SetShaderGenParamsVars(CVarBlock* pBlock)
{
	RecordUndo("Change Shader GenMask");

	uint64 nGenMask = MaterialHelpers::SetShaderGenParamsVars(GetShaderItem().m_pShader, pBlock);
	if (m_nShaderGenMask != nGenMask)
	{
		m_bRegetPublicParams = true;
		m_bKeepPublicParamsValues = true;
		m_nShaderGenMask = nGenMask;
	}
}

void CMaterial::SetShaderGenMaskFromUI(uint64 mask)
{
	if (m_nShaderGenMask != mask)
	{
		m_bRegetPublicParams = true;
		m_bKeepPublicParamsValues = true;
		m_nShaderGenMask = mask;
	}
}

//////////////////////////////////////////////////////////////////////////
unsigned int CMaterial::GetTexmapUsageMask() const
{
	int mask = 0;
	if (m_shaderItem.m_pShader)
	{
		IShader* pTempl = m_shaderItem.m_pShader;
		if (pTempl)
			mask = pTempl->GetUsedTextureTypes();
	}
	return mask;
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::Update()
{
	// Reload shader item with new resources and shader.
	LoadShader();

	// Mark library as modified.
	SetModified();

	GetIEditorImpl()->SetModifiedFlag();

	// When modifying pure child, mark his parent as modified.
	if (IsPureChild() && m_pParent)
	{
		m_pParent->SetModified();
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void CMaterial::Serialize(SerializeContext& ctx)
{
	//CBaseLibraryItem::Serialize( ctx );

	XmlNodeRef node = ctx.node;
	if (ctx.bLoading)
	{
		m_bIgnoreNotifyChange = true;
		m_bRegetPublicParams = true;

		m_shaderResources = gEnv->pRenderer->EF_CreateInputShaderResource();
		SInputShaderResources& sr = *m_shaderResources;

		// Loading
		int flags = m_mtlFlags;
		if (node->getAttr("MtlFlags", flags))
		{
			m_mtlFlags &= ~(MTL_FLAGS_SAVE_MASK);
			m_mtlFlags |= (flags & (MTL_FLAGS_SAVE_MASK));
		}

		CMaterial* pMtlTmpl = NULL;

		if (!IsMultiSubMaterial())
		{
			node->getAttr("MatTemplate", m_matTemplate);

			//
			if (!m_matTemplate.IsEmpty() && m_matTemplate != GetName())
			{
				pMtlTmpl = GetIEditorImpl()->GetMaterialManager()->LoadMaterial(m_matTemplate, false);
				if (!pMtlTmpl)
				{
					m_matTemplate = "";
				}
			}
			else
			{
				m_matTemplate = "";
			}

			if (!pMtlTmpl)
			{
				node->getAttr("Shader", m_shaderName);
				node->getAttr("GenMask", m_nShaderGenMask);

				if (!(m_mtlFlags & MTL_64BIT_SHADERGENMASK))
				{
					uint32 nShaderGenMask = 0;
					node->getAttr("GenMask", nShaderGenMask);
					m_nShaderGenMask = nShaderGenMask;
				}
				else
					node->getAttr("GenMask", m_nShaderGenMask);

				// Remap flags if needed
				if (!(m_mtlFlags & MTL_64BIT_SHADERGENMASK))
				{
					m_nShaderGenMask = GetIEditorImpl()->GetRenderer()->EF_GetRemapedShaderMaskGen((const char*) m_shaderName, m_nShaderGenMask);
					m_mtlFlags |= MTL_64BIT_SHADERGENMASK;
				}

				if (node->getAttr("StringGenMask", m_pszShaderGenMask))
					m_nShaderGenMask = GetIEditorImpl()->GetRenderer()->EF_GetShaderGlobalMaskGenFromString((const char*) m_shaderName, (const char*)m_pszShaderGenMask, m_nShaderGenMask);   // get common mask gen
				else
				{
					// version doens't has string gen mask yet ? Remap flags if needed
					m_nShaderGenMask = GetIEditorImpl()->GetRenderer()->EF_GetRemapedShaderMaskGen((const char*) m_shaderName, m_nShaderGenMask, ((m_mtlFlags & MTL_64BIT_SHADERGENMASK) != 0));
				}
				m_mtlFlags |= MTL_64BIT_SHADERGENMASK;

				node->getAttr("SurfaceType", m_surfaceType);
				node->getAttr("LayerAct", m_allowLayerActivation);

				MaterialHelpers::SetLightingFromXml(sr, node);
			}
			else
			{
				m_mtlFlags = (m_mtlFlags & ~MTL_FLAGS_TEMPLATE_MASK) | (pMtlTmpl->m_mtlFlags & MTL_FLAGS_TEMPLATE_MASK);
				m_shaderName = pMtlTmpl->m_shaderName;
				m_nShaderGenMask = pMtlTmpl->m_nShaderGenMask;
				m_surfaceType = pMtlTmpl->m_surfaceType;
				m_shaderResources = pMtlTmpl->m_shaderResources;
			}

			{
				MaterialHelpers::SetTexturesFromXml(sr, node, GetRoot()->GetName().GetString());
				MaterialHelpers::MigrateXmlLegacyData(sr, node);
			}

			if (pMtlTmpl)
			{
				for (EEfResTextures texId = EFTT_DIFFUSE; texId < EFTT_MAX; texId = EEfResTextures(texId + 1))
				{
					sr.m_Textures[texId].m_bUTile = pMtlTmpl->m_shaderResources->m_Textures[texId].m_bUTile;
					sr.m_Textures[texId].m_bVTile = pMtlTmpl->m_shaderResources->m_Textures[texId].m_bVTile;
					sr.m_Textures[texId].m_Sampler.m_eTexType = pMtlTmpl->m_shaderResources->m_Textures[texId].m_Sampler.m_eTexType;
					sr.m_Textures[texId].m_Filter = pMtlTmpl->m_shaderResources->m_Textures[texId].m_Filter;
					sr.m_Textures[texId].m_Ext = pMtlTmpl->m_shaderResources->m_Textures[texId].m_Ext;
				}
			}

			//clear all submaterials
			SetSubMaterialCount(0);
		}

		if (!pMtlTmpl)
		{
			//////////////////////////////////////////////////////////////////////////
			// Check if we have a link name and if any propagation settings were
			// present
			//////////////////////////////////////////////////////////////////////////
			XmlNodeRef pLinkName = node->findChild("MaterialLinkName");
			if (pLinkName)
				m_linkedMaterial = pLinkName->getAttr("name");
			else
				m_linkedMaterial = string();

			XmlNodeRef pPropagationFlags = node->findChild("MaterialPropagationFlags");
			if (pPropagationFlags)
				pPropagationFlags->getAttr("flags", m_propagationFlags);
			else
				m_propagationFlags = 0;

			//////////////////////////////////////////////////////////////////////////
			// Check if we have vertex deform.
			//////////////////////////////////////////////////////////////////////////
			MaterialHelpers::SetVertexDeformFromXml(*m_shaderResources, node);
		}
		else
		{
			m_shaderResources->m_DeformInfo = pMtlTmpl->m_shaderResources->m_DeformInfo;
		}

		//////////////////////////////////////////////////////////////////////////
		// Check for detail decal
		//////////////////////////////////////////////////////////////////////////
		if (m_mtlFlags & MTL_FLAG_DETAIL_DECAL)
		{
			MaterialHelpers::SetDetailDecalFromXml(*m_shaderResources, node);
		}
		else
		{
			m_shaderResources->m_DetailDecalInfo.Reset();
		}

		ClearAllSubMaterials();

		// Serialize sub materials.
		XmlNodeRef childsNode = node->findChild("SubMaterials");
		if (childsNode && !ctx.bIgnoreChilds)
		{
			string name;
			int nSubMtls = childsNode->getChildCount();
			SetSubMaterialCount(nSubMtls);
			for (int i = 0; i < nSubMtls; i++)
			{
				XmlNodeRef mtlNode = childsNode->getChild(i);
				if (mtlNode->isTag("Material"))
				{
					mtlNode->getAttr("Name", name);
					CMaterial* pSubMtl = new CMaterial(name, MTL_FLAG_PURE_CHILD);
					SetSubMaterial(i, pSubMtl);

					SerializeContext childCtx(ctx);
					childCtx.node = mtlNode;
					pSubMtl->Serialize(childCtx);

					pSubMtl->m_shaderResources->m_SortPrio = nSubMtls - i - 1;
				}
				else
				{
					if (mtlNode->getAttr("Name", name))
					{
						CMaterial* pMtl = GetIEditorImpl()->GetMaterialManager()->LoadMaterial(name);
						if (pMtl)
							SetSubMaterial(i, pMtl);
					}
				}
			}
		}
		else if(IsMultiSubMaterial())
		{
			//Do not leave empty slots where most of the code assumes they won't be
			SetSubMaterialCount(0);
		}

		//////////////////////////////////////////////////////////////////////////
		// Load public parameters.
		//////////////////////////////////////////////////////////////////////////
		if (!pMtlTmpl)
		{
			m_publicVarsCache = node->findChild("PublicParams");
		}
		else
		{
			m_publicVarsCache = node->newChild("PublicParams");
			if (pMtlTmpl->m_publicVarsCache)
			{
				m_publicVarsCache = pMtlTmpl->m_publicVarsCache->clone();
			}
			else
			{
				MaterialHelpers::SetXmlFromShaderParams(*pMtlTmpl->m_shaderResources, m_publicVarsCache);
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Load material layers data
		//////////////////////////////////////////////////////////////////////////
		if (pMtlTmpl)
		{
			for (int n = 0; n < MTL_LAYER_MAX_SLOTS; n++)
			{
				m_pMtlLayerResources[n] = pMtlTmpl->m_pMtlLayerResources[n];
			}
		}
		else
		{
			XmlNodeRef mtlLayersNode = node->findChild("MaterialLayers");
			if (mtlLayersNode)
			{
				int nChildCount = min((int) MTL_LAYER_MAX_SLOTS, (int)  mtlLayersNode->getChildCount());
				for (int l = 0; l < nChildCount; ++l)
				{
					XmlNodeRef layerNode = mtlLayersNode->getChild(l);
					if (layerNode)
					{
						if (layerNode->getAttr("Name", m_pMtlLayerResources[l].m_shaderName))
						{
							m_pMtlLayerResources[l].m_bRegetPublicParams = true;

							bool bNoDraw = false;
							layerNode->getAttr("NoDraw", bNoDraw);

							m_pMtlLayerResources[l].m_publicVarsCache = layerNode->findChild("PublicParams");

							if (bNoDraw)
								m_pMtlLayerResources[l].m_nFlags |= MTL_LAYER_USAGE_NODRAW;
							else
								m_pMtlLayerResources[l].m_nFlags &= ~MTL_LAYER_USAGE_NODRAW;

							bool bFadeOut = false;
							layerNode->getAttr("FadeOut", bFadeOut);
							if (bFadeOut)
							{
								m_pMtlLayerResources[l].m_nFlags |= MTL_LAYER_USAGE_FADEOUT;
							}
							else
							{
								m_pMtlLayerResources[l].m_nFlags &= ~MTL_LAYER_USAGE_FADEOUT;
							}
						}
					}
				}
			}
		}

		if (ctx.bUndo)
		{
			LoadShader();
			UpdateMatInfo();
		}

		m_bIgnoreNotifyChange = false;
		SetModified(false);

		// If copy pasting or undo send update event.
		if (ctx.bCopyPaste || ctx.bUndo)
			NotifyChanged();
	}
	else
	{
		int extFlags = MTL_64BIT_SHADERGENMASK;
		{
			const string& name = GetName();
			const size_t len = name.GetLength();
			if (len > 4 && strstri(((const char*)name) + (len - 4), "_con"))
				extFlags |= MTL_FLAG_CONSOLE_MAT;
		}

		// Saving.
		node->setAttr("MtlFlags", m_mtlFlags | extFlags);

		if (!IsMultiSubMaterial())
		{
			// store shader gen bit mask string
			m_pszShaderGenMask = GetIEditorImpl()->GetRenderer()->EF_GetStringFromShaderGlobalMaskGen((const char*)m_shaderName, m_nShaderGenMask);

			node->setAttr("Shader", m_shaderName);
			node->setAttr("GenMask", m_nShaderGenMask);
			node->setAttr("StringGenMask", m_pszShaderGenMask);
			node->setAttr("SurfaceType", m_surfaceType);
			node->setAttr("MatTemplate", m_matTemplate);

			// Regarding "ctx.bCopyPaste ? "" : ..." below:
			//
			// When a material is copied by the user (and so, serialized to XML) the serializer tries
			// to convert texture filepaths to paths relative to material's filepath, so the resulting
			// XML node sometimes contains texture filepaths in form of "./xxx".
			//
			// When a material is pasted by the user (and so, de-serialized from XML) into *some other*
			// folder and the material contains "./xxx" texture filepaths, the de-seriliazation code
			// has no chance but to assume that those "./xxx" filepaths are relative to that "some
			// other" folder, which is obviously wrong.
			//
			// So, to prevent incorrect de-serialization of materials with "./xxx" on pasting, we simply
			// disable converting texture filepaths to "./xxx" format at the copying stage, by providing
			// an empty string (instead of material filename) to SetXmlFromTextures().
			{
				MaterialHelpers::SetXmlFromLighting(*m_shaderResources, node);
				MaterialHelpers::SetXmlFromTextures(*m_shaderResources, node, ctx.bCopyPaste ? "" : GetRoot()->GetName().GetString());
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Save out the link name if present and the propagation flags
		//////////////////////////////////////////////////////////////////////////
		if (m_linkedMaterial && m_linkedMaterial[0])
		{
			XmlNodeRef pLinkName = node->newChild("MaterialLinkName");
			pLinkName->setAttr("name", m_linkedMaterial);
		}

		if (m_propagationFlags)
		{
			XmlNodeRef pPropagationFlags = node->newChild("MaterialPropagationFlags");
			pPropagationFlags->setAttr("flags", m_propagationFlags);
		}

		//////////////////////////////////////////////////////////////////////////
		// Check if we have vertex deform.
		//////////////////////////////////////////////////////////////////////////
		MaterialHelpers::SetXmlFromVertexDeform(*m_shaderResources, node);

		//////////////////////////////////////////////////////////////////////////
		// Check for detail decal
		//////////////////////////////////////////////////////////////////////////
		if (m_mtlFlags & MTL_FLAG_DETAIL_DECAL)
		{
			MaterialHelpers::SetXmlFromDetailDecal(*m_shaderResources, node);
		}

		if (GetSubMaterialCount() > 0)
		{
			// Serialize sub materials.

			// Let's not serialize empty submaterials at the end of the list.
			// Note that IDs of the remaining submaterials stay intact.
			int count = GetSubMaterialCount();
			while (count > 0 && !GetSubMaterial(count - 1))
			{
				--count;
			}

			XmlNodeRef childsNode = node->newChild("SubMaterials");

			for (int i = 0; i < count; ++i)
			{
				CMaterial* const pSubMtl = GetSubMaterial(i);
				if (pSubMtl && pSubMtl->IsPureChild())
				{
					XmlNodeRef mtlNode = childsNode->newChild("Material");
					mtlNode->setAttr("Name", pSubMtl->GetName());
					SerializeContext childCtx(ctx);
					childCtx.node = mtlNode;
					pSubMtl->Serialize(childCtx);
				}
				else
				{
					XmlNodeRef mtlNode = childsNode->newChild("MaterialRef");
					if (pSubMtl)
					{
						mtlNode->setAttr("Name", pSubMtl->GetName());
					}
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Save public parameters.
		//////////////////////////////////////////////////////////////////////////
		if (m_publicVarsCache)
		{
			node->addChild(m_publicVarsCache);
		}
		else if (!m_shaderResources->m_ShaderParams.empty())
		{
			XmlNodeRef publicsNode = node->newChild("PublicParams");
			MaterialHelpers::SetXmlFromShaderParams(*m_shaderResources, publicsNode);
		}

		//////////////////////////////////////////////////////////////////////////
		// Save material layers data
		//////////////////////////////////////////////////////////////////////////

		bool bMaterialLayers = false;
		for (int l(0); l < MTL_LAYER_MAX_SLOTS; ++l)
		{
			if (!m_pMtlLayerResources[l].m_shaderName.IsEmpty())
			{
				bMaterialLayers = true;
				break;
			}
		}

		if (bMaterialLayers)
		{
			XmlNodeRef mtlLayersNode = node->newChild("MaterialLayers");
			for (int l(0); l < MTL_LAYER_MAX_SLOTS; ++l)
			{
				XmlNodeRef layerNode = mtlLayersNode->newChild("Layer");
				if (!m_pMtlLayerResources[l].m_shaderName.IsEmpty())
				{
					layerNode->setAttr("Name", m_pMtlLayerResources[l].m_shaderName);
					layerNode->setAttr("NoDraw", m_pMtlLayerResources[l].m_nFlags & MTL_LAYER_USAGE_NODRAW);
					layerNode->setAttr("FadeOut", m_pMtlLayerResources[l].m_nFlags & MTL_LAYER_USAGE_FADEOUT);

					if (m_pMtlLayerResources[l].m_publicVarsCache)
					{
						layerNode->addChild(m_pMtlLayerResources[l].m_publicVarsCache);
					}
					else if (!m_pMtlLayerResources[l].m_shaderResources->m_ShaderParams.empty())
					{
						XmlNodeRef publicsNode = layerNode->newChild("PublicParams");
						MaterialHelpers::SetXmlFromShaderParams(*m_pMtlLayerResources[l].m_shaderResources, publicsNode);
					}
				}
			}
		}

		if (GetSubMaterialCount() == 0 || GetParent())
			node->setAttr("LayerAct", m_allowLayerActivation);
	}
}

/*
   //////////////////////////////////////////////////////////////////////////
   void CMaterial::SerializePublics( XmlNodeRef &node,bool bLoading )
   {
   if (bLoading)
   {
   }
   else
   {
    if (m_shaderParams.empty())
      return;
    XmlNodeRef publicsNode = node->newChild( "PublicParams" );

    for (int i = 0; i < m_shaderParams.size(); i++)
    {
      XmlNodeRef paramNode = node->newChild( "Param" );
      SShaderParam *pParam = &m_shaderParams[i];
      paramNode->setAttr( "Name",pParam->m_Name );
      switch (pParam->m_Type)
      {
      case eType_BYTE:
        paramNode->setAttr( "Value",(int)pParam->m_Value.m_Byte );
        paramNode->setAttr( "Type",(int)pParam->m_Value.m_Byte );
        break;
      case eType_SHORT:
        paramNode->setAttr( "Value",(int)pParam->m_Value.m_Short );
        break;
      case eType_INT:
        paramNode->setAttr( "Value",(int)pParam->m_Value.m_Int );
        break;
      case eType_FLOAT:
        paramNode->setAttr( "Value",pParam->m_Value.m_Float );
        break;
      case eType_STRING:
        paramNode->setAttr( "Value",pParam->m_Value.m_String );
      break;
      case eType_FCOLOR:
        paramNode->setAttr( "Value",Vec3(pParam->m_Value.m_Color[0],pParam->m_Value.m_Color[1],pParam->m_Value.m_Color[2]) );
        break;
      case eType_VECTOR:
        paramNode->setAttr( "Value",Vec3(pParam->m_Value.m_Vector[0],pParam->m_Value.m_Vector[1],pParam->m_Value.m_Vector[2]) );
        break;
      }
    }
   }
   }
 */

//////////////////////////////////////////////////////////////////////////
void CMaterial::AssignToEntity(IRenderNode* pEntity)
{
	assert(pEntity);
	pEntity->SetMaterial(GetMatInfo());
}

//////////////////////////////////////////////////////////////////////////
bool CMaterial::IsBreakable2D() const
{
	if ((GetFlags() & MTL_FLAG_NODRAW) != 0)
		return false;

	int result = 0;

	const string& surfaceTypeName = GetSurfaceTypeName();
	if (ISurfaceTypeManager* pSurfaceManager = GetIEditorImpl()->Get3DEngine()->GetMaterialManager()->GetSurfaceTypeManager())
	{
		ISurfaceType* pSurfaceType = pSurfaceManager->GetSurfaceTypeByName(surfaceTypeName);
		if (pSurfaceType && pSurfaceType->GetBreakable2DParams() != 0)
			return true;
	}

	int count = GetSubMaterialCount();
	for (int i = 0; i < count; ++i)
	{
		const CMaterial* pSub = GetSubMaterial(i);
		if (!pSub)
			continue;
		if (pSub->IsBreakable2D())
			return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::SetFromMatInfo(IMaterial* pMatInfo)
{
	assert(pMatInfo);

	m_bFromEngine = true;
	m_shaderName = "";

	ClearMatInfo();
	SetModified(true);

	m_mtlFlags = pMatInfo->GetFlags();
	if (m_mtlFlags & MTL_FLAG_MULTI_SUBMTL)
	{
		// Create sub materials.
		SetSubMaterialCount(pMatInfo->GetSubMtlCount());
		for (int i = 0; i < GetSubMaterialCount(); i++)
		{
			IMaterial* pChildMatInfo = pMatInfo->GetSubMtl(i);
			if (!pChildMatInfo)
				continue;

			if (pChildMatInfo->GetFlags() & MTL_FLAG_PURE_CHILD)
			{
				CMaterial* pChild = new CMaterial(pChildMatInfo->GetName(), pChildMatInfo->GetFlags());
				pChild->SetFromMatInfo(pChildMatInfo);
				SetSubMaterial(i, pChild);
			}
			else
			{
				CMaterial* pChild = GetIEditorImpl()->GetMaterialManager()->LoadMaterial(pChildMatInfo->GetName());
				pChild->SetFromMatInfo(pChildMatInfo);
				SetSubMaterial(i, pChild);
			}
		}
	}
	else
	{
		SAFE_RELEASE(m_shaderItem.m_pShader);
		SAFE_RELEASE(m_shaderItem.m_pShaderResources);
		m_shaderItem = pMatInfo->GetShaderItem();
		if (m_shaderItem.m_pShader)
			m_shaderItem.m_pShader->AddRef();
		if (m_shaderItem.m_pShaderResources)
			m_shaderItem.m_pShaderResources->AddRef();

		if (m_shaderItem.m_pShaderResources)
		{
			m_shaderResources = gEnv->pRenderer->EF_CreateInputShaderResource(m_shaderItem.m_pShaderResources);
		}
		if (m_shaderItem.m_pShader)
		{
			// Get name of template.
			IShader* pTemplShader = m_shaderItem.m_pShader;
			if (pTemplShader)
			{
				m_shaderName = pTemplShader->GetName();
				m_nShaderGenMask = pTemplShader->GetGenerationMask();
			}
		}
		ISurfaceType* pSurfaceType = pMatInfo->GetSurfaceType();
		if (pSurfaceType)
			m_surfaceType = pSurfaceType->GetName();
		else
			m_surfaceType = "";
		//
		IMaterial* pMatTemplate = pMatInfo->GetMatTemplate();
		if (pMatTemplate)
			m_matTemplate = pMatTemplate->GetName();
		else
			m_matTemplate = "";
	}

	// Mark as not modified.
	SetModified(false);

	// Material link names
	if (const char* szLinkName = pMatInfo->GetMaterialLinkName())
	{
		m_linkedMaterial = szLinkName;
	}

	//////////////////////////////////////////////////////////////////////////
	// Assign mat info.
	m_pMatInfo = pMatInfo;
	m_pMatInfo->SetUserData(this);
	AddRef(); // Let IMaterial keep a reference to us.
}

//////////////////////////////////////////////////////////////////////////
int CMaterial::GetSubMaterialCount() const
{
	return m_subMaterials.size();
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::SetSubMaterialCount(int nSubMtlsCount)
{
	//Remove parenting link from severed submaterials
	const int currentCount = m_subMaterials.size();
	if (nSubMtlsCount == currentCount)
		return;

	RecordUndo("Multi Material Change");

	if (nSubMtlsCount < currentCount)
	{
		for (int i = nSubMtlsCount; i < currentCount; i++)
		{
			if (m_subMaterials[i])
			{
				m_subMaterials[i]->m_pParent = NULL;
				m_subMaterials[i] = NULL;
			}
		}
	}

	m_subMaterials.resize(nSubMtlsCount);
	UpdateMatInfo();
	NotifyChanged();
	signalSubMaterialsChanged(SlotCountChanged);
}

//////////////////////////////////////////////////////////////////////////
CMaterial* CMaterial::GetSubMaterial(int index) const
{
	const int nSubMats = m_subMaterials.size();
	assert(index >= 0 && index < nSubMats);

	if (index < 0 || index >= nSubMats)
		return NULL;

	return m_subMaterials[index];
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::SetSubMaterial(int nSlot, CMaterial* mtl)
{
	assert(nSlot >= 0 && nSlot < m_subMaterials.size());

	if (mtl == m_subMaterials[nSlot])
		return;

	RecordUndo("Multi Material Change");
	if (mtl)
	{
		if (mtl->m_bFromEngine && !m_bFromEngine)
		{
			// Do not allow to assign engine materials to sub-slots of real materials.
			return;
		}
		if (mtl->IsMultiSubMaterial())
			return;
		if (mtl->IsPureChild())
			mtl->m_pParent = this;
	}

	if (m_subMaterials[nSlot])
		m_subMaterials[nSlot]->m_pParent = NULL;
	m_subMaterials[nSlot] = mtl;

	if (!m_subMaterials[nSlot])
	{
		m_subMaterials.erase(m_subMaterials.begin() + nSlot);
	}

	UpdateMatInfo();
	NotifyChanged();

	if (mtl)
		signalSubMaterialsChanged(SubMaterialSet);
	else
		signalSubMaterialsChanged(SlotCountChanged);
}

void CMaterial::ResetSubMaterial(int nSlot, bool bOnlyIfEmpty)
{
	if (bOnlyIfEmpty && GetSubMaterial(nSlot) != nullptr)
		return;

	// Allocate pure childs for all empty slots.
	string name;
	name.Format("Sub-Material-%d", nSlot+1);
	_smart_ptr<CMaterial> pChild = new CMaterial(name, MTL_FLAG_PURE_CHILD);
	SetSubMaterial(nSlot, pChild);
	signalSubMaterialsChanged(SubMaterialSet);
}

void CMaterial::RemoveSubMaterial(int nSlot)
{
	SetSubMaterial(nSlot, nullptr);
}

//////////////////////////////////////////////////////////////////////////
CVarBlock* CMaterial::UpdateTextureNames(CSmartVariableArray textureVars[EFTT_MAX])
{
	CVarBlock* pTextureSlots = new CVarBlock;
	IShader* pTemplShader = m_shaderItem.m_pShader;
	int nTech = max(0, m_shaderItem.m_nTechnique);
	SShaderTexSlots* pShaderSlots = pTemplShader ? pTemplShader->GetUsedTextureSlots(nTech) : NULL;

	for (EEfResTextures texId = EEfResTextures(0); texId < EFTT_MAX; texId = EEfResTextures(texId + 1))
	{
		if (!MaterialHelpers::IsAdjustableTexSlot(texId))
			continue;

		IVariable* pVar = textureVars[texId].GetVar();
		SShaderTextureSlot* pSlot = pShaderSlots ? pShaderSlots->m_UsedSlots[texId] : NULL;
		const string& sTexName = m_shaderResources->m_Textures[texId].m_Name;

		// if slot is NULL, fall back to default name
		pVar->SetName(pSlot && pSlot->m_Name.length() ? pSlot->m_Name.c_str() : MaterialHelpers::LookupTexName(texId));
		pVar->SetHumanName(pVar->GetName());
		pVar->SetDescription(pSlot ? pSlot->m_Description.c_str() : "");

		int flags = pVar->GetFlags();

		// TODO: slot->m_TexType gives expected sampler type (2D vs Cube etc). Could check/extract this here.

		// Not sure why this needs COLLAPSED added again, but without this all the slots expand
		flags |= IVariable::UI_COLLAPSED;

		// if slot is NULL, but we have reflection information, this slot isn't used - make the variable invisible
		// unless there's a texture in the slot
		if (pShaderSlots && !pSlot && sTexName.empty())
			flags |= IVariable::UI_INVISIBLE;
		else
			flags &= ~IVariable::UI_INVISIBLE;

		pVar->SetFlags(flags);

		pTextureSlots->AddVariable(pVar);
	}

	return pTextureSlots;
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::ClearAllSubMaterials()
{
	if (m_subMaterials.size() == 0)
		return;

	RecordUndo("Multi Material Change");
	for (int i = 0; i < m_subMaterials.size(); i++)
	{
		if (m_subMaterials[i])
		{
			m_subMaterials[i]->m_pParent = NULL;
			m_subMaterials[i] = NULL;
		}
	}
	UpdateMatInfo();
	NotifyChanged();
	signalSubMaterialsChanged(SubMaterialSet);
}

int CMaterial::FindSubMaterial(CMaterial* pMaterial)
{
	CRY_ASSERT(pMaterial && pMaterial->GetParent() == this);
	int index = -1;

	for (int i = 0; i < m_subMaterials.size(); i++)
	{
		if (m_subMaterials[i] == pMaterial)
		{
			index = i;
			break;
		}
	}

	return index;
}

void CMaterial::ConvertToMultiMaterial()
{
	CRY_ASSERT(!IsMultiSubMaterial());
	RecordUndo("Multi Material Change");

	CMaterial* pChild = new CMaterial(GetShortName(), MTL_FLAG_PURE_CHILD);
	pChild->CopyFrom(this);

	SetFlags(GetFlags() | MTL_FLAG_MULTI_SUBMTL);
	SetSubMaterialCount(1);
	SetSubMaterial(0, pChild);

	signalSubMaterialsChanged(MaterialConverted);
}

void CMaterial::ConvertToSingleMaterial()
{
	CRY_ASSERT(IsMultiSubMaterial());
	RecordUndo("Multi Material Change");

	SetSubMaterialCount(0);

	SetFlags(GetFlags() & ~MTL_FLAG_MULTI_SUBMTL);

	signalSubMaterialsChanged(MaterialConverted);
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::Validate()
{
	if (IsDummy())
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Material %s file not found", (const char*)GetName());
	}
	// Reload shader.
	LoadShader();

	// Validate sub materials.
	for (int i = 0; i < m_subMaterials.size(); i++)
	{
		if (m_subMaterials[i])
			m_subMaterials[i]->Validate();
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::GatherUsedResources(CUsedResources& resources)
{
	SInputShaderResources& sr = GetShaderResources();
	for (int texid = 0; texid < EFTT_MAX; texid++)
	{
		if (!sr.m_Textures[texid].m_Name.empty())
		{
			resources.Add(sr.m_Textures[texid].m_Name.c_str());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CMaterial::CanModify(bool bSkipReadOnly)
{
	if (m_bDummyMaterial)
		return false;

	if (m_bFromEngine)
		return false;

	if (IsPureChild() && GetParent())
		return GetParent()->CanModify(bSkipReadOnly);

	if (bSkipReadOnly)
	{
		// If read only or in pack, do not save.
		if (m_scFileAttributes & (SCC_FILE_ATTRIBUTE_READONLY | SCC_FILE_ATTRIBUTE_INPAK))
			return false;

		// Managed file must be checked out.
		if ((m_scFileAttributes & SCC_FILE_ATTRIBUTE_MANAGED) && !(m_scFileAttributes & SCC_FILE_ATTRIBUTE_CHECKEDOUT))
			return false;
	}
	
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CMaterial::Save(bool bSkipReadOnly)
{
	// Save our parent
	if (IsPureChild())
	{
		if (m_pParent)
			return m_pParent->Save(bSkipReadOnly);
		return false;
	}

	GetFileAttributes();

	if (bSkipReadOnly && IsModified())
	{
		// If read only or in pack, do not save.
		if (m_scFileAttributes & (SCC_FILE_ATTRIBUTE_READONLY | SCC_FILE_ATTRIBUTE_INPAK))
			gEnv->pLog->LogError("Can't save material %s (read-only)", GetName());

		// Managed file must be checked out.
		if ((m_scFileAttributes & SCC_FILE_ATTRIBUTE_MANAGED) && !(m_scFileAttributes & SCC_FILE_ATTRIBUTE_CHECKEDOUT))
			gEnv->pLog->LogError("Can't save material %s (need to check out)", GetName());
	}

	if (!CanModify(bSkipReadOnly))
		return false;

	// If filename is empty do not not save.
	if (GetFilename(true).IsEmpty())
		return false;

	// Save material XML to a file that corresponds to the material name with extension .mtl.
	XmlNodeRef mtlNode = XmlHelpers::CreateXmlNode("Material");
	CBaseLibraryItem::SerializeContext ctx(mtlNode, false);
	Serialize(ctx);

	//CMaterialManager *pMatMan = (CMaterialManager*)GetLibrary()->GetManager();
	// get file name from material name.
	//string filename = pMatMan->MaterialToFilename( GetName() );

	//char path[ICryPak::g_nMaxPath];
	//filename = gEnv->pCryPak->AdjustFileName( filename,path,0 );

	if (XmlHelpers::SaveXmlNode(mtlNode, GetFilename(true)))
	{
		// If material successfully saved, clear modified flag.
		SetModified(false);
		for (int i = 0; i < GetSubMaterialCount(); ++i)
		{
			CMaterial* pSubMaterial = GetSubMaterial(i);
			if (pSubMaterial)
				pSubMaterial->SetModified(false);
		}

		return true;
	}
	else
		return false;
}

void CMaterial::CopyFrom(CMaterial* pOther)
{
	XmlNodeRef mtlNode = XmlHelpers::CreateXmlNode("Material");
	CBaseLibraryItem::SerializeContext ctx(mtlNode, false);
	pOther->Serialize(ctx);

	ctx.bLoading = true;
	Serialize(ctx);
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::ClearMatInfo()
{
	IMaterial* pMatInfo = m_pMatInfo;
	m_pMatInfo = 0;
	// This call can release CMaterial.
	if (pMatInfo)
		Release();
}

//////////////////////////////////////////////////////////////////////////
IMaterial* CMaterial::GetMatInfo(bool bUseExistingEngineMaterial)
{
	if (!m_pMatInfo)
	{
		if (m_bDummyMaterial)
		{
			m_pMatInfo = GetIEditorImpl()->Get3DEngine()->GetMaterialManager()->GetDefaultMaterial();
			AddRef(); // Always keep dummy materials.
			return m_pMatInfo;
		}

		if (!IsMultiSubMaterial() && !m_shaderItem.m_pShader)
		{
			LoadShader();
		}

		if (!IsPureChild())
		{
			if (bUseExistingEngineMaterial)
				m_pMatInfo = GetIEditorImpl()->Get3DEngine()->GetMaterialManager()->FindMaterial(GetName());

			if (!m_pMatInfo)
				m_pMatInfo = GetIEditorImpl()->Get3DEngine()->GetMaterialManager()->CreateMaterial(GetName(), m_mtlFlags);
		}
		else
		{
			// Pure child should not be registered with the name.
			m_pMatInfo = GetIEditorImpl()->Get3DEngine()->GetMaterialManager()->CreateMaterial("", m_mtlFlags);
			m_pMatInfo->SetName(GetName());
		}
		m_mtlFlags = m_pMatInfo->GetFlags();
		UpdateMatInfo();

		if (m_pMatInfo->GetUserData() != this)
		{
			m_pMatInfo->SetUserData(this);
			AddRef(); // Let IMaterial keep a reference to us.
		}
	}

	return m_pMatInfo;
}

void CMaterial::NotifyPropertiesUpdated()
{
	GetIEditor()->GetMaterialManager()->OnUpdateProperties(this);
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::NotifyChanged()
{
	if (m_bIgnoreNotifyChange)
		return;

	if (!CanModify() && !IsModified() && CUndo::IsRecording())
	{
		// Display Warning message.
		Warning("Modifying read only material %s\r\nChanges will not be saved!", (const char*)GetName());
	}

	SetModified();

	GetIEditorImpl()->GetMaterialManager()->OnItemChanged(this);
}

//////////////////////////////////////////////////////////////////////////
class CUndoMaterial : public IUndoObject
{
public:
	CUndoMaterial(CMaterial* pMaterial, const char* undoDescription, bool bForceUpdate)
	{
		assert(pMaterial);

		// Stores the current state of this object.
		m_undoDescription = undoDescription;

		m_bIsSubMaterial = pMaterial->IsPureChild();

		if (m_bIsSubMaterial)
		{
			CMaterial* pParentMaterial = pMaterial->GetParent();
			assert(pParentMaterial && !pParentMaterial->IsPureChild());
			if (pParentMaterial && !pParentMaterial->IsPureChild())
			{
				bool bFound = false;
				const int subMaterialCount = pParentMaterial->GetSubMaterialCount();
				for (int i = 0; i < subMaterialCount; ++i)
				{
					CMaterial* pSubMaterial = pParentMaterial->GetSubMaterial(i);
					if (pSubMaterial == pMaterial)
					{
						bFound = true;
						m_subMaterialName = pSubMaterial->GetName();
						break;
					}
				}
				assert(bFound);
				m_mtlName = pParentMaterial->GetName();
			}
		}
		else
		{
			m_mtlName = pMaterial->GetName();
		}

		// Save material XML to a file that corresponds to the material name with extension .mtl.
		m_undo = XmlHelpers::CreateXmlNode("Material");
		CBaseLibraryItem::SerializeContext ctx(m_undo, false);
		pMaterial->Serialize(ctx);
		m_bForceUpdate = bForceUpdate;
	}

protected:
	virtual void Release() { delete this; };

	virtual const char* GetDescription() { return m_undoDescription; };

	virtual void        Undo(bool bUndo)
	{
		CMaterial* pMaterial = GetMaterial();

		assert(pMaterial);
		if (!pMaterial)
			return;

		if (bUndo)
		{
			// Save current object state.
			m_redo = XmlHelpers::CreateXmlNode("Material");
			CBaseLibraryItem::SerializeContext ctx(m_redo, false);
			pMaterial->Serialize(ctx);
		}

		CBaseLibraryItem::SerializeContext ctx(m_undo, true);
		ctx.bUndo = bUndo;
		pMaterial->Serialize(ctx);

		if (m_bForceUpdate && bUndo)
		{
			GetIEditorImpl()->GetMaterialManager()->OnUpdateProperties(pMaterial);
		}
	}

	virtual void Redo()
	{
		CMaterial* pMaterial = GetMaterial();

		if (!pMaterial)
		{
			return;
		}

		CBaseLibraryItem::SerializeContext ctx(m_redo, true);
		ctx.bUndo = true;
		pMaterial->Serialize(ctx);

		if (m_bForceUpdate)
		{
			GetIEditorImpl()->GetMaterialManager()->OnUpdateProperties(pMaterial);
		}
	}

private:
	CMaterial* GetMaterial()
	{
		CMaterial* pMaterial = (CMaterial*)GetIEditorImpl()->GetMaterialManager()->FindItemByName(m_mtlName);
		assert(pMaterial);

		if (pMaterial && m_bIsSubMaterial)
		{
			bool bFound = false;
			const int subMaterialCount = pMaterial->GetSubMaterialCount();
			for (int i = 0; i < subMaterialCount; ++i)
			{
				CMaterial* pSubMaterial = pMaterial->GetSubMaterial(i);
				if (pSubMaterial && (pSubMaterial->GetName() == m_subMaterialName))
				{
					bFound = true;
					pMaterial = pSubMaterial;
					break;
				}
			}
			assert(bFound && pMaterial);
		}

		return pMaterial;
	}

	string    m_undoDescription;
	string    m_mtlName;
	bool       m_bIsSubMaterial;
	string    m_subMaterialName;
	XmlNodeRef m_undo;
	XmlNodeRef m_redo;
	bool       m_bForceUpdate;
};

//////////////////////////////////////////////////////////////////////////
void CMaterial::RecordUndo(const char* sText, bool bForceUpdate)
{
	if (CUndo::IsRecording())
	{
		CUndo::Record(new CUndoMaterial(this, sText, bForceUpdate));
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::OnMakeCurrent()
{
	UpdateFileAttributes();

	// If Shader not yet loaded, load it now.
	if (!m_shaderItem.m_pShader)
		LoadShader();
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::SetSurfaceTypeName(const string& surfaceType, bool bUpdateMatInfo /*= true*/)
{
	m_surfaceType = surfaceType;
	if(bUpdateMatInfo)
		UpdateMatInfo();
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::SetMatTemplate(const string& matTemplate)
{
	m_matTemplate = matTemplate;
	//UpdateMatInfo();
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::Reload()
{
	if (IsPureChild())
	{
		if (m_pParent)
			m_pParent->Reload();
		return;
	}
	if (IsDummy())
		return;

	XmlNodeRef mtlNode = GetISystem()->LoadXmlFromFile(GetFilename());
	if (!mtlNode)
	{
		return;
	}
	CBaseLibraryItem::SerializeContext serCtx(mtlNode, true);
	serCtx.bUndo = true; // Simulate undo.
	Serialize(serCtx);

	// was called by Simulate undo.
	//UpdateMatInfo();
	//NotifyChanged();
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::LinkToMaterial(const string& name)
{
	m_linkedMaterial = name;
	UpdateMatInfo();
}

static ColorF Interpolate(const ColorF& a, const ColorF& b, float phase)
{
	const float oneMinusPhase = 1.0f - phase;
	return ColorF(b.r * phase + a.r * oneMinusPhase,
	              b.g * phase + a.g * oneMinusPhase,
	              b.b * phase + a.b * oneMinusPhase,
	              b.a * phase + a.a * oneMinusPhase);
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::UpdateHighlighting()
{
	if (!(GetFlags() & MTL_FLAG_NODRAW))
	{
		const CInputLightMaterial& original = m_shaderResources->m_LMaterial;
		CInputLightMaterial lm = original;

		ColorF highlightColor(0.0f, 0.0f, 0.0f, 1.0f);
		float highlightIntensity = 0.0f;

		MAKE_SURE(GetIEditorImpl()->GetMaterialManager(), return );
		GetIEditorImpl()->GetMaterialManager()->GetHighlightColor(&highlightColor, &highlightIntensity, m_highlightFlags);

		if (m_shaderItem.m_pShaderResources)
		{
			ColorF diffuseColor = Interpolate(original.m_Diffuse, highlightColor, highlightIntensity);
			ColorF emissiveColor = Interpolate(original.m_Emittance, highlightColor, highlightIntensity);
			ColorF specularColor = Interpolate(original.m_Specular, highlightColor, highlightIntensity);

			// do not touch alpha channel
			m_shaderItem.m_pShaderResources->SetColorValue(EFTT_DIFFUSE, diffuseColor);
			m_shaderItem.m_pShaderResources->SetColorValue(EFTT_SPECULAR, specularColor);
			m_shaderItem.m_pShaderResources->SetColorValue(EFTT_EMITTANCE, emissiveColor);

			m_shaderItem.m_pShaderResources->UpdateConstants(m_shaderItem.m_pShader);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CMaterial::SetHighlightFlags(int highlightFlags)
{
	m_highlightFlags = highlightFlags;

	UpdateHighlighting();
}

