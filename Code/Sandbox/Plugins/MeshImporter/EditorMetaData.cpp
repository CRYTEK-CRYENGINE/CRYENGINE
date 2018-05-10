// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EditorMetaData.h"
#include "SceneUserData.h"

#include <CrySerialization/Enum.h>
#include <CrySerialization/Color.h>

void LogPrintf(const char* szFormat, ...);

YASLI_ENUM_BEGIN_NESTED(FbxTool, EMaterialPhysicalizeSetting, "PhysicalizeSetting")
YASLI_ENUM(FbxTool::eMaterialPhysicalizeSetting_None, "no", "render")                           // No physicalization
YASLI_ENUM(FbxTool::eMaterialPhysicalizeSetting_Default, "default", "render, collide, raytest") // Default physicalization
YASLI_ENUM(FbxTool::eMaterialPhysicalizeSetting_Obstruct, "obstruct", "render, collide")        // Obstructing physicalization
YASLI_ENUM(FbxTool::eMaterialPhysicalizeSetting_NoCollide, "no_collide", "render, raytest")     // Non-colliding physicalization
YASLI_ENUM(FbxTool::eMaterialPhysicalizeSetting_ProxyOnly, "proxy_only", "collide, raytest")    // Proxy-only physicalization
YASLI_ENUM_END()

namespace FbxTool
{
namespace Meta
{

namespace
{

void WriteMeta(const CScene& scene, const SNode* pNode, SNodeMeta& meta)
{
	meta.path = GetPath(pNode);
	meta.exportSetting = scene.GetNodeExportSetting(pNode);
	meta.lod = scene.GetNodeLod(pNode);
	meta.bIsProxy = scene.IsProxy(pNode);
	meta.customProperties = scene.GetUserDefinedProperties(pNode);
	meta.physicalProperties = scene.GetPhysicalProperties(pNode);
}

void ReadMeta(const SNodeMeta& meta, CScene& scene, const SNode* pNode)
{
	scene.SetNodeExportSetting(pNode, meta.exportSetting);
	scene.SetNodeLod(pNode, meta.lod);
	scene.SetProxy(pNode, meta.bIsProxy);
	scene.SetUserDefinedProperties(pNode, meta.customProperties);
	scene.SetPhysicalProperties(pNode, meta.physicalProperties);
}

void WriteMeta(const CScene& scene, const SMaterial* pMaterial, SMaterialMeta& meta)
{
	meta.name = pMaterial->szName;
	meta.physicalizeSetting = scene.GetMaterialPhysicalizeSetting(pMaterial);
	meta.subMaterialIndex = scene.GetMaterialSubMaterialIndex(pMaterial);
	meta.color = scene.GetMaterialColor(pMaterial);
	meta.bMarkedForIndexAutoAssignment = scene.IsMaterialMarkedForIndexAutoAssignment(pMaterial);
}

void ReadMeta(const SMaterialMeta& meta, CScene& scene, const SMaterial* pMaterial)
{
	scene.SetMaterialPhysicalizeSetting(pMaterial, meta.physicalizeSetting);
	scene.SetMaterialSubMaterialIndex(pMaterial, meta.subMaterialIndex);
	scene.SetMaterialColor(pMaterial, meta.color);
	scene.MarkMaterialForIndexAutoAssignment(pMaterial, meta.bMarkedForIndexAutoAssignment);
}

}   // namespace

void SNodeMeta::Serialize(Serialization::IArchive& ar)
{
	ar(path, "path");
	ar(exportSetting, "export_setting");
	ar(lod, "lod");
	ar(bIsProxy, "is_proxy");

	static const CNodeProperties s_defaultPhysicalProperties;
	if (ar.isInput() || physicalProperties != s_defaultPhysicalProperties)
	{
		ar(physicalProperties, "options");
	}

	if (ar.isInput() || !customProperties.empty())
	{
		ar(customProperties, "customOptions");
	}
}

void WriteNodeMetaData(const CScene& scene, std::vector<SNodeMeta>& nodeMeta)
{
	std::vector<const SNode*> nodeStack;
	nodeStack.push_back(scene.GetRootNode());
	while (!nodeStack.empty())
	{
		const SNode* pNode = nodeStack.back();
		nodeStack.pop_back();

		if (!HasDefaultUserData(&scene, pNode))
		{
			SNodeMeta meta;
			WriteMeta(scene, pNode, meta);
			nodeMeta.push_back(meta);
		}

		for (int i = 0; i < pNode->numChildren; ++i)
		{
			nodeStack.push_back(pNode->ppChildren[i]);
		}
	}
}

void ReadNodeMetaData(const std::vector<SNodeMeta>& nodeMeta, CScene& scene)
{
	for (const auto& meta : nodeMeta)
	{
		const SNode* pNode = FindChildNode(scene.GetRootNode(), meta.path);
		if (pNode)
		{
			ReadMeta(meta, scene, pNode);
		}
	}
}

void SMaterialMeta::Serialize(Serialization::IArchive& ar)
{
	ar(name, "name");
	ar(physicalizeSetting, "physicalize_setting");
	ar(subMaterialIndex, "sub_material_index");
	ar(bMarkedForIndexAutoAssignment, "auto_id");
	ar(color, "color");
}

void WriteMaterialMetaData(const CScene& scene, std::vector<SMaterialMeta>& materialMeta)
{
	for (int i = 0; i < scene.GetMaterialCount(); ++i)
	{
		SMaterialMeta meta;
		WriteMeta(scene, scene.GetMaterialByIndex(i), meta);
		materialMeta.push_back(meta);
	}
}

void ReadMaterialMetaData(const std::vector<SMaterialMeta>& materialMeta, CScene& scene)
{
	for (size_t i = 0; i < materialMeta.size(); ++i)
	{
		const SMaterial* pMaterial = scene.GetMaterialByName(materialMeta[i].name);
		if (pMaterial)
		{
			ReadMeta(materialMeta[i], scene, pMaterial);
		}
		else
		{
			LogPrintf("%s: Skipping unknown material '%s' referenced by meta data",
			          __FUNCTION__, materialMeta[i].name);
		}
	}
}

} // namespace Meta
} // namespace FbxTool

SEditorMetaData::SEditorMetaData()
{
}

void SEditorMetaData::Serialize(yasli::Archive& ar)
{
	ar(editorGlobalImportSettings, "globalSettings");
	ar(editorNodeMeta, "editorNodeMeta");
	ar(editorMaterialMeta, "editorMaterialMeta");
}

std::unique_ptr<FbxMetaData::IEditorMetaData> SEditorMetaData::Clone() const
{
	return std::make_unique<SEditorMetaData>(*this);
}

