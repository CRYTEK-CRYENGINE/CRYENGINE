// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>
#include <SharedData.h>

class QPropertyTree;
class QLabel;
class QString;

namespace ACE
{
class CConnectionsWidget;
class CAsset;

class CPropertiesWidget final : public QWidget
{
	Q_OBJECT

public:

	explicit CPropertiesWidget(QWidget* const pParent);
	virtual ~CPropertiesWidget() override;

	CPropertiesWidget() = delete;

	void Reset();
	void OnAboutToReload();
	void OnReloaded();

signals:

	void SignalSelectConnectedImplItem(ControlId const itemId);

public slots:

	void OnSetSelectedAssets(Assets const& selectedAssets, bool const restoreSelection);

private:

	void RevertPropertyTree();

	CConnectionsWidget* const m_pConnectionsWidget;
	QPropertyTree* const      m_pPropertyTree;
	QLabel*                   m_pConnectionsLabel;
	std::unique_ptr<QString>  m_pUsageHint;
	bool                      m_suppressUpdates;
};
} // namespace ACE

