// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AbstractNodeGraphViewModelItem.h"

#include <CrySandbox/CrySignal.h>

namespace CryGraphEditor {

class CNodeGraphViewModel;
class CNodeGraphView;
class CCommentWidget;

class EDITOR_COMMON_API CAbstractCommentItem : public CAbstractNodeGraphViewModelItem
{
public:
	enum : int32 { Type = eItemType_Comment };

public:
	CAbstractCommentItem(CNodeGraphViewModel& viewModel);
	virtual ~CAbstractCommentItem();

	// CAbstractNodeGraphViewModelItem
	virtual int32   GetType() const override     { return Type; }

	virtual QPointF GetPosition() const override { return m_position; }
	virtual void    SetPosition(QPointF position) override;
	// ~CAbstractNodeGraphViewModelItem

	virtual CCommentWidget* CreateWidget(CNodeGraphView& view) = 0;
	virtual QVariant        GetIdentifier() const = 0;

public:
	CCrySignal<void()> SignalPositionChanged;

private:
	QPointF m_position;
};

}

