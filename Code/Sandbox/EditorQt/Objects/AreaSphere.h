// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EntityObject.h"
#include "AreaBox.h"

/*!
 *	CAreaSphere is a sphere volume in space where entities can be attached to.
 *
 */
class CAreaSphere : public CAreaObjectBase
{
public:
	DECLARE_DYNCREATE(CAreaSphere)

	//////////////////////////////////////////////////////////////////////////
	// Overrides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool         CreateGameObject();
	virtual void InitVariables();
	void         Display(CObjectRenderHelper& objRenderHelper) override;
	bool         IsScalable() const override  { return false; }
	bool         IsRotatable() const override { return false; }
	void         GetLocalBounds(AABB& box);
	bool         HitTest(HitContext& hc);

	virtual void PostLoad(CObjectArchive& ar);

	void         Serialize(CObjectArchive& ar);
	XmlNodeRef   Export(const string& levelPath, XmlNodeRef& xmlNode);

	void         SetAreaId(int nAreaId);
	int          GetAreaId();
	void         SetRadius(float fRadius);
	float        GetRadius();

	virtual void OnEntityAdded(IEntity const* const pIEntity);
	virtual void OnEntityRemoved(IEntity const* const pIEntity);

	virtual void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

private:

	void UpdateGameArea();
	void UpdateAttachedEntities();

protected:
	//! Dtor must be protected.
	CAreaSphere();

	void DeleteThis() { delete this; }

	void Reload(bool bReloadScript = false) override;
	void OnAreaChange(IVariable* pVar) override;
	void OnSizeChange(IVariable* pVar);

	CVariable<float> m_innerFadeDistance;
	CVariable<int>   m_areaId;
	CVariable<float> m_edgeWidth;
	CVariable<float> m_radius;
	CVariable<int>   mv_groupId;
	CVariable<int>   mv_priority;
	CVariable<bool>  mv_filled;
};

/*!
 * Class Description of AreaBox.
 */
class CAreaSphereClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_VOLUME; }
	const char*    ClassName()         { return "AreaSphere"; }
	const char*    UIName()            { return "Sphere"; }
	const char*    Category()          { return "Area"; }
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CAreaSphere); }
};

