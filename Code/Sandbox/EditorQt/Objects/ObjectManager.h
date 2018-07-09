// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IObjectManager.h"
#include "Objects/BaseObject.h"
#include <Objects/ObjectPropertyWidget.h>
#include "SelectionGroup.h"
#include <CrySandbox/CrySignal.h>
#include "IEditorImpl.h"

// forward declarations.
class CGizmoManager;
class CCameraObject;
class CEntityObject;
class CObjectArchive;
class CObjectLayer;
class CObjectClassDesc;
class CWaitProgress;
class CViewport;
class CObjectManipulatorOwner;
class CGeomCacheEntity;

//////////////////////////////////////////////////////////////////////////
// Helper class to signal when we are exportin a level to game
//////////////////////////////////////////////////////////////////////////
class CObjectManagerLevelIsExporting
{
public:
	CObjectManagerLevelIsExporting()
	{
		GetIEditorImpl()->GetObjectManager()->SetExportingLevel(true);
	}

	~CObjectManagerLevelIsExporting()
	{
		GetIEditorImpl()->GetObjectManager()->SetExportingLevel(false);
	}
};

/*!
 *  CObjectManager is a singleton object that
 *  manages global set of objects in level.
 */
class CObjectManager : public IObjectManager
{
public:
	//! Selection functor callback.
	//! Callback function must return a boolean value.
	//! Return true if selection should proceed, or false to abort object selection.
	CObjectManager();
	~CObjectManager();

	bool         CanCreateObject() const override;

	CBaseObject* NewObject(CObjectClassDesc* cls, CBaseObject* prev = 0, const string& file = "");
	CBaseObject* NewObject(const string& typeName, CBaseObject* prev = 0, const string& file = "");

	void         DeleteObject(CBaseObject* obj);
	void         DeleteObjects(std::vector<CBaseObject*>& objects);

	void         DeleteAllObjects();
	CBaseObject* CloneObject(CBaseObject* obj);

	//! Get number of objects manager by ObjectManager (not contain sub objects of groups).
	int GetObjectCount() const;

	//! Get array of objects, managed by manager (not contain sub objects of groups).
	//! @param layer if 0 get objects for all layers, or layer to get objects from.
	void GetObjects(CBaseObjectsArray& objects, const CObjectLayer* layer = 0) const;
	void GetObjects(DynArray<CBaseObject*>& objects, const CObjectLayer* layer = 0) const;
	void GetObjects(CBaseObjectsArray& objects, const AABB& box) const;
	//! Works much faster for repetitive calls with similar (not very big) search area
	void GetObjectsRepeatFast(CBaseObjectsArray& objects, const AABB& box);

	//! Get array of objects that pass the filter.
	//! @param filter The filter functor, return true if you want to get the certain obj, return false if want to skip it.
	void                               GetObjects(CBaseObjectsArray& objects, BaseObjectFilterFunctor const& filter) const;

	virtual std::vector<CObjectLayer*> GetUniqueLayersRelatedToObjects(const std::vector<CBaseObject*>& objects) const override;

	//! Keep only top-most parents in the resulting array of objects
	void FilterParents(const CBaseObjectsArray& objects, CBaseObjectsArray& out) const;
	void FilterParents(const std::vector<_smart_ptr<CBaseObject>>& objects, CBaseObjectsArray& out) const;
	//! Keep only top-most links in the resulting array of objects
	void FilterLinkedObjects(const CBaseObjectsArray& objects, CBaseObjectsArray& out) const;
	void FilterLinkedObjects(const std::vector<_smart_ptr<CBaseObject>>& objects, CBaseObjectsArray& out) const;

	//! Link selection to target
	virtual void LinkSelectionTo(CBaseObject* pLinkTo, const char* szTargetName = "") override;
	//! Link single object to target
	virtual void Link(CBaseObject* pObject, CBaseObject* pLinkTo, const char* szTargetName = "") override;
	//! Link multiple objects to target
	virtual void Link(const std::vector<CBaseObject*> objects, CBaseObject* pLinkTo, const char* szTargetName = "") override;

	//! Get all camera sources in object manager
	void GetCameras(std::vector<CEntityObject*>& objects);

	//! Update objects.
	void Update();

	//! Display objects on display context.
	void Display(CObjectRenderHelper& objRenderHelper);

	//! Called when selecting without selection helpers - this is needed since
	//! the visible object cache is normally not updated when not displaying helpers.
	void ForceUpdateVisibleObjectCache(DisplayContext& dc);

	//! Check intersection with objects.
	//! Find intersection with nearest to ray origin object hit by ray.
	//! If distance tollerance is specified certain relaxation applied on collision test.
	//! @return true if hit any object, and fills hitInfo structure.
	bool HitTest(HitContext& hitInfo);

	//! Check intersection with an object.
	//! @return true if hit, and fills hitInfo structure.
	bool HitTestObject(CBaseObject* obj, HitContext& hc);

	//! Send event to all objects.
	//! Will cause OnEvent handler to be called on all objects.
	void SendEvent(ObjectEvent event);

	//! Send event to all objects within given bounding box.
	//! Will cause OnEvent handler to be called on objects within bounding box.
	void SendEvent(ObjectEvent event, const AABB& bounds);

	//////////////////////////////////////////////////////////////////////////
	//! Find object by ID.
	CBaseObject* FindObject(const CryGUID& guid) const;
	//////////////////////////////////////////////////////////////////////////
	//! Find object by name.
	// When runtime class is specified, returns valid object only if it belongs to specified runtime class.
	CBaseObject* FindObject(const char* objectName, CRuntimeClass* pRuntimeClass = 0) const;
	//////////////////////////////////////////////////////////////////////////
	//! Find objects of given type.
	void FindObjectsOfType(const CRuntimeClass* pClass, std::vector<CBaseObject*>& result) override;
	void FindObjectsOfType(ObjectType type, std::vector<CBaseObject*>& result) override;
	//////////////////////////////////////////////////////////////////////////
	//! Find objects which intersect with a given AABB.
	virtual void FindObjectsInAABB(const AABB& aabb, std::vector<CBaseObject*>& result) const;

	//////////////////////////////////////////////////////////////////////////
	// Find object from in game physical entity.
	CBaseObject* FindPhysicalObjectOwner(struct IPhysicalEntity* pPhysicalEntity);

	//////////////////////////////////////////////////////////////////////////
	// Operations on objects.
	//////////////////////////////////////////////////////////////////////////
	//! Makes object visible or invisible.
	void HideObject(CBaseObject* obj, bool hide);
	//! Isolates the current object's visibility
	void ToggleHideAllBut(CBaseObject* pObj) override;
	//! Unhide all hidden objects.
	void UnhideAll();
	//! Freeze object, making it unselectable.
	void FreezeObject(CBaseObject* obj, bool freeze);
	//! Isolates the current object's frozen state
	void ToggleFreezeAllBut(CBaseObject* pObject) override;
	//! Unfreeze all frozen objects.
	void UnfreezeAll();

	//////////////////////////////////////////////////////////////////////////
	// Object Selection.
	//////////////////////////////////////////////////////////////////////////
	void SelectObject(CBaseObject* obj);
	void UnselectObject(CBaseObject* obj);

	void SelectObjects(const std::vector<CBaseObject*>& objects) override;
	void UnselectObjects(const std::vector<CBaseObject*>& objects) override;
	void ToggleSelectObjects(const std::vector<CBaseObject*>& objects) override;
	void SelectAndUnselectObjects(const std::vector<CBaseObject*>& selectObjects, const std::vector<CBaseObject*>& unselectObjects) override;
	void SelectAll() override;

	int  MoveObjects(const AABB& box, const Vec3& offset, int nRot = 0, bool bIsCopy = false);

	//! Selects/Unselects all objects within 2d rectangle in given viewport.
	void SelectObjectsInRect(CViewport* view, const CRect& rect, ESelectOp bSelect);
	void FindSelectableObjectsInRect(CViewport* view, const CRect& rect, std::vector<CBaseObject*>& selectableObjects);
	void FindObjectsInRect(CViewport* view, const CRect& rect, std::vector<CryGUID>& guids);

	bool IsObjectSelectableInHitContext(CViewport* view, HitContext& hc, CBaseObject* obj);

	//! Clear default selection set.
	void ClearSelection() override;

	//! Deselect all current selected objects and selects object that were unselected.
	void InvertSelection() override;

	//! Get current selection.
	const CSelectionGroup* GetSelection() const { return &m_currSelection; };

	bool                   IsObjectDeletionAllowed(CBaseObject* pObject);

	//! Delete all objects in selection group.
	void   DeleteSelection();

	uint32 ForceID() const     { return m_forceID; }
	void   ForceID(uint32 FID) { m_forceID = FID; }

	//! Generates uniq name base on type name of object.
	string GenUniqObjectName(const string& typeName);
	//! Register object name in object manager, needed for generating uniq names.
	void   RegisterObjectName(const string& name);
	//! Decrease name number and remove if it was last in object manager, needed for generating uniq names.
	void   UpdateRegisterObjectName(const string& name);
	//! Enable/Disable generating of unique object names (Enabled by default).
	//! Return previous value.

	//! Register XML template of runtime class.
	void RegisterClassTemplate(XmlNodeRef& templ);
	//! Load class templates for specified directory,
	void LoadClassTemplates(const string& path);

	//! Find object class by name.
	CObjectClassDesc*           FindClass(const string& className);
	void                        GetClassCategories(std::vector<string>& categories);
	void                        GetClassCategoryToolClassNamePairs(std::vector<std::pair<string, string>>& categoryToolClassNamePairs) override;
	void                        GetCreatableClassTypes(const string& category, std::vector<CObjectClassDesc*>& types);

	virtual std::vector<string> GetAllClasses() const override;

	//! Export objects to xml.
	//! When onlyShared is true ony objects with shared flags exported, overwise only not shared object exported.
	void Export(const string& levelPath, XmlNodeRef& rootNode, bool onlyShared);

	//! Serialize Objects in manager to specified XML Node.
	//! @param flags Can be one of SerializeFlags.
	void Serialize(XmlNodeRef& rootNode, bool bLoading, int flags = SERIALIZE_ALL);

	//! Load objects from object archive.
	//! @param bSelect if set newly loaded object will be selected.
	void LoadObjects(CObjectArchive& ar, bool bSelect);

	//! Delete from Object manager all objects without SHARED flag.
	void DeleteNotSharedObjects();
	//! Delete from Object manager all objects with SHARED flag.
	void DeleteSharedObjects();

	bool AddObject(CBaseObject* obj);
	void RemoveObject(CBaseObject* obj);
	void RemoveObjects(const std::vector<CBaseObject*>& objects);
	void ChangeObjectId(const CryGUID& oldId, const CryGUID& newId);
	void NotifyPrefabObjectChanged(CBaseObject* pObject);
	bool IsDuplicateObjectName(const string& newName) const
	{
		return FindObject(newName) ? true : false;
	}
	bool IsInvalidObjectName(const string& newName) const override;
	void ShowDuplicationMsgWarning(CBaseObject* obj, const string& newName, bool bShowMsgBox) const;
	void ShowInvalidNameMsgWarning(CBaseObject* obj, const string& newName, bool bShowMsgBox) const override;
	//! Attempt to change an object's name
	//! @param bGenerateUnique If a name is duplicated then instead of failing we will generate a unique name
	bool ChangeObjectName(CBaseObject* obj, const string& newName, bool bGenerateUnique = false) override;

	void RegisterTypeConverter(CRuntimeClass* pSource, const char* szTargetName, std::function<void(CBaseObject* pObject)> conversionFunc) override;

	void IterateTypeConverters(CRuntimeClass* pSource, std::function<void(const char* szTargetName, std::function<void(CBaseObject* pObject)> conversionFunc)> callback) override;

	// Enables/Disables creating of game objects.
	void SetCreateGameObject(bool enable) { m_createGameObjects = enable; };
	//! Return true if objects loaded from xml should immidiatly create game objects associated with them.
	bool IsCreateGameObjects() const      { return m_createGameObjects; };

	//////////////////////////////////////////////////////////////////////////
	//! Get acess to object layers manager.
	CObjectLayerManager* GetLayersManager() const { return m_pLayerManager; }

	//////////////////////////////////////////////////////////////////////////
	//! Get acess to object layers manager.
	IObjectLayerManager* GetIObjectLayerManager() const;

	//////////////////////////////////////////////////////////////////////////
	//! Invalidate visibily settings of objects.
	void InvalidateVisibleList();

	//////////////////////////////////////////////////////////////////////////
	// ObjectManager notification Callbacks.
	//////////////////////////////////////////////////////////////////////////

	// Legacy object notification, use signalObjectChanged.Connect(); instead
	void AddObjectEventListener(const EventCallback& cb);

	// Legacy object notification, use signalObjectChanged.DisconnectById()
	// or signalObjectChanged.DisconnectByIdDisconnectObject instead
	void RemoveObjectEventListener(const EventCallback& cb);

	//////////////////////////////////////////////////////////////////////////
	// Used to indicate starting and ending of objects loading.
	//////////////////////////////////////////////////////////////////////////
	void StartObjectsLoading(int numObjects);
	void EndObjectsLoading();

	//////////////////////////////////////////////////////////////////////////
	// Gathers all resources used by all objects.
	void GatherUsedResources(CUsedResources& resources, CObjectLayer* pLayer = 0);

	//////////////////////////////////////////////////////////////////////////
	IStatObj*           GetGeometryFromObject(CBaseObject* pObject);
	ICharacterInstance* GetCharacterFromObject(CBaseObject* pObject);

	// Called when object gets modified.
	void                         OnObjectModified(CBaseObject* pObject, bool bDelete, bool boModifiedTransformOnly);

	void                         UnregisterNoExported();
	void                         RegisterNoExported();

	virtual void                 FindAndRenameProperty2(const char* property2Name, const string& oldValue, const string& newValue);
	virtual void                 FindAndRenameProperty2If(const char* property2Name, const string& oldValue, const string& newValue, const char* otherProperty2Name, const string& otherValue);

	virtual void                 SaveEntitiesInternalState(struct IDataWriteStream& writer) const;
	virtual void                 AssignLayerIDsToRenderNodes();

	void                         ResolveMissingObjects();
	void                         ResolveMissingMaterials();

	class CObjectPhysicsManager* GetPhysicsManager()
	{ return m_pPhysicsManager; }

	bool         IsLoading() const                           { return m_bLoadingObjects; }

	void         SetExportingLevel(bool bExporting) override { m_bLevelExporting = bExporting; }
	bool         IsExportingLevelInprogress() const override { return m_bLevelExporting; }

	void         SetSelectionMask(int mask) const override;

	virtual void EmitPopulateInspectorEvent() const override;

	virtual bool IsLayerChanging() const override { return m_bLayerChanging; }

	CCrySignal<void(int)> signalSelectionMaskChanged;

private:
	friend CObjectArchive;
	friend class CBaseObject;
	/** Creates and serialize object from xml node.
	   @param objectNode Xml node to serialize object info from.
	   @param pUndoObject Pointer to deleted object for undo.
	   @param bLoadInCurrentLayer set true to load the object in the current layer.
	 */
	CBaseObject* NewObject(CObjectArchive&, CBaseObject* pUndoObject, bool bLoadInCurrentLayer) override;

	//! Update visibility of all objects.
	void UpdateVisibilityList();
	//! Get array of all objects in manager.
	void GetAllObjects(TBaseObjects& objects) const;

	void LinkToObject(CBaseObject* pObject, CBaseObject* pLinkTo);
	void LinkToObject(const std::vector<CBaseObject*> objects, CBaseObject* pLinkTo);
	void LinkToBone(const std::vector<CBaseObject*> objects, CEntityObject* pLinkTo);
	void LinkToGeomCacheNode(const std::vector<CBaseObject*> objects, CGeomCacheEntity* pLinkTo, const char* target);

	void UnlinkObjects(const std::vector<CBaseObject*>& objects);

	//! Selects an individual object, note that this path is *not* used for multi-selection, see MarkSelectObjects!
	bool SetObjectSelected(CBaseObject* pObject, bool bSelect, bool shouldNotify = true);

	// Recursive functions potentially taking into child objects into account
	void SelectObjectInRect(CBaseObject* pObj, CViewport* view, HitContext hc, ESelectOp bSelect);
	void HitTestObjectAgainstRect(CBaseObject* pObj, CViewport* view, HitContext hc, std::vector<CryGUID>& guids);

	// Legacy object notification, use signalObjectChanged(event, data); instead
	void NotifyObjectListeners(CBaseObject* pObject, EObjectListenerEvent event);

	void FindDisplayableObjects(DisplayContext& dc, const SRenderingPassInfo* passInfo, bool bDisplay);

	void UpdateAttachedEntities();

	//Selects or deselects the objects and sends object notification but not global notification
	bool         MarkSelectObjects(const std::vector<CBaseObject*>& objects, std::vector<CBaseObject*>& selected);
	bool         MarkUnselectObjects(const std::vector<CBaseObject*>& objects, std::vector<CBaseObject*>& deselected);
	void         MarkToggleSelectObjects(const std::vector<CBaseObject*>& objects, std::vector<CBaseObject*>& selected, std::vector<CBaseObject*>& deselected);

	void         NotifySelectionChanges(const std::vector<CBaseObject*>& selected, const std::vector<CBaseObject*>& deselected);

	bool         IsAncestortInSet(CBaseObject* pObject, const std::set<const CBaseObject*>& objectsSet) const;
	bool         IsLinkedAncestorInSet(CBaseObject* pObject, const std::set<const CBaseObject*>& objectsSet) const;

	virtual void SetLayerChanging(bool bLayerChanging) override { m_bLayerChanging = bLayerChanging; }

	void         RemoveObjectsAndNotify(const std::vector<CBaseObject*>& objects);

	void         FilterOutDeletedObjects(std::vector<CBaseObject*>& objects);

private:
	typedef std::unordered_map<CryGUID, CBaseObjectPtr> Objects;
	Objects           m_objects;

	AABB              m_cachedAreaBox = AABB::RESET;
	CBaseObjectsArray m_cachedAreaObjects;

	//! Used for forcing IDs of "GetEditorObjectID" of PreFabs, as they used to have random IDs on each load
	uint32 m_forceID;

	//! Array of currently visible objects.
	TBaseObjects m_visibleObjects;
	bool         m_bVisibleObjectValid;
	unsigned int m_lastHideMask;

	float        m_maxObjectViewDistRatio;

	//////////////////////////////////////////////////////////////////////////
	// Selection.
	//! Current selection group.
	//! This is a pointer due to the legacy support of supporting saved selections by name and switching them by switching the pointer
	//! Leaving the pointer for now
	CSelectionGroup m_currSelection;

	//TODO : these should not be necessary
	bool m_createGameObjects;
	bool m_bLoadingObjects;
	bool m_bLevelExporting;

	bool m_isUpdateVisibilityList;
	bool m_bLayerChanging;

	friend CObjectLayerManager;
	//! Manager of object layers.
	CObjectLayerManager* m_pLayerManager;

	//////////////////////////////////////////////////////////////////////////
	// Loading progress.
	CWaitProgress* m_pLoadProgress;
	int            m_loadedObjects;
	int            m_totalObjectsToLoad;
	//////////////////////////////////////////////////////////////////////////

	class CObjectPhysicsManager* m_pPhysicsManager;

	//////////////////////////////////////////////////////////////////////////
	// Numbering for names.
	//////////////////////////////////////////////////////////////////////////
	typedef std::map<string, std::set<uint16>, stl::less_stricmp<string>> NameNumbersMap;
	NameNumbersMap m_nameNumbersMap;

	struct SConverter
	{
		CRuntimeClass*                            pSource;
		string                                    targetName;
		std::function<void(CBaseObject* pObject)> conversionFunc;
	};

	std::vector<SConverter> m_typeConverters;
};

