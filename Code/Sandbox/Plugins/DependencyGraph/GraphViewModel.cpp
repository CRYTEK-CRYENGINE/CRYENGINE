// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Model.h"
#include "GraphViewModel.h"
#include "GraphView.h"
#include "NodeGraph/AbstractPinItem.h"
#include "NodeGraph/AbstractConnectionItem.h"
#include "NodeGraph/NodeWidget.h"
#include "NodeGraph/AbstractNodeContentWidget.h"
#include "NodeGraph/PinGridNodeContentWidget.h"
#include "NodeGraph/ConnectionWidget.h"
#include "NodeGraph/NodeHeaderWidgetStyle.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/DependencyTracker.h"
#include "Notifications/NotificationCenter.h"
#include <IEditor.h>

class CGraphViewModel;

namespace Private_GraphViewModel
{

struct SAssetData
{
	SAssetData(CAsset* pAsset, const string& assetPath, size_t depth) : p(pAsset), path(assetPath), depth(depth)
	{
	}

	CAsset* p = nullptr;
	string  path;
	size_t  depth = 0;
	bool    bSelected = false;
	size_t  node = 0;
};

}

class CNodeEntry : public CAbstractDictionaryEntry
{
public:
	CNodeEntry(const CAssetNodeBase& node)
		: CAbstractDictionaryEntry()
		, m_node(node)
	{}
	virtual ~CNodeEntry() {}

	virtual uint32 GetType() const override
	{
		return Type_Entry;
	}
	virtual QVariant GetColumnValue(int32 columnIndex) const override
	{
		return QVariant::fromValue(m_node.GetName());
	}
	virtual QVariant GetIdentifier() const override
	{
		return QVariant::fromValue(QtUtil::ToQString(m_node.GetPath()));
	}
	virtual QString GetToolTip() const
	{
		return m_node.GetToolTipText();
	}
private:
	const CAssetNodeBase& m_node;
};

class CNodesDictionary : public CAbstractDictionary
{
public:
	CNodesDictionary(CryGraphEditor::CNodeGraphViewModel& model)
		: CAbstractDictionary()
		, m_model(model)
	{
	}
	virtual ~CNodesDictionary() {}

	virtual int32 GetNumColumns() const
	{
		return 1;
	}
	virtual int32 GetNumEntries() const override
	{
		return m_model.GetNodeItemCount();
	}
	virtual const CAbstractDictionaryEntry* GetEntry(int32 index) const override
	{
		return m_model.GetNodeItemByIndex(index)->Cast<CAssetNodeBase>()->GetNodeEntry();
	}

private:
	CryGraphEditor::CNodeGraphViewModel& m_model;
};

class CGraphRuntimeContext : public CryGraphEditor::INodeGraphRuntimeContext
{
public:
	CGraphRuntimeContext(CGraphViewModel& model)
		: m_pStyle(new CryGraphEditor::CNodeGraphViewStyle("Dependencies"))
		, m_pNodesDictionary(new CNodesDictionary(model))
	{
		const CryGraphEditor::CNodeWidgetStyle* pStyle = m_pStyle.get()->GetNodeWidgetStyle("Node");

		for (const CAssetType* pType : model.GetModel()->GetSupportedTypes())
		{
			AddNodeStyle(GetAssetStyleId(pType, true), pType->GetIcon(), QColor::fromRgb(8, 255, 64), pStyle->GetBackgroundColor());
			AddNodeStyle(GetAssetStyleId(pType, false), pType->GetIcon(), QColor::fromRgb(255, 64, 8), QColor::fromRgb(64, 16, 2));
		}
	}

	virtual const char* GetTypeName() const override
	{
		return "asset_dependency_graph";
	}
	virtual CAbstractDictionary* GetAvailableNodesDictionary() override
	{
		return m_pNodesDictionary.get();
	}
	virtual const CryGraphEditor::CNodeGraphViewStyle* GetStyle() const override
	{
		return m_pStyle.get();
	}

	static string GetAssetStyleId(const CAssetType* pAssetType, bool bAssetFound)
	{
		if (pAssetType && bAssetFound)
		{
			return pAssetType->GetTypeName();
		}
		else if (pAssetType)
		{
			return string().Format("%s_not_found", pAssetType->GetTypeName());
		}

		return bAssetFound ? "cryasset" : "cryasset_not_found";
	}

protected:

	void AddNodeStyle(const char* szStyleId, CryIcon& icon, QColor iconColor, QColor backgroundColor)
	{
		CryGraphEditor::CNodeWidgetStyle* pStyle = new CryGraphEditor::CNodeWidgetStyle(szStyleId, *(m_pStyle.get()));

		QPixmap iconPixmap = icon.pixmap(pStyle->GetHeaderWidgetStyle().GetNodeIconViewSize(), QIcon::Normal);
		iconPixmap.detach();

		pStyle->GetHeaderWidgetStyle().SetNodeIconMenuColor(iconColor);
		pStyle->GetHeaderWidgetStyle().SetNodeIconViewDefaultColor(iconColor);
		pStyle->GetHeaderWidgetStyle().SetNodeIcon(CryIcon(iconPixmap));
		pStyle->SetBackgroundColor(backgroundColor);
	}

	std::unique_ptr<CryGraphEditor::CNodeGraphViewStyle> m_pStyle;
	std::unique_ptr<CNodesDictionary>                    m_pNodesDictionary;
};

class CBasePinItem : public CryGraphEditor::CAbstractPinItem
{
public:
	CBasePinItem(CryGraphEditor::CAbstractNodeItem& nodeItem)
		: CryGraphEditor::CAbstractPinItem(nodeItem.GetViewModel())
		, m_nodeItem(nodeItem)
	{
	}
	virtual CryGraphEditor::CAbstractNodeItem& GetNodeItem() const override
	{
		return m_nodeItem;
	}
	CryGraphEditor::CPinWidget* CreateWidget(CryGraphEditor::CNodeWidget& nodeWidget, CryGraphEditor::CNodeGraphView& view) override
	{
		return new CryGraphEditor::CPinWidget(*this, nodeWidget, view, IsOutputPin() && IsConnected());
	}
	virtual bool CanConnect(const CAbstractPinItem* pOtherPin) const override
	{
		// Dependency graph does not allow editing.
		return false;
	}
protected:
	CryGraphEditor::CAbstractNodeItem& m_nodeItem;
};

class CInPinItem : public CBasePinItem
{
public:
	CInPinItem(CryGraphEditor::CAbstractNodeItem& nodeItem)
		: CBasePinItem(nodeItem)
	{
	}
	virtual QString GetName() const override
	{
		return QString("Asset");
	}
	virtual QString GetDescription() const override
	{
		return QString("Asset");
	}
	virtual QString GetTypeName() const override
	{
		return QString("Dependency");
	}
	virtual QVariant GetId() const override
	{
		return QVariant::fromValue(QString("In"));
		;
	}
	virtual bool HasId(QVariant id) const override
	{
		QString name = "In";
		return (name == id.value<QString>());
	}
	virtual bool IsInputPin() const override
	{
		return true;
	}
	virtual bool IsOutputPin() const override
	{
		return false;
	}
};

class COutPinItem : public CBasePinItem
{
public:
	COutPinItem(CryGraphEditor::CAbstractNodeItem& nodeItem)
		: CBasePinItem(nodeItem)
	{
	}
	virtual QString GetName() const override
	{
		return QString("Dependencies");
	}
	virtual QString GetDescription() const override
	{
		return QString("Dependencies");
	}
	virtual QString GetTypeName() const override
	{
		return QString("Dependency");
	}
	virtual QVariant GetId() const override
	{
		return QVariant::fromValue(QString("Out"));
		;
	}
	virtual bool HasId(QVariant id) const override
	{
		QString name = "Out";
		return (name == id.value<QString>());
	}
	virtual bool IsInputPin() const override
	{
		return false;
	}
	virtual bool IsOutputPin() const override
	{
		return true;
	}
};

class CFeatureGridNodeContentWidget : public CryGraphEditor::CAbstractNodeContentWidget
{
public:
	CFeatureGridNodeContentWidget(CryGraphEditor::CNodeWidget& node, CryGraphEditor::CNodeGraphView& view)
		: CAbstractNodeContentWidget(node, view)
	{
	}
};

CAssetNodeBase::CAssetNodeBase(CryGraphEditor::CNodeGraphViewModel& viewModel, CAsset* pAsset, const CAssetType* pAssetType, const string& path)
	: CAbstractNodeItem(viewModel)
	, m_pAsset(pAsset)
	, m_pAssetType(pAssetType)
	, m_path(path)
{
}

bool CAssetNodeBase::CanBeEdited() const
{
	return m_pAsset && m_pAsset->GetType()->CanBeEdited();
}

void CAssetNodeBase::EditAsset() const
{
	CRY_ASSERT(CanBeEdited());
	m_pAsset->Edit();
}

bool CAssetNodeBase::CanBeCreated() const
{
	return !m_pAsset && m_pAssetType && m_pAssetType->CanBeCreated();
}

void CAssetNodeBase::CreateAsset() const
{
	CRY_ASSERT(CanBeCreated());
	m_pAssetType->Create(m_path + ".cryasset");
}

bool CAssetNodeBase::CanBeImported() const
{
	return !m_pAsset && m_pAssetType && m_pAssetType->IsImported();
}

std::vector<string> CAssetNodeBase::GetSourceFileExtensions() const
{
	CRY_ASSERT(CanBeImported());
	return static_cast<CGraphViewModel*>(&GetViewModel())->GetModel()->GetSourceFileExtensions(m_pAssetType);
}

void CAssetNodeBase::ImportAsset(const string& sourceFilename) const
{
	CRY_ASSERT(CanBeImported());
	m_pAssetType->Import(sourceFilename, PathUtil::GetPathWithoutFilename(m_path), PathUtil::GetFileName(m_path));
}

//! Asset view model.
class CAssetNode : public CAssetNodeBase
{
public:
	enum EPin
	{
		ePin_In = 0,
		ePin_Out
	};
public:
	CAssetNode(CryGraphEditor::CNodeGraphViewModel& viewModel, CAsset* pAsset, const CAssetType* pAssetType, const string& path)
		: CAssetNodeBase(viewModel, pAsset, pAssetType, path)
		, m_In(*this)
		, m_Out(*this)
		, m_pins({ &m_In, &m_Out })
		, m_nodeEntry(*this)
	{
		CRY_ASSERT(m_pins[ePin_In] == &m_In && m_pins[ePin_Out] == &m_Out);

		SetAcceptsDeletion(false);
		SetAcceptsCopy(false);
		SetAcceptsPaste(false);
		SetAcceptsDeactivation(false);
	}
	virtual ~CAssetNode()
	{
	}

	virtual CryGraphEditor::CNodeWidget* CreateWidget(CryGraphEditor::CNodeGraphView& view) override
	{
		CAssetWidget* pNode = new CAssetWidget(*this, view);
		pNode->SetHeaderNameWidth(120);
		auto pContent = new CryGraphEditor::CPinGridNodeContentWidget(*pNode, view);

		pNode->SetContentWidget(pContent);
		return pNode;
	}
	virtual QVariant GetId() const override
	{
		return QVariant();
	}
	virtual bool HasId(QVariant id) const override
	{
		return false;
	}
	virtual QVariant GetTypeId() const override
	{
		return QVariant();
	}
	virtual const CryGraphEditor::PinItemArray& GetPinItems() const override
	{
		return m_pins;
	}
	virtual const char* GetStyleId() const override
	{
		return CGraphRuntimeContext::GetAssetStyleId(m_pAssetType, m_pAsset != nullptr);
	}

	virtual QString GetToolTipText() const override
	{
		if (m_pAsset)
		{
			return QtUtil::ToQString(m_path);
		}

		return QString("%1%2").arg(QObject::tr("Asset not found:\n"), QtUtil::ToQString(m_path));
	}

	virtual const CAbstractDictionaryEntry* GetNodeEntry() const override
	{
		return &m_nodeEntry;
	}

private:
	CInPinItem                   m_In;
	COutPinItem                  m_Out;
	CryGraphEditor::PinItemArray m_pins;
	CNodeEntry                   m_nodeEntry;
};

class CConnectionItem : public CryGraphEditor::CAbstractConnectionItem
{
public:
	CConnectionItem(CryGraphEditor::CAbstractPinItem& sourcePin, CryGraphEditor::CAbstractPinItem& targetPin, CryGraphEditor::CNodeGraphViewModel& viewModel)
		: CryGraphEditor::CAbstractConnectionItem(viewModel)
		, m_sourcePin(sourcePin)
		, m_targetPin(targetPin)
	{
		m_sourcePin.AddConnection(*this);
		m_targetPin.AddConnection(*this);

		SetAcceptsDeletion(false);
		SetAcceptsCopy(false);
		SetAcceptsPaste(false);
		SetAcceptsDeactivation(false);
	}
	virtual ~CConnectionItem()
	{
		m_sourcePin.RemoveConnection(*this);
		m_targetPin.RemoveConnection(*this);
	}
	virtual QVariant GetId() const override
	{
		return QVariant();
	}
	virtual bool HasId(QVariant id) const override
	{
		return false;
	}
	virtual CryGraphEditor::CConnectionWidget* CreateWidget(CryGraphEditor::CNodeGraphView& view) override
	{
		return new CryGraphEditor::CConnectionWidget(this, view);
	}
	virtual CryGraphEditor::CAbstractPinItem& GetSourcePinItem() const override
	{
		return m_sourcePin;
	}
	virtual CryGraphEditor::CAbstractPinItem& GetTargetPinItem() const override
	{
		return m_targetPin;
	}
private:
	CryGraphEditor::CAbstractPinItem& m_sourcePin;
	CryGraphEditor::CAbstractPinItem& m_targetPin;
};

CGraphViewModel::CGraphViewModel(CModel* pModel)
	: m_pRuntimeContext(new CGraphRuntimeContext(*this))
	, m_pModel(pModel)
	, m_bSuppressComplexityWarning(false)
{
	m_pModel->signalBeginChange.Connect(this, &CGraphViewModel::OnBeginModelChange);
	m_pModel->signalEndChange.Connect(this, &CGraphViewModel::OnEndModelChange);
}

CryGraphEditor::INodeGraphRuntimeContext& CGraphViewModel::GetRuntimeContext()
{
	return *(m_pRuntimeContext.get());
}

QString CGraphViewModel::GetGraphName()
{
	return QtUtil::ToQString("CGraphViewModel::GetGraphName()");
}

uint32 CGraphViewModel::GetNodeItemCount() const
{
	return m_nodes.size();
}

CryGraphEditor::CAbstractNodeItem* CGraphViewModel::GetNodeItemByIndex(uint32 index) const
{
	CRY_ASSERT(index < m_nodes.size());
	return m_nodes[index].get();
}

CryGraphEditor::CAbstractNodeItem* CGraphViewModel::GetNodeItemById(QVariant id) const
{
	const auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [path = QtUtil::ToString(id.toString())](const auto& x)
	{
		return x.get()->Cast<CAssetNodeBase>()->GetPath() == path;
	});

	return it != m_nodes.end() ? it->get() : nullptr;
}

uint32 CGraphViewModel::GetConnectionItemCount() const
{
	return m_connections.size();
}

CryGraphEditor::CAbstractConnectionItem* CGraphViewModel::GetConnectionItemByIndex(uint32 index) const
{
	CRY_ASSERT(index < m_connections.size());
	return m_connections[index].get();
}

CryGraphEditor::CAbstractConnectionItem* CGraphViewModel::GetConnectionItemById(QVariant id) const
{
	CRY_ASSERT("NotImplemented");
	return nullptr;
}

void CGraphViewModel::OnBeginModelChange()
{
	m_nodes.clear();
	m_connections.clear();
	SignalInvalidated();
}

void CGraphViewModel::OnEndModelChange()
{
	using namespace Private_GraphViewModel;

	CRY_ASSERT(m_pModel);

	if (!m_pModel->GetAsset())
	{
		return;
	}

	std::vector<SAssetData> assets;
	std::vector<std::pair<size_t, size_t>> connections;

	std::unordered_map<string, size_t, stl::hash_strcmp<string>> map(1000);
	std::vector<size_t> stack(8, 0);
	
	assets.reserve(1000);
	connections.reserve(4000);

	m_pModel->ForAllDependencies(m_pModel->GetAsset(), [this, &stack, &map, &assets, &connections](CAsset* pAsset, const string& assetPath, size_t depth)
	{
		// Ignore dependencies on the engine assets.
		if (assetPath[0] == '%')
		{
			return;
		}

		if (stack.size() <= depth)
		{
			stack.resize(depth + 1, 0);
		}

		CAssetNode* pNode = nullptr;
		const string assetKey = string(assetPath).MakeLower();
		const auto it = map.find(assetKey);
		if (it == map.end())
		{
			map.insert(std::make_pair(assetKey, assets.size()));
			stack[depth] = assets.size();
			assets.emplace_back(pAsset, assetPath, depth);

			if (!pAsset)
			{
				// Mark parent assets.
				for (size_t i = 0; i <= depth; ++i)
				{
					assets[stack[i]].bSelected = true;
				}
			}
		}
		else
		{
			assets[it->second].depth = std::max(assets[it->second].depth, depth);
			stack[depth] = it->second;
		}

		if (depth)
		{
			connections.emplace_back(stack[depth - 1], stack[depth]);
		}
	});

	static const size_t maximumNumberOfNodes = 100;
	const bool bAllNodes = assets.size() <= maximumNumberOfNodes;

	// Create a simple table layout.
	// Having pre-order depth-first traversing of the nodes, we move to the next table row If we do not go to the next depth level.
	size_t row = 0;
	size_t column = 0;
	size_t totalCountOfUnresolvedNodes = 0;
	size_t countOfUnresolvedNodesToBeShown = 0;
	for (SAssetData& asset : assets)
	{
		if (!asset.p)
		{
			++totalCountOfUnresolvedNodes;
		}

		if (!asset.bSelected && !bAllNodes)
		{
			continue;
		}

		if (!bAllNodes && (m_nodes.size() >= maximumNumberOfNodes))
		{
			asset.bSelected = false;
			continue;
		}

		if (asset.depth && column >= asset.depth)
		{
			++row;
		}
		column = asset.depth;

		const CAssetType* pAssetType = asset.p ? asset.p->GetType() : m_pModel->FindAssetTypeByFile(asset.path);
		CAssetNode* pNode = new CAssetNode(*this, asset.p, pAssetType, asset.path);
		pNode->SetAcceptsRenaming(true);
		pNode->SetName(QtUtil::ToQString(PathUtil::GetFileName(asset.path)));
		pNode->SetAcceptsRenaming(false);

		size_t x = column * 500;
		size_t y = row * 100;
		pNode->SetPosition(QPointF(x, y));

		asset.node = m_nodes.size();
		m_nodes.emplace_back(pNode);

		if (!asset.p)
		{
			++countOfUnresolvedNodesToBeShown;
		}
	}

	for (const auto c : connections)
	{
		if (bAllNodes || (assets[c.first].bSelected && assets[c.second].bSelected))
		{
			CryGraphEditor::CAbstractPinItem* pSrcPinItem = m_nodes[assets[c.first].node]->GetPinItemByIndex(CAssetNode::ePin_Out);
			CryGraphEditor::CAbstractPinItem* pDstPinItem = m_nodes[assets[c.second].node]->GetPinItemByIndex(CAssetNode::ePin_In);
			m_connections.emplace_back(new CConnectionItem(*pSrcPinItem, *pDstPinItem, *this));
		}
	}

	if (!bAllNodes && !m_bSuppressComplexityWarning)
	{
		GetIEditor()->GetNotificationCenter()->ShowWarning(tr("Dependency Graph"), tr("\"%1\" has too many dependencies to display the whole graph."
			"\n%2 out of %3 missing assets will be displayed.")
			.arg(QtUtil::ToQString(m_pModel->GetAsset()->GetName()))
			.arg(countOfUnresolvedNodesToBeShown)
			.arg(totalCountOfUnresolvedNodes));

		m_bSuppressComplexityWarning = true;
	}

	SignalInvalidated();
}

