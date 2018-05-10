// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UndoEntityParam.h"
#include "EntityObject.h"

CUndoEntityParam::CUndoEntityParam(const char* pObjectName, const char* pVarPath, const char* pUndoDescription)
{
	m_entityName = pObjectName;
	m_paramName = pVarPath;
	m_undo = PyGetEntityParam(pObjectName, pVarPath);
	m_undoDescription = pUndoDescription;

}

const char* CUndoEntityParam::GetDescription()
{
	return m_undoDescription;
}

void CUndoEntityParam::Undo(bool bUndo)
{
	if (bUndo)
	{
		m_redo = PyGetEntityParam(m_entityName, m_paramName);
	}
	PySetEntityParam(m_entityName, m_paramName, m_undo);
}

void CUndoEntityParam::Redo()
{
	PySetEntityParam(m_entityName, m_paramName, m_redo);
}

