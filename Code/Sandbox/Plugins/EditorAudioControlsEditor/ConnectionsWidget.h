// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/SharedData.h"
#include <QWidget>

class QPropertyTreeLegacy;
class QAttributeFilterProxyModel;

namespace ACE
{
class CControl;
class CConnectionsModel;
class CTreeView;

class CConnectionsWidget final : public QWidget
{
	Q_OBJECT

public:

	CConnectionsWidget() = delete;
	CConnectionsWidget(CConnectionsWidget const&) = delete;
	CConnectionsWidget(CConnectionsWidget&&) = delete;
	CConnectionsWidget& operator=(CConnectionsWidget const&) = delete;
	CConnectionsWidget& operator=(CConnectionsWidget&&) = delete;

	explicit CConnectionsWidget(QWidget* const pParent);
	virtual ~CConnectionsWidget() override;

	void SetControl(CControl* const pControl, bool const restoreSelection);
	void Reset();
	void OnBeforeReload();
	void OnAfterReload();
	void OnFileImporterOpened();
	void OnFileImporterClosed();
	void OnConnectionAdded(ControlId const id);

private slots:

	void OnContextMenu(QPoint const& pos);

private:

	// QObject
	bool eventFilter(QObject* pObject, QEvent* pEvent) override;
	// ~QObject

	void       RemoveSelectedConnection();
	void       RefreshConnectionProperties();
	void       UpdateSelectedConnections();
	void       ResizeColumns();
	void       ExecuteConnection();
	XmlNodeRef ConstructTemporaryTriggerConnections(CControl const* const pControl);

	CControl*                         m_pControl;
	QPropertyTreeLegacy* const              m_pConnectionProperties;
	QAttributeFilterProxyModel* const m_pAttributeFilterProxyModel;
	CConnectionsModel* const          m_pConnectionModel;
	CTreeView* const                  m_pTreeView;
};
} // namespace ACE
