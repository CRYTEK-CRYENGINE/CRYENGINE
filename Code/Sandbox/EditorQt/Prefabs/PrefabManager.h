// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseLibraryManager.h"
#include "Objects/BaseObject.h"
#include "IEditorImpl.h"
#include <AssetSystem/AssetType.h>

class CPrefabItem;
class CPrefabObject;
class CPrefabLibrary;
class CPrefabEvents;

typedef _smart_ptr<CPrefabObject> CPrefabObjectPtr;

class CUndoAddObjectsToPrefab : public IUndoObject
{
public:
	CUndoAddObjectsToPrefab(CPrefabObject* prefabObj, std::vector<CBaseObject*>& objects);

protected:
	virtual const char* GetDescription() { return "Add Objects To Prefab"; }

	virtual void        Undo(bool bUndo);
	virtual void        Redo();

private:

	struct SObjectsLinks
	{
		CryGUID              m_object;
		CryGUID              m_objectParent;
		std::vector<CryGUID> m_objectsChilds;
	};

	typedef std::vector<SObjectsLinks> TObjectsLinks;

	CPrefabObjectPtr m_pPrefabObject;
	TObjectsLinks    m_addedObjects;
};

//! \sa CAssetType::Create
struct SPrefabCreateParams : CAssetType::SCreateParams
{
	//! This instructs the prefab asset type to create a prefab item AND a prefab object containing the objects in the pNewMembersSelection selection group
	//! nullptr selection means that a new prefab object should not be created
	SPrefabCreateParams(const ISelectionGroup* pSelectionToAdd)
		: pNewMembersSelection(pSelectionToAdd)
	{

	}
	const ISelectionGroup* pNewMembersSelection;
};

/** Manages Prefab libraries and systems.
 */
class SANDBOX_API CPrefabManager : public CBaseLibraryManager, public IUndoManagerListener
{
public:
	struct SkipPrefabUpdate
	{
		SkipPrefabUpdate()
		{
			GetIEditorImpl()->GetPrefabManager()->SetSkipPrefabUpdate(true);
		}
		~SkipPrefabUpdate()
		{
			GetIEditorImpl()->GetPrefabManager()->SetSkipPrefabUpdate(false);
		}
	};

	CPrefabManager();
	~CPrefabManager();

	// Clear all prototypes
	void           ClearAll();

	IDataBaseItem* CreateItem(const string& filename);
	//virtual IDataBaseItem* CreateItem(IDataBaseLibrary* pLibrary) override { return CBaseLibraryManager::CreateItem(pLibrary); }

	//! Delete item from library and manager.
	void DeleteItem(IDataBaseItem* pItem);

	//! Load item by its GUID.
	IDataBaseItem* LoadItem(const CryGUID& guid);

	//! Serialize manager.
	virtual void Serialize(XmlNodeRef& node, bool bLoading) override;

	//! Make new prefab item from selection. Displays a pop-up dialog to save new prefab.
	CPrefabItem* MakeFromSelection(const CSelectionGroup* pSelectionGroup);

	//! Add selected objects to prefab (which selected too)
	void         AddSelectionToPrefab();
	void         AddSelectionToPrefab(CPrefabObject* pPrefab);

	void         OpenSelected();
	void         CloseSelected();

	virtual void CloneObjectsFromPrefabs(std::vector<CBaseObject*>& childObjects);
	void         CloneAllFromPrefabs(std::vector<CPrefabObject*>& pPrefabs);
	void         CloneAllFromSelection();

	virtual void ExtractObjectsFromPrefabs(std::vector<CBaseObject*>& childObjects);
	void         ExtractAllFromPrefabs(std::vector<CPrefabObject*>& pPrefabs);
	void         ExtractAllFromSelection();

	virtual bool AttachObjectToPrefab(CPrefabObject* pPrefab, CBaseObject* pObject);

	bool         ClosePrefabs(std::vector<CPrefabObject*>& prefabObjects);
	bool         OpenPrefabs(std::vector<CPrefabObject*>& prefabObjects);

	//! Get all prefab objects in level
	void GetPrefabObjects(std::vector<CPrefabObject*>& outPrefabObjects);

	//! Get prefab instance count used in level
	int GetPrefabInstanceCount(IDataBaseItem* pPrefabItem);

	//! Get prefab events
	ILINE CPrefabEvents*      GetPrefabEvents() const            { return m_pPrefabEvents; }

	bool                      ShouldSkipPrefabUpdate() const     { return m_skipPrefabUpdate; }
	void                      SetSkipPrefabUpdate(bool skip)     { m_skipPrefabUpdate = skip; }

	virtual void              BeginRestoreTransaction() override { SetSkipPrefabUpdate(true); }
	virtual void              EndRestoreTransaction() override   { SetSkipPrefabUpdate(false); }

	virtual IDataBaseLibrary* AddLibrary(const string& library, bool bSetFullFilename = false) override;
	virtual IDataBaseLibrary* LoadLibrary(const string& filename, bool bReload = false) override;
	virtual string            MakeFilename(const string& library) override;

	// Allows to generate prefab assets from level files of CE5.5
	static void importAssetsFromLevel(XmlNodeRef& levelRoot);

	//Updates all the assets to the latest version
	void UpdateAllPrefabsToLatestVersion();

	//Get all prefab items contained in the objects vector
	std::vector<CPrefabItem*> GetAllPrefabItems(const std::vector<CBaseObject*>& objects);
	//Find all instances of pPrefabItem in the current level
	std::vector<CBaseObject*> FindAllInstancesOfItem(const CPrefabItem* pPrefabItem);
	//Find all instances of pPrefabItem in the current level, clear the selection and add them
	void                      SelectAllInstancesOfItem(const CPrefabItem* pPrefabItem);

protected:

	virtual CBaseLibraryItem* MakeNewItem() override;
	virtual CBaseLibrary*     MakeNewLibrary() override;
	virtual string            GetRootNodeName() override;
	virtual string            GetLibsPath() override;
	virtual const char*       GetFileExtension() const override { return "prefab"; }

private:
	void ExpandGroup(CBaseObject* pObject, CSelectionGroup& selection) const;

private:
	CPrefabEvents* m_pPrefabEvents;
	bool           m_skipPrefabUpdate;
};
