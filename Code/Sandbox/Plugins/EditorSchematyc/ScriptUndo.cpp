// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "stdafx.h"
#include "ScriptUndo.h"

#include "MainWindow.h"

#include <AssetSystem/AssetEditor.h>

namespace CrySchematycEditor {

CScriptUndoObject::CScriptUndoObject(const char* szDesc, CMainWindow& editor)
	: m_desc(szDesc)
	, m_pEditor(&editor)
	, m_pAsset(editor.GetAsset())
{
	m_before = GetIEditor()->GetSystem()->CreateXmlNode();
	m_after = GetIEditor()->GetSystem()->CreateXmlNode();

	m_pEditor->SaveUndo(m_before);
	QObject::connect(m_pEditor, &QObject::destroyed, [this]()
		{
			m_pEditor = nullptr;
	  });
}

CScriptUndoObject::~CScriptUndoObject()
{

}

void CScriptUndoObject::Undo(bool bUndo)
{
	if (m_pEditor && m_pAsset && m_pEditor->GetAsset() == m_pAsset && m_pAsset->IsBeingEdited())
	{
		m_after->removeAllChilds();
		m_pEditor->SaveUndo(m_after);
		m_pEditor->RestoreUndo(m_before);
	}
}

void CScriptUndoObject::Redo()
{
	if (m_pEditor && m_pAsset && m_pEditor->GetAsset() == m_pAsset && m_pAsset->IsBeingEdited())
	{
		m_pEditor->RestoreUndo(m_after);
	}
}

}

