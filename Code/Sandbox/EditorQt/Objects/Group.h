// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __Group_h__
#define __Group_h__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "Objects/BaseObject.h"

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
 *
 */
class SANDBOX_API CGroup : public CBaseObject
{
public:
	DECLARE_DYNCREATE(CGroup)

	static void ForEachParentOf(const std::vector<CBaseObject*>& objects, std::function<void(CGroup*, std::vector<CBaseObject*>&)>);

	//////////////////////////////////////////////////////////////////////////
	// Overwrites from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool Init(CBaseObject* prev, const string& file);
	void Done();

	void Display(CObjectRenderHelper& objRenderHelper);

	void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

	//! Attach new child node.
	virtual void        AddMember(CBaseObject* pMember, bool bKeepPos = true) override;
	virtual void        AddMembers(std::vector<CBaseObject*>& objects, bool shouldKeepPos = true) override;
	virtual void        RemoveMember(CBaseObject* pMember, bool shouldKeepPos = true, bool shouldPlaceOnRoot = false) override;
	virtual void        RemoveMembers(std::vector<CBaseObject*>& members, bool shouldKeepPos = true, bool shouldPlaceOnRoot = false) override;

	const TBaseObjects& GetMembers() const { return m_members; }
	virtual void        DetachThis(bool bKeepPos, bool bPlaceOnRoot = false) override;
	virtual void        DetachChildren(std::vector<CBaseObject*>& objects, bool shouldKeepPos = true, bool shouldPlaceOnRoot = false) override;

	//! Detach all childs of this node.
	virtual void        DetachAll(bool bKeepPos = true, bool bPlaceOnRoot = false) override;
	virtual void        AttachChildren(std::vector<CBaseObject*>& objects, bool shouldKeepPos = true, bool shouldInvalidateTM = true) override;

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
	//! Select objects within specified distance from given position.
	//! Return number of selected objects.
	int SelectObjects(const AABB& box, bool bUnselect = false);

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
	bool IsOpen() const { return m_opened; };

	//! Called by child object, when it changes.
	void OnChildModified();

	virtual void DeleteAllMembers();
	virtual void GetAllLinkedObjects(std::vector<CBaseObject*>& objects, CBaseObject* pObject);

	void BindToParent();

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
	void DeleteThis() { delete this; };

	void SetChildsParent(CBaseObject* pObj) { pObj->m_parent = this; }

	void FilterOutNonMembers(std::vector<CBaseObject*>& objects);

	// This list contains children which are actually members of this group, rather than regular attached ones.
	TBaseObjects m_members;

	AABB         m_bbox;
	bool         m_bBBoxValid : 1;
	bool         m_opened : 1;
	bool         m_bAlwaysDrawBox : 1;
	bool         m_ignoreChildModify : 1;
	bool         m_bUpdatingPivot : 1;

	friend Private_Group::CBatchAttachChildrenTransformationsHandler;
	friend Private_Group::CUndoBatchAttachBaseObject;
	friend CBaseObject;
	friend CObjectManager;
};

/*!
 * Class Description of Group.
 */
class CGroupClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()   { return OBJTYPE_GROUP; };
	const char*    ClassName()       { return "Group"; };
	const char*    Category()        { return ""; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CGroup); };
};

#endif // __Group_h__

