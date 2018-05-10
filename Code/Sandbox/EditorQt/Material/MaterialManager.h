// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseLibraryManager.h"
#include "Material.h"

class CMaterial;
class CMaterialLibrary;
class CMaterialHighlighter;

#define MATERIAL_FILE_EXT ".mtl"
#define MATERIALS_PATH    "materials/"

enum EHighlightMode
{
	eHighlight_Pick          = BIT(0),
	eHighlight_Breakable     = BIT(1),
	eHighlight_NoSurfaceType = BIT(2),
	eHighlight_All           = 0xFFFFFFFF
};

/** Manages all entity prototypes and material libraries.
 */
class SANDBOX_API CMaterialManager : public CBaseLibraryManager, public IMaterialManagerListener
{
public:
	//! Notification callback.
	typedef Functor0 NotifyCallback;

	CMaterialManager();
	~CMaterialManager();

	// Clear all prototypes and material libraries.
	void ClearAll();

	//////////////////////////////////////////////////////////////////////////
	// Materials.
	//////////////////////////////////////////////////////////////////////////

	// Loads material.
	CMaterial*   LoadMaterial(const string& sMaterialName, bool bMakeIfNotFound = true);
	CMaterial*   LoadMaterial(const char* sMaterialName, bool bMakeIfNotFound = true);
	virtual void OnRequestMaterial(IMaterial* pMaterial);
	// Creates a new material from a xml node.
	CMaterial*   CreateMaterial(const string& sMaterialName, XmlNodeRef& node = XmlNodeRef(), int nMtlFlags = 0, unsigned long nLoadingFlags = 0);
	CMaterial*   CreateMaterial(const char* sMaterialName, XmlNodeRef& node = XmlNodeRef(), int nMtlFlags = 0, unsigned long nLoadingFlags = 0);

	// Duplicate material and do nothing more.
	CMaterial* DuplicateMaterial(const char* newName, CMaterial* pOriginal);

	//! Export property manager to game.
	void         Export(XmlNodeRef& node);
	int          ExportLib(CMaterialLibrary* pLib, XmlNodeRef& libNode);

	virtual void SetSelectedItem(IDataBaseItem* pItem);
	void         SetCurrentMaterial(CMaterial* pMtl);
	//! Get currently active material.
	CMaterial*   GetCurrentMaterial() const;

	void         SetCurrentFolder(const string& folder);

	// This material will be highlighted
	void SetHighlightedMaterial(CMaterial* pMtl);
	const CMaterial* GetHighlightedMaterial() const { return m_pHighlightMaterial; }
	void GetHighlightColor(ColorF* color, float* intensity, int flags);
	void HighlightedMaterialChanged(CMaterial* pMtl);
	// highlightMask is a combination of EHighlightMode flags
	void SetHighlightMask(int highlightMask);
	int  GetHighlightMask() const { return m_highlightMask; }
	void SetMarkedMaterials(const std::vector<_smart_ptr<CMaterial>>& markedMaterials);
	void OnLoadShader(CMaterial* pMaterial);

	//! Serialize property manager.
	virtual void Serialize(XmlNodeRef& node, bool bLoading);

	//! Enumerates shaders if necessary, returns count
	int GetShaderCount();
	//! Enumerates shaders if necessary, returns names of shaders found
	const std::vector<string>& GetShaderList();

	virtual void SaveAllLibs();

	//////////////////////////////////////////////////////////////////////////
	// IMaterialManagerListener implementation
	//////////////////////////////////////////////////////////////////////////
	// Called when material manager tries to load a material.
	virtual IMaterial* OnLoadMaterial(const char* sMtlName, bool bForceCreation = false, unsigned long nLoadingFlags = 0);
	virtual void       OnCreateMaterial(IMaterial* pMaterial);
	virtual void       OnDeleteMaterial(IMaterial* pMaterial);
	//////////////////////////////////////////////////////////////////////////

	// Convert filename of material file into the name of the material.
	string FilenameToMaterial(const string& filename);

	// Convert name of the material to the filename.
	string MaterialToFilename(const string& sMaterialName, bool bForWriting = false);

	//////////////////////////////////////////////////////////////////////////
	// Convert 3DEngine IMaterial to Editor's CMaterial pointer.
	CMaterial* FromIMaterial(IMaterial* pMaterial);

	// Open File selection dialog to create a new material.
	CMaterial* SelectNewMaterial(int nMtlFlags, const char* sStartPath = NULL);

	// Synchronize material between 3dsMax and editor.
	void SyncMaterialEditor();

	//////////////////////////////////////////////////////////////////////////
	void GotoMaterial(CMaterial* pMaterial);
	void GotoMaterial(IMaterial* pMaterial);

	// Gather resources from the game material.
	static void GatherResources(IMaterial* pMaterial, CUsedResources& resources);
	void        CreateTerrainLayerFromMaterial(CMaterial* pMaterial);

	void        Command_Create();
	void        Command_CreateMulti();
	void        Command_ConvertToMulti();
	void        Command_Duplicate();
	void        Command_Merge();
	void        Command_Delete();
	void        Command_AssignToSelection();
	void        Command_ResetSelection();
	void        Command_SelectAssignedObjects();
	void        Command_SelectFromObject();
	void        Command_CreateTerrainLayer();

protected:
	// Delete specified material, erases material file, and unassigns from all objects.
	bool                      DeleteMaterial(CMaterial* pMtl);
	// Open save as dialog for saving materials.
	static bool               SelectSaveMaterial(string& itemName, const char* defaultStartPath);

	void                      OnEditorNotifyEvent(EEditorNotifyEvent event);

	virtual CBaseLibraryItem* MakeNewItem();
	virtual CBaseLibrary*     MakeNewLibrary();
	//! Root node where this library will be saved.
	virtual string           GetRootNodeName();
	//! Path to libraries in this manager.
	virtual string           GetLibsPath();
	virtual void              ReportDuplicateItem(CBaseLibraryItem* pItem, CBaseLibraryItem* pOldItem);

	void                      UpdateHighlightedMaterials();
	void                      AddForHighlighting(CMaterial* pMaterial);
	void                      RemoveFromHighlighting(CMaterial* pMaterial, int mask);
	int                       GetHighlightFlags(CMaterial* pMaterial) const;

	// For material syncing with 3dsMax.
	void PickPreviewMaterial(HWND hWndCaller);
	void InitMatSender();

	void OnDebugFlagsChanged();

	void EnumerateShaders();

protected:
	string m_libsPath;

	// Currently selected (focused) material, in material browser.
	_smart_ptr<CMaterial>              m_pCurrentMaterial;
	// current selected folder
	string                            m_currentFolder;
	// List of materials selected in material browser tree.
	std::vector<_smart_ptr<CMaterial>> m_markedMaterials;
	// IMaterial is needed to not let 3Dengine release selected IMaterial
	_smart_ptr<IMaterial>              m_pCurrentEngineMaterial;

	// Material highlighting
	_smart_ptr<CMaterial>  m_pHighlightMaterial;
	CMaterialHighlighter*  m_pHighlighter;
	int                    m_highlightMask;

	bool                   m_bHighlightingMaterial;

	bool                   m_bMaterialsLoaded;

	class CMaterialSender* m_MatSender;

	bool				   m_bShadersEnumerated;
	std::vector<string>	   m_shaderList;
};

