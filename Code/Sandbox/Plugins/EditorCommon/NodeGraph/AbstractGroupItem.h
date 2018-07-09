// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AbstractNodeGraphViewModelItem.h"

#include <CrySandbox/CrySignal.h>

#include <QVariant>
#include <QString>
#include <QColor>
#include <QPointF>

namespace CryGraphEditor {

class CNodeGraphViewModel;

typedef std::vector<CAbstractNodeGraphViewModelItem*> ItemArray;

class EDITOR_COMMON_API CAbstractGroupItem : public CAbstractNodeGraphViewModelItem
{
public:
	enum : int32 { Type = eItemType_Group };

public:
	CAbstractGroupItem(CNodeGraphViewModel& viewModel);
	virtual ~CAbstractGroupItem();

	// CAbstractNodeGraphViewModelItem
	virtual int32   GetType() const override     { return Type; }

	virtual QPointF GetPosition() const override { return m_position; }
	virtual void    SetPosition(QPointF position) override;
	// ~CAbstractNodeGraphViewModelItem

	//virtual CGroupWidget*      CreateWidget(CNodeGraphView& view) = 0;
	//virtual QVariant            GetInstanceId() const = 0;

	//virtual const ItemArray& GetItems() const = 0;

	//
	const QString& GetName() const  { return m_name; }
	const QColor&  GetColor() const { return m_color; }

public:
	CCrySignal<void()> SignalPositionChanged;

private:
	QPointF m_position;
	QString m_name;
	QColor  m_color;
};

}

