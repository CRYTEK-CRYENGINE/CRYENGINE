// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMath/Cry_Geo.h>
#include "HitContext.h"
#include "ClassDesc.h"
#include "Util/Variable.h"
#include "CryExtension/CryGUID.h"
#include "Objects/DisplayContext.h"
#include "IIconManager.h"

//////////////////////////////////////////////////////////////////////////
// forward declarations.
class CUndoBaseObject;
class CObjectArchive;
struct IEditorMaterial;
class CEdGeometry;
struct ISelectionGroup;
struct SRayHitInfo;
struct DisplayContext;
struct IObjectManager;
struct IStatObj;
struct Ray;
struct IDisplayViewport;
struct IObjectLayer;
class SubObjectSelectionReferenceFrameCalculator;
class CPopupMenuItem;
class CInspectorWidgetCreator;

//////////////////////////////////////////////////////////////////////////
typedef _smart_ptr<CBaseObject>     CBaseObjectPtr;
typedef std::vector<CBaseObjectPtr> TBaseObjects;

class CGroup;
class CGuidCollisionResolver;

//////////////////////////////////////////////////////////////////////////
/*!
   This class used for object references remapping during cloning operation.
 */
class CObjectCloneContext
{
public:
	//! Add cloned object.
	EDITOR_COMMON_API void AddClone(CBaseObject* pFromObject, CBaseObject* pToObject);

	//! Find cloned object for given object.
	EDITOR_COMMON_API CBaseObject* FindClone(CBaseObject* pFromObject) const;

	typedef std::map<CBaseObject*, CBaseObject*> ObjectsMap;
	ObjectsMap m_objectsMap;
};

//////////////////////////////////////////////////////////////////////////
enum EObjectChangedOpType
{
	eOCOT_Empty = 0,
	eOCOT_Modify,
	eOCOT_ModifyTransform,
	eOCOT_ModifyTransformInLibOnly,
	eOCOT_Add,
	eOCOT_Delete,
	eOCOT_Count
};

struct SObjectChangedContext
{
	CryGUID              m_modifiedObjectGuidInPrefab; //! The object that was modified identified by guid in the prefab
	CryGUID              m_modifiedObjectGlobalId;     //! The object id globally unique and used in ObjectManager
	EObjectChangedOpType m_operation;                  //! What was the operation on the modified object
	Matrix34             m_localTM;                    //! If we are in modify transform case this is the local TM info

	SObjectChangedContext(EObjectChangedOpType optype) : m_modifiedObjectGuidInPrefab(CryGUID::Null()), m_modifiedObjectGlobalId(CryGUID::Null()), m_operation(optype) { m_localTM.SetIdentity(); }
	SObjectChangedContext() : m_modifiedObjectGuidInPrefab(CryGUID::Null()), m_modifiedObjectGlobalId(CryGUID::Null()), m_operation(eOCOT_Empty) { m_localTM.SetIdentity(); }
};

//////////////////////////////////////////////////////////////////////////
enum ObjectFlags
{
	OBJFLAG_SELECTED    = 0x0001, //!< Object is selected. (Do not set this flag explicitly).
	OBJFLAG_HIDDEN      = 0x0002, //!< Object is hidden.
	OBJFLAG_FROZEN      = 0x0004, //!< Object is frozen (Visible but cannot be selected)
	OBJFLAG_SHARED      = 0x0010, //!< This object is shared between missions.

	OBJFLAG_PREFAB      = 0x0020, //!< This object is part of prefab object.

	OBJFLAG_NO_HITTEST  = 0x0080, //!< This object will be not a target of ray hit test for deep selection mode.

	OBJFLAG_DELETED        = 0x04000, //!< This object is deleted.
	OBJFLAG_HIGHLIGHT      = 0x08000, //!< Object is highlighted (When mouse over).
	OBJFLAG_INVISIBLE      = 0x10000, //!< This object is invisible.
	OBJFLAG_SUBOBJ_EDITING = 0x20000, //!< This object is in the sub object editing mode.

	OBJFLAG_SHOW_ICONONTOP = 0x100000, //!< Icon will be drawn on top of the object.
	OBJFLAG_HIDE_HELPERS   = 0x200000, //!< Helpers will be hidden.

	OBJFLAG_JOINTGEN_USED  = 0x400000, //!< Automatic joint generation was applied to the object at some point

	OBJFLAG_PERSISTMASK    = OBJFLAG_HIDDEN | OBJFLAG_FROZEN,
};

#define ERF_GET_WRITABLE(flags) (flags)

///////////////////////////////////////////////////////////////////////////
enum MaterialChangeFlags
{
	MATERIALCHANGE_SURFACETYPE = 0x001,
	MATERIALCHANGE_ALL         = 0xFFFFFFFF,
};

//////////////////////////////////////////////////////////////////////////
//! Return values from CBaseObject::MouseCreateCallback method.
enum MouseCreateResult
{
	MOUSECREATE_CONTINUE = 0, //!< Continue placing this object.
	MOUSECREATE_ABORT,        //!< Abort creation of this object.
	MOUSECREATE_OK,           //!< Accept this object.
};

// Flags used for object interaction
enum EObjectUpdateFlags
{
	eObjectUpdateFlags_UserInput       = 0x00001,
	eObjectUpdateFlags_PositionChanged = 0x00002,
	eObjectUpdateFlags_RotationChanged = 0x00004,
	eObjectUpdateFlags_ScaleChanged    = 0x00008,
	eObjectUpdateFlags_DoNotInvalidate = 0x00100,  // Do not cause InvalidateTM call.
	eObjectUpdateFlags_ParentChanged   = 0x00200,  // When parent transformation change.
	eObjectUpdateFlags_Undo            = 0x00400,  // When doing undo operation.
	eObjectUpdateFlags_RestoreUndo     = 0x00800,  // When doing RestoreUndo operation (This is different from normal undo).
	eObjectUpdateFlags_Animated        = 0x01000,  // When doing animation.
	eObjectUpdateFlags_MoveTool        = 0x02000,  // Transformation changed by the move tool
	eObjectUpdateFlags_ScaleTool       = 0x04000,  // Transformation changed by the scale tool
	eObjectUpdateFlags_IgnoreSelection = 0x08000,  // Transformation should ignore selection state of object (trackview)
	eObjectUpdateFlags_UserInputUndo   = 0x10000,  // Undo operation related to user input rather than actual Undo
};

#define OBJECT_TEXTURE_ICON_SIZE 32
#define OBJECT_TEXTURE_ICON_SCALE 10.0f

enum EScaleWarningLevel
{
	eScaleWarningLevel_None,
	eScaleWarningLevel_Rescaled,
	eScaleWarningLevel_RescaledNonUniform,
};

enum ERotationWarningLevel
{
	eRotationWarningLevel_None = 0,
	eRotationWarningLevel_Rotated,
	eRotationWarningLevel_RotatedNonRectangular,
};

// Used for external control of object position without changing the object's real position (e.g. TrackView)
class ITransformDelegate
{
public:
	// Called when matrix got invalidated
	virtual void MatrixInvalidated() = 0;

	// Returns current delegated transforms, base transform is passed for delegates
	// that need it, e.g. for overriding only X
	virtual Vec3 GetTransformDelegatePos(const Vec3& basePos) const = 0;
	virtual Quat GetTransformDelegateRotation(const Quat& baseRotation) const = 0;
	virtual Vec3 GetTransformDelegateScale(const Vec3& baseScale) const = 0;

	// Sets the delegate transform.
	virtual void SetTransformDelegatePos(const Vec3& position, bool ignoreSelection) = 0;
	virtual void SetTransformDelegateRotation(const Quat& rotation, bool ignoreSelection) = 0;
	virtual void SetTransformDelegateScale(const Vec3& scale, bool ignoreSelection) = 0;

	// If those return true the base object uses its own transform instead
	virtual bool IsPositionDelegated(bool ignoreSelection) const = 0;
	virtual bool IsRotationDelegated(bool ignoreSelection) const = 0;
	virtual bool IsScaleDelegated(bool ignoreSelection) const = 0;
};

class CObjectRenderHelper
{
public:
	CObjectRenderHelper(DisplayContext& _displayContext, const SRenderingPassInfo& _passinfo) : displayContext(_displayContext), passInfo(_passinfo) {}

	DisplayContext& GetDisplayContextRef()
	{
		return displayContext;
	}

	const SRenderingPassInfo& GetPassInfo()
	{
		return passInfo;
	}

	void Render(const Matrix34& tm, int objType = eStatObject_Anchor)
	{
		displayContext.RenderObject(objType, tm, passInfo);
	}

private:
	DisplayContext& displayContext;
	const SRenderingPassInfo& passInfo;
};

/*!
 *	CBaseObject is the base class for all objects which can be placed in map.
 *	Every object belongs to class specified by ClassDesc.
 *	Specific object classes must override this class, to provide specific functionality.
 *	Objects are reference counted and only destroyed when last reference to object
 *	is destroyed.
 *
 */

 //! Generic object event struct. To be used with EObjectListenerEvent
struct CObjectEvent
{
	CObjectEvent(EObjectListenerEvent type, CBaseObject* pObj)
		: m_type(type)
		, m_pObj(pObj)
	{
	};

	EObjectListenerEvent m_type;
	// !Caller of this event
	CBaseObject*         m_pObj;
};

//! Data for handling EObjectListenerEvent on multiple objects
struct CObjectsEvent
{
	EObjectListenerEvent m_type;
	const std::vector<CBaseObject*>& objects;
};

struct CObjectLayerChangeEvent : public CObjectEvent
{
	CObjectLayerChangeEvent(CBaseObject* pObj, IObjectLayer* oldLayer)
		: CObjectEvent(OBJECT_ON_LAYERCHANGE, pObj)
		, m_poldLayer(oldLayer)
	{
	}

	IObjectLayer* m_poldLayer;
};

struct CObjectPreLinkEvent : public CObjectEvent
{
	CObjectPreLinkEvent(CBaseObject* pObj, CBaseObject* pLinkedTo)
		: CObjectEvent(OBJECT_ON_PRELINKED, pObj)
		, m_pLinkedTo(pLinkedTo)
	{
	}

	CBaseObject* m_pLinkedTo;
};

struct CObjectUnLinkEvent : public CObjectEvent
{
	CObjectUnLinkEvent(CBaseObject* pObj, CBaseObject* pLinkedTo)
		: CObjectEvent(OBJECT_ON_UNLINKED, pObj)
		, m_pLinkedTo(pLinkedTo)
	{
	}

	// !Object we are unlinking from. Necessary because pObj has already been detached and has no parent
	CBaseObject* m_pLinkedTo;
};

class EDITOR_COMMON_API CBaseObject : public CObject, public _i_reference_target_t
{
	DECLARE_DYNAMIC(CBaseObject);
public:

	//! This callback will be called if object is deleted.
	typedef Functor2<CBaseObject*, int> EventCallback;

	//! Childs structure.
	typedef std::vector<_smart_ptr<CBaseObject>> BaseObjectArray;

	//! Retrieve class description of this object.
	CObjectClassDesc* GetClassDesc() const { return m_classDesc; };

	/** Check if both object are of same class.
	 */
	virtual bool       IsSameClass(CBaseObject* obj);

	virtual ObjectType GetType() const { return m_classDesc->GetObjectType(); };
	//	const char* GetTypeName() const { return m_classDesc->ClassName(); };
	string             GetTypeName() const;
	virtual string     GetTypeDescription() const { return m_classDesc->ClassName(); };

	//////////////////////////////////////////////////////////////////////////
	// Layer support.
	//////////////////////////////////////////////////////////////////////////
	void          SetLayer(string layerFullName);
	void          SetLayer(IObjectLayer* layer);
	IObjectLayer* GetLayer() const { return m_layer; };
	void          SetLayerModified();

	static bool   FilterByLayer(CBaseObject const& obj, void* pLayer);

	//////////////////////////////////////////////////////////////////////////
	// Flags.
	//////////////////////////////////////////////////////////////////////////
	void SetFlags(int flags) { m_flags |= flags; };
	int  GetFlags() const { return m_flags; }
	void ClearFlags(int flags) { m_flags &= ~flags; };
	bool CheckFlags(int flags) const { return (m_flags & flags) != 0; };

	//! Returns true if object visible.
	bool IsVisible() const { return !IsHidden(); }
	//! Returns true if object hidden.
	bool IsHidden() const;
	//! Check against min spec.
	bool IsHiddenBySpec() const;
	//! Returns true if object frozen.
	bool IsFrozen() const;
	//! Returns true if object is selected.
	bool IsSelected() const { return CheckFlags(OBJFLAG_SELECTED); }
	//! Returns true if object is shared between missions.
	bool IsShared() const { return CheckFlags(OBJFLAG_SHARED); }

	//! Returns the Game entity associated with this object if applicable, i.e. if this is an entity object
	virtual IEntity* GetIEntity() { return nullptr; }

	// Check if object potentially can be selected.
	bool IsSelectable() const;

	// Return texture icon.
	bool HaveTextureIcon() const { return m_nTextureIcon != 0; };
	int  GetTextureIcon() const { return m_nTextureIcon; }
	void SetTextureIcon(int nTexIcon) { m_nTextureIcon = nTexIcon; }

	//! Set the name of the entity script. (This is temporary workaround allowing to mark CEntityObject as light source before CEntityObject::InitVariables() call. TODO: Remove it when light object is converted into separate class.)
	virtual void SetScriptName(const string& file, CBaseObject* pPrev) {};

	//! Set shared between missions flag.
	virtual void                SetShared(bool bShared);
	//! Set object hidden status.
	virtual void                SetHidden(bool bHidden, bool bAnimated = false);
	//! Set object visible status.
	virtual void                SetVisible(bool bVisible, bool bAnimated = false) { SetHidden(!bVisible, bAnimated); }
	//! Set object frozen status.
	virtual void                SetFrozen(bool bFrozen);

	//! Return associated 3DEngine render node
	virtual struct IRenderNode* GetEngineNode() const { return NULL; };
	//! Set object highlighted (Note: not selected)
	void                        SetHighlight(bool bHighlight);
	//! Check if object is highlighted.
	bool                        IsHighlighted() const { return CheckFlags(OBJFLAG_HIGHLIGHT); }
	//! Check if object can have measurement axises.
	virtual bool                HasMeasurementAxis() const { return true; }

	//////////////////////////////////////////////////////////////////////////
	// Object Id.
	//////////////////////////////////////////////////////////////////////////
	//! Get unique object id.
	//! Every object will have its own unique id assigned.
	const CryGUID& GetId() const { return m_guid; };

	//////////////////////////////////////////////////////////////////////////
	// Prefabs support
	//////////////////////////////////////////////////////////////////////////
	//! To identify an object WHITHIN a prefab uniquely use this GUID
	const CryGUID& GetIdInPrefab() const { return m_guidInPrefab; }
	void           SetIdInPrefab(const CryGUID& guid) { m_guidInPrefab = guid; }
	bool           IsPartOfPrefab() const;
	//! Called when something changed and we have to sync a prefab representation
	void           UpdatePrefab(EObjectChangedOpType updateReason = eOCOT_Modify);
	CBaseObject*   GetPrefab() const;

	//////////////////////////////////////////////////////////////////////////
	// Name.
	//////////////////////////////////////////////////////////////////////////
	//! Get name of object.
	virtual const string& GetName() const;
	virtual string        GetComment() const { return ""; }
	virtual string        GetWarningsText() const;

	//! Change name of object.
	virtual void SetName(const string& name);
	//! Change name of object.
	inline void  SetName(const char* name) { SetName(string(name)); } // for CString conversion
	//! Set object name and make sure it is unique.
	void         SetUniqName(const string& name);
	//! Set object name and make sure it is unique.
	inline void  SetUniqName(const char* name) { SetUniqName(string(name)); } // for CString conversion

	//////////////////////////////////////////////////////////////////////////
	// Geometry.
	//////////////////////////////////////////////////////////////////////////
	//! Set object position.
	bool SetPos(const Vec3& pos, int flags = 0);

	//! Set object rotation angles.
	bool SetRotation(const Quat& rotate, int flags = 0);

	//! Set object scale.
	bool SetScale(const Vec3& scale, int flags = 0);

	//! Get object position.
	//! This will get position from delegate if delegate is set
	const Vec3 GetPos() const;

	//! Get object local rotation quaternion.
	//! This will get rotation from delegate if delegate is set
	const Quat GetRotation() const;

	//! Get object scale.
	//! This will get scale from delegate if delegate is set
	const Vec3   GetScale() const;

	virtual bool IsScalable() const { return true; }
	virtual bool IsRotatable() const { return true; }

	//! Assign display color to the object.
	virtual void ChangeColor(COLORREF color);
	//! Get object color.
	COLORREF     GetColor() const { return m_color; };

	//! Set current transform delegate. Pass nullptr to unset.
	//! When this is set, the delegate takes over the transform of the object. Used for instance for preview with the TrackView.
	//! @param bInvalidateTM if true, triggers InvalidateTM().
	virtual void        SetTransformDelegate(ITransformDelegate* pTransformDelegate, bool bInvalidateTM);
	ITransformDelegate* GetTransformDelegate() const { return m_pTransformDelegate; }

	//! An asset file changed on disk.
	virtual void InvalidateGeometryFile(const string& gamePath) {}

	//////////////////////////////////////////////////////////////////////////
	// CHILDS
	//////////////////////////////////////////////////////////////////////////

	//! Return true if node have children.
	bool   HaveChilds() const { return !m_children.empty(); }
	//! Return the number of attached children.
	size_t GetChildCount() const { return m_children.size(); }

	//! Return the number of linked objects.
	size_t       GetLinkedObjectCount() const { return m_linkedObjects.size(); }
	//! Get linked object by index.
	CBaseObject* GetLinkedObject(size_t i) const;

	//! Get child by index.
	CBaseObject* GetChild(size_t const i) const;
	//! Return parent node if exist.
	CBaseObject* GetParent() const { return m_parent; };
	//! Return object we're linked to
	CBaseObject* GetLinkedTo() const { return m_pLinkedTo; };
	//! Scans hierarchy up to determine if we are a descendant of pObject
	virtual bool IsDescendantOf(const CBaseObject* pObject) const;
	//! Scans hierarchy up to determine if we child of specified node.
	virtual bool IsChildOf(CBaseObject* node);
	//! Scans hierarchy up to determine if we are linked to the specified object.
	virtual bool IsLinkedDescendantOf(CBaseObject* pObject);
	//! Get all child objects
	void         GetAllChildren(TBaseObjects& outAllChildren, CBaseObject* pObj = NULL) const;
	void         GetAllChildren(DynArray<_smart_ptr<CBaseObject>>& outAllChildren, CBaseObject* pObj = NULL) const;
	void         GetAllChildren(ISelectionGroup& outAllChildren, CBaseObject* pObj = NULL) const;
	void         GetAllPrefabFlagedChildren(std::vector<CBaseObject*>& outAllChildren, CBaseObject* pObj = NULL) const;
	void         GetAllPrefabFlagedChildren(ISelectionGroup& outAllChildren, CBaseObject* pObj = NULL) const;
	//! Attach new child node.
	//! @param bKeepPos if true Child node will keep its world space position.
	//! @param bInvalidateTM if true, trigger InvalidateTM() on parent and child nodes.
	void AttachChild(CBaseObject* child, bool bKeepPos = true, bool bInvalidateTM = true);
	//! Attach new child node when the object is not a sort of a group object like AttachChild()
	//! but if the object is a group object, the group object should be set to all children objects recursively.
	//! and if the object is a prefab object, the prefab object should be loaded from the prefab item.
	//! @param bKeepPos if true Child node will keep its world space position.
	virtual void AddMember(CBaseObject* pMember, bool bKeepPos = true);
	virtual void RemoveMember(CBaseObject* pMember, bool bKeepPos = true, bool bPlaceOnRoot = false) {};

	//! Detach all children of this node.
	virtual void DetachAll(bool bKeepPos = true, bool bPlaceOnRoot = false) {};

	virtual void AttachChildren(std::vector<CBaseObject*>& objects, bool shouldKeepPos = true, bool shouldInvalidateTM = true) {}
	virtual void DetachChildren(std::vector<CBaseObject*>& objects, bool shouldKeepPos = true, bool shouldPlaceOnRoot = false) {}
	virtual void AddMembers(std::vector<CBaseObject*>& objects, bool shouldKeepPos = true) {}
	virtual void RemoveMembers(std::vector<CBaseObject*>& members, bool shouldKeepPos = true, bool shouldPlaceOnRoot = false) {}

	// Detach this node from parent.
	virtual void DetachThis(bool bKeepPos = true, bool bPlaceOnRoot = false);
	//! Checks if the object we want to link to is a valid target
	bool CanLinkTo(CBaseObject* pLinkTo) const;
	//! Link to object
	virtual void LinkTo(CBaseObject* pParent, bool bKeepPos = true);
	// Unlink this object from parent
	virtual void UnLink(bool placeOnRoot = false);
	// Unlink all children
	virtual void UnLinkAll();

	// Check if all linked descendants are contained in the same layer
	bool AreLinkedDescendantsInLayer(const IObjectLayer* pLayer) const;
	// Check if all parent links are contained in the same layer
	bool IsLinkedAncestryInLayer(const IObjectLayer* pLayer) const;
	// Is any object in the linking hierarchy contained by layer
	bool IsAnyLinkedAncestorInLayer(const IObjectLayer* pLayer) const;

	//////////////////////////////////////////////////////////////////////////
	// MATRIX
	//////////////////////////////////////////////////////////////////////////
	//! Get objects' local transformation matrix.
	Matrix34 GetLocalTM() const { Matrix34 tm; CalcLocalTM(tm); return tm; };

	//! Get objects' world-space transformation matrix.
	const Matrix34& GetWorldTM() const;
	//! Get parent world matrix. 
	//! If parented to a group it will return to group's world matrix
	//! If linked to an object it will return the object's world matrix
	//! If attached to an entity's bone, it'll return the world matrix of the bone
	Matrix34 GetParentWorldTM() const;

	// Gets matrix of link attachment point
	virtual Matrix34 GetLinkAttachPointWorldTM() const;

	bool             GetManipulatorMatrix(RefCoordSys coordSys, Matrix34& tm) const;
	// Checks if the attachment point is valid
	virtual bool     IsParentAttachmentValid() const;

	//! Set position in world space.
	virtual void SetWorldPos(const Vec3& pos, int flags = 0);

	//! Get position in world space.
	Vec3 GetWorldPos() const { return GetWorldTM().GetTranslation(); };
	Ang3 GetWorldAngles() const;

	//! Set xform of object given in world space.
	virtual void SetWorldTM(const Matrix34& tm, int flags = 0);

	//! Set object xform.
	virtual void SetLocalTM(const Matrix34& tm, int flags = 0);

	// Set object xform.
	virtual void SetLocalTM(const Vec3& pos, const Quat& rotate, const Vec3& scale, int flags = 0);

	//////////////////////////////////////////////////////////////////////////
	// Interface to be implemented in plugins.
	//////////////////////////////////////////////////////////////////////////

	//! Called when object is being created (use GetMouseCreateCallback for more advanced mouse creation callback).
	virtual int MouseCreateCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int flags);

	//! Offset from terrain during creation (only used if object is positioned relative to terrain)
	virtual float GetCreationOffsetFromTerrain() const { return 1.f; }

	//! Draw object to specified viewport.
	virtual void Display(CObjectRenderHelper& displayInfo) = 0;

	//! Perform intersection testing of this object.
	//! Return true if was hit.
	virtual bool HitTest(HitContext& hc) { return false; };

	//! Perform intersection testing of this object with rectangle.
	//! Return true if was hit.
	virtual bool HitTestRect(HitContext& hc);

	//! Perform intersection testing of this object based on its icon helper.
	//! Return true if was hit.
	virtual bool HitHelperTest(HitContext& hc);

		//! Get bounding box for display of object in world coordinate space.
	virtual void GetDisplayBoundBox(AABB& box);

	//! Get bounding box of object in world coordinate space.
	virtual void GetBoundBox(AABB& box);

	//! Get bounding box of object in local object space.
	virtual void GetLocalBounds(AABB& box);

	//! Called after some parameter been modified.
	virtual void SetModified(bool boModifiedTransformOnly, bool bNotifyObjectManager);

	//! Called when visibility of this object changes.
	//! Derived class may override this to respond to new visibility setting.
	virtual void UpdateVisibility(bool bVisible);

	//! Serialize object to/from xml.
	//! @param xmlNode XML node to load/save serialized data to.
	//! @param bLoading true if loading data from xml.
	//! @param bUndo true if loading or saving data for Undo/Redo purposes.
	virtual void Serialize(CObjectArchive& ar);

	//// Preload called before serialize after all objects where completely loaded.
	//virtual void PreLoad( CObjectArchive &ar ) {};
	// Post load called after all objects where completely loaded.
	virtual void PostLoad(CObjectArchive& ar) {};

	//! Export object to xml.
	//! Return created object node in xml.
	//! This function exports object to the Engine
	virtual XmlNodeRef Export(const string& levelPath, XmlNodeRef& xmlNode);

	//! Handle events received by object.
	//! Override in derived classes, to handle specific events.
	virtual void OnEvent(ObjectEvent event);

	//! Generate dynamic context menu for the object
	virtual void OnContextMenu(CPopupMenuItem* menu);

	//////////////////////////////////////////////////////////////////////////
	// Group support.
	//////////////////////////////////////////////////////////////////////////
	//! Get group this object belong to.
	CBaseObject* GetGroup() const;
	//! Called after some changes such as transformation or properties etc.
	virtual void UpdateGroup();

	//////////////////////////////////////////////////////////////////////////
	// LookAt Target.
	//////////////////////////////////////////////////////////////////////////
	virtual void SetLookAt(CBaseObject* target);
	CBaseObject* GetLookAt() const { return m_lookat; };
	//! Returns true if this object is a look-at target.
	bool         IsLookAtTarget() const;
	CBaseObject* GetLookAtSource() const { return m_lookatSource; };

	//! Gets physics collision entity of this object.
	virtual struct IPhysicalEntity* GetCollisionEntity() const { return 0; };

	IObjectManager*                 GetObjectManager() const;

	//! Store undo information for this object.
	void StoreUndo(const char* undoDescription, bool minimal = false, int flags = 0);

	//! Add event listener callback.
	void AddEventListener(const EventCallback& cb);
	//! Remove event listener callback.
	void RemoveEventListener(const EventCallback& cb);

	//////////////////////////////////////////////////////////////////////////
	//! Material handling for this base object.
	//! Override in derived classes.
	//////////////////////////////////////////////////////////////////////////
	//! Assign new material to this object.
	virtual void             SetMaterial(IEditorMaterial* mtl);
	//! Assign new material to this object as a material name.
	virtual void             SetMaterial(const string& materialName);
	//! Get assigned material for this object.
	virtual IEditorMaterial* GetMaterial() const       { return m_pMaterial; };
	// Get actual rendering material for this object.
	virtual IEditorMaterial* GetRenderMaterial() const { return m_pMaterial; };
	// Get the material name. Even though the material pointer is null, the material name can exist separately.
	virtual string           GetMaterialName() const;

	//////////////////////////////////////////////////////////////////////////
	//! Analyze errors for this object.
	virtual void Validate();

	//////////////////////////////////////////////////////////////////////////
	//! Get main asset file
	virtual string GetAssetPath() const { return ""; }

	//////////////////////////////////////////////////////////////////////////
	//! Gather resources of this object.
	virtual void GatherUsedResources(CUsedResources& resources);

	//////////////////////////////////////////////////////////////////////////
	//! Check if specified object is very similar to this one.
	virtual bool IsSimilarObject(CBaseObject* pObject);

	//////////////////////////////////////////////////////////////////////////
	// Material Layers Mask.
	//////////////////////////////////////////////////////////////////////////
	virtual void SetMaterialLayersMask(uint32 nLayersMask) { m_nMaterialLayersMask = nLayersMask; }
	uint32       GetMaterialLayersMask() const             { return m_nMaterialLayersMask; };

	//////////////////////////////////////////////////////////////////////////
	// Object minimal usage spec (All/Low/Medium/High)
	//////////////////////////////////////////////////////////////////////////
	uint32       GetMinSpec() const { return m_nMinSpec; }
	virtual void SetMinSpec(uint32 nSpec, bool bSetChildren = true);

	//////////////////////////////////////////////////////////////////////////
	// SubObj selection.
	//////////////////////////////////////////////////////////////////////////
	// Return true if object support selecting of this sub object element type.
	virtual bool StartSubObjSelection(int elemType)                                                                 { return false; };
	virtual void EndSubObjectSelection()                                                                            {};
	virtual void CalculateSubObjectSelectionReferenceFrame(SubObjectSelectionReferenceFrameCalculator* pCalculator) {};

	// Request a geometry pointer from the object.
	// Return NULL if geometry can not be retrieved or object does not support geometries.
	virtual CEdGeometry* GetGeometry() { return 0; };

	//! In This function variables of the object must be initialized.
	virtual void InitVariables() {};

	virtual void OnPropertyChanged(IVariable*);
	virtual void OnMultiSelPropertyChanged(IVariable*);

	bool         IntersectRayMesh(const Vec3& raySrc, const Vec3& rayDir, SRayHitInfo& outHitInfo) const;

	bool         CanBeHightlighted() const;
	bool         IsSkipSelectionHelper() const;

	//////////////////////////////////////////////////////////////////////////
	// Statistics
	//////////////////////////////////////////////////////////////////////////
	virtual string    GetMouseOverStatisticsText() const { return ""; }

	virtual IStatObj* GetIStatObj()                      { return NULL; }
	//! Display length of each axis.
	void              DrawDimensionsImpl(DisplayContext& dc, const AABB& localBoundBox, AABB* pMergedBoundBox = NULL);

	// Invalidates cached transformation matrix.
	// nWhyFlags - Flags that indicate the reason for matrix invalidation.
	virtual void InvalidateTM(int nWhyFlags);

	virtual void RegisterOnEngine();
	virtual void UnRegisterFromEngine();

	// Called once per selected object type to create any necessary inspector widgets
	// Duplicates will be discarded by the widget creator
	virtual void CreateInspectorWidgets(CInspectorWidgetCreator& creator);

	// Update UI variables.
	void         UpdateUIVars();
	//! update state to render nodes so that objects can be drawn for highlighting
	virtual void UpdateHighlightPassState(bool bSelected, bool bHighlighted) {};

	//! Must be called after cloning the object on clone of object.
	//! This will make sure object references are cloned correctly.
	virtual void PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx);

	CVarObject*   GetVarObject() const { return m_pVarObject.get(); }

	// Recursive set descendant's layer
	void SetDescendantsLayer(IObjectLayer* pLayer);

protected:
	friend class CObjectManager;
	friend class CObjectLayer;

	//! Ctor is protected to restrict direct usage.
	CBaseObject();
	//! Dtor is protected to restrict direct usage.
	virtual ~CBaseObject();

	//! Initialize Object.
	//! If previous object specified it must be of exactly same class as this object.
	//! All data is copied from previous object.
	//! Optional file parameter specify initial object or script for this object.
	virtual bool Init(CBaseObject* prev, const string& file);
	virtual void PostInit(const string& file) {}

	//! Must be implemented by derived class to create game related objects.
	virtual bool CreateGameObject() { return true; };

	/** Called when object is about to be deleted.
	    All Game resources should be freed in this function.
	 */
	virtual void Done();

	/** Change current id of object.
	 */
	//virtual void SetId( uint32 objectId ) { m_id = objectId; };

	//! Call this to delete an object.
	virtual void DeleteThis() = 0;

	//! Called when object need to be converted from different object.
	virtual bool ConvertFromObject(CBaseObject* object);

	//! Called when local transformation matrix is calculated.
	void CalcLocalTM(Matrix34& tm) const;

	//! Called when child position changed.
	virtual void OnChildModified();

	//! Remove child from our children list.
	virtual void RemoveChild(CBaseObject* node);
	virtual void RemoveChildren(const std::vector<CBaseObject*>& children) {}

	//! Resolve parent from callback.
	void ResolveParent(CBaseObject* object);
	//! Resolve linkedTo from callback.
	void ResolveLinkedTo(CBaseObject* object);
	void SetColor(COLORREF color);

	//! Draw default object items.
	virtual void DrawDefault(DisplayContext& dc, COLORREF labelColor = RGB(255, 255, 255));
	//! Draw object label.
	void         DrawLabel(DisplayContext& dc, const Vec3& pos, COLORREF labelColor = RGB(255, 255, 255), float alpha = 1.0f, float size = 1.2f);
	//! Draw selection helper.
	void         DrawSelectionHelper(DisplayContext& dc, const Vec3& pos, COLORREF labelColor = RGB(255, 255, 255), float alpha = 1.0f);
	//! Draw helper icon.
	virtual void DrawTextureIcon(DisplayContext& dc, const Vec3& pos, float alpha = 1.0f, bool bDisplaySelectionHelper = false, float distanceSquared = 0);
	//! Draw warning icons
	virtual void DrawWarningIcons(DisplayContext& dc, const Vec3& pos);
	//! Display text with a 3d world coordinate.
	void         DrawTextOn2DBox(DisplayContext& dc, const Vec3& pos, const char* text, float textScale, const ColorF& TextColor, const ColorF& TextBackColor);
	//! Check if dimension's figures can be displayed before draw them.
	virtual void DrawDimensions(DisplayContext& dc, AABB* pMergedBoundBox = NULL);

	//! Draw highlight.
	virtual void DrawHighlight(DisplayContext& dc);

	//! Returns if the object can be drawn, and if its selection helper should also be drawn.
	bool CanBeDrawn(const DisplayContext& dc, bool& outDisplaySelectionHelper) const;

	//! Returns if object is in the camera view.
	virtual bool  IsInCameraView(const CCamera& camera);
	//! Returns vis ratio of object in camera
	virtual float GetCameraVisRatio(const CCamera& camera);

	// Do basic intersection tests
	virtual bool IntersectRectBounds(const AABB& bbox);
	virtual bool IntersectRayBounds(const Ray& ray);

	// Do hit testing on specified bounding box.
	// Function can be used by derived classes.
	bool HitTestRectBounds(HitContext& hc, const AABB& box);

	// Do helper hit testing as specific location.
	bool HitHelperAtTest(HitContext& hc, const Vec3& pos);

	// Do helper hit testing taking child objects into account (e.g. opened prefab)
	virtual bool HitHelperTestForChildObjects(HitContext& hc) { return false; }

	CBaseObject* FindObject(CryGUID id) const;

	// Returns true if game objects should be created.
	bool IsCreateGameObjects() const;

	//! Notify all listeners about event.
	void NotifyListeners(EObjectListenerEvent event);

	//! Only used by ObjectManager.
	bool IsPotentiallyVisible() const;

	//////////////////////////////////////////////////////////////////////////
	// Material Layers Mask.
	//////////////////////////////////////////////////////////////////////////
	// Call this function at constructor of derived object to enable material layer mask ui for object.
	void UseMaterialLayersMask(bool bUse = true) { m_bUseMaterialLayersMask = bUse; }

	//////////////////////////////////////////////////////////////////////////
	// May be overridden in derived classes to handle helpers scaling.
	//////////////////////////////////////////////////////////////////////////
	virtual void  SetHelperScale(float scale)         {};
	virtual float GetHelperScale()                    { return 1; };

	void          SetDrawTextureIconProperties(DisplayContext& dc, const Vec3& pos, float alpha = 1.0f);
	const Vec3& GetTextureIconDrawPos() { return m_vDrawIconPos; };
	int         GetTextureIconFlags()   { return m_nIconFlags; };

	Matrix33    GetWorldRotTM() const;
	Matrix33    GetWorldScaleTM() const;

	void        InvalidateWorldBox() { m_bWorldBoxValid = false; }

	//! This should be used in places like context menus to ensure the right clicked object will be selected.
	//! Use this carefully since it will begin undo!
	void BeginUndoAndEnsureSelection();

	//! This is meant to react on selection state changed, not to select the object. Call the object manager if you need this.
	virtual void SetSelected(bool bSelect);

protected:
	virtual void SerializeGeneralProperties(Serialization::IArchive& ar, bool bMultiEdit);
	virtual void SerializeTransformProperties(Serialization::IArchive& ar);

private:

	// Be careful when using the variables bellow directly. In most cases use accessors (GetPos, GetRotation, GetScale), to make sure the system handles delegated mode properly
	//! World space object's position.
	Vec3 m_pos;
	//! Object's Rotation angles.
	Quat m_rotate;
	//! Object's scale value.
	Vec3 m_scale;

	friend class CUndoBaseObject;
	friend class CPropertiesPanel;
	friend class CObjectArchive;
	friend class CSelectionGroup;

	void OnMenuProperties();

	

	//! Set class description for this object,
	//! Only called once after creation by ObjectManager.
	void SetClassDesc(CObjectClassDesc* classDesc);

	// From CObject, (not implemented)
	virtual void          Serialize(CArchive& ar) {};

	EScaleWarningLevel    GetScaleWarningLevel() const;
	ERotationWarningLevel GetRotationWarningLevel() const;

	// auto resolving
	void OnMtlResolved(uint32 id, bool success, const char* orgName, const char* newName);

	bool IsInSelectionBox() const { return m_bInSelectionBox; }

	// For subclasses to do actual attach/detach work
	virtual void OnAttachChild(CBaseObject* pChild) {}
	virtual void OnDetachThis() {}
	void         SetMaterialByName(const char* mtlName);

	virtual void OnLink(CBaseObject* pParent) {}
	virtual void OnUnLink()                   {}

	void         SetId(CryGUID guid)          { m_guid = guid; }

	// Before translating, rotating or scaling, we ask our subclasses for whether
	// they want us to notify CGameEngine of the upcoming change of our AABB.
	virtual bool ShouldNotifyOfUpcomingAABBChanges() const { return false; }

	// Notifies the CGameEngine about an upcoming change of our AABB.
	void OnBeforeAreaChange();

	//////////////////////////////////////////////////////////////////////////
	// PRIVATE FIELDS
	//////////////////////////////////////////////////////////////////////////
private:
	//! Unique object Id.
	CryGUID m_guid;
	//! This Id identifies an object within a prefab uniquely
	CryGUID m_guidInPrefab;

	//! Layer this object is assigned to.
	IObjectLayer* m_layer;

	//! Flags of this object.
	int m_flags;

	// Id of the texture icon for this object.
	int m_nTextureIcon;

	//! Display color.
	COLORREF m_color;

	//! World transformation matrix of this object.
	mutable Matrix34 m_worldTM;

	//////////////////////////////////////////////////////////////////////////
	//! Look At target entity.
	_smart_ptr<CBaseObject> m_lookat;
	//! If we are lookat target. this is pointer to source.
	//Note that this model doesn't work when several objects look at the same object
	CBaseObject*            m_lookatSource;

	//! Object's name.
	string            m_name;
	//! Class description for this object.
	CObjectClassDesc* m_classDesc;

	//! Number of reference to this object.
	//! When reference count reach zero, object will delete itself.
	int m_numRefs;
	//////////////////////////////////////////////////////////////////////////
	//! Child objects
	BaseObjectArray      m_children;
	//! Linked objects
	BaseObjectArray      m_linkedObjects;
	//! Pointer to parent node.
	mutable CBaseObject* m_parent;

	//! Used when linking to not modify the parent hierarchy, that way we can keep linking between different layers
	CBaseObject* m_pLinkedTo;

	//! Material of this object.
	IEditorMaterial* m_pMaterial;

	AABB             m_worldBounds;
	bool             m_bInSelectionBox;

	// The transform delegate
	ITransformDelegate* m_pTransformDelegate;

	//////////////////////////////////////////////////////////////////////////
	// Listeners.
	std::vector<EventCallback> m_eventListeners;

	//////////////////////////////////////////////////////////////////////////
	// Flags and bit masks.
	//////////////////////////////////////////////////////////////////////////
	mutable uint32 m_bMatrixInWorldSpace    : 1;
	mutable uint32 m_bMatrixValid           : 1;
	mutable uint32 m_bWorldBoxValid         : 1;
	uint32         m_bUseMaterialLayersMask : 1;
	bool           m_bSuppressUpdatePrefab  : 1;
	uint32         m_nMaterialLayersMask    : 8;
	uint32         m_nMinSpec               : 8;

	Vec3           m_vDrawIconPos;
	int            m_nIconFlags;

	friend class CGroup;
	friend class CGuidCollisionResolver;

protected:
	// child classes should override this if they don't want to display a bounding box during highlight
	bool           m_bSupportsBoxHighlight;

	std::unique_ptr<CVarObject> m_pVarObject;
};

//////////////////////////////////////////////////////////////////////////
// Array of editor objects.
//////////////////////////////////////////////////////////////////////////
class CBaseObjectsCache
{
public:
	int          GetObjectCount() const { return m_objects.size(); }
	CBaseObject* GetObject(int nIndex) const { return m_objects[nIndex]; }
	void         AddObject(CBaseObject* pObject) { m_objects.push_back(pObject); }
	void         ClearObjects() { m_objects.clear(); }
	void         Reserve(int nCount) { m_objects.reserve(nCount); }
private:
	//! List of objects that was displayed at last frame.
	std::vector<_smart_ptr<CBaseObject>> m_objects;
};
