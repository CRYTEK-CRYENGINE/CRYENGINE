// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "FbxScene.h"
#include "FbxMetaData.h"
#include "GlobalImportSettings.h"
#include "NodeProperties.h"

#include <CryString/CryString.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/yasli/Archive.h>
#include <CrySerialization/yasli/STL.h>
#include <CrySerialization/yasli/Enum.h>
#include <CrySerialization/yasli/JSONIArchive.h>
#include <CrySerialization/yasli/JSONOArchive.h>

namespace FbxTool
{
namespace Meta
{

struct SNodeMeta
{
	std::vector<string>         path;
	FbxTool::ENodeExportSetting exportSetting;
	int                         lod;
	bool                        bIsProxy;
	std::vector<string>         customProperties;
	CNodeProperties             physicalProperties;

	void                        Serialize(Serialization::IArchive& ar);
};

void WriteNodeMetaData(const CScene& scene, std::vector<SNodeMeta>& nodeMeta);
void ReadNodeMetaData(const std::vector<SNodeMeta>& nodeMeta, CScene& scene);

struct SMaterialMeta
{
	string                      name;
	EMaterialPhysicalizeSetting physicalizeSetting;
	int                         subMaterialIndex;
	bool                        bMarkedForIndexAutoAssignment;
	ColorF                      color;

	void                        Serialize(Serialization::IArchive& ar);
};

void WriteMaterialMetaData(const CScene& scene, std::vector<SMaterialMeta>& materialMeta);
void ReadMaterialMetaData(const std::vector<SMaterialMeta>& materialMeta, CScene& scene);

} // namespace Meta
} // namespace FbxTool

//! Data used to restore state of editor. Not passed to RC.
struct SEditorMetaData : FbxMetaData::IEditorMetaData
{
	CGlobalImportSettings editorGlobalImportSettings;
	std::vector<FbxTool::Meta::SNodeMeta>     editorNodeMeta;
	std::vector<FbxTool::Meta::SMaterialMeta> editorMaterialMeta;

	SEditorMetaData();

	// IEditorMetaData implementation.
	virtual void Serialize(yasli::Archive& ar) override;
	virtual std::unique_ptr<FbxMetaData::IEditorMetaData> Clone() const override;
};


