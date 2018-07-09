// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QAbstractItemModel>
#include <QAdvancedTreeView.h>
#include <QWidget>

// Forward declare classes.
class QBoxLayout;
class QItemSelection;
class QLineEdit;
class QMenu;
class QPushButton;
class QSplitter;

namespace Schematyc
{
	// Forward declare classes.
	class CEnvBrowserFilter;

	struct EEnvBrowserColumn
	{
		enum : int
		{
			Name = 0,
			Count
		};
	};

	class CEnvBrowserItem;

	DECLARE_SHARED_POINTERS(CEnvBrowserItem)

	typedef std::vector<CEnvBrowserItemPtr> EnvBrowserItems;

	class CEnvBrowserItem
	{
	public:

		CEnvBrowserItem(const CryGUID& guid, const char* szName, const char* szIcon);

		CryGUID GetGUID() const;
		const char* GetName() const;
		const char* GetIcon() const;

		CEnvBrowserItem* GetParent();
		void AddChild(const CEnvBrowserItemPtr& pChild);
		void RemoveChild(CEnvBrowserItem* pChild);
		int GetChildCount() const;
		int GetChildIdx(CEnvBrowserItem* pChild);
		CEnvBrowserItem* GetChildByIdx(int childIdx);

	private:

		CryGUID            m_guid;
		string           m_name;
		string           m_iconName;
		CEnvBrowserItem* m_pParent;
		EnvBrowserItems  m_children;
	};

	class CEnvBrowserModel : public QAbstractItemModel
	{
		Q_OBJECT

	private:

		typedef std::unordered_map<CryGUID, CEnvBrowserItem*> ItemsByGUID;

	public:

		CEnvBrowserModel(QObject* pParent);

		// QAbstractItemModel
		QModelIndex index(int row, int column, const QModelIndex& parent) const override;
		QModelIndex parent(const QModelIndex& index) const override;
		int rowCount(const QModelIndex& index) const override;
		int columnCount(const QModelIndex& parent) const override;
		bool hasChildren(const QModelIndex &parent) const override;
		QVariant data(const QModelIndex& index, int role) const override;
		bool setData(const QModelIndex &index, const QVariant &value, int role) override;
		QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
		Qt::ItemFlags flags(const QModelIndex& index ) const override;
		// ~QAbstractItemModel

		QModelIndex ItemToIndex(CEnvBrowserItem* pItem, int column = 0) const;
		CEnvBrowserItem* ItemFromIndex(const QModelIndex& index) const;
		CEnvBrowserItem* ItemFromGUID(const CryGUID& guid) const;

	public slots:

	private:

		void Populate();

	private:

		EnvBrowserItems m_items;
		ItemsByGUID     m_itemsByGUID;
	};

	class CEnvBrowserWidget : public QWidget
	{
		Q_OBJECT

	public:

		CEnvBrowserWidget(QWidget* pParent);

		~CEnvBrowserWidget();

		void InitLayout();

	public slots:

		void OnSearchFilterChanged(const QString& text);
		void OnTreeViewCustomContextMenuRequested(const QPoint& position);

	private:

		void ExpandAll();

	private:

		QBoxLayout*        m_pMainLayout;
		QBoxLayout*        m_pFilterLayout;
		QLineEdit*         m_pSearchFilter;
		QAdvancedTreeView* m_pTreeView;
		CEnvBrowserModel*  m_pModel;
		CEnvBrowserFilter* m_pFilter;
	};
}

