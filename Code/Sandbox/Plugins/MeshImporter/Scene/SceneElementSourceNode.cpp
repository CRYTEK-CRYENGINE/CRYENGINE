// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SceneElementSourceNode.h"
#include "SceneElementTypes.h"
#include "Scene.h"
#include "Serialization/Serialization.h"

#include <CrySerialization/Enum.h>

SERIALIZATION_ENUM_BEGIN_NESTED(FbxTool, ENodeExportSetting, "NodeExportSetting")
SERIALIZATION_ENUM(FbxTool::eNodeExportSetting_Exclude, "excluded", "Excluded")
SERIALIZATION_ENUM(FbxTool::eNodeExportSetting_Include, "included", "Included")
SERIALIZATION_ENUM_END()

CSceneElementSourceNode::CSceneElementSourceNode(CScene* pScene, int id)
	: CSceneElementCommon(pScene, id)
	, m_pNode(nullptr)
	, m_pFbxScene(nullptr)
{
}

const FbxTool::SNode*  CSceneElementSourceNode::GetNode() const
{
	return m_pNode;
}

void CSceneElementSourceNode::SetSceneAndNode(FbxTool::CScene* pScene, const FbxTool::SNode* pNode)
{
	m_pNode = pNode;
	m_pFbxScene = pScene;
}

ESceneElementType CSceneElementSourceNode::GetType() const
{
	return ESceneElementType::SourceNode;
}

void CSceneElementSourceNode::Serialize(Serialization::IArchive& ar)
{
	FbxTool::ENodeExportSetting exportSetting = m_pFbxScene->GetNodeExportSetting(GetNode());
	ar(exportSetting, "exp", "Included in Export");
	m_pFbxScene->SetNodeExportSetting(GetNode(), exportSetting);

	if (!GetNode()->pParent)
	{
		/*
		// Do not show any more properties for the scene root node.
		CSceneModel* pModel = static_cast<CSceneModel*>(GetModel());
		pModel->OnDataSerialized(this, ar.isInput());
		*/

		return;
	}

	const bool bProxy = m_pFbxScene->IsProxy(GetNode());

	// LODs must not show physics options.
	if (m_pFbxScene->GetNodeLod(GetNode()) == 0)
	{
		CNodeProperties::ENodeType nodeType;
		if (bProxy)
		{
			nodeType = CNodeProperties::ENodeType::eProxy;
		}
		else if (GetNode()->numMeshes)
		{
			nodeType = CNodeProperties::ENodeType::eMesh;
		}
		else
		{
			nodeType = CNodeProperties::ENodeType::eDummy;
		}

		auto& physicalProperties = m_pFbxScene->GetPhysicalProperties(GetNode());
		physicalProperties.SetNodeType(nodeType);
		ar(physicalProperties, "physical_options", "Physical Options");
	}

	struct SNodeCustomProperties
	{
		std::vector<string> custom;

		void                Serialize(Serialization::IArchive& ar)
		{
			ar(custom, "custom", "Custom");
		}
	};

	SNodeCustomProperties customProperties = { m_pFbxScene->GetUserDefinedProperties(GetNode()) };
	ar(customProperties, "custom_properties", "Custom Properties");
	m_pFbxScene->SetUserDefinedProperties(GetNode(), customProperties.custom);

	SNodeInfo nodeInfo = { GetNode() };

	ar(nodeInfo, "nodeinfo", "Node information");

	if (GetNode()->numMeshes)
	{
		SMeshInfo meshInfo = { GetNode()->ppMeshes[0] };

		ar(meshInfo, "meshinfo", "Mesh information");
	}

	struct SAutoLodInfo
	{
		const CAutoLodSettings::sNodeParam& nodeParam;

		void                                Serialize(Serialization::IArchive& ar)
		{
			ar((bool)nodeParam.m_bAutoGenerate, "autogenerate", "IsAutoGenerate");
			ar((int)nodeParam.m_iLodCount, "lodcount", "LodCount");
			ar((float)nodeParam.m_fPercent, "percent", "Percent");
		}
	};

	if (m_pNode->numMeshes >= 0 && !m_pFbxScene->GetNodeLod(m_pNode))
	{
		SAutoLodInfo autoLodInfo = { m_pFbxScene->GetAutoLodProperties(m_pNode) };
		ar(autoLodInfo, "autolodinfo", "Auto Generate Lods");
		if (ar.isInput())
		{
			GetScene()->signalPropertiesChanged(this);
		}
	}
}

