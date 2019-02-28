// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AbstractNodeGraphViewModelItem.h"

#include <CrySandbox/CrySignal.h>

class QString;
class QPixmap;

namespace CryGraphEditor {

class CNodeGraphViewModel;

class CAbstractNodeItem;
class CAbstractConnectionItem;

class CNodeWidget;
class CPinWidget;
class CNodeGraphView;

typedef std::set<CAbstractConnectionItem*> ConnectionItemSet;

class EDITOR_COMMON_API CAbstractPinItem : public CAbstractNodeGraphViewModelItem
{
	friend class CAbstractNodeItem;

public:
	enum { Type = eItemType_Pin };

public:
	CAbstractPinItem(CNodeGraphViewModel& viewModel);
	virtual ~CAbstractPinItem();

	// CAbstractNodeGraphViewModelItem
	virtual int32 GetType() const override       { return Type; }

	virtual bool  IsDeactivated() const override { return m_isDeactivated; }
	virtual void  SetDeactivated(bool isDeactivated) override;
	// ~CAbstractNodeGraphViewModelItem

	virtual CPinWidget*              CreateWidget(CNodeWidget& nodeWidget, CNodeGraphView& view) { return nullptr; }
	virtual const char*              GetStyleId() const                                          { return "Pin"; }

	virtual CAbstractNodeItem&       GetNodeItem() const = 0;

	virtual QString                  GetName() const = 0;
	virtual QString                  GetDescription() const = 0;
	virtual QString                  GetTypeName() const = 0;

	virtual QVariant                 GetId() const = 0;
	virtual bool                     HasId(QVariant id) const = 0;

	virtual bool                     IsInputPin() const = 0;
	virtual bool                     IsOutputPin() const = 0;

	virtual bool                     CanConnect(const CAbstractPinItem* pOtherPin) const = 0;
	virtual bool                     IsConnected() const        { return (m_connections.size() > 0); }

	virtual const ConnectionItemSet& GetConnectionItems() const { return m_connections; };

	virtual void                     AddConnection(CAbstractConnectionItem& connection);
	virtual void                     RemoveConnection(CAbstractConnectionItem& connection);
	virtual void                     Disconnect();

	uint32                           GetIndex() const;

public:
	CCrySignal<void()> SignalDeactivatedChanged;

protected:
	CryGraphEditor::ConnectionItemSet m_connections;

private:
	uint32 m_index;
	bool   m_isDeactivated : 1;
};

}

