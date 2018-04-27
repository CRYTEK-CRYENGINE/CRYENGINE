// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>
#include <QAbstractItemModel>

class QFilteringPanel;
class QAdvancedTreeView;
class QAttributeFilterProxyModel;
class QToolButton;
class QItemSelection;
class QPopupWidget;
class QMenu;

class CDictionaryWidget;
class CAbstractDictionaryEntry;

namespace CrySchematycEditor {

class CStateItem;
class CComponentItem;
class CAbstractObjectStructureModel;
class CAbstractObjectStructureModelItem;

class CObjectStructureModel;

class CGraphsWidget : public QWidget
{
	Q_OBJECT;

public:
	CGraphsWidget(QWidget* pParent = nullptr);
	~CGraphsWidget();

	CAbstractObjectStructureModel* GetModel() const { return m_pModel; }
	void                           SetModel(CAbstractObjectStructureModel* pModel);

Q_SIGNALS:
	void SignalEntryClicked(CAbstractObjectStructureModelItem& item);
	void SignalEntryDoubleClicked(CAbstractObjectStructureModelItem& item);
	void SignalEntrySelected(CAbstractObjectStructureModelItem& item);

private:
	void         OnClicked(const QModelIndex& index);
	void         OnDoubleClicked(const QModelIndex& index);
	void         OnSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

	void         OnContextMenu(const QPoint& point);
	void         OnAddPressed();
	bool         OnDeleteEvent();

	virtual void customEvent(QEvent* pEvent) override;

	void         EditItem(CAbstractObjectStructureModelItem& item) const;
	void         PopulateContextMenuForItem(QMenu& menu, CStateItem& stateItem) const;

private:
	CAbstractObjectStructureModel* m_pModel;

	QFilteringPanel*               m_pFilter;
	QAdvancedTreeView*             m_pComponentsList;
	CObjectStructureModel*         m_pDataModel;
	QAttributeFilterProxyModel*    m_pFilterProxy;
	QToolButton*                   m_pAddButton;
	QPopupWidget*                  m_pContextMenu;
	CDictionaryWidget*             m_pContextMenuContent;
};

}

