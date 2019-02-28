// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _COLLISION_CLASS_FILTERING_H__
#define _COLLISION_CLASS_FILTERING_H__

class CCollisionFilteringProperties
{
public:
	enum {k_maxCollisionClasses = 32};

public:
	CCollisionFilteringProperties();
	~CCollisionFilteringProperties();

	// Add the variables to the object properties
	void AddToObject(CVarObject* object, IVariable::OnSetCallback func);

	// Helper to add the filtering to the physical entity
	void                   ApplyToPhysicalEntity(IPhysicalEntity* pPhysics);

	const SCollisionClass& GetCollisionClass() { return m_collisionClass; }
	int                    GetCollisionClassExportId();

protected:

	void OnVarChange(IVariable* var);

	CVariableArray           mv_collision_classes;
	CVariableArray           mv_collision_classes_type;
	CVariableArray           mv_collision_classes_ignore;
	CVariable<bool>          mv_collision_classes_type_flag[k_maxCollisionClasses];
	CVariable<bool>          mv_collision_classes_ignore_flag[k_maxCollisionClasses];
	IVariable::OnSetCallback m_callbackFunc;
	SCollisionClass          m_collisionClass;

	static bool              m_bNamesInitialised;
	static string           m_collision_class_names[k_maxCollisionClasses];
};

#endif

