// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IObjectManager_h__
#define __IObjectManager_h__
#pragma once

// forward declarations.
class CEntityObject;
struct DisplayContext;
struct IGizmoManager;
class CObjectLayer;
class CTrackViewAnimNode;
class CUsedResources;
class CSelectionGroup;
class CObjectLayerManager;
class CObjectClassDesc;
class CObjectArchive;
class CObjectPhysicsManager;
class CRect;
class CViewport;
class CBaseObject;
struct CObjectEvent;
struct HitContext;
struct IObjectLayerManager;
class CSelectionGroup;
class CObjectRenderHelper;

enum EObjectListenerEvent;

#include "ObjectEvent.h"
#include <CryExtension/CryGUID.h>

enum SerializeFlags
{
	SERIALIZE_ALL            = 0,
	SERIALIZE_ONLY_SHARED    = 1,
	SERIALIZE_ONLY_NOTSHARED = 2,
};

//////////////////////////////////////////////////////////////////////////
typedef std::vector<CBaseObject*>                                    CBaseObjectsArray;
typedef std::pair<bool(CALLBACK*)(CBaseObject const&, void*), void*> BaseObjectFilterFunctor;

class CBatchProcessDispatcher
{
public:
	~CBatchProcessDispatcher();
	void Start(const CBaseObjectsArray& objects, bool force = false);
	void Finish();

private:
	bool m_shouldDispatch { false };
};

class IGuidProvider
{
public:
	virtual ~IGuidProvider() {};
	virtual CryGUID GetFrom(const CryGUID& loadedGuid) const = 0;
	CryGUID         GetFor(CBaseObject*) const;
};

class CRandomUniqueGuidProvider : public IGuidProvider
{
public:
	virtual CryGUID GetFrom(const CryGUID&) const override;
};

//////////////////////////////////////////////////////////////////////////
//
// Interface to access editor objects scene graph.
//
//////////////////////////////////////////////////////////////////////////
struct IObjectManager
{
public:
	enum class ESelectOp
	{
		eSelect,
		eUnselect,
		eToggle,
		eNone
	};

	//! This callback will be called on response to object event.
	typedef Functor2<CBaseObject*, int> EventCallback;

	virtual bool         CanCreateObject() const = 0;

	virtual CBaseObject* NewObject(CObjectClassDesc*, CBaseObject* prev = nullptr, const string& file = "") = 0;
	virtual CBaseObject* NewObject(const string& typeName, CBaseObject* prev = nullptr, const string& file = "") = 0;
	virtual CBaseObject* NewObject(CObjectArchive&, CBaseObject* pUndoObject = nullptr, bool loadInCurrentLayer = false) = 0;

	virtual void         DeleteObject(CBaseObject* obj) = 0;
	virtual void         DeleteObjects(std::vector<CBaseObject*>& objects) = 0;
	virtual void         DeleteAllObjects() = 0;
	virtual CBaseObject* CloneObject(CBaseObject* obj) = 0;

	//! Get number of objects manager by ObjectManager (not contain sub objects of groups).
	virtual int GetObjectCount() const = 0;

	//! Get array of objects, managed by manager (not contain sub objects of groups).
	//! @param layer if 0 get objects for all layers, or layer to get objects from.
	virtual void GetObjects(CBaseObjectsArray& objects, const CObjectLayer* layer = 0) const = 0;
	virtual void GetObjects(DynArray<CBaseObject*>& objects, const CObjectLayer* layer = 0) const = 0;
	virtual void GetObjects(CBaseObjectsArray& objects, const AABB& box) const = 0;
	//! Works much faster for repetitive calls with similar (not very big) search area
	virtual void GetObjectsRepeatFast(CBaseObjectsArray& objects, const AABB& box) = 0;

	//! Get array of objects that pass the filter.
	//! @param filter The filter functor, return true if you want to get the certain obj, return false if want to skip it.
	virtual void GetObjects(CBaseObjectsArray& objects, BaseObjectFilterFunctor const& filter) const = 0;

	//! Get array of unique layers related to objects
	virtual std::vector<CObjectLayer*> GetUniqueLayersRelatedToObjects(const std::vector<CBaseObject*>& objects) const = 0;

	//! Keep only top-most parents in the resulting array of objects
	virtual void FilterParents(const CBaseObjectsArray& objects, CBaseObjectsArray& out) const = 0;
	virtual void FilterParents(const std::vector<_smart_ptr<CBaseObject>>& objects, CBaseObjectsArray& out) const = 0;
	//! Keep only top-most links in the resulting array of objects
	virtual void FilterLinkedObjects(const CBaseObjectsArray& objects, CBaseObjectsArray& out) const = 0;
	virtual void FilterLinkedObjects(const std::vector<_smart_ptr<CBaseObject>>& objects, CBaseObjectsArray& out) const = 0;

	//! Link selection to target
	virtual void LinkSelectionTo(CBaseObject* pLinkTo, const char* szTargetName = "") = 0;
	//! Link single object to target
	virtual void Link(CBaseObject* pObject, CBaseObject* pLinkTo, const char* szTargetName = "") = 0;
	//! Link multiple objects to target
	virtual void Link(const std::vector<CBaseObject*> objects, CBaseObject* pLinkTo, const char* szTargetName = "") = 0;

	//! Display objects on specified display context.
	virtual void Display(CObjectRenderHelper& objRenderHelper) = 0;

	//! Called when selecting without selection helpers - this is needed since
	//! the visible object cache is normally not updated when not displaying helpers.
	virtual void ForceUpdateVisibleObjectCache(DisplayContext& dc) = 0;

	//! Check intersection with objects.
	//! Find intersection with nearest to ray origin object hit by ray.
	//! If distance tollerance is specified certain relaxation applied on collision test.
	//! @return true if hit any object, and fills hitInfo structure.
	virtual bool HitTest(HitContext& hitInfo) = 0;

	//! Check intersection with an object.
	//! @return true if hit, and fills hitInfo structure.
	virtual bool HitTestObject(CBaseObject* obj, HitContext& hc) = 0;

	//! Send event to all objects.
	//! Will cause OnEvent handler to be called on all objects.
	virtual void SendEvent(ObjectEvent event) = 0;

	//! Send event to all objects within given bounding box.
	//! Will cause OnEvent handler to be called on objects within bounding box.
	virtual void SendEvent(ObjectEvent event, const AABB& bounds) = 0;

	//////////////////////////////////////////////////////////////////////////
	//! Find object by ID.
	virtual CBaseObject* FindObject(const CryGUID& guid) const = 0;
	//////////////////////////////////////////////////////////////////////////
	//! Find object by name.
	// When runtime class is specified, returns valid object only if it belongs to specified runtime class.
	virtual CBaseObject* FindObject(const char* objectName, CRuntimeClass* pRuntimeClass = 0) const = 0;
	//////////////////////////////////////////////////////////////////////////
	//! Find objects of given type.
	virtual void FindObjectsOfType(const CRuntimeClass* pClass, std::vector<CBaseObject*>& result) = 0;
	virtual void FindObjectsOfType(ObjectType type, std::vector<CBaseObject*>& result) = 0;
	//////////////////////////////////////////////////////////////////////////
	//! Find objects which intersect with a given AABB.
	virtual void FindObjectsInAABB(const AABB& aabb, std::vector<CBaseObject*>& result) const = 0;

	//////////////////////////////////////////////////////////////////////////
	// Find object from in game physical entity.
	CBaseObject* FindPhysicalObjectOwner(struct IPhysicalEntity* pPhysicalEntity);

	//////////////////////////////////////////////////////////////////////////
	// Operations on objects.
	//////////////////////////////////////////////////////////////////////////
	//! Makes object visible or invisible.
	virtual void HideObject(CBaseObject* obj, bool hide) = 0;
	//! Isolates the current object's visibility
	virtual void ToggleHideAllBut(CBaseObject* obj) = 0;
	//! Unhide all hidden objects.
	virtual void UnhideAll() = 0;
	//! Freeze object, making it unselectable.
	virtual void FreezeObject(CBaseObject* obj, bool freeze) = 0;
	//! Isolates the current object's frozen state
	virtual void ToggleFreezeAllBut(CBaseObject* pObject) = 0;
	//! Unfreeze all frozen objects.
	virtual void UnfreezeAll() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Object Selection.
	//////////////////////////////////////////////////////////////////////////
	virtual void SelectObject(CBaseObject* obj) = 0;
	virtual void UnselectObject(CBaseObject* obj) = 0;

	virtual void SelectObjects(const std::vector<CBaseObject*>& objects) = 0;
	virtual void UnselectObjects(const std::vector<CBaseObject*>& objects) = 0;
	virtual void ToggleSelectObjects(const std::vector<CBaseObject*>& objects) = 0;
	virtual void SelectAndUnselectObjects(const std::vector<CBaseObject*>& selectObjects, const std::vector<CBaseObject*>& unselectObjects) = 0;
	virtual void SelectAll() = 0;

	virtual int  MoveObjects(const AABB& box, const Vec3& offset, int nRot = 0, bool isCopy = false) = 0;

	//! Selects/Unselects all objects within 2d rectangle in given viewport.
	virtual void SelectObjectsInRect(CViewport* view, const CRect& rect, ESelectOp bSelect) = 0;
	virtual void FindObjectsInRect(CViewport* view, const CRect& rect, std::vector<CryGUID>& guids) = 0;

	//! Clear default selection set.
	virtual void ClearSelection() = 0;

	//! Deselect all current selected objects and selects object that were unselected.
	virtual void InvertSelection() = 0;

	//! Get current selection.
	virtual const CSelectionGroup* GetSelection() const = 0;

	//! Delete all objects in current selection group.
	virtual void DeleteSelection() = 0;

	//! Generates uniq name base on type name of object.
	virtual string GenUniqObjectName(const string& typeName) = 0;
	//! Register object name in object manager, needed for generating uniq names.
	virtual void   RegisterObjectName(const string& name) = 0;

	//! Find object class by name.
	virtual CObjectClassDesc*   FindClass(const string& className) = 0;
	virtual void                GetClassCategories(std::vector<string>& categories) = 0;
	virtual void                GetClassCategoryToolClassNamePairs(std::vector<std::pair<string, string>>& categoryToolClassNamePairs) = 0;
	virtual void                GetCreatableClassTypes(const string& category, std::vector<CObjectClassDesc*>& types) = 0;
	virtual std::vector<string> GetAllClasses() const = 0;

	//! Export objects to xml.
	//! When onlyShared is true ony objects with shared flags exported, overwise only not shared object exported.
	virtual void Export(const string& levelPath, XmlNodeRef& rootNode, bool isOnlyShared) = 0;

	//! Serialize Objects in manager to specified XML Node.
	//! @param flags Can be one of SerializeFlags.
	virtual void Serialize(XmlNodeRef& rootNode, bool isLoading, int flags = SERIALIZE_ALL) = 0;

	//! Load objects from object archive.
	//! @param bSelect if set newly loaded object will be selected.
	virtual void LoadObjects(CObjectArchive& ar, bool select) = 0;

	virtual void ChangeObjectId(const CryGUID& oldId, const CryGUID& newId) = 0;
	virtual void NotifyPrefabObjectChanged(CBaseObject* pObject) = 0;
	virtual bool IsDuplicateObjectName(const string& newName) const = 0;
	virtual bool IsInvalidObjectName(const string& newName) const = 0;
	virtual void ShowDuplicationMsgWarning(CBaseObject* obj, const string& newName, bool showMsgBox) const = 0;
	virtual void ShowInvalidNameMsgWarning(CBaseObject* obj, const string& newName, bool showMsgBox) const = 0;
	//! Attempt to change an object's name
	//! @param bGenerateUnique If a name is duplicated then instead of failing we will generate a unique name
	virtual bool ChangeObjectName(CBaseObject* obj, const string& newName, bool generateUnique = false) = 0;

	//! while loading PreFabs we need to force this IDs
	//! to force always the same IDs, on each load.
	//! needed for RAM-maps assignments
	virtual uint32 ForceID() const = 0;
	virtual void   ForceID(uint32 FID) = 0;

	//! Registers a method of converting one object type to another
	virtual void RegisterTypeConverter(CRuntimeClass* pSource, const char* szTargetName, std::function<void(CBaseObject* pObject)> conversionFunc) = 0;

	//! Goes through available converters for the source type
	virtual void IterateTypeConverters(CRuntimeClass* pSource, std::function<void(const char* szTargetName, std::function<void(CBaseObject*)> )> callback) = 0;

	//TODO: this is always true, remove this mechanic entirely
	// Enables/Disables creating of game objects.
	virtual void SetCreateGameObject(bool enable) = 0;
	//! Return true if objects loaded from xml should immediately create game objects associated with them.
	virtual bool IsCreateGameObjects() const = 0;

	//////////////////////////////////////////////////////////////////////////
	//! Get acess to object layers manager.
	virtual CObjectLayerManager* GetLayersManager() const = 0;

	//////////////////////////////////////////////////////////////////////////
	//! Get acess to object layers manager.
	virtual IObjectLayerManager* GetIObjectLayerManager() const = 0;

	//////////////////////////////////////////////////////////////////////////
	//! Get acess to object physics manager
	virtual CObjectPhysicsManager* GetPhysicsManager() = 0;

	//////////////////////////////////////////////////////////////////////////
	//! Invalidate visibily settings of objects.
	virtual void InvalidateVisibleList() = 0;

	//////////////////////////////////////////////////////////////////////////
	// ObjectManager notification Callbacks.
	//////////////////////////////////////////////////////////////////////////
	virtual void AddObjectEventListener(const EventCallback& cb) = 0;
	virtual void RemoveObjectEventListener(const EventCallback& cb) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Used to indicate starting and ending of objects loading.
	//////////////////////////////////////////////////////////////////////////
	virtual void StartObjectsLoading(int numObjects) = 0;
	virtual void EndObjectsLoading() = 0;

	//////////////////////////////////////////////////////////////////////////
	// Engine Helper functions.
	//////////////////////////////////////////////////////////////////////////
	virtual struct IStatObj*           GetGeometryFromObject(CBaseObject* pObject) = 0;
	virtual struct ICharacterInstance* GetCharacterFromObject(CBaseObject* pObject) = 0;

	//////////////////////////////////////////////////////////////////////////
	// Gathers all resources used by all objects.
	virtual void GatherUsedResources(CUsedResources& resources, CObjectLayer* pLayer = 0) = 0;

	//////////////////////////////////////////////////////////////////////////
	virtual void FindAndRenameProperty2(const char* property2Name, const string& oldValue, const string& newValue) = 0;
	virtual void FindAndRenameProperty2If(const char* property2Name, const string& oldValue, const string& newValue, const char* otherProperty2Name, const string& otherValue) = 0;

	virtual void ResolveMissingObjects() = 0;

	virtual bool IsLoading() const = 0;

	// Save internal state (used for LiveCreate level export)
	virtual void SaveEntitiesInternalState(struct IDataWriteStream& writer) const = 0;

	// Make sure all render nodes have their layerID assigned
	virtual void AssignLayerIDsToRenderNodes() = 0;

	virtual void SetExportingLevel(bool isExporting) = 0;
	virtual bool IsExportingLevelInprogress() const = 0;

	virtual void SetSelectionMask(int mask) const = 0;

	virtual void EmitPopulateInspectorEvent() const = 0;

	// Legacy object notification, use signalObjectChanged(event, data); instead
	virtual void NotifyObjectListeners(CBaseObject* pObject, enum EObjectListenerEvent event) = 0;

	// Called when object gets modified.
	virtual void            OnObjectModified(CBaseObject* pObject, bool shouldDelete, bool isModifiedTransformOnly) = 0;

	virtual bool            IsLayerChanging() const = 0;

	CBatchProcessDispatcher GetBatchProcessDispatcher() { return {}; }

private:
	friend class CBaseObject;

	virtual void SetLayerChanging(bool isLayerChanging) = 0;

public:
	//! Substitute old interface-based observer
	CCrySignal<void(CObjectEvent&)> signalObjectChanged;

	//! New method of determining when selection is changed
	//! Query IObjectManager::GetSelection to see what selection currently is
	CCrySignal<void(const std::vector<CBaseObject*>&, const std::vector<CBaseObject*>&)>                 signalSelectionChanged;

	CCrySignal<void(CBaseObject* pParent, const std::vector<CBaseObject*>& objects)>                     signalObjectsDetached;
	CCrySignal<void(CBaseObject* pParent, const std::vector<CBaseObject*>& objects, bool keepTransform)> signalBeforeObjectsDetached;
	CCrySignal<void(CBaseObject* pParent, const std::vector<CBaseObject*>& objects)>                     signalObjectsAttached;
	CCrySignal<void(CBaseObject* pParent, const std::vector<CBaseObject*>& objects, bool keepTransform)> signalBeforeObjectsAttached;
	CCrySignal<void(const CObjectLayer&, const std::vector<CBaseObject*>& objects)>                      signalBeforeObjectsDeleted;
	CCrySignal<void(const CObjectLayer&, const std::vector<CBaseObject*>& objects)>                      signalObjectsDeleted;

	CCrySignal<void(const std::vector<CBaseObject*>& objects)> signalBatchProcessStarted;
	CCrySignal<void()> signalBatchProcessFinished;
};

#endif // __IObjectManager_h__

