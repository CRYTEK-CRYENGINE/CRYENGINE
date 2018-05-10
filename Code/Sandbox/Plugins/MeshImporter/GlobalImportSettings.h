// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "FbxScene.h"
#include "FbxMetaData.h"
#include <CrySerialization/yasli/Archive.h>

class CMaterial;

class CGlobalImportSettings
{
private:
	struct SGeneralSettings
	{
		string              inputFilePath;
		string              outputFilePath;

		string              inputFilename;
		string              outputFilename;
		SGeneralSettings();

		void Serialize(yasli::Archive& ar);
	};

	struct SStaticMeshSettings
	{
		bool                bMergeAllNodes;
		bool                bSceneOrigin;
		bool                bComputeNormals;
		bool                bComputeUv;

		SStaticMeshSettings();

		void Serialize(yasli::Archive& ar);
	};

	struct SConversionSettings
	{
		// Displayed in property tree.
		FbxMetaData::Units::EUnitSetting unit;
		float                            scale;
		FbxTool::Axes::EAxis             forward;
		FbxTool::Axes::EAxis             up;

		QString                          fileUnitSizeInCm;

		// By comparing forward and up to their old values, we can figure
		// out which one changed most recently.
		FbxTool::Axes::EAxis oldForward;
		FbxTool::Axes::EAxis oldUp;

		SConversionSettings();

		void Serialize(yasli::Archive& ar);

		void SetForwardAxis(FbxTool::Axes::EAxis axis);
		void SetUpAxis(FbxTool::Axes::EAxis axis);
	};

	struct SOutputSettings
	{
		bool bVertexPositionFormatF32 = false;
		void Serialize(yasli::Archive& ar);
	};

public:
	CGlobalImportSettings();
	~CGlobalImportSettings();

	FbxMetaData::Units::EUnitSetting GetUnit() const;
	float                            GetScale() const;
	FbxTool::Axes::EAxis             GetUpAxis();
	FbxTool::Axes::EAxis             GetForwardAxis();
	bool                             IsMergeAllNodes() const;
	bool                             IsSceneOrigin() const;
	bool                             IsComputeNormals() const;
	bool                             IsComputeUv() const;

	void                             SetInputFilePath(const string& path);
	void                             SetOutputFilePath(const string& path);
	void                             ClearFilePaths();

	void                             SetFileUnitSizeInCm(double scale);
	void                             SetUnit(FbxMetaData::Units::EUnitSetting unit);
	void                             SetScale(float scale);
	void                             SetForwardAxis(FbxTool::Axes::EAxis axis);
	void                             SetUpAxis(FbxTool::Axes::EAxis axis);
	void                             SetMergeAllNodes(bool bMergeAllNodes);
	void                             SetSceneOrigin(bool bSceneOrigin);
	void                             SetComputeNormals(bool bComputeNormals);
	void                             SetComputeUv(bool bComputeUv);

	bool                             IsVertexPositionFormatF32() const;
	void                             SetVertexPositionFormatF32(bool bIs32Bit);



	void SetStaticMeshSettingsEnabled(bool bEnabled);

	void                             Serialize(yasli::Archive& ar);
private:
	SGeneralSettings m_generalSettings;
	SStaticMeshSettings m_staticMeshSettings;
	SConversionSettings m_conversionSettings;
	SOutputSettings m_outputSettings;
	bool m_bStaticMeshSettingsEnabled;
};

