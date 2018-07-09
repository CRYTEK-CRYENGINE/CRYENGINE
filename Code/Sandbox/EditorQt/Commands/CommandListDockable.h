// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "QtViewPane.h"
#include "Commands/CommandModel.h"
#include <memory>

class QAdvancedTreeView;

class CCommandListModel : public CommandModel
{
	friend CommandModelFactory;

public:
	enum Roles
	{
		SortRole = static_cast<int>(CommandModel::Roles::Max),
	};

	virtual ~CCommandListModel();

	virtual void          Initialize() override;

	virtual int           columnCount(const QModelIndex& parent) const override { return s_ColumnCount; }
	virtual QVariant      data(const QModelIndex& index, int role) const override;
	virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
	virtual bool          hasChildren(const QModelIndex& parent) const override;
	virtual QVariant      headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual QModelIndex   index(int row, int column, const QModelIndex& parent) const override;
	virtual QModelIndex   parent(const QModelIndex& index) const override;
	virtual int           rowCount(const QModelIndex& parent) const override;

protected:
	CCommandListModel();

	virtual void Rebuild() override;

	static const int                     s_ColumnCount = 2;
	static const char*                   s_ColumnNames[s_ColumnCount];

	std::vector<const CCommandArgument*> m_params;
};

class CCommandListDockable : public CDockableWidget
{
public:
	CCommandListDockable(QWidget* const pParent = nullptr);
	~CCommandListDockable();

	//////////////////////////////////////////////////////////
	// CDockableWidget implementation
	virtual IViewPaneClass::EDockingDirection GetDockingDirection() const override { return IViewPaneClass::DOCK_FLOAT; }
	virtual const char*                       GetPaneTitle() const override        { return "Console Commands"; };
	virtual QRect                             GetPaneRect() override               { return QRect(0, 0, 800, 500); }
	//////////////////////////////////////////////////////////

private:
	std::unique_ptr<CommandModel> m_pModel;
	QAdvancedTreeView*            m_pTreeView;
};

