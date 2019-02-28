// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>
#include <QPointer>

class QSearchBox;
class QAdvancedTreeView;
class QDeepFilterProxyModel;
class QToolButton;
class QItemSelection;
class QPopupWidget;

class CDictionaryWidget;
class CBroadcastManager;
class BroadcastEvent;

namespace CrySchematycEditor {

class CAbstractVariablesModel;
class CAbstractVariablesModelItem;

class CVariablesModel;

class CVariablesWidget : public QWidget
{
	Q_OBJECT;

public:
	CVariablesWidget(QString label, QWidget* pParent = nullptr);
	~CVariablesWidget();

	CAbstractVariablesModel* GetModel() const { return m_pModel; }
	void                     SetModel(CAbstractVariablesModel* pModel);

	void                     ConnectToBroadcast(CBroadcastManager* pBroadcastManager);

Q_SIGNALS:
	void SignalEntryClicked(CAbstractVariablesModelItem& variableItem);
	void SignalEntryDoubleClicked(CAbstractVariablesModelItem& variableItem);
	void SignalEntrySelected(CAbstractVariablesModelItem& variableItem);

private:
	void         OnClicked(const QModelIndex& index);
	void         OnDoubleClicked(const QModelIndex& index);
	void         OnSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

	void         OnContextMenu(const QPoint& point);
	void         OnAddPressed();

	bool         OnDeleteEvent();
	void         OnEditorEvent(BroadcastEvent& event);

	void         Disconnect();

	virtual void customEvent(QEvent* pEvent) override;

private:
	CAbstractVariablesModel*    m_pModel;

	QSearchBox*                 m_pFilter;
	QAdvancedTreeView*          m_pVariablesList;
	CVariablesModel*            m_pDataModel;
	QDeepFilterProxyModel*      m_pFilterProxy;
	QToolButton*                m_pAddButton;
	QPopupWidget*               m_pContextMenu;
	CDictionaryWidget*          m_pContextMenuContent;

	QPointer<CBroadcastManager> m_broadcastManager;
};

}

