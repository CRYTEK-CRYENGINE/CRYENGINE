// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IUndoObject.h>

class CAsset;

namespace CrySchematycEditor {

class CMainWindow;

class CScriptUndoObject : public IUndoObject
{
public:
	CScriptUndoObject(const char* szDesc, CMainWindow& editor);
	~CScriptUndoObject();

	// IUndoObject
	virtual const char* GetDescription() final { return m_desc.c_str(); }

	virtual void        Undo(bool bUndo) override;
	virtual void        Redo() override;
	// ~UndoObject

private:
	CMainWindow* m_pEditor;
	CAsset*      m_pAsset;
	string       m_desc;
	XmlNodeRef   m_before;
	XmlNodeRef   m_after;
};

}

