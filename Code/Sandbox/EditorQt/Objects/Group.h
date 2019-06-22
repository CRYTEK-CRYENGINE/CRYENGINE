// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "Objects/BaseObject.h"
#include "SandboxAPI.h"

class CGroupPanel;
class CObjectManager;

namespace Private_Group
{
class CBatchAttachChildrenTransformationsHandler;
class CUndoBatchAttachBaseObject;
}

/*!
 *	CGroup groups object together.
 *  Object can only be assigned to one group.
 */
class SANDBOX_API CGroup : public CBaseObject
{
public:
	DECLARE_DYNCREATE(CGroup)

	static void ForEachParentOf(const std::vector<CBaseObject*>& objects, std::function<void(CGroup*, std::vector<CBaseObject*>&)> );
	static bool CanCreateFrom(std::vector<CBaseObject*>& objects);
	static CGroup* CreateFrom(std::vector<CBaseObject*>& objects, Vec3 center);

	// Creates a new group with provided objects
	virtual bool CreateFrom(std::vector<CBaseObject*>& objects);

	//////////////////////////////////////////////////////////////////////////
	// Overwrites from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init(CBaseObject* prev, const string& file);
	void Done();

	void Display(CObjectRenderHelper& objRenderHelper);

	//! Draws selection preview highlight
	const ColorB& GetSelectionPreviewHighlightColor() override;
	void          DrawSelectionPreviewHighlight(SDisplayContext& dc) override;

	void          CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

	//! Add/remove members
	virtual bool        CanAddMembers(std::vector<CBaseObject*>& objects);
	virtual bool        CanAddMember(CBaseObject* pMember);
	virtual void        AddMember(CBaseObject* pMember, bool bKeepPos = true) override;
	virtual void        AddMembers(std::vector<CBaseObject*>& objects, bool shouldKeepPos = true) override;
	virtual void        RemoveMember(CBaseObject* pMember, bool shouldKeepPos = true, bool shouldPlaceOnRoot = false) override;
	virtual void        RemoveMembers(std::vector<CBaseObject*>& members, bool shouldKeepPos = true, bool shouldPlaceOnRoot = false) override;

	virtual void        DetachChildren(std::vector<CBaseObject*>& objects, bool shouldKeepPos = true, bool shouldPlaceOnRoot = false) override;

	//! Detach all childs of this node.
	virtual void DetachAll(bool bKeepPos = true, bool bPlaceOnRoot = false) override;
	virtual void AttachChildren(std::vector<CBaseObject*>& objects, bool shouldKeepPos = true, bool shouldInvalidateTM = true) override;

	virtual void        SetMaterial(IEditorMaterial* mtl);

	void                GetBoundBox(AABB& box);
	void                GetLocalBounds(AABB& box);
	bool                HitTest(HitContext& hc);
	virtual bool        HitHelperTestForChildObjects(HitContext& hc);

	void                Serialize(CObjectArchive& ar);

	//! Overwrite event handler from CBaseObject.
	void OnEvent(ObjectEvent event);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Group interface
	//////////////////////////////////////////////////////////////////////////
	//! Remove all childs from this group.
	void Ungroup();

	//! Open group.
	//! Make childs accessible from top user interface.
	void Open();

	//! Close group.
	//! Make childs inaccessible from top user interface.
	//! In Closed state group only display objects but not allow to select them.
	void Close();

	//! Return true if group is in Open state.
	bool IsOpen() const { return m_opened; }

	//! Called by child object, when it changes.
	void         OnChildModified();

	virtual void DeleteAllMembers();
	virtual void GetAllLinkedObjects(std::vector<CBaseObject*>& objects, CBaseObject* pObject);

	virtual bool SuspendUpdate(bool bForceSuspend = true) { return true; }
	virtual void ResumeUpdate()                           {}

	virtual void PostLoad(CObjectArchive& ar)
	{
		CalcBoundBox();
	}

	virtual void OnContextMenu(CPopupMenuItem* menu);

	void         InvalidateBBox();

	void         UpdateGroup() override;

	virtual void UpdatePivot(const Vec3& newWorldPivotPos);
	void         UpdateHighlightPassState(bool bSelected, bool bHighlighted) override;

	virtual void PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx) override;

protected:
	//! Dtor must be protected.
	CGroup();

	virtual bool HitTestMembers(HitContext& hc);
	void         SerializeMembers(CObjectArchive& ar);
	virtual void CalcBoundBox();

	//! Get combined bounding box of all childs in hierarchy.
	static void RecursivelyGetBoundBox(CBaseObject* object, AABB& box, const Matrix34& parentTM);

	// Overwritten from CBaseObject.
	virtual void RemoveChildren(const std::vector<CBaseObject*>& children) override;
	virtual void RemoveChild(CBaseObject* node) override;
	void         DeleteThis()                       { delete this; }

	void         SetChildsParent(CBaseObject* pObj) { pObj->m_parent = this; }

	//Groups and derived shouldn't have visual properties visible. Specifically in prefabs where there is no way to reliably store global prefab properties (they get saved in the layer and not in the xml)
	virtual void SerializeGeneralVisualProperties(Serialization::IArchive& ar, bool bMultiEdit) override;

	AABB         m_bbox;
	bool         m_bBBoxValid        : 1;
	bool         m_opened            : 1;
	bool         m_bAlwaysDrawBox    : 1;
	bool         m_ignoreChildModify : 1;
	bool         m_bUpdatingPivot    : 1;

	friend Private_Group ::CBatchAttachChildrenTransformationsHandler;
	friend Private_Group ::CUndoBatchAttachBaseObject;
	friend CBaseObject;
	friend CObjectManager;
};

/*!
 * Class Description of Group.
 */
class CGroupClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()   { return OBJTYPE_GROUP; }
	const char*    ClassName()       { return "Group"; }
	const char*    Category()        { return ""; }
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CGroup); }
};
