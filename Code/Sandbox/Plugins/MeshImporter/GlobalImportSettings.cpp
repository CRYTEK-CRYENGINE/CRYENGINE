// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "GlobalImportSettings.h"

#include <Cry3DEngine/I3DEngine.h>
#include <IEditor.h>
#include <IResourceSelectorHost.h>
#include <Material\Material.h>
#include <Material\MaterialManager.h>

#include <CrySerialization/yasli/Archive.h>
#include <CrySerialization/yasli/Enum.h>
#include <CrySerialization/StringList.h>

YASLI_ENUM_BEGIN_NESTED2(FbxTool, Axes, EAxis, "Axis")
YASLI_ENUM(FbxTool::Axes::EAxis::NegX, "-X", "-X");
YASLI_ENUM(FbxTool::Axes::EAxis::NegY, "-Y", "-Y");
YASLI_ENUM(FbxTool::Axes::EAxis::NegZ, "-Z", "-Z");
YASLI_ENUM(FbxTool::Axes::EAxis::PosX, "+X", "+X");
YASLI_ENUM(FbxTool::Axes::EAxis::PosY, "+Y", "+Y");
YASLI_ENUM(FbxTool::Axes::EAxis::PosZ, "+Z", "+Z");
YASLI_ENUM_END()

// If 'scale' corresponds to one of the known standard units (with some small
// tolerance), the unit's symbol is returned. Otherwise, the value of 'scale'
// is converted to string.
static QString FormatUnitSizeInCm(double scale)
{
	for (int i = 0; i < FbxMetaData::Units::eUnitSetting_COUNT; ++i)
	{
		const auto unit = (FbxMetaData::Units::EUnitSetting)i;
		if (FbxMetaData::Units::eUnitSetting_FromFile != unit)
		{
			if (IsEquivalent((float)scale, (float)FbxMetaData::Units::GetUnitSizeInCm(unit, -1.0)))
			{
				return QString::fromLatin1(FbxMetaData::Units::GetSymbol(unit));
			}
		}
	}
	return QString::fromLatin1("%1").arg(scale);
}

static FbxTool::Axes::EAxis GetNextAxisWrapped(FbxTool::Axes::EAxis axis)
{
	return FbxTool::Axes::EAxis(((int)axis + 1) % (int)FbxTool::Axes::EAxis::COUNT);
}

////////////////////////////////////////////////////

CGlobalImportSettings::SConversionSettings::SConversionSettings()
	: unit(FbxMetaData::Units::EUnitSetting::eUnitSetting_FromFile)
	, scale(1.0f)
	, forward(FbxTool::Axes::EAxis::PosX)
	, up(FbxTool::Axes::EAxis::PosY)
	, oldForward(FbxTool::Axes::EAxis::PosX)
	, oldUp(FbxTool::Axes::EAxis::PosY)
{}

void CGlobalImportSettings::SConversionSettings::Serialize(yasli::Archive& ar)
{
	Serialization::StringList options;
	FbxMetaData::Units::ForallNames([&options](const char* szUnitName)
	{
		options.push_back(szUnitName);
	});
	options[0] += string(" (") + QtUtil::ToString(fileUnitSizeInCm) + string(")");

	Serialization::StringListValue dropDown(options, (int)unit);
	ar(dropDown, "unit", "Unit");

	if (ar.isInput())
	{
		unit = (FbxMetaData::Units::EUnitSetting)dropDown.index();
	}

	ar(scale, "scale", "Scale");
	scale = std::max(0.0f, scale);

	ar(forward, "forward", "Forward");
	ar(up, "up", "Up");

	// Make axes selection consistent.

	if (forward != oldForward)
	{
		assert(!ar.isEdit() || up == oldUp);
		SetForwardAxis(forward);
	}
	else if (up != oldUp)
	{
		assert(!ar.isEdit() || forward == oldForward);
		SetUpAxis(up);
	}
}

void CGlobalImportSettings::SConversionSettings::SetForwardAxis(FbxTool::Axes::EAxis axis)
{
	oldForward = axis;
	forward = axis;
	while (FbxTool::Axes::Abs(forward) == FbxTool::Axes::Abs(up))
	{
		up = GetNextAxisWrapped(up);
	}
}

void CGlobalImportSettings::SConversionSettings::SetUpAxis(FbxTool::Axes::EAxis axis)
{
	oldUp = axis;
	up = axis;
	while (FbxTool::Axes::Abs(forward) == FbxTool::Axes::Abs(up))
	{
		forward = GetNextAxisWrapped(forward);
	}
}

////////////////////////////////////////////////////

CGlobalImportSettings::SGeneralSettings::SGeneralSettings()
{}

CGlobalImportSettings::SStaticMeshSettings::SStaticMeshSettings()
	: bMergeAllNodes(false)
	, bSceneOrigin(false)
	, bComputeNormals(false)
	, bComputeUv(false)
{}

CGlobalImportSettings::CGlobalImportSettings()
	: m_bStaticMeshSettingsEnabled(true)
{}

CGlobalImportSettings::~CGlobalImportSettings() {}

FbxMetaData::Units::EUnitSetting CGlobalImportSettings::GetUnit() const
{
	return m_conversionSettings.unit;
}

float CGlobalImportSettings::GetScale() const
{
	return m_conversionSettings.scale;
}

FbxTool::Axes::EAxis CGlobalImportSettings::GetUpAxis()
{
	return m_conversionSettings.up;
}

FbxTool::Axes::EAxis CGlobalImportSettings::GetForwardAxis()
{
	return m_conversionSettings.forward;
}

bool CGlobalImportSettings::IsMergeAllNodes() const
{
	return m_staticMeshSettings.bMergeAllNodes;
}

bool CGlobalImportSettings::IsSceneOrigin() const
{
	return m_staticMeshSettings.bSceneOrigin;
}

bool CGlobalImportSettings::IsComputeNormals() const
{
	return m_staticMeshSettings.bComputeNormals;
}

bool CGlobalImportSettings::IsComputeUv() const
{
	return m_staticMeshSettings.bComputeUv;
}

void CGlobalImportSettings::SetInputFilePath(const string& filePath)
{
	m_generalSettings.inputFilePath = filePath;
	m_generalSettings.inputFilename = PathUtil::GetFileName(m_generalSettings.inputFilePath);
}

void CGlobalImportSettings::SetOutputFilePath(const string& filePath)
{
	m_generalSettings.outputFilePath = filePath;
	m_generalSettings.outputFilename = PathUtil::GetFileName(m_generalSettings.outputFilePath);
}

void CGlobalImportSettings::ClearFilePaths()
{
	m_generalSettings.inputFilePath.clear();
	m_generalSettings.outputFilePath.clear();
	m_generalSettings.inputFilename.clear();
	m_generalSettings.outputFilename.clear();
}

void CGlobalImportSettings::SetFileUnitSizeInCm(double scale)
{
	m_conversionSettings.fileUnitSizeInCm = FormatUnitSizeInCm(scale);
}

void CGlobalImportSettings::SetUnit(FbxMetaData::Units::EUnitSetting unit)
{
	m_conversionSettings.unit = unit;
}

void CGlobalImportSettings::SetScale(float scale)
{
	m_conversionSettings.scale = std::max(0.0f, scale);
}

void CGlobalImportSettings::SetForwardAxis(FbxTool::Axes::EAxis axis)
{
	m_conversionSettings.SetForwardAxis(axis);
}

void CGlobalImportSettings::SetUpAxis(FbxTool::Axes::EAxis axis)
{
	m_conversionSettings.SetUpAxis(axis);
}

void CGlobalImportSettings::SetMergeAllNodes(bool bMergeAllNodes)
{
	m_staticMeshSettings.bMergeAllNodes = bMergeAllNodes;
}

void CGlobalImportSettings::SetSceneOrigin(bool bSceneOrigin)
{
	m_staticMeshSettings.bSceneOrigin = bSceneOrigin;
}

void CGlobalImportSettings::SetComputeNormals(bool bComputeNormals)
{
	m_staticMeshSettings.bComputeNormals = bComputeNormals;
}

void CGlobalImportSettings::SetComputeUv(bool bComputeUv)
{
	m_staticMeshSettings.bComputeUv = bComputeUv;
}

bool CGlobalImportSettings::IsVertexPositionFormatF32() const
{
	return m_outputSettings.bVertexPositionFormatF32;
}

void CGlobalImportSettings::SetVertexPositionFormatF32(bool bIs32Bit)
{
	m_outputSettings.bVertexPositionFormatF32 = bIs32Bit;
}

void CGlobalImportSettings::SGeneralSettings::Serialize(yasli::Archive& ar)
{
	ar(inputFilename, "input_filename", "!Input filename");
	ar.doc(inputFilePath);

	ar(outputFilename, "output_filename", "!Output filename");
	ar.doc(outputFilePath);
}

void CGlobalImportSettings::SStaticMeshSettings::Serialize(yasli::Archive& ar)
{
	ar(bMergeAllNodes, "merge_all_nodes", "Merge all nodes");
	ar(bSceneOrigin, "scene_origin", "Scene origin");
	ar(bComputeNormals, "compute_normals", "Compute normals");
	ar.doc("If true the importer computes normals,\n"
		"otherwise the normals are imported from the file.\n"
		"Tangents are computed in any case.");
	ar(bComputeUv, "compute_uv", "Compute UV's");
	ar.doc("If true the importer generates an automatic UV mapping,\n"
		"otherwise the UV's are imported from the file.\n"
		"Useful for correct calculation of tangents, if the source model does not use textured materials.");
}

void CGlobalImportSettings::Serialize(yasli::Archive& ar)
{
	ar(m_generalSettings, "general_settings", "General settings");
	if (ar.isInput() || m_bStaticMeshSettingsEnabled)
	{
		ar(m_staticMeshSettings, "static_mesh_settings", "Static mesh settings");
	}
	ar(m_conversionSettings, "conversion_settings", "Conversion settings");
	ar(m_outputSettings, "output_settings", "Output settings");
}

void CGlobalImportSettings::SetStaticMeshSettingsEnabled(bool bEnabled)
{
	m_bStaticMeshSettingsEnabled = bEnabled;
}

void CGlobalImportSettings::SOutputSettings::Serialize(yasli::Archive& ar)
{
	ar(bVertexPositionFormatF32, "use_32_bit_positions", "Use 32bit precision");
	ar.doc("When this option is selected, the importer stores vertex positions using 32 bit per coordinate instead of 16 bit.");
}

