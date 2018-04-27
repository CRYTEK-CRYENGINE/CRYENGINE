// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QWidget>
#include <QTreeView>

#include "EditorCommonAPI.h"
#include "IObjectEnumerator.h"
#include <CrySandbox/CrySignal.h>
#include "QAdvancedTreeView.h"

class QStandardItemModel;
class QStandardItem;
class QHBoxLayout;
class QSplitter;
class QDeepFilterProxyModel;

class EDITOR_COMMON_API QObjectTreeView : public QAdvancedTreeView
{
public:
	enum Roles
	{
		Id = Qt::UserRole + 1,
		NodeType,
		Sort
	};

	QObjectTreeView(QWidget* pParent = nullptr);
	void SetIndexFilter(std::function<void(QModelIndexList&)> indexFilter) { m_indexFilterFn = indexFilter; }

protected:
	virtual void startDrag(Qt::DropActions supportedActions) override;

public:
	std::function<void(QModelIndexList&)> m_indexFilterFn;
};

class EDITOR_COMMON_API QObjectTreeWidget : public QWidget, public IObjectEnumerator
{
	Q_OBJECT
public:
	enum NodeType
	{
		Group,
		Entry
	};

public:
	QObjectTreeWidget(QWidget* pParent = nullptr, const char* szRegExp = "[/\\\\.]");

	QStringList    GetSelectedIds() const;
	const QRegExp& GetRegExp() const                { return m_regExp; }
	void           SetRegExp(const QRegExp& regExp) { m_regExp = regExp; }

	virtual bool   PathExists(const char* path);

	virtual void   AddEntry(const char* path, const char* id, const char* sortStr = "") override;
	virtual void   RemoveEntry(const char* path, const char* id) override;
	virtual void   ChangeEntry(const char* path, const char* id, const char* sortStr = "") override;

	void           Clear();

	void           ExpandAll();
	void           CollapseAll();

protected:
	QStandardItem* Find(const QString& itemName, QStandardItem* pParent = nullptr);
	QStandardItem* FindOrCreate(const QString& itemName, QStandardItem* pParent = nullptr);
	QStandardItem* TakeItemForId(const QString& id, const QModelIndex& parentIdx = QModelIndex());
	virtual void   dragEnterEvent(QDragEnterEvent* pEvent) override;
	virtual void   paintEvent(QPaintEvent* pEvent) override;

public:
	CCrySignal<void(const char*)>              signalOnClickFile;
	CCrySignal<void(const char*)>              signalOnDoubleClickFile;
	CCrySignal<void(QDragEnterEvent*, QDrag*)> signalOnDragStarted;
	CCrySignal<void()>                         singalOnDragEnded;

protected:
	QObjectTreeView*       m_pTreeView;
	QStandardItemModel*    m_pModel;
	QHBoxLayout*           m_pToolLayout;
	QSplitter*             m_pSplitter;
	QDeepFilterProxyModel* m_pProxy;
	QRegExp                m_regExp;
	bool                   m_bIsDragTracked;
};

Q_DECLARE_METATYPE(QObjectTreeWidget::NodeType)

