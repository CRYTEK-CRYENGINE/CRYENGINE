// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "ImportRequest.h"

#include "PathHelpers.h"
#include "StringHelpers.h"
#include "Plugins/MeshImporter/NodeProperties.h"
#include "../CryEngine/Cry3DEngine/CGF/ChunkFile.h"

#include <CrySerialization/yasli/Archive.h>
#include <CrySerialization/yasli/STL.h>
#include <CrySerialization/yasli/Enum.h>
#include <CrySerialization/yasli/JSONIArchive.h>
#include <CrySerialization/yasli/JSONOArchive.h>

#include <numeric> // std::accumulate

namespace Serialization
{
	static void Serialize(yasli::Archive& ar, CImportRequest::SNodeInfoType& value)
	{
		assert(ar.isInput());

		ar(value.path, "path", "Path to a node in the source scene");
		ar(value.name, "name", "Name of the node in the resulting scene");

		CNodeProperties np;
		std::vector<string> udp;

		np.Serialize(ar);
		ar(udp, "udp", " User-defined properties");

		value.properties = std::accumulate(
			udp.begin(), 
			udp.end(), 
			np.GetAsString(), 
			[](const string& a, const string& b) 
		{
			return a.empty() ? b : (a + "\n" + b);
		});

		ar(value.children, "nodes", "Child nodes in the resulting scene");
	}

	static void Serialize(yasli::Archive& ar, CImportRequest::SMaterialInfo& value)
	{
		assert(ar.isInput());

		ar(value.sourceName, "name", "Name of the material in the source scene");

		string s;
		ar(s, "physicalize", "Physicalization type");

		if (StringHelpers::EqualsIgnoreCase(s, "no"))
		{
			value.physicalization = PHYS_GEOM_TYPE_NONE;
		}
		else if (StringHelpers::EqualsIgnoreCase(s, "default"))
		{
			value.physicalization = PHYS_GEOM_TYPE_DEFAULT;
		}
		else if (StringHelpers::EqualsIgnoreCase(s, "obstruct"))
		{
			value.physicalization = PHYS_GEOM_TYPE_OBSTRUCT;
		}
		else if (StringHelpers::EqualsIgnoreCase(s, "no_collide"))
		{
			value.physicalization = PHYS_GEOM_TYPE_NO_COLLIDE;
		}
		else if (StringHelpers::EqualsIgnoreCase(s, "proxy_only"))
		{
			value.physicalization = PHYS_GEOM_TYPE_DEFAULT_PROXY;
		}
		else
		{
			value.physicalization = PHYS_GEOM_TYPE_NONE;
		}

		ar(value.finalSubmatIndex, "sub_index", "Submaterial id to store in output asset files");
		if (value.finalSubmatIndex >= MAX_SUB_MATERIALS)
		{
			value.finalSubmatIndex = -1;
		}
	}

	static void Serialize(yasli::Archive& ar, CImportRequest::SAnimation& value)
	{
		assert(ar.isInput());

		ar(value.name, "name", "FBX Animation Stack Name");
		ar(value.motionNodePath, "motionNodePath", "Path to a scene node to derive character's motion from");
		ar(value.startFrame, "startFrame", "Start time of the animation, expressed in frames (30fps)");
		ar(value.endFrame, "endFrame", "End time of the animation, expressed in frames (30fps)");
	}

	static void Serialize(yasli::Archive& ar, CImportRequest::SJointLimits& value)
	{
		assert(ar.isInput());
		static const char* axisLimitNames[] =
		{
			"min_x",
			"min_y",
			"min_z",
			"max_x",
			"max_y",
			"max_z"
		};
		value.mask = 0;
		for (int i = 0; i < 6; ++i)
		{
			const bool bHasLimit = ar(value.values[i], axisLimitNames[i]);
			if (bHasLimit)
			{
				value.mask |= (1 << i);
			}
		}
	}

	static void Serialize(yasli::Archive& ar, CImportRequest::SJointPhysicsData& value)
	{
		assert(ar.isInput());

		ar(value.jointNodePath, "jointNodePath", "Path to a node in the source scene");
		ar(value.proxyNodePath, "proxyNodePath", "Path to a node in the source scene");
		ar(value.bSnapToJoint, "snapToJoint", "Move this proxy to its joint origin");
		ar(value.jointLimits, "jointLimits");
	}

	static void Serialize(yasli::Archive& ar, CImportRequest& value)
	{
		assert(ar.isInput());

		ar(value.sourceFilename, "source_filename", "Filename of the source scene");

		ar(value.outputFilenameExt, "output_ext", "Extension of the output file");
		value.outputFilenameExt = value.outputFilenameExt.MakeLower();

		ar(value.materialFilename, "material_filename", "Filename of the material");
		if (value.materialFilename.empty())
		{
			value.materialFilename = "default";
		}
		ar(value.forwardUpAxes, "forward_up_axes", "World axes");
		ar(value.sourceUnitSizeText, "unit_size", "Size of the unit ('file', 'cm', 'm', 'inch', etc)");
		ar(value.scale, "scale", "Scale (size multiplier)");

		string s;
		ar(s, "physics_primitive", "Type of primitive to use when physicalizing geometry");
		if (StringHelpers::EqualsIgnoreCase(s, "box"))
		{
			value.bPhysicalizeWithBoxes = true;
		}
		else
		{
			value.bPhysicalizeWithBoxes = false;
		}

		ar(value.bMergeAllNodes, "merge_all_nodes", "Merge all nodes");
		ar(value.bSceneOrigin, "scene_origin", "true - use scene's origin, false - use origins of root nodes");
		ar(value.bIgnoreCustomNormals, "ignore_custom_normals", "Ignore Custom Normals");
		ar(value.bIgnoreTextureCoordinates, "ignore_uv", "Ignore Texture Coordinates");

		ar(value.materials, "materials", "Materials");

		ar(value.nodes, "nodes", "Nodes in the resulting scene");

		ar(value.autoLodSettings,"autolodsettings","Auto Lod Setting");

		ar(value.animation, "animation", "Animation to import");
		ar(value.jointPhysicsData, "jointPhysicsData", "Joint physics data");
	}
	
	template<typename T>
	struct SerializerHelper : yasli::Serializer
	{
		SerializerHelper(T& value)
			: yasli::Serializer(yasli::TypeID::get<T>(), static_cast<void*>(&value), sizeof(T), &SerializerHelper::YASLI_SERIALIZE_METHOD)
		{
		}

		static bool YASLI_SERIALIZE_METHOD(void* pObject, yasli::Archive& ar)
		{
			::Serialization::Serialize(ar, *static_cast<T*>(pObject));
			return true;
		}
	};
}

#define DEFINE_YASLI_SERIALIZE_OVERRIDE(type) \
namespace yasli \
{ \
	static bool YASLI_SERIALIZE_OVERRIDE(Archive& ar, type& value, const char* szName, const char* szLabel) \
	{ \
		::Serialization::SerializerHelper<type> serializer(value); \
		return ar(static_cast<yasli::Serializer&>(serializer), szName, szLabel); \
	} \
}

DEFINE_YASLI_SERIALIZE_OVERRIDE(CImportRequest::SNodeInfoType)
DEFINE_YASLI_SERIALIZE_OVERRIDE(CImportRequest::SMaterialInfo)
DEFINE_YASLI_SERIALIZE_OVERRIDE(CImportRequest::SAnimation)
DEFINE_YASLI_SERIALIZE_OVERRIDE(CImportRequest::SJointLimits)
DEFINE_YASLI_SERIALIZE_OVERRIDE(CImportRequest::SJointPhysicsData)
DEFINE_YASLI_SERIALIZE_OVERRIDE(CImportRequest)

#undef DEFINE_YASLI_SERIALIZE_OVERRIDE


static bool ReadFile(std::vector<char>& result, const char* const filename)
{
	result.clear();

	FILE* const file = fopen(filename, "rb");
	if (!file)
	{
		RCLogError("Failed to open file '%s'", filename);
		return false;
	}

	fseek(file, 0, SEEK_END);
	const long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (size == 0)
	{
		fclose(file);
		RCLogError("File '%s' is empty", filename);
		return false;
	}

	result.resize(size);

	const bool bOk = fread(result.data(), size, 1, file) == 1;

	fclose(file);

	if (!bOk)
	{
		RCLogError("Failed to read file '%s'", filename);
		return false;
	}

	return true;
}


bool CImportRequest::LoadFromFile(const char* filename)
{
	Clear();

	CChunkFile cf;
	if (cf.Read(filename))
	{
		// Reading JSON data from ChunkType_ImportSettings chunk of a chunk file.		
		const IChunkFile::ChunkDesc* const pChunk = cf.FindChunkByType(ChunkType_ImportSettings);
		if (!pChunk)
		{
			RCLogError("ChunkType_ImportSettings chunk (JSON) is missing in '%s'", filename);
			return false;
		}
		const char* const pBegin = static_cast<const char*>(pChunk->data);
		jsonData.assign(pBegin, pBegin + pChunk->size);
	}
	else
	{
		// Reading JSON data from a pure .json file.
		if (!ReadFile(jsonData, filename))
		{
			return false;
		}
	}

	yasli::JSONIArchive ar;
	if (!ar.open(jsonData.data(), jsonData.size()))
	{
		RCLogError("Failed to read JSON data from '%s'", filename);
		return false;
	}
	if (!ar(*this, "request", "Import request"))
	{
		RCLogError("Failed to parse JSON data read from '%s'", filename);
		return false;
	}

	return true;
}


const CImportRequest::SMaterialInfo* CImportRequest::FindMaterialInfoByName(const string& name) const
{
	for (size_t i = 0; i < materials.size(); ++i)
	{
		if (StringHelpers::EqualsIgnoreCase(name, materials[i].sourceName))
		{
			return &materials[i];
		}
	}
	return nullptr;
}
