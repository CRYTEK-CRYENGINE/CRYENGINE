// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EntityObject.h"
#include "SafeObjectsArray.h"

class CPickEntitiesPanel;

//////////////////////////////////////////////////////////////////////////
// Base class for area objects.
//////////////////////////////////////////////////////////////////////////
class SANDBOX_API CAreaObjectBase : public CEntityObject, public IPickEntitesOwner
{
public:

	virtual void           AddEntity(CBaseObject* const pBaseObject);
	virtual CEntityObject* GetEntity(size_t const index);
	virtual size_t         GetEntityCount() const { return m_entities.GetCount(); }
	virtual void           RemoveEntity(size_t const index);

	virtual void           OnEntityAdded(IEntity const* const pIEntity)   {}
	virtual void           OnEntityRemoved(IEntity const* const pIEntity) {}

	bool                   HasMeasurementAxis() const                     { return true; }

	void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;
	
	virtual void           Serialize(CObjectArchive& ar);

	virtual void           OnAreaChange(IVariable* pVariable) = 0;
	virtual void           OnSizeChange(IVariable* pVariable) = 0;

protected:

	CAreaObjectBase();
	~CAreaObjectBase();

	void         DrawEntityLinks(DisplayContext& dc);
	virtual void PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx);
	void         SerializeEntityTargets(Serialization::IArchive& ar, bool bMultiEdit);

	//! List of bound entities.
	CSafeObjectsArray m_entities;

private:

	void OnObjectEvent(CBaseObject* const pBaseObject, int const event);
};

/*!
 *	CAreaBox is a box volume in space where entities can be attached to.
 *
 */
class CAreaBox : public CAreaObjectBase
{
public:
	DECLARE_DYNCREATE(CAreaBox)

	//////////////////////////////////////////////////////////////////////////
	// Overrides from CBaseObject.
	//////////////////////////////////////////////////////////////////////////
	bool         Init(CBaseObject* prev, const string& file);
	void         Done();
	bool         CreateGameObject();
	virtual void InitVariables();
	void         Display(CObjectRenderHelper& objRenderHelper) override;
	void         InvalidateTM(int nWhyFlags);
	void         GetLocalBounds(AABB& box);
	bool         HitTest(HitContext& hc);
	bool         IsScalable() const { return false; };

	void CreateInspectorWidgets(CInspectorWidgetCreator& creator) override;

	virtual void PostLoad(CObjectArchive& ar);

	virtual void Serialize(CObjectArchive& ar);
	XmlNodeRef   Export(const string& levelPath, XmlNodeRef& xmlNode);

	void         SetAreaId(int nAreaId);
	int          GetAreaId();
	void         SetBox(AABB box);
	AABB         GetBox();

	virtual void OnEntityAdded(IEntity const* const pIEntity);
	virtual void OnEntityRemoved(IEntity const* const pIEntity);

private:

	struct SBoxFace
	{
		SBoxFace(Vec3 const* const _pP1,
		         Vec3 const* const _pP2,
		         Vec3 const* const _pP3,
		         Vec3 const* const _pP4)
			: pP1(_pP1),
			pP2(_pP2),
			pP3(_pP3),
			pP4(_pP4){}

		Vec3 const* const pP1;
		Vec3 const* const pP2;
		Vec3 const* const pP3;
		Vec3 const* const pP4;
	};

	void UpdateGameArea();
	void UpdateAttachedEntities();
	void UpdateSoundPanelParams();
	void SerializeProperties(Serialization::IArchive& ar, bool bMultiEdit);

protected:
	//! Dtor must be protected.
	CAreaBox();

	void         DeleteThis() { delete this; };

	void         Reload(bool bReloadScript = false) override;
	virtual void OnAreaChange(IVariable* pVariable) override;
	virtual void OnSizeChange(IVariable* pVariable) override;
	void         OnSoundParamsChange(IVariable* pVar);
	void         OnPointChange(IVariable* var);

	CVariable<float> m_innerFadeDistance;
	CVariable<int>   m_areaId;
	CVariable<float> m_edgeWidth;
	CVariable<float> mv_width;
	CVariable<float> mv_length;
	CVariable<float> mv_height;
	CVariable<int>   mv_groupId;
	CVariable<int>   mv_priority;
	CVariable<bool>  mv_filled;
	CVariable<bool>  mv_displaySoundInfo;

	//! Local volume space bounding box.
	AABB   m_box;

	uint32 m_bIgnoreGameUpdate : 1;

	typedef std::vector<bool> tSoundObstruction;

	// Sound specific members
	tSoundObstruction m_abObstructSound;
	CVarBlockPtr      m_pOwnSoundVarBlock;
};

/*!
 * Class Description of AreaBox.
 */
class CAreaBoxClassDesc : public CObjectClassDesc
{
public:
	ObjectType     GetObjectType()     { return OBJTYPE_VOLUME; };
	const char*    ClassName()         { return "AreaBox"; };
	const char*    UIName()            { return "Box"; };
	const char*    Category()          { return "Area"; };
	CRuntimeClass* GetRuntimeClass()   { return RUNTIME_CLASS(CAreaBox); };
};

