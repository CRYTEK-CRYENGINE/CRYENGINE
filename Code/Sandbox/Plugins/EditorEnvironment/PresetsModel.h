// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QtCore/QAbstractItemModel>

#include <CryCore/CryFlags.h>
#include <CryCore/smartptr.h>

struct CPresetsModelNode : public _i_reference_target_t
{
	typedef std::vector<CPresetsModelNode*> ChildNodes;

	enum EType
	{
		EType_Group,
		EType_Leaf
	};

	enum EFlags
	{
		EFlags_Modified = BIT(0),
		EFlags_InPak    = BIT(1),
		EFlags_InFolder = BIT(2),
		//EFlags_Default     = BIT(3),
	};
	typedef CCryFlags<uint8> NodeFlags;

	CPresetsModelNode(EType _type, const char* _szName, const char* _szPath, CPresetsModelNode* parent)
		: type(_type)
		, name(_szName)
		, path(_szPath)
		, pParent(parent)
	{

	}

	~CPresetsModelNode()
	{
		for (size_t i = 0, childCount = children.size(); i < childCount; ++i)
			delete children[i];
	}

	EType              type;
	NodeFlags          flags;
	string             name;
	string             path;

	CPresetsModelNode* pParent;
	ChildNodes         children;
};
typedef _smart_ptr<CPresetsModelNode> CPresetsModelNodePtr;

class QPresetsWidget;

class CPresetsModel : public QAbstractItemModel
{
	Q_OBJECT

	enum EColumn
	{
		eColumn_Name,
		eColumn_PakStatus,

		eColumn_COUNT
	};

public:
	CPresetsModel(QPresetsWidget& presetsWidget, QObject* pParent);

	QModelIndex               index(int row, int column, const QModelIndex& parent) const override;
	Qt::ItemFlags             flags(const QModelIndex& index) const;

	int                       rowCount(const QModelIndex& parent) const override;
	int                       columnCount(const QModelIndex& parent) const override;

	QVariant                  headerData(int section, Qt::Orientation orientation, int role) const override;
	bool                      hasChildren(const QModelIndex& parent) const override;
	QVariant                  data(const QModelIndex& index, int role) const override;
	bool                      setData(const QModelIndex& index, const QVariant& value, int role /* = Qt::EditRole */) override;
	QModelIndex               parent(const QModelIndex& index) const override;
	const char*               GetHeaderSectionName(int section) const;

	QModelIndex               ModelIndexFromNode(CPresetsModelNode* pNode) const;
	static CPresetsModelNode* GetEntryNode(const QModelIndex& index);

public slots:
	void OnSignalBeginAddEntryNode(CPresetsModelNode* pEntryNode);
	void OnSignalEndAddEntryNode(CPresetsModelNode* pEntryNode);
	void OnSignalBeginDeleteEntryNode(CPresetsModelNode* pEntryNode);
	void OnSignalEndDeleteEntryNode();
	void OnSignalBeginResetModel();
	void OnSignalEndResetModel();
	void OnSignalEntryNodeDataChanged(CPresetsModelNode* pEntryNode);
protected:
	QPresetsWidget& m_presetsWidget;
};

