// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>

namespace SourceAsset
{
using std::vector;
using Serialization::IArchive;

struct Node
{
	string      name;
	int         mesh;
	vector<int> children;

	Node()
		: mesh(-1)
	{
	}

	void Serialize(IArchive& ar)
	{
		ar(name, "name", "^");
		ar(mesh, "mesh", "Mesh");
		ar(children, "children", "Children");
	}
};
typedef vector<Node> Nodes;

struct Mesh
{
	string name;

	void   Serialize(IArchive& ar)
	{
		ar(name, "name", "^");
	}
};
typedef vector<Mesh> Meshes;

struct Scene
{
	Nodes  nodes;
	Meshes meshes;

	void   Serialize(IArchive& ar)
	{
		ar(nodes, "nodes", "Nodes");
		ar(meshes, "meshes", "Meshes");
	}
};

}

