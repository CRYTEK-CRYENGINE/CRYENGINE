// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>
#include <SystemTypes.h>

class QToolButton;
class QVBoxLayout;

namespace ACE
{
class CSystemAssetsManager;
class CSystemControl;
class CMiddlewareDataFilterProxyModel;
class CMiddlewareDataModel;
class CAudioTreeView;
class CElidedLabel;
class CImplItem;

class CMiddlewareDataWidget final : public QWidget
{
	Q_OBJECT

public:

	CMiddlewareDataWidget(CSystemAssetsManager* pAssetsManager);
	virtual ~CMiddlewareDataWidget() override;

	void Reset();
	void BackupTreeViewStates();
	void RestoreTreeViewStates();

signals:

	void SelectConnectedSystemControl(CSystemControl const* const pControl);

private slots:

	void OnContextMenu(QPoint const& pos);

private:

	void InitFilterWidgets(QVBoxLayout* const pMainLayout);

	CSystemAssetsManager* const            m_pAssetsManager;
	CMiddlewareDataFilterProxyModel* const m_pFilterProxyModel;
	CMiddlewareDataModel* const            m_pAssetsModel;
	CElidedLabel* const                    m_pImplNameLabel;
	QToolButton* const                     m_pHideAssignedButton;
	CAudioTreeView* const                  m_pTreeView;
	QString                                m_filter;
};
} // namespace ACE
