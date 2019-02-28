// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Cry3DEngine/CGF/CryHeaders.h>  // enum EPhysicsGeomType
#include "../../../../Sandbox/Plugins/MeshImporter/AutoLodSettings.h"

class CImportRequest
{
public:
	struct SNodeInfoType
	{
		string name;
		std::vector<string> path;
		string properties;
		std::vector<SNodeInfoType> children;
	};

	struct SMaterialInfo
	{
		string sourceName;
		EPhysicsGeomType physicalization;
		int finalSubmatIndex;  // < 0 means "delete faces with this material"
	};

	struct SAnimation
	{
		string name;
		std::vector<string> motionNodePath;
		int startFrame; // Start and End time of the animation, expressed in frames(a constant frame rate of 30fps is assumed).
		int endFrame;
	};

	struct SJointLimits
	{
		float values[6];
		char mask;

		SJointLimits()
			: mask(0)
		{
		}
	};

	struct SJointPhysicsData
	{
		std::vector<string> jointNodePath;  // Path to the node this data refers to.

		std::vector<string> proxyNodePath;  // If this path is not empty, use the corresponding node as proxy geometry.
		bool bSnapToJoint;  // Move the proxy to its joint origin.

		SJointLimits jointLimits;

		SJointPhysicsData()
			: bSnapToJoint(false)
		{
		}
	};

public:
	CImportRequest()
	{
		Clear();
	}

	void Clear()
	{
		jsonData.clear();

		sourceFilename.clear();
		materialFilename.clear();
		outputFilenameExt = "cgf";

		forwardUpAxes.clear();
		sourceUnitSizeText.clear();
		scale = -1.0f;

		bPhysicalizeWithBoxes = false;
		bSceneOrigin = false;
		bMergeAllNodes = false;
		bIgnoreCustomNormals = false;
		bIgnoreTextureCoordinates = false;

		animation.startFrame = -1;
		animation.endFrame = -1;

		nodes.clear();
		materials.clear();
		jointPhysicsData.clear();
	}

	// Supported files: chunk files (JSON data are in ChunkType_ImportSettings chunk), .json files.
	bool LoadFromFile(const char* filename);

	const SMaterialInfo* FindMaterialInfoByName(const string& name) const;

public:
	std::vector<char> jsonData;

	string sourceFilename;
	string materialFilename;
	string outputFilenameExt; // without '.'

	string forwardUpAxes;  // "<signOfForwardAxis><forwardAxis><signOfUpAxis><upAxis>" e.g. "-Y+Z". Empty string means 'use file settings'. 
	string sourceUnitSizeText;
	float scale;

	bool bPhysicalizeWithBoxes;
	bool bSceneOrigin;  // note: used only if !nodes.empty()
	bool bMergeAllNodes;
	bool bIgnoreCustomNormals;
	bool bIgnoreTextureCoordinates;

	// list of fbx scene nodes to import, may be empty.
	std::vector<SNodeInfoType> nodes;

	std::vector<SMaterialInfo> materials;

	CAutoLodSettings autoLodSettings;

	SAnimation animation;
	std::vector<SJointPhysicsData> jointPhysicsData;
};
