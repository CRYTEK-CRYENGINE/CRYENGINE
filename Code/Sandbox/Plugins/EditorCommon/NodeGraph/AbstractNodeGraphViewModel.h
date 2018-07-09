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
#include <CrySandbox/CrySignal.h>

namespace CryGraphEditor {

class CAbstractNodeItem;
class CAbstractConnectionItem;
class CAbstractPinItem;
class CAbstractCommentItem;
class CAbstractGroupItem;
class CItemCollection;

// TODO: Rename the class to: CAbstractNodeGraphViewModel!
class EDITOR_COMMON_API CNodeGraphViewModel : public QObject
{
	Q_OBJECT

public:
	enum { InvalidIndex = -1 };

	CNodeGraphViewModel() {}
	virtual ~CNodeGraphViewModel();

	virtual INodeGraphRuntimeContext& GetRuntimeContext() = 0;
	virtual QString                   GetGraphName()                                                             { return QString(); }

	virtual uint32                    GetNodeItemCount() const                                                   { return 0; }
	virtual CAbstractNodeItem*        GetNodeItemByIndex(uint32 index) const                                     { return nullptr; }
	virtual CAbstractNodeItem*        GetNodeItemById(QVariant id) const                                         { return nullptr; }
	virtual CAbstractNodeItem*        CreateNode(QVariant typeId, const QPointF& position = QPointF())           { return false; }
	virtual bool                      RemoveNode(CAbstractNodeItem& node)                                        { return false; }

	virtual uint32                    GetConnectionItemCount() const                                             { return 0; }
	virtual CAbstractConnectionItem*  GetConnectionItemByIndex(uint32 index) const                               { return nullptr; }
	virtual CAbstractConnectionItem*  GetConnectionItemById(QVariant id) const                                   { return nullptr; }
	virtual CAbstractConnectionItem*  CreateConnection(CAbstractPinItem& sourcePin, CAbstractPinItem& targetPin) { return false; }
	virtual bool                      RemoveConnection(CAbstractConnectionItem& connection)                      { return false; }

	virtual uint32                    GetCommentItemCount() const                                                { return 0; }
	virtual CAbstractCommentItem*     GetCommentItemByIndex(uint32 index) const                                  { return nullptr; }
	virtual CAbstractCommentItem*     GetCommentItemById(QVariant id) const                                      { return nullptr; }
	virtual CAbstractCommentItem*     CreateComment()                                                            { return false; }
	virtual bool                      RemoveComment(CAbstractCommentItem& comment)                               { return false; }

	virtual uint32                    GetGroupItemCount() const                                                  { return 0; }
	virtual CAbstractGroupItem*       GetGroupItemByIndex(uint32 index) const                                    { return nullptr; }
	virtual CAbstractGroupItem*       GetGroupItemById(QVariant id) const                                        { return nullptr; }
	virtual CAbstractGroupItem*       CreateGroup()                                                              { return false; }
	virtual bool                      RemoveGroup(CAbstractGroupItem& group)                                     { return false; }

	virtual CItemCollection*          CreateClipboardItemsCollection()                                           { return nullptr; }

	CCrySignal<void()> SignalDestruction;

Q_SIGNALS:
	void SignalInvalidated();

	void SignalCreateNode(CAbstractNodeItem& node);
	void SignalCreateConnection(CAbstractConnectionItem& connection);
	void SignalCreateComment(CAbstractCommentItem& comment);
	void SignalCreateGroup(CAbstractGroupItem& group);

	void SignalRemoveNode(CAbstractNodeItem& node);
	void SignalRemoveConnection(CAbstractConnectionItem& connection);
	void SignalRemoveComment(CAbstractCommentItem& comment);
	void SignalRemoveGroup(CAbstractGroupItem& group);

	void SignalRemoveCustomItem(CAbstractNodeGraphViewModelItem&);
};

}

