// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/ControlInfo.h"
#include <QWidget>

class QVBoxLayout;

namespace ACE
{
class CMiddlewareDataWidget final : public QWidget
{
	Q_OBJECT

public:

	CMiddlewareDataWidget() = delete;
	CMiddlewareDataWidget(CMiddlewareDataWidget const&) = delete;
	CMiddlewareDataWidget(CMiddlewareDataWidget&&) = delete;
	CMiddlewareDataWidget& operator=(CMiddlewareDataWidget const&) = delete;
	CMiddlewareDataWidget& operator=(CMiddlewareDataWidget&&) = delete;

	explicit CMiddlewareDataWidget(QWidget* const pParent);
	virtual ~CMiddlewareDataWidget() override;

private:

	void InitImplDataWidget();
	void ClearImplDataWidget();
	void GetConnectedControls(ControlId const implItemId, SControlInfos& controlInfos);

	QVBoxLayout* const m_pLayout;
	QWidget*           m_pImplDataPanel;
};
} // namespace ACE
