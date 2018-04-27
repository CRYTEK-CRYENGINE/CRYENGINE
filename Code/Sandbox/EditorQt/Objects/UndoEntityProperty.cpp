// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UndoEntityProperty.h"
#include "EntityObject.h"

CUndoEntityProperty::CUndoEntityProperty(const char* pEntityName, const char* pPropertyName, const char* pUndoDescription)
{
	m_entityName = pEntityName;
	m_propertyName = pPropertyName;
	m_undoDescription = pUndoDescription;
	m_undo = PyGetEntityProperty(pEntityName, pPropertyName);
}

const char* CUndoEntityProperty::GetDescription()
{
	return m_undoDescription;
}

void CUndoEntityProperty::Undo(bool bUndo)
{
	if (bUndo)
	{
		m_redo = PyGetEntityProperty(m_entityName, m_propertyName);
	}
	PySetEntityProperty(m_entityName, m_propertyName, m_undo);
}

void CUndoEntityProperty::Redo()
{
	PySetEntityProperty(m_entityName, m_propertyName, m_redo);
}

