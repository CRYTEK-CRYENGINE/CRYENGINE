// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "BaseLibraryManager.h"
#include "Objects/BaseObject.h"

class CPrefabItem;
class CPrefabObject;
class CPrefabLibrary;
class CPrefabEvents;

typedef _smart_ptr<CPrefabObject> CPrefabObjectPtr;

class CUndoAddObjectsToPrefab : public IUndoObject
{
public:
	CUndoAddObjectsToPrefab(CPrefabObject* prefabObj, TBaseObjects& objects);

protected:
	virtual const char* GetDescription() { return "Add Objects To Prefab"; };

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
	void ClearAll();

	//! Delete item from library and manager.
	void DeleteItem(IDataBaseItem* pItem);

	//! Serialize manager.
	virtual void Serialize(XmlNodeRef& node, bool bLoading);

	//! Make new prefab item from selection.
	CPrefabItem* MakeFromSelection();

	//! Add selected objects to prefab (which selected too)
	void AddSelectionToPrefab();
	void AddSelectionToPrefab(CPrefabObject* pPrefab);

	void OpenSelected();
	void CloseSelected();

	virtual void CloneObjectsFromPrefabs(std::vector<CBaseObject*>& childObjects);
	void         CloneAllFromPrefabs(std::vector<CPrefabObject*>& pPrefabs);
	void         CloneAllFromSelection();

	virtual void ExtractObjectsFromPrefabs(std::vector<CBaseObject*>& childObjects);
	void         ExtractAllFromPrefabs(std::vector<CPrefabObject*>& pPrefabs);
	void         ExtractAllFromSelection();

	virtual bool AttachObjectToPrefab(CPrefabObject* prefab, CBaseObject* obj);

	bool         ClosePrefabs(std::vector<CPrefabObject*>& prefabObjects);
	bool         OpenPrefabs(std::vector<CPrefabObject*>& prefabObjects);

	//! Get all prefab objects in level
	void GetPrefabObjects(std::vector<CPrefabObject*>& outPrefabObjects);

	//! Get prefab instance count used in level
	int GetPrefabInstanceCount(CPrefabItem* pPrefabItem);

	//! Get prefab events
	ILINE CPrefabEvents* GetPrefabEvents() const { return m_pPrefabEvents; }

	IDataBaseLibrary*    LoadLibrary(const string& filename, bool bReload = false);

	bool                 ShouldSkipPrefabUpdate() const { return m_skipPrefabUpdate; }
	void                 SetSkipPrefabUpdate(bool skip) { m_skipPrefabUpdate = skip; }

	virtual void BeginRestoreTransaction() override { SetSkipPrefabUpdate(true); }
	virtual void EndRestoreTransaction() override { SetSkipPrefabUpdate(false); }

private:
	void ExpandGroup(CBaseObject* pObject, CSelectionGroup& selection) const;

protected:
	virtual CBaseLibraryItem* MakeNewItem();
	virtual CBaseLibrary*     MakeNewLibrary();
	//! Root node where this library will be saved.
	virtual string           GetRootNodeName();
	//! Path to libraries in this manager.
	virtual string           GetLibsPath();

	string        m_libsPath;

	CPrefabEvents* m_pPrefabEvents;
	bool           m_skipPrefabUpdate;
};

