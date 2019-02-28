// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "Controls/EditorDialog.h"

class CWnd;

class StandaloneMaterialEditor : public CEditorDialog
{
public:
	StandaloneMaterialEditor();
	~StandaloneMaterialEditor();

	void Execute();

private:

	virtual void closeEvent(QCloseEvent * event) override;

	void OnIdleCallback();

	CWnd* m_materialWnd;
};

