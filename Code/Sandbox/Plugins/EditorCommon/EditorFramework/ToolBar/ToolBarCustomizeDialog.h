// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include "ToolBarService.h"

#include <Controls/EditorDialog.h>
#include <CrySandbox/CrySignal.h>

class CEditor;
class CCommandModel;

class QAdvancedTreeView;
class QDeepFilterProxyModel;
class QEditableComboBox;
class QHBoxLayout;
class QLineEdit;
class QListView;
class QSearchBox;
class QToolBar;
class QToolButton;

class EDITOR_COMMON_API CToolBarCustomizeDialog : public CEditorDialog
{
	Q_OBJECT
private:
	class QDropContainer;
	class QCustomToolBar;

	// Only needed because CEditorMainFrame is able to host toolbars. We'll need to keep it this way until we manage to unlink Level Editing from the QtMainFrame.
	friend class CEditorMainFrame;
	CToolBarCustomizeDialog(QWidget* pParent, const char* szEditorName);

public:
	CToolBarCustomizeDialog(CEditor* pEditor);

	QString GetCurrentToolBarText();
	void    IconSelected(const char* szIconPath);
	void    CVarsSelected(const QStringList& selectedCVars);
	void    SetCVarName(const char* cvarName);
	void    SetCVarValue(const char* cvarValue);
	void    SetCommandName(const char* commandName);
	void    SetIconPath(const char* szIconPath);
	void    OnSelectedItemChanged(std::shared_ptr<CToolBarService::QItemDesc> selectedItem);

	CCrySignal<void(QToolBar*)>              signalToolBarAdded;
	CCrySignal<void(QToolBar*)>              signalToolBarModified;
	CCrySignal<void(const char*, QToolBar*)> signalToolBarRenamed;
	CCrySignal<void(const char*)>            signalToolBarRemoved;

protected:
	virtual void  dragEnterEvent(QDragEnterEvent* pEvent) override;
	virtual void  dropEvent(QDropEvent* pEvent) override;
	virtual QSize sizeHint() const override { return QSize(640, 480); }

private:
	void CurrentItemChanged();
	void OnAddToolBar();
	void OnToolBarModified(std::shared_ptr<CToolBarService::QToolBarDesc> pToolBarDesc);
	void OnRemoveToolBar();
	void OnContextMenu(const QPoint& position) const;

	void RenameToolBar(const QString& before, const QString& after);
	void ShowCommandWidgets();
	void ShowCVarWidgets();
	void SetCVarValueRegExp();

private:
	std::shared_ptr<CToolBarService::QItemDesc> m_pSelectedItem;
	QDropContainer*                             m_pDropContainer;
	QEditableComboBox*                          m_pToolbarSelect;
	QAdvancedTreeView*                          m_pTreeView;
	CCommandModel*                              m_pItemModel;
	QDeepFilterProxyModel*                      m_pProxyModel;
	QListView*        m_pPreview;
	QLineEdit*        m_pNameInput;
	QLineEdit*        m_pCommandInput;
	QLineEdit*        m_pIconInput;
	QLineEdit*        m_pCVarInput;
	QLineEdit*        m_pCVarValueInput;
	QToolButton*      m_pIconBrowserButton;
	QToolButton*      m_pCVarBrowserButton;
	QSearchBox*       m_pSearchBox;

	const CEditor*    m_pEditor;
	string            m_editorName;

	QVector<QWidget*> m_commandWidgets;
	QVector<QWidget*> m_cvarWidgets;
};

//////////////////////////////////////////////////////////////////////////

class CToolBarCustomizeDialog::QDropContainer : public QWidget
{
	Q_OBJECT
public:
	QDropContainer(CToolBarCustomizeDialog* pParent);
	~QDropContainer();

	void                                           AddItem(const QVariant& itemVariant, int idx = -1);
	void                                           AddCommand(const CCommand* pCommand, int idx = -1);
	void                                           AddSeparator(int sourceIdx = -1, int targetIdx = -1);
	void                                           AddCVar(const QString& cvarName, int idx = -1);

	void                                           RemoveCommand(const CCommand* pCommand);
	void                                           RemoveItem(std::shared_ptr<CToolBarService::QItemDesc> pItem);
	void                                           RemoveItemAt(int idx);
	void                                           SetCurrentToolBarDesc(std::shared_ptr<CToolBarService::QToolBarDesc>& toolBarDesc);
	void                                           BuildPreview();

	QCustomToolBar*                                CreateToolBar(const QString& title, const std::shared_ptr<CToolBarService::QToolBarDesc>& pToolBarDesc);
	std::shared_ptr<CToolBarService::QToolBarDesc> GetCurrentToolBarDesc() { return m_pCurrentToolBarDesc; }
	void                                           SetSelectedActionIcon(const char* szPath);

	virtual bool                                   eventFilter(QObject* pObject, QEvent* pEvent) override;
	virtual void                                   mouseMoveEvent(QMouseEvent* pEvent) override;

	virtual void                                   dragEnterEvent(QDragEnterEvent* pEvent) override;
	virtual void                                   dragMoveEvent(QDragMoveEvent* pEvent) override;
	virtual void                                   dropEvent(QDropEvent* pEvent) override;
	virtual void                                   dragLeaveEvent(QDragLeaveEvent* pEvent) override;

	void                                           ShowContextMenu(const QPoint& position);

	static const char*                             GetToolBarItemMimeType() { return s_toolBarItemMimeType; }

	CCrySignal<void(std::shared_ptr<CToolBarService::QItemDesc> )>    selectedItemChanged;
	CCrySignal<void(std::shared_ptr<CToolBarService::QToolBarDesc> )> signalToolBarModified;

protected:
	int GetIndexFromMouseCoord(const QPoint& globalPos);
	void paintEvent(QPaintEvent *event) override;

private:
	void UpdateToolBar();

private:
	std::shared_ptr<CToolBarService::QItemDesc>    m_pSelectedItem;
	QPoint m_DragStartPosition;
	CToolBarCustomizeDialog*                       m_pToolBarCreator;
	QHBoxLayout*                                   m_pPreviewLayout;
	QCustomToolBar*                                m_pCurrentToolBar;
	std::shared_ptr<CToolBarService::QToolBarDesc> m_pCurrentToolBarDesc;
	static const char*                             s_toolBarItemMimeType;
	bool m_bDragStarted;
};
