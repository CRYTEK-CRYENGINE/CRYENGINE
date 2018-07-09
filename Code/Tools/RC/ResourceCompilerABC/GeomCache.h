// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2012.
// -------------------------------------------------------------------------
//  File name:   GeomCache.cpp
//  Created:     6/8/2012 by Axel Gneiting
//
////////////////////////////////////////////////////////////////////////////

#pragma once

#include <Cry3DEngine/GeomCacheFileFormat.h>
#include "ThreadUtils.h"

#include <CryPhysics/primitives.h>
#include <CryPhysics/physinterface.h>

#include <unordered_set>

namespace GeomCache
{
	struct MeshData
	{			
		std::vector<GeomCacheFile::Position> m_positions;
		std::vector<GeomCacheFile::Texcoords> m_texcoords;
		std::vector<GeomCacheFile::QTangent> m_qTangents;
		std::vector<GeomCacheFile::Color> m_reds;
		std::vector<GeomCacheFile::Color> m_greens;
		std::vector<GeomCacheFile::Color> m_blues;
		std::vector<GeomCacheFile::Color> m_alphas;		
	};

	struct RawMeshFrame 
	{
		RawMeshFrame()
			: m_bDone(false)
			, m_bEncoded(false) 
			, m_frameUseCount(0)
		{
		}

		bool m_bDone;
		bool m_bEncoded;
		unsigned int m_frameUseCount;
		MeshData m_meshData;
	};

	// Data stored for each mesh
	struct Mesh
	{
		Mesh()
			: m_firstRawFrameIndex(0)
			, m_bUsePredictor(false)
			, m_bHasNormals(false)
			, m_bHasTexcoords(false)
			, m_bHasColors(false)
		{
		}

		// Variance
		GeomCacheFile::EStreams m_constantStreams;
		GeomCacheFile::EStreams m_animatedStreams;

		// Mesh hash
		uint64 m_hash;
		
		// Static mesh AABB
		AABB m_aabb;

		// The number of required position quantization bits for each axis
		uint8 m_positionPrecision[3];

		// Static mesh data
		MeshData m_staticMeshData;

		// Compile buffer (multiple frames can overlap in computation)
		std::vector<RawMeshFrame> m_meshDataBuffer;		
		
		// Raw animated data frames for encoder		
		mutable ThreadUtils::CriticalSection m_rawFramesCS;
		uint m_firstRawFrameIndex;
		std::deque<RawMeshFrame> m_rawFrames;		

		// Encoded animated data frames for writer
		mutable ThreadUtils::CriticalSection m_encodedFramesCS;
		std::deque<std::vector<uint8>> m_encodedFrames;

		// Material ID -> material indices. Needs to be std::map, because materials need to be sorted by their id.
		std::map<uint16, std::vector<uint32>> m_indicesMap;

		// Face ID -> Material ID
		std::unordered_map<uint32, uint16> m_materialIdMap;

		// Predictor data
		std::vector<uint16> m_predictorData;
		bool m_bUsePredictor;

		// Compilation data
		bool m_bHasNormals;
		bool m_bHasTexcoords;
		bool m_bHasColors;		
		std::string m_colorParamName;
		std::vector<uint32> m_abcIndexToGeomCacheIndex; // map from alembic indices to GPU indices				
		Alembic::AbcGeom::IPolyMesh m_abcMesh; // The alembic poly mesh this originated from
		std::vector<bool> m_reflections;
	};

	struct NodeData
	{
		bool m_bVisible;
		QuatTNS m_transform;
		
		NodeData()
			: m_bVisible(false)
		{
		}
	};

	// A node in the cache transform hierarchy. 
	// Can be a plain parent transform, a transform and mesh combined
	// or a transform and a physics geometry combined.
	struct Node
	{
		Node()
			: m_type(GeomCacheFile::eNodeType_Transform)
			, m_transformType(GeomCacheFile::eTransformType_Constant)
		{
		}

		// Node type
		GeomCacheFile::ENodeType m_type;

		// Transform type
		GeomCacheFile::ETransformType m_transformType;	

		// Static node data
		NodeData m_staticNodeData;

		// Compile buffer (multiple frames can overlap in computation)
		std::vector<NodeData> m_nodeDataBuffer;
		
		// Animated data frames for encoder
		mutable ThreadUtils::CriticalSection m_animatedNodeDataCS;
		std::deque<NodeData> m_animatedNodeData;

		// Encoded animated data frames for writer
		mutable ThreadUtils::CriticalSection m_encodedFramesCS;
		std::deque<std::vector<uint8>> m_encodedFrames;

		// Mesh (if mesh node)
		std::shared_ptr<Mesh> m_pMesh;

		// Serialized physics geometry (if physics geometry node)
		std::vector<char> m_physicsGeometry;

		// Children
		std::vector<std::unique_ptr<Node>> m_children;

		// The alembic object
		Alembic::Abc::IObject m_abcObject;

		// The alembic xform stack for this node that will be merged down to one 
		// transform matrix in the compilation process
		std::vector<Alembic::AbcGeom::IXform> m_abcXForms;

		// For debug output
		string m_name;
	};
}
