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

class CNodeGraphView;

class CNodeWidget;
class CCommentWidget;
class CConnectionWidget;

class CAbstractPinItem;
typedef std::vector<CAbstractPinItem*> PinItemArray;

class EDITOR_COMMON_API CAbstractNodeItem : public CAbstractNodeGraphViewModelItem
{
public:
	enum : int32
	{
		Type = eItemType_Node
	};

public:
	CAbstractNodeItem(CNodeGraphViewModel& viewModel);
	virtual ~CAbstractNodeItem();

	// CAbstractNodeGraphViewModelItem
	virtual int32   GetType() const override     { return Type; }

	virtual QPointF GetPosition() const override { return m_position; }
	virtual void    SetPosition(QPointF position) override;

	virtual bool    IsDeactivated() const override { return m_isDeactivated; }
	virtual void    SetDeactivated(bool isDeactivated) override;

	virtual bool    HasWarnings() const override { return m_hasWarnings; }
	virtual void    SetWarnings(bool hasWarnings) override;

	virtual bool    HasErrors() const override { return m_hasErrors; }
	virtual void    SetErrors(bool hasErrors) override;
	// ~CAbstractNodeGraphViewModelItem

	virtual CNodeWidget*        CreateWidget(CNodeGraphView& view) = 0;
	virtual const char*         GetStyleId() const { return "Node"; }

	virtual QVariant            GetId() const = 0;
	virtual bool                HasId(QVariant id) const = 0;     // TODO: Rename -> IsEqualId
	virtual QVariant            GetTypeId() const = 0;

	virtual const PinItemArray& GetPinItems() const = 0;
	virtual QString             GetName() const { return m_name; }
	virtual void                SetName(const QString& name);

	virtual CAbstractPinItem*   GetPinItemById(QVariant id) const;
	virtual CAbstractPinItem*   GetPinItemByIndex(uint32 index) const;

	uint32                      GetPinItemIndex(const CAbstractPinItem& pin) const;
	bool                        GetAcceptsRenaming() const               { return m_acceptsRenaming; }
	void                        SetAcceptsRenaming(bool acceptsRenaming) { m_acceptsRenaming = acceptsRenaming; }

public:
	CCrySignal<void()>                      SignalPositionChanged;
	CCrySignal<void(bool isDeactivated)>    SignalDeactivatedChanged;
	CCrySignal<void()>                      SignalNameChanged;

	CCrySignal<void(CAbstractPinItem& pin)> SignalPinAdded;
	CCrySignal<void(CAbstractPinItem& pin)> SignalPinRemoved;
	CCrySignal<void(CAbstractPinItem& pin)> SignalPinInvalidated;

protected:
	// TODO: Remove!
	QString m_name;
	QPointF m_position;
	// ~TODO;

private:
	bool m_isDeactivated   : 1;
	bool m_hasWarnings     : 1;
	bool m_hasErrors       : 1;
	bool m_acceptsRenaming : 1;
};

}

