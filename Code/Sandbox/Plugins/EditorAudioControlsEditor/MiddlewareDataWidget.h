// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>

#include <ControlInfo.h>
#include <FileImportInfo.h>

#include <FileDialogs/ExtensionFilter.h>

class QVBoxLayout;

namespace ACE
{
class CMiddlewareDataWidget final : public QWidget
{
	Q_OBJECT

public:

	explicit CMiddlewareDataWidget(QWidget* const pParent);
	virtual ~CMiddlewareDataWidget() override;

	CMiddlewareDataWidget() = delete;

signals:

	void SignalSelectConnectedSystemControl(ControlId const systemControlId, ControlId const implItemId);

private:

	void InitImplDataWidget();
	void ClearImplDataWidget();
	void GetConnectedControls(ControlId const implItemId, SControlInfos& controlInfos);
	void OnImportFiles(ExtensionFilterVector const& extensionFilters, QStringList const& supportedTypes, QString const& targetFolderName);
	void OpenFileImporter(FileImportInfos const& fileImportInfos, QString const& targetFolderName);

	QVBoxLayout* const m_pLayout;
	QWidget*           m_pImplDataPanel;
};
} // namespace ACE
