// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QTViewPane.h>
#include <QLabel>

#include <IEditor.h>
#include <DockedWidget.h>
#include <Controls/EditorDialog.h>

#include "Settings.h"
#include "EditorContext.h"

class CUqsQueryDocument;
class CUqsDatabaseSerializationCache;
class CQueryListProvider;

class QPropertyTree;

class QLineEdit;
class QDialogButtonBox;

namespace Explorer
{
struct ExplorerEntry;
class ExplorerData;
class ExplorerFileList;
class ExplorerPanel;
} // namespace Explorer

class CNewQueryDialog : public CEditorDialog
{
	Q_OBJECT

	typedef CEditorDialog BaseClass;
public:
	CNewQueryDialog(const QString& newName, QWidget* pParent);

	// CEditorDialog
	virtual void accept() override;
	// ~CEditorDialog

	const QString& GetResultingString();

private:
	QLineEdit*        m_pLineEdit;
	QDialogButtonBox* m_pButtons;
	QString           m_resultingString;
};

class CMainEditorWindow 
	: public CDockableWindow
	, public IEditorNotifyListener
{
	Q_OBJECT

public:

	CMainEditorWindow();
	~CMainEditorWindow();

	// CDockableWindow
	virtual const char*                       GetPaneTitle() const override;
	virtual IViewPaneClass::EDockingDirection GetDockingDirection() const override { return IViewPaneClass::DOCK_FLOAT; }
	// ~CDockableWindow

	// IEditorNotifyListener
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent ev) override;
	// ~IEditorNotifyListener

private:

	void                     CreateNewDocument();

	void                     BuildLibraryPanel();
	void                     BuildMenu();

	void                     OnMenuActionFileNew();
	void                     OnMenuActionFileSave();
	void                     OnMenuActionFileSaveAs();

	void                     OnMenuActionViewUseSelectionHelpers(bool checked);
	void                     OnMenuActionViewShowInputParamTypes(bool checked);
	void                     OnMenuActionViewFilterInputsByType(bool checked);

	void                     OnMenuActionHelpOnlineDocumentation();

	void                     OnLibraryExplorerSelectionChanged();
	void                     OnLibraryExplorerActivated(const Explorer::ExplorerEntry* pExplorerEntry);

	void                     OnPropertyTreeChanged();

	void                     OnDocumentAboutToBeRemoved(CUqsQueryDocument* pDocument);

	Explorer::ExplorerPanel* GetLibraryExplorerWidget() const;

	QPropertyTree*           GetDocumentPropertyTreeWidget() const;

	void                     SetCurrentDocumentFromExplorerEntry(const Explorer::ExplorerEntry* pExplorerEntry);

private:

	CUqsEditorContext                       m_editorContext;

	std::unique_ptr<Explorer::ExplorerData> m_pExplorerData;

	DockedWidget<QPropertyTree>*            m_pPropertyTreeWidget;
	DockedWidget<Explorer::ExplorerPanel>*  m_pLibraryPanel;

	QTabWidget*                             m_pDocumentTabsWidget;
	QPropertyTree*                          m_pDocumentPropertyTree;

	CUqsQueryDocument*                      m_pCurrentDocument;
};
