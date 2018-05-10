// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CUNDOENTITYPROPERTY__H__
#define __CUNDOENTITYPROPERTY__H__

#include "Util/BoostPythonHelpers.h"
#include "EntityObject.h"

class CUndoEntityProperty : public IUndoObject
{
public:
	CUndoEntityProperty(const char* pEntityName, const char* pPropertyName, const char* pUndoDescription = "Set Entity Property");

protected:
	const char* GetDescription();
	void        SetEntityProperty(SPyWrappedProperty& value);
	void        Undo(bool bUndo);
	void        Redo();

private:
	string            m_entityName;
	string            m_propertyName;
	SPyWrappedProperty m_undo;
	SPyWrappedProperty m_redo;
	const char*        m_undoDescription;
};

#endif // __CUNDOENTITYPROPERTY__H__

