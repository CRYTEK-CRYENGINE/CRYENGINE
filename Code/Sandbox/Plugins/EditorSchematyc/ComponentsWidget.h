// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>

class QFilteringPanel;
class QAdvancedTreeView;
class QAttributeFilterProxyModel;
class QToolButton;
class QItemSelection;
class QPopupWidget;

class CDictionaryWidget;
class CAbstractDictionaryEntry;

namespace CrySchematycEditor {

class CComponentItem;
class CAbstractComponentsModel;

class CMainWindow;

class CComponentsWidget : public QWidget
{
	Q_OBJECT;

public:
	CComponentsWidget(CMainWindow& editor, QWidget* pParent = nullptr);
	~CComponentsWidget();

	CAbstractComponentsModel* GetModel() const { return m_pModel; }
	void                      SetModel(CAbstractComponentsModel* pModel);

Q_SIGNALS:
	void SignalComponentClicked(CComponentItem& component);
	void SignalComponentDoubleClicked(CComponentItem& component);

private:
	void         OnClicked(const QModelIndex& index);
	void         OnDoubleClicked(const QModelIndex& index);
	void         OnSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

	void         OnContextMenu(const QPoint& point);
	void         OnAddButton(bool checked);

	void         OnAddComponent(CAbstractDictionaryEntry& entry);

	bool         OnDeleteEvent();
	bool         OnCopyEvent();
	bool         OnPasteEvent();

	virtual void customEvent(QEvent* pEvent) override;

private:
	CMainWindow*                m_pEditor;
	CAbstractComponentsModel*   m_pModel;

	QFilteringPanel*            m_pFilter;
	QAdvancedTreeView*          m_pComponentsList;
	QAttributeFilterProxyModel* m_pFilterProxy;
	QToolButton*                m_pAddButton;
	QPopupWidget*               m_pContextMenu;
	CDictionaryWidget*          m_pContextMenuContent;
};

}

