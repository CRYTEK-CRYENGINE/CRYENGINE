// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ParticleGraphItems.h"

#include "../Undo.h"

#include "Widgets/FeatureGridNodeContentWidget.h"
#include "Widgets/NodeIcons.h"

#include "ParticleGraphModel.h"

#include "QtUtil.h"

#include <NodeGraph/NodeWidget.h>
#include <NodeGraph/PinWidget.h>
#include <NodeGraph/ConnectionWidget.h>
#include <NodeGraph/NodeGraphViewStyle.h>

#include <QVariant>

namespace CryParticleEditor {

CNodeItem::CNodeItem(pfx2::IParticleComponent& component, CryGraphEditor::CNodeGraphViewModel& viewModel)
	: CAbstractNodeItem(viewModel)
	, m_component(component)
{
	SetAcceptsDeactivation(true);
	SetAcceptsRenaming(true);

	m_name = component.GetName();
	m_position.setX(component.GetNodePosition().x);
	m_position.setY(component.GetNodePosition().y);

	uint32 featureCount = m_component.GetNumFeatures();
	m_pins.reserve(featureCount + 1);

	m_pins.push_back(new CParentPinItem(*this));
	m_pins.push_back(new CChildPinItem(*this));

	for (uint32 i = 0; i < featureCount; ++i)
	{
		pfx2::IParticleFeature* pFeature = m_component.GetFeature(i);
		if (pFeature)
		{
			CFeatureItem* pFeatureItem = new CFeatureItem(*pFeature, *this, viewModel);
			m_features.push_back(pFeatureItem);
			if (pFeatureItem->HasComponentConnector())
			{
				m_pins.push_back(pFeatureItem->GetPinItem());
			}
		}
	}
}

CNodeItem::~CNodeItem()
{
	for (CryGraphEditor::CAbstractPinItem* pItem : m_pins)
	{
		CBasePinItem* pPintItem = static_cast<CBasePinItem*>(pItem);
		if (pPintItem->GetPinType() == EPinType::Parent || pPintItem->GetPinType() == EPinType::Child)
			delete pPintItem;
	}

	for (CFeatureItem* pItem : m_features)
	{
		delete pItem;
	}
}

void CNodeItem::SetPosition(QPointF position)
{
	m_component.SetNodePosition(Vec2(position.x(), position.y()));
	CAbstractNodeItem::SetPosition(position);
	if (GetIEditor()->GetIUndoManager()->IsUndoRecording())
	{
		CryGraphEditor::CUndoNodeMove* pUndoObject = new CryGraphEditor::CUndoNodeMove(*this);
		CUndo::Record(pUndoObject);
	}
}

CryGraphEditor::CNodeWidget* CNodeItem::CreateWidget(CryGraphEditor::CNodeGraphView& view)
{
	CryGraphEditor::CNodeWidget* pNode = new CryGraphEditor::CNodeWidget(*this, view);
	pNode->SetHeaderNameWidth(120);
	CFeatureGridNodeContentWidget* pContent = new CFeatureGridNodeContentWidget(*pNode, static_cast<CParentPinItem&>(*m_pins[0]), static_cast<CChildPinItem&>(*m_pins[1]), view);

	pNode->AddHeaderIcon(new CEmitterActiveIcon(*pNode), CryGraphEditor::CNodeHeader::EIconSlot::Right);
	pNode->AddHeaderIcon(new CEmitterVisibleIcon(*pNode), CryGraphEditor::CNodeHeader::EIconSlot::Right);
	pNode->AddHeaderIcon(new CSoloEmitterModeIcon(*pNode), CryGraphEditor::CNodeHeader::EIconSlot::Right);

	pNode->SetContentWidget(pContent);

	return pNode;
}

QVariant CNodeItem::GetId() const
{
	return QVariant::fromValue(QString(m_component.GetName()));
}

bool CNodeItem::HasId(QVariant id) const
{
	QString nodeName = m_component.GetName();
	return (nodeName == id.value<QString>());
}

QVariant CNodeItem::GetTypeId() const
{
	// Note: Nodes in particle system don't have a type yet.
	return QVariant::fromValue(QString());
}

QString CNodeItem::GetName() const
{
	return QString(m_component.GetName());
}

void CNodeItem::SetName(const QString& name)
{
	const string newName = QtUtil::ToString(name);
	const string oldName = m_component.GetName();
	if (oldName != newName)
	{
		m_component.SetName(newName.c_str());
		CAbstractNodeItem::SetName(GetName());

		// TODO: Move this into an CAbstractNodeItem method.
		if (GetIEditor()->GetIUndoManager()->IsUndoRecording())
		{
			CUndo::Record(new CryGraphEditor::CUndoNodeNameChange(*this, oldName.c_str()));
		}
		// ~TODO
	}
}

bool CNodeItem::IsDeactivated() const
{
	return !m_component.IsEnabled();
}

void CNodeItem::SetDeactivated(bool isDeactivated)
{
	if (IsDeactivated() != isDeactivated)
	{
		m_component.SetEnabled(!isDeactivated);
		SignalDeactivatedChanged(isDeactivated);
	}
}

void CNodeItem::Serialize(Serialization::IArchive& archive)
{
	const string oldName = m_component.GetName();

	m_component.Serialize(archive);

	// Note: Update everything.
	const uint32 numFeatures = m_component.GetNumFeatures();
	FeatureItemArray features;
	features.reserve(numFeatures);
	CryGraphEditor::PinItemArray pins;
	pins.push_back(m_pins[0]);

	size_t movedFeatures = 0;

	for (uint32 i = 0; i < numFeatures; ++i)
	{
		pfx2::IParticleFeature* pFeatureInterface = m_component.GetFeature(i);
		auto predicate = [pFeatureInterface](CFeatureItem* pFeatureItem) -> bool
		{
			return (pFeatureItem && pFeatureInterface == &pFeatureItem->GetFeatureInterface());
		};

		auto result = std::find_if(m_features.begin(), m_features.end(), predicate);
		if (result == m_features.end())
		{
			CFeatureItem* pFeatureItem = new CFeatureItem(*pFeatureInterface, *this, GetViewModel());
			features.push_back(pFeatureItem);
			if (pFeatureItem->HasComponentConnector())
			{
				pins.push_back(pFeatureItem->GetPinItem());
			}

			SignalFeatureAdded(*pFeatureItem);
		}
		else
		{
			CFeatureItem* pFeatureItem = *result;
			features.push_back(pFeatureItem);
			if (pFeatureItem->HasComponentConnector())
			{
				pins.push_back(pFeatureItem->GetPinItem());
			}

			pFeatureItem->SignalInvalidated();
			*result = nullptr;
			++movedFeatures;
		}
	}

	for (size_t i = 0; i < m_features.size(); ++i)
	{
		if (m_features[i] != nullptr)
			RemoveFeature(i);
	}

	CRY_ASSERT(m_features.size() == movedFeatures);

	m_features.swap(features);
	m_pins.swap(pins);

	CRY_ASSERT(m_features.size() == numFeatures);

	SignalInvalidated();
}

CParentPinItem* CNodeItem::GetParentPinItem()
{
	return static_cast<CParentPinItem*>(m_pins[0]);
}

CChildPinItem* CNodeItem::GetChildPinItem()
{
	return static_cast<CChildPinItem*>(m_pins[1]);
}

uint32 CNodeItem::GetIndex() const
{
	CParticleGraphModel& model = static_cast<CParticleGraphModel&>(GetViewModel());
	const pfx2::IParticleEffectPfx2& effect = model.GetEffectInterface();

	const uint32 numNodes = effect.GetNumComponents();
	for (uint32 i = 0; i < numNodes; ++i)
	{
		if (effect.GetComponent(i) == &m_component)
			return i;
	}

	return ~0;
}

CFeatureItem* CNodeItem::AddFeature(uint32 index, const pfx2::SParticleFeatureParams& featureParams)
{
	m_component.AddFeature(index, featureParams);

	pfx2::IParticleFeature* pFeature = m_component.GetFeature(index);
	if (pFeature)
	{
		CFeatureItem* pFeatureItem = new CFeatureItem(*pFeature, *this, GetViewModel());
		m_features.insert(m_features.begin() + index, pFeatureItem);
		if (pFeatureItem->HasComponentConnector())
		{
			m_pins.push_back(pFeatureItem->GetPinItem());
		}
		SignalFeatureAdded(*pFeatureItem);

		if (GetIEditor()->GetIUndoManager()->IsUndoRecording())
		{
			CUndo::Record(new CUndoFeatureCreate(*pFeatureItem));
		}

		return pFeatureItem;
	}
	return nullptr;
}

void CNodeItem::RemoveFeature(uint32 index)
{
	if (m_features.size() > index)
	{
		CFeatureItem* pFeatureItem = m_features.at(index);

		GetViewModel().SignalRemoveCustomItem(*pFeatureItem);
		SignalFeatureRemoved(*pFeatureItem);

		m_features.erase(m_features.begin() + index);

		if (CFeaturePinItem* pFeaturePinItem = pFeatureItem->GetPinItem())
		{
			if (pFeaturePinItem->IsConnected())
			{
				CParticleGraphModel& model = static_cast<CParticleGraphModel&>(GetViewModel());
				for (CryGraphEditor::CAbstractConnectionItem* pConnectionItem : pFeaturePinItem->GetConnectionItems())
				{
					model.RemoveConnection(*pConnectionItem);
				}
			}
			auto result = std::find(m_pins.begin(), m_pins.end(), pFeatureItem->GetPinItem());
			CRY_ASSERT(result != m_pins.end());
			m_pins.erase(result);
		}

		if (GetIEditor()->GetIUndoManager()->IsUndoRecording())
			CUndo::Record(new CUndoFeatureRemove(*pFeatureItem));

		delete pFeatureItem;
	}

	if (index < m_component.GetNumFeatures())
		m_component.RemoveFeature(index);
}

bool CNodeItem::MoveFeatureAtIndex(uint32 featureIndex, uint32 destIndex)
{
	const uint32 maxIndex = m_features.size() - 1;
	destIndex = std::min(destIndex, maxIndex);
	if (featureIndex != destIndex)
	{
		const size_t numFeatures = m_features.size();
		std::vector<uint32> ids;
		ids.reserve(m_features.size());

		for (size_t i = 0; i < numFeatures; ++i)
			ids.push_back(i);

		CFeatureItem* pFeatureItem = m_features[featureIndex];

		ids.erase(ids.begin() + featureIndex);
		ids.insert(ids.begin() + destIndex, featureIndex);

		m_component.SwapFeatures(&ids[0], ids.size());

		m_features.erase(m_features.begin() + featureIndex);
		m_features.insert(m_features.begin() + destIndex, pFeatureItem);

		if (m_component.GetFeature(destIndex) == &pFeatureItem->GetFeatureInterface())
		{
			SignalFeatureMoved(*pFeatureItem);
			if (GetIEditor()->GetIUndoManager()->IsUndoRecording())
			{
				CUndo::Record(new CUndoFeatureMove(*pFeatureItem, featureIndex));
				return true;
			}
		}
	}
	return false;
}

void CNodeItem::SetVisible(bool isVisible)
{
	if (isVisible != m_component.IsVisible())
	{
		m_component.SetVisible(isVisible);
		SignalVisibleChanged(isVisible);
	}
}

bool CNodeItem::IsVisible()
{
	return m_component.IsVisible();
}

CFeaturePinItem::CFeaturePinItem(CFeatureItem& feature)
	: CBasePinItem(feature.GetNodeItem())
	, m_featureItem(feature)
{
}

QString CFeaturePinItem::GetName() const
{
	return m_featureItem.GetName();
}

QVariant CFeaturePinItem::GetId() const
{
	return QVariant::fromValue(m_featureItem.GetName());
}

bool CFeaturePinItem::HasId(QVariant id) const
{
	return (m_featureItem.GetName() == id.value<QString>());
}

bool CFeaturePinItem::CanConnect(const CryGraphEditor::CAbstractPinItem* pOtherPin) const
{
	if (pOtherPin)
		return pOtherPin->IsInputPin() && !pOtherPin->IsConnected();

	return true;
}

CFeatureItem::CFeatureItem(pfx2::IParticleFeature& feature, CNodeItem& node, CryGraphEditor::CNodeGraphViewModel& viewModel)
	: CAbstractNodeGraphViewModelItem(viewModel)
	, m_featureInterface(feature)
	, m_node(node)
{
	m_pPin = HasComponentConnector() ? new CFeaturePinItem(*this) : nullptr;

	SetAcceptsDeactivation(true);
	SetAcceptsHighlightning(true);
	SetAcceptsSelection(true);
}

CFeatureItem::~CFeatureItem()
{
	delete m_pPin;
}

bool CFeatureItem::IsDeactivated() const
{
	return m_featureInterface.IsEnabled() == false;
}

void CFeatureItem::SetDeactivated(bool isDeactivated)
{
	if (!m_featureInterface.IsEnabled() != isDeactivated)
	{
		m_featureInterface.SetEnabled(!isDeactivated);
		m_node.GetComponentInterface().SetChanged();
		SignalItemDeactivatedChanged(isDeactivated);

		// Note: Property tree listens only to node properties changed
		//			 events. So at least for now we need to invalidate the whole node.
		m_node.SignalInvalidated();
	}
}

void CFeatureItem::Serialize(Serialization::IArchive& archive)
{
	pfx2::IParticleComponent& component = GetNodeItem().GetComponentInterface();
	Serialization::SContext context(archive, &component);
	m_featureInterface.Serialize(archive);
	if (archive.isInput())
	{
		component.SetChanged();
		SignalInvalidated();
	}
}

QString CFeatureItem::GetGroupName() const
{
	const pfx2::SParticleFeatureParams& params = m_featureInterface.GetFeatureParams();
	return QString(params.m_groupName);
}

QString CFeatureItem::GetName() const
{
	const pfx2::SParticleFeatureParams& params = m_featureInterface.GetFeatureParams();
	return QString(params.m_featureName);
}

uint32 CFeatureItem::GetIndex() const
{
	pfx2::IParticleComponent& component = m_node.GetComponentInterface();

	for (uint32 i = 0; i < component.GetNumFeatures(); ++i)
	{
		if (component.GetFeature(i) == &m_featureInterface)
		{
			return i;
		}
	}

	CRY_ASSERT_MESSAGE(0, "Invalid feature item!");
	return ~0;
}

bool CFeatureItem::HasComponentConnector() const
{
	const pfx2::SParticleFeatureParams& params = m_featureInterface.GetFeatureParams();
	return params.m_hasComponentConnector;
}

QColor CFeatureItem::GetColor() const
{
	const pfx2::SParticleFeatureParams& params = m_featureInterface.GetFeatureParams();
	return QColor(params.m_color.r, params.m_color.g, params.m_color.b);
}

CConnectionItem::CConnectionItem(CBasePinItem& sourcePin, CBasePinItem& targetPin, CryGraphEditor::CNodeGraphViewModel& viewModel)
	: CryGraphEditor::CAbstractConnectionItem(viewModel)
	, m_sourcePin(sourcePin)
	, m_targetPin(targetPin)
{
	m_sourcePin.AddConnection(*this);
	m_targetPin.AddConnection(*this);

	string sourceNodeName = QtUtil::ToString(m_sourcePin.GetNodeItem().GetName());
	string sourceNodePin = QtUtil::ToString(m_sourcePin.GetName());
	string targetNodeName = QtUtil::ToString(m_targetPin.GetNodeItem().GetName());
	string targetNodePin = QtUtil::ToString(m_targetPin.GetName());

	string id;
	id.Format("%s-%s::%s-%s", sourceNodeName, sourceNodePin, targetNodeName, targetNodePin);

	m_id = CCrc32::Compute(id.c_str());
}

CConnectionItem::~CConnectionItem()
{

}

CryGraphEditor::CConnectionWidget* CConnectionItem::CreateWidget(CryGraphEditor::CNodeGraphView& view)
{
	return new CryGraphEditor::CConnectionWidget(this, view);
}

QVariant CConnectionItem::GetId() const
{
	return QVariant::fromValue(m_id);
}

bool CConnectionItem::HasId(QVariant id) const
{
	return (GetId().value<uint32>() == id.value<uint32>());
}

void CConnectionItem::OnConnectionRemoved()
{
	m_sourcePin.RemoveConnection(*this);
	m_targetPin.RemoveConnection(*this);
}

}

