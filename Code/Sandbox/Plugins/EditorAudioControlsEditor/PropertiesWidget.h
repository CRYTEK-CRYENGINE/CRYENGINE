// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"
#include <QWidget>

class QPropertyTreeLegacy;
class QLabel;

namespace ACE
{
class CConnectionsWidget;

class CPropertiesWidget final : public QWidget
{
	Q_OBJECT

public:

	CPropertiesWidget() = delete;
	CPropertiesWidget(CPropertiesWidget const&) = delete;
	CPropertiesWidget(CPropertiesWidget&&) = delete;
	CPropertiesWidget& operator=(CPropertiesWidget const&) = delete;
	CPropertiesWidget& operator=(CPropertiesWidget&&) = delete;

	explicit CPropertiesWidget(QWidget* const pParent);
	virtual ~CPropertiesWidget() override;

	void Reset();
	void OnBeforeReload();
	void OnAfterReload();
	void OnFileImporterOpened();
	void OnFileImporterClosed();
	void OnSetSelectedAssets(Assets const& selectedAssets, bool const restoreSelection);
	void OnConnectionAdded(ControlId const id);

private:

	void RevertPropertyTree();

	CConnectionsWidget* const m_pConnectionsWidget;
	QPropertyTreeLegacy* const      m_pPropertyTree;
	QLabel*                   m_pConnectionsLabel;
	std::unique_ptr<QString>  m_pUsageHint;
	bool                      m_suppressUpdates;
};
} // namespace ACE
