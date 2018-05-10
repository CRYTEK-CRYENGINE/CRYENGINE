// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QAbstractItemModel>
#include <QAdvancedTreeView.h>
#include <CrySerialization/Forward.h>

class CContentCGF;

class CTargetMeshModel : public QAbstractItemModel
{
	struct SItem;
public:
	// Can be attached to a property tree.
	struct SItemProperties
	{
		SItem* pItem;

		void   Serialize(Serialization::IArchive& ar);
	};
private:
	struct SItem
	{
		SItem*              m_pParent;
		std::vector<SItem*> m_children;
		int                 m_siblingIndex;
		string              m_name;
		SItemProperties     m_properties;

		SItem()
			: m_pParent(nullptr)
			, m_children()
			, m_siblingIndex(0)
		{
			m_properties.pItem = this;
		}

		void SetName(const string& name)
		{
			m_name = name;
		}

		void AddChild(SItem* pItem)
		{
			CRY_ASSERT(pItem && !pItem->m_pParent);
			pItem->m_pParent = this;
			pItem->m_siblingIndex = m_children.size();
			m_children.push_back(pItem);
		}
	};

	enum EColumnType
	{
		eColumnType_Name,
		eColumnType_COUNT
	};
public:
	explicit CTargetMeshModel(QObject* pParent = nullptr);
	void                   LoadCgf(const char* filename);
	void                   Clear();
	SItemProperties*       GetProperties(const QModelIndex& index);
	const SItemProperties* GetProperties(const QModelIndex& index) const;

	// QAbstractItemModel implementation.

	virtual QModelIndex index(int row, int column, const QModelIndex& parent) const override;
	virtual QModelIndex parent(const QModelIndex& index) const override;
	virtual int         rowCount(const QModelIndex& index) const override;
	virtual int         columnCount(const QModelIndex& index) const override;
	virtual QVariant    data(const QModelIndex& index, int role) const override;
	virtual QVariant    headerData(int column, Qt::Orientation orientation, int role) const override;

private:
	void RebuildInternal(CContentCGF& cgf);

private:
	std::vector<SItem>  m_items;
	std::vector<SItem*> m_rootItems;
};

class CTargetMeshView : public QAdvancedTreeView
{
public:
	explicit CTargetMeshView(QWidget* pParent = nullptr);

	CTargetMeshModel* model();

	virtual void      reset() override;
private:
	std::unique_ptr<CTargetMeshModel> m_pModel;
};

