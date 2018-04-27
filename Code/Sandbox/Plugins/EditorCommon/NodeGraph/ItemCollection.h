// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AbstractNodeGraphViewModelItem.h"

#include "EditorCommonAPI.h"

#include <QObject>
#include <QVariant>
#include <QString>
#include <QRectF>
#include <QPointF>

#include <CrySerialization/Forward.h>
#include <CrySerialization/IArchive.h>

namespace CryGraphEditor {

class CAbstractNodeItem;
class CAbstractConnectionItem;
class CAbstractPinItem;
class CAbstractCommentItem;
class CAbstractGroupItem;

class CNodeGraphViewGraphicsWidget;

class EDITOR_COMMON_API CItemCollection
{
public:
	typedef std::vector<CAbstractNodeItem*>               Nodes;
	typedef std::vector<CAbstractConnectionItem*>         Connections;
	typedef std::vector<CAbstractNodeGraphViewModelItem*> CustomItems;

public:
	CItemCollection();
	virtual ~CItemCollection() {}

	virtual void                  Serialize(Serialization::IArchive& archive);

	void                          AddNode(CAbstractNodeItem& node);
	void                          AddConnection(CAbstractConnectionItem& connection);
	void                          AddCustomItem(CAbstractNodeGraphViewModelItem& customItem);

	const Nodes&                  GetNodes() const                                         { return m_nodes; }
	const Connections&            GetConnections() const                                   { return m_connections; }
	const CustomItems&            GetCustomItems() const                                   { return m_customItems; }

	const QRectF&                 GetItemsRect() const                                     { return m_itemsRect; }
	void                          SetItemsRect(const QRectF& rect)                         { m_itemsRect = rect; }

	const QPointF&                GetScenePosition() const                                 { return m_scenePosition; }
	void                          SetScenePosition(const QPointF& position)                { m_scenePosition = position; }

	CNodeGraphViewGraphicsWidget* GetViewWidget() const                                    { return m_pViewWidget; }
	void                          SetViewWidget(CNodeGraphViewGraphicsWidget* pViewWidget) { m_pViewWidget = pViewWidget; }

	CNodeGraphViewModel*          GetModel() const                                         { return m_pModel;  }
	void                          SetModel(CNodeGraphViewModel* pModel)                    { m_pModel = pModel; }

protected:
	QRectF  m_itemsRect;
	QPointF m_scenePosition;

private:
	Nodes                         m_nodes;
	Connections                   m_connections;
	CustomItems                   m_customItems;

	CNodeGraphViewGraphicsWidget* m_pViewWidget;
	CNodeGraphViewModel*          m_pModel;

	yasli::Context                contextContainer;
};

}

