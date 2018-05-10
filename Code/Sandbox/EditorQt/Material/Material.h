// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __material_h__
#define __material_h__
#pragma once

#include "SandboxAPI.h"
#include <Cry3DEngine/IMaterial.h>
#include <CryRenderer/IRenderer.h>
#include "BaseLibraryItem.h"
#include "Util/Variable.h"
#include "IEditorMaterial.h"
#include <vector>
#include <CrySandbox/CrySignal.h>

// forward declarations,
class CMaterialManager;
class CVarBlock;
struct SAssetDependencyInfo;
struct IVariable;

enum eMTL_PROPAGATION
{
	MTL_PROPAGATE_OPACITY           = 1 << 0,
	MTL_PROPAGATE_LIGHTING          = 1 << 1,
	MTL_PROPAGATE_ADVANCED          = 1 << 2,
	MTL_PROPAGATE_TEXTURES          = 1 << 3,
	MTL_PROPAGATE_SHADER_PARAMS     = 1 << 4,
	MTL_PROPAGATE_SHADER_GEN        = 1 << 5,
	MTL_PROPAGATE_VERTEX_DEF        = 1 << 6,
	MTL_PROPAGATE_LAYER_PRESETS     = 1 << 7,
	MTL_PROPAGATE_MATERIAL_SETTINGS = 1 << 8,
	MTL_PROPAGATE_ALL               = (
	  MTL_PROPAGATE_OPACITY |
	  MTL_PROPAGATE_LIGHTING |
	  MTL_PROPAGATE_ADVANCED |
	  MTL_PROPAGATE_TEXTURES |
	  MTL_PROPAGATE_SHADER_PARAMS |
	  MTL_PROPAGATE_SHADER_GEN |
	  MTL_PROPAGATE_VERTEX_DEF |
	  MTL_PROPAGATE_LAYER_PRESETS |
	  MTL_PROPAGATE_MATERIAL_SETTINGS),
	MTL_PROPAGATE_RESERVED = 1 << 9
};

/** CMaterial class
    Every Material is a member of material library.
    Materials can have child sub materials,
    Sub materials are applied to the same geometry of the parent material in the other material slots.
 */

struct SANDBOX_API SMaterialLayerResources
{
	SMaterialLayerResources();

	void SetFromUI(const char* shaderName, bool bNoDraw, bool bFadeOut);

	uint8                             m_nFlags;
	bool                              m_bRegetPublicParams;
	string                           m_shaderName;

	_smart_ptr<IMaterialLayer>        m_pMatLayer;
	_smart_ptr<SInputShaderResources> m_shaderResources;
	XmlNodeRef                        m_publicVarsCache;
};

class SANDBOX_API CMaterial : public IEditorMaterial, public CBaseLibraryItem
{
public:
	//////////////////////////////////////////////////////////////////////////
	CMaterial(const string& name, int nFlags = 0);
	~CMaterial();

	virtual void AddRef() const override { CBaseLibraryItem::AddRef(); }
	virtual void Release() const override { CBaseLibraryItem::Release(); }
	virtual const string& GetName() const override { return CBaseLibraryItem::GetName(); }

	virtual EDataBaseItemType GetType() const { return EDB_TYPE_MATERIAL; };

	//Note: this does not handle undo properly. Use RenameSubMaterial in order to properly rename a submaterial while handling undo.
	void                      SetName(const string& name);

	//! Renames a submaterial and correctly handles undo actions
	void					  RenameSubMaterial(int subMtlSlot, const string& newName);
	//! Renames a submaterial and correctly handles undo actions
	void					  RenameSubMaterial(CMaterial* pSubMaterial, const string& newName);

	//////////////////////////////////////////////////////////////////////////
	string GetFullName() const { return m_name; };

	//////////////////////////////////////////////////////////////////////////
	// File properties of the material.
	//////////////////////////////////////////////////////////////////////////
	string GetFilename(bool bForWriting = false) const;

	//! Collect filenames of texture sources used in material
	//! Return number of filenames
	int    GetTextureFilenames(std::vector<string>& outFilenames) const;
	int    GetAnyTextureFilenames(std::vector<string>& outFilenames) const;

	//! Returns number of dependencies and file paths (game path) for asset saving
	int	   GetTextureDependencies(std::vector<SAssetDependencyInfo>& outFilenames) const;

	void   UpdateFileAttributes();
	uint32 GetFileAttributes();
	//////////////////////////////////////////////////////////////////////////

	//! Sets one or more material flags from EMaterialFlags enum.
	void SetFlags(int flags)        { m_mtlFlags = flags; };
	//! Query this material flags.
	virtual int  GetFlags() const override           { return m_mtlFlags; }
	virtual bool IsMultiSubMaterial() const override { return (m_mtlFlags & MTL_FLAG_MULTI_SUBMTL) != 0; };
	bool IsPureChild() const        { return (m_mtlFlags & MTL_FLAG_PURE_CHILD) != 0; }

	virtual void GatherUsedResources(CUsedResources& resources);

	//! Set name of shader used by this material.
	void                   SetShaderName(const string& shaderName);
	//! Get name of shader used by this material.
	string                GetShaderName() const { return m_shaderName; };

	SInputShaderResources& GetShaderResources()  { return *m_shaderResources; };

	//! Get public parameters of material in variable block.
	CVarBlock* GetPublicVars(SInputShaderResources& pShaderResources);

	//! Sets variable block of public shader parameters.
	//! VarBlock must be in same format as returned by GetPublicVars().
	void SetPublicVars(CVarBlock* pPublicVars, CMaterial* pMtl);

	//! Update names/descriptions in this variable array, return a variable block for replacing
	CVarBlock* UpdateTextureNames(CSmartVariableArray textureVars[EFTT_MAX]);

	//////////////////////////////////////////////////////////////////////////
	//TODO : These methods do not belong here, CVarBlock would belong more to MaterialDialog
	//Instead accessors of CMaterial should help exposing public vars and shader gen params in a UI-agnostic way
	CVarBlock* GetShaderGenParamsVars();
	void       SetShaderGenParamsVars(CVarBlock* pBlock);
	uint64     GetShaderGenMask()            { return m_nShaderGenMask; }
	void       SetShaderGenMask(uint64 mask) { m_nShaderGenMask = mask; }
	void       SetShaderGenMaskFromUI(uint64 mask);

	//! Return variable block of shader params.
	SShaderItem& GetShaderItem() { return m_shaderItem; };

	//! Return material layers resources
	SMaterialLayerResources* GetMtlLayerResources() { return m_pMtlLayerResources; };

	const SMaterialLayerResources* GetMtlLayerResources() const { return m_pMtlLayerResources; };

	//! Get texture map usage mask for shader in this material.
	unsigned int GetTexmapUsageMask() const;

	//! Load new shader.
	bool LoadShader();

	//! Reload shader, update all shader parameters.
	void Update();

	// Reload material settings from file.
	void Reload();

	//! Serialize material settings to xml.
	virtual void Serialize(SerializeContext& ctx);

	//! Assign this material to static geometry.
	void AssignToEntity(IRenderNode* pEntity);

	//////////////////////////////////////////////////////////////////////////
	// Surface types.
	//////////////////////////////////////////////////////////////////////////
	void           SetSurfaceTypeName(const string& surfaceType, bool bUpdateMatInfo = true);
	const string& GetSurfaceTypeName() const { return m_surfaceType; };
	bool           IsBreakable2D() const;

	//////////////////////////////////////////////////////////////////////////
	// MatTemplate.
	//////////////////////////////////////////////////////////////////////////
	void           SetMatTemplate(const string& matTemplate);
	const string& GetMatTemplate() { return m_matTemplate; };

	//////////////////////////////////////////////////////////////////////////
	// Child Sub materials.
	//////////////////////////////////////////////////////////////////////////
	//! Get number of sub materials childs.
	int        GetSubMaterialCount() const;
	//! Set number of sub materials childs, does not allocate empty slots
	void       SetSubMaterialCount(int nSubMtlsCount);
	//! Get sub material child by index.
	CMaterial* GetSubMaterial(int index) const;
	// Set a material to the sub materials slot.
	// Use NULL material pointer to clear slot.
	void SetSubMaterial(int nSlot, CMaterial* mtl);
	//! Allocates a new sub material for this slot. bOnlyIfEmpty can be used to avoid overwriting non-empty slots.
	void ResetSubMaterial(int nSlot, bool bOnlyIfEmpty);
	//! Removes a sub material and deletes the slot
	void RemoveSubMaterial(int nSlot);
	//! Remove all sub materials, does not change number of sub material slots.
	void ClearAllSubMaterials();
	//! Finds the index of a submaterial, or returns -1
	int FindSubMaterial(CMaterial* pMaterial);

	void ConvertToMultiMaterial();
	void ConvertToSingleMaterial();

	//! Emitted when sub material status changes: slot added or removed, submaterial reset, converted to single/multi material
	enum SubMaterialChange
	{
		SlotCountChanged,
		SubMaterialSet,
		MaterialConverted
	};

	CCrySignal<void(SubMaterialChange)> signalSubMaterialsChanged;

	//! Return pointer to engine material.
	IMaterial* GetMatInfo(bool bUseExistingEngineMaterial = false);
	// Clear stored pointer to engine material.
	void       ClearMatInfo();

	//! Validate materials for errors.
	void Validate();

	// Check if material file can be modified.
	// Will check file attributes if it is not read only.
	bool CanModify(bool bSkipReadOnly = true);

	// Save material to file.
	bool Save(bool bSkipReadOnly = true);

	//! Sets this to be a copy of the other material (pOther)
	void CopyFrom(CMaterial* pOther);

	// Dummy material is just a placeholder item for materials that have not been found on disk.
	void SetDummy(bool bDummy) { m_bDummyMaterial = bDummy; }
	bool IsDummy() const       { return m_bDummyMaterial != 0; }

	bool IsFromEngine() const  { return m_bFromEngine != 0; }

	// Called by material manager when material selected as a current material.
	void OnMakeCurrent();

	void SetFromMatInfo(IMaterial* pMatInfo);

	// Link a submaterial by name (used for value propagation in CMaterialUI)
	void           LinkToMaterial(const string& name);
	void           LinkToMaterial(const char* name) { LinkToMaterial(string(name)); } // for CString conversion
	const string& GetLinkedMaterialName() const { return m_linkedMaterial; }

	// Return parent material for submaterial
	CMaterial* GetParent() const { return m_pParent; }

	// Return root material for submaterial
	const CMaterial* GetRoot() const
	{
		const CMaterial* p = this;
		while (p->m_pParent)
		{
			p = p->m_pParent;
		}
		return p;
	}

	//! Loads material layers
	bool LoadMaterialLayers();
	//! Updates material layers
	void UpdateMaterialLayers();

	virtual void DisableHighlight() override { SetHighlightFlags(0); }

	void SetHighlightFlags(int highlightFlags);
	void UpdateHighlighting();
	void RecordUndo(const char* sText, bool bForceUpdate = false);

	int  GetPropagationFlags() const          { return m_propagationFlags; }
	void SetPropagationFlags(const int flags) { m_propagationFlags = flags; }

	bool LayerActivationAllowed() const       { return m_allowLayerActivation; }
	void SetLayerActivation(bool allowed)     { m_allowLayerActivation = allowed; }

	//! Notifies listeners that internal properties have changed
	void NotifyPropertiesUpdated();

	//! Notifies listeners that externally important things have changed (name, filename, internal structure such as submaterials etc)
	void NotifyChanged();

private:
	void UpdateMatInfo();
	void CheckSpecialConditions();
	
	//Material is non-copyable by design. Use CopyFrom to copy all the attributes to another material through serialization
	CMaterial(const CMaterial&) = delete;
	CMaterial(CMaterial&&) = delete;

	CMaterial& operator=(const CMaterial&) = delete;
	CMaterial& operator=(CMaterial&&) = delete;

private:
	//////////////////////////////////////////////////////////////////////////
	// Variables.
	//////////////////////////////////////////////////////////////////////////
	string m_shaderName;
	string m_surfaceType;
	string m_matTemplate;
	string m_linkedMaterial;

	//! Material flags.
	int m_mtlFlags;

	// Parent material, Only valid for Pure Childs.
	CMaterial* m_pParent;

	//////////////////////////////////////////////////////////////////////////
	// Shader resources.
	//////////////////////////////////////////////////////////////////////////
	SShaderItem                       m_shaderItem;
	_smart_ptr<SInputShaderResources> m_shaderResources;

	//CVarBlockPtr m_shaderParamsVar;
	//! Common shader flags.
	uint64                  m_nShaderGenMask;
	string                  m_pszShaderGenMask;

	SMaterialLayerResources m_pMtlLayerResources[MTL_LAYER_MAX_SLOTS];

	_smart_ptr<IMaterial>   m_pMatInfo;

	XmlNodeRef              m_publicVarsCache;

	//! Array of sub materials.
	std::vector<_smart_ptr<CMaterial>> m_subMaterials;

	uint32                             m_scFileAttributes;

	unsigned char                      m_highlightFlags;

	// The propagation flags are a bit combination of the MTL_PROPAGATION enum above
	// and determine which properties get propagated to an optional linked material
	// during ui editing
	int m_propagationFlags;

	//! Material Used in level.
	int  m_bDummyMaterial          : 1; // Dummy material, name specified but material file not found.
	int  m_bFromEngine             : 1; // Material from engine.
	int  m_bIgnoreNotifyChange     : 1; // Do not send notifications about changes.
	int  m_bRegetPublicParams      : 1;
	int  m_bKeepPublicParamsValues : 1;

	bool m_allowLayerActivation;
};

#endif

