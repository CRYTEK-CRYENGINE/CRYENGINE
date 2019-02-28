// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Undo.h"

#include <CryParticleSystem/IParticlesPfx2.h>

#include "Models/ParticleGraphModel.h"
#include "Models/ParticleGraphItems.h"

#include <CrySandbox/CrySignal.h>

namespace CryParticleEditor {

CUndoFeatureCreate::CUndoFeatureCreate(CFeatureItem& feature, const string& description)
	: CUndoObject(&feature.GetViewModel())
	, m_params(feature.GetFeatureInterface().GetFeatureParams())
{
	m_nodeId = feature.GetNodeItem().GetId();
	m_featureIndex = feature.GetIndex();

	Serialization::SaveJsonBuffer(m_featureData, feature);
	m_featureData.push_back('\0');

	if (description.empty())
	{
		string oldName = QtUtil::ToString(feature.GetName());
		m_description.Format("Undo creation of feature '%s'", oldName);
	}
	else
		m_description = description;
}

void CUndoFeatureCreate::Undo(bool bUndo)
{
	if (bUndo && m_pModel)
	{
		CNodeItem* pNodeItem = static_cast<CNodeItem*>(m_pModel->GetNodeItemById(m_nodeId));
		if (pNodeItem)
		{
			pNodeItem->RemoveFeature(m_featureIndex);
		}
	}
}

void CUndoFeatureCreate::Redo()
{
	if (m_pModel)
	{
		CNodeItem* pNodeItem = static_cast<CNodeItem*>(m_pModel->GetNodeItemById(m_nodeId));
		if (pNodeItem)
		{
			if (CFeatureItem* pFeatureItem = pNodeItem->AddFeature(m_params))
			{
				pNodeItem->MoveFeatureAtIndex(pFeatureItem->GetIndex(), m_featureIndex);
				Serialization::LoadJsonBuffer(*pFeatureItem, m_featureData.data(), m_featureData.size());
			}
		}
	}
}

CUndoFeatureRemove::CUndoFeatureRemove(CFeatureItem& feature, const string& description)
	: CUndoObject(&feature.GetViewModel())
	, m_params(feature.GetFeatureInterface().GetFeatureParams())
{
	m_nodeId = feature.GetNodeItem().GetId();
	m_featureIndex = feature.GetIndex();

	Serialization::SaveJsonBuffer(m_featureData, feature);
	m_featureData.push_back('\0');

	if (description.empty())
	{
		string oldName = QtUtil::ToString(feature.GetName());
		m_description.Format("Undo delete of feature '%s'", oldName);
	}
	else
		m_description = description;
}

void CUndoFeatureRemove::Undo(bool bUndo)
{
	if (bUndo && m_pModel)
	{
		CNodeItem* pNodeItem = static_cast<CNodeItem*>(m_pModel->GetNodeItemById(m_nodeId));
		if (pNodeItem)
		{
			if (CFeatureItem* pFeatureItem = pNodeItem->AddFeature(m_params))
			{
				pNodeItem->MoveFeatureAtIndex(pFeatureItem->GetIndex(), m_featureIndex);
				Serialization::LoadJsonBuffer(*pFeatureItem, m_featureData.data(), m_featureData.size());
			}
		}
	}
}

void CUndoFeatureRemove::Redo()
{
	if (m_pModel)
	{
		CNodeItem* pNodeItem = static_cast<CNodeItem*>(m_pModel->GetNodeItemById(m_nodeId));
		if (pNodeItem)
		{
			pNodeItem->RemoveFeature(m_featureIndex);
		}
	}
}

CUndoFeatureMove::CUndoFeatureMove(CFeatureItem& feature, uint32 oldIndex, const string& description)
	: CUndoObject(&feature.GetViewModel())
{
	m_nodeId = feature.GetNodeItem().GetId();
	m_oldFeatureIndex = oldIndex;
	m_newFeatureIndex = feature.GetIndex();

	if (description.empty())
		m_description.Format("Undo feature move ");
	else
		m_description = description;
}

void CUndoFeatureMove::Undo(bool bUndo)
{
	if (bUndo && m_pModel)
	{
		CNodeItem* pNodeItem = static_cast<CNodeItem*>(m_pModel->GetNodeItemById(m_nodeId));
		if (pNodeItem)
		{
			pNodeItem->MoveFeatureAtIndex(m_newFeatureIndex, m_oldFeatureIndex);
		}
	}
}

void CUndoFeatureMove::Redo()
{
	if (m_pModel)
	{
		CNodeItem* pNodeItem = static_cast<CNodeItem*>(m_pModel->GetNodeItemById(m_nodeId));
		if (pNodeItem)
		{
			pNodeItem->MoveFeatureAtIndex(m_oldFeatureIndex, m_newFeatureIndex);
		}
	}
}

}

