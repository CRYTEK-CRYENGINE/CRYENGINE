// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AbstractNodeGraphViewModelItem.h"

#include <CrySandbox/CrySignal.h>

class QColor;

namespace CryGraphEditor {

class CNodeGraphViewModel;
class CNodeGraphView;

class CAbstractPinItem;
class CConnectionWidget;

class EDITOR_COMMON_API CAbstractConnectionItem : public CAbstractNodeGraphViewModelItem
{
public:
	enum : int32 { Type = eItemType_Connection };

public:
	CAbstractConnectionItem(CNodeGraphViewModel& viewModel);
	virtual ~CAbstractConnectionItem();

	// CAbstractNodeGraphViewModelItem
	virtual int32 GetType() const override       { return Type; }

	virtual bool  IsDeactivated() const override { return m_isDeactivated; }
	virtual void  SetDeactivated(bool isDeactivated) override;
	// ~CAbstractNodeGraphViewModelItem

	virtual QVariant           GetId() const = 0;
	virtual bool               HasId(QVariant id) const = 0;

	virtual CConnectionWidget* CreateWidget(CNodeGraphView& view) = 0;
	virtual const char*        GetStyleId() const { return "Connection"; }

	virtual CAbstractPinItem& GetSourcePinItem() const = 0;
	virtual CAbstractPinItem& GetTargetPinItem() const = 0;

public:
	CCrySignal<void()> SignalDeactivatedChanged;

private:
	bool m_isDeactivated : 1;
};

}

