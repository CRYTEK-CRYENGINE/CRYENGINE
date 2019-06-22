// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include "GroupEditorData.h"
#include "AbstractNodeGraphViewModelItem.h"

#include <CrySandbox/CrySignal.h>
#include <Serialization/PropertyTreeLegacy/Serialization.h>

#include <QColor>
#include <QString>
#include <QPointF>
#include <QVariant>

namespace CryGraphEditor {

class CGroupWidget;
class CNodeGraphView;
class CNodeGraphViewModel;

class EDITOR_COMMON_API CAbstractGroupItem : public CAbstractNodeGraphViewModelItem
{
public:
	enum : int32 { Type = eItemType_Group };

public:	
	CAbstractGroupItem(CGroupEditorData& data, CNodeGraphViewModel& viewModel);
	virtual ~CAbstractGroupItem();

public:
	// CAbstractNodeGraphViewModelItem
	virtual QVariant           GetId() const override { return m_data.GetId(); }
	virtual int32              GetType() const override { return Type; }
	virtual QString            GetName() const override;
	virtual void               SetName(const QString& name) override;
	virtual QPointF            GetPosition() const override;
	virtual void               SetPosition(QPointF position) override;
	// ~CAbstractNodeGraphViewModelItem

public:
	virtual CGroupWidget*      GetWidget() const { return nullptr; }
	virtual CGroupWidget*      CreateWidget(CNodeGraphView& view) { return nullptr; }
	virtual const char*        GetStyleId() const { return "Group"; }
	
public:
	CGroupEditorData&          GetEditorData()        { return m_data; }
	const CGroupEditorData&    GetEditorData() const  { return m_data; }

	CGroupItems&               GetItems()       { return GetEditorData().GetItems(); }
	const CGroupItems&         GetItems() const { return GetEditorData().GetItems(); }

	void                       LinkItem(CAbstractNodeGraphViewModelItem& item);
	void                       UnlinkItem(CAbstractNodeGraphViewModelItem& item);

public:
	void                       OnDataChanged();

public:
	CCrySignal<void()>                    SignalAABBChanged;
	CCrySignal<void()>                    SignalPositionChanged;
	CCrySignal<void(CAbstractGroupItem&)> SignalItemsChanged;

private:
	void                       UpdateLinkedItemsPositions();

private:
	CGroupEditorData&          m_data;
	QColor                     m_color;
};

} //namespace CryGraphEditor
