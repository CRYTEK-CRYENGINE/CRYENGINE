// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AbstractNodeContentWidget.h"
#include "PinWidget.h"

#include <QGraphicsWidget>
#include <QVector>

class QGraphicsGridLayout;
class QGraphicsWidget;

namespace CryGraphEditor {

class CNodeWidget;
class CPinWidget;

class EDITOR_COMMON_API CPinGridNodeContentWidget : public CAbstractNodeContentWidget
{
	Q_OBJECT

public:
	CPinGridNodeContentWidget(CNodeWidget& node, CNodeGraphView& view);

	virtual void OnInputEvent(CNodeWidget* pSender, SMouseInputEventArgs& args) override;

protected:
	virtual ~CPinGridNodeContentWidget();

	// CAbstractNodeContentWidget
	virtual void DeleteLater() override;
	virtual void OnItemInvalidated() override;
	virtual void OnLayoutChanged() override;
	// ~CAbstractNodeContentWidget

	void AddPin(CPinWidget& pinWidget);
	void RemovePin(CPinWidget& pinWidget);

	void OnPinAdded(CAbstractPinItem& item);
	void OnPinRemoved(CAbstractPinItem& item);

private:
	QGraphicsGridLayout* m_pLayout;
	uint8                m_numLeftPins;
	uint8                m_numRightPins;
	CPinWidget*          m_pLastEnteredPin;
};

}

