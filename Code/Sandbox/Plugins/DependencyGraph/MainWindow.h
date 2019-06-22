// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "EditorFramework/Editor.h"

class CModel;
class CGraphViewModel;
class CAsset;

class CMainWindow : public CDockableEditor
{
	Q_OBJECT;
public:
	CMainWindow(QWidget* pParent = nullptr);

	// CEditor
	virtual const char* GetEditorName() const override;
	// ~CEditor

	static void  CreateNewWindow(CAsset* asset);
protected:
	virtual void CreateDefaultLayout(CDockableContainer* pSender) override;
private:
	void         InitMenu();
	void         RegisterActions();
	void         RegisterDockingWidgets();
	bool         OnOpen();
	bool         OnOpenFile(const QString& path) final override;
	bool         OnClose();
	bool         Open(CAsset* pAsset);
private:
	std::unique_ptr<CModel>          m_pModel;
	std::unique_ptr<CGraphViewModel> m_pGraphViewModel;
};
