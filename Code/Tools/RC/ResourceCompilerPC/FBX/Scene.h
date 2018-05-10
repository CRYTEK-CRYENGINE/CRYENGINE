// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once


namespace MeshUtils
{
	class Mesh;
};


namespace Scene
{

struct SNode
{
	enum EAttributeType
	{
		eAttributeType_Mesh,
		eAttributeType_Unknown,
	};

	struct Attribute
	{
		EAttributeType eType;
		int index;  // index in a scene array; array depends on attribute type (see IScene::GetMesh() for example)

		Attribute()
			: eType(eAttributeType_Unknown)
			, index(-1)
		{
		}

		Attribute(EAttributeType a_eType, int a_index)
			: eType(a_eType)
			, index(a_index)
		{
		}
	};

	string name;
	Matrix34 worldTransform;  // allowed to be skewed, left- or right-handed, non-uniformly scaled
	Matrix34 geometryOffset;  // the geometry offset to a node. It is never inherited by the children.
	std::vector<Attribute> attributes;

	int parent;  // <0 if there is no parent; use IScene::GetNode() to access the parent node
	std::vector<int> children;  // use IScene::GetNode(children[i]) to access i-th child node

public:
	SNode() : parent(-1) {}

	int FindFirstMatchingAttribute(EAttributeType eType) const
	{	
		for (int i = 0; i < (int)attributes.size(); ++i)
		{
			if (attributes[i].eType == eType)
			{
				return i;
			}
		}
		return -1;
	}
};

struct SMaterial
{
	string name;
};

struct STrs
{
	STrs() : translation(ZERO), rotation(IDENTITY), scale(IDENTITY) {}

	Vec3 translation;
	Quat rotation;
	Vec3 scale;
};

struct SLinkInfo
{
	SLinkInfo()
		: initialTransform(IDENTITY)
		, initialLinkTransform(IDENTITY)
		, nodeIndex(-1)
	{
	}

	SLinkInfo(const Matrix34& initialTransform, const Matrix34& initialLinkTransform, int nodeIndex)
		: initialTransform(initialTransform)
		, initialLinkTransform(initialLinkTransform)
		, nodeIndex(nodeIndex)
	{
	}

	Matrix34 initialTransform; // Global initial transform of mesh node.
	Matrix34 initialLinkTransform; // Global initial transform of linked nodes.
	int nodeIndex;
};

struct SLinksInfo
{
	std::vector<SLinkInfo> linkedNodes;
};

struct SAnimation
{
	string name;
	int startFrame; // Expressed in frames(a constant frame rate of 30fps is assumed).
	int endFrame;
};

struct IScene
{
	virtual ~IScene() {}

	// Returns an ASCIIZ string in form "<signOfForwardAxis><forwardAxis><signOfUpAxis><upAxis>",
	// for example: "-Y+Z", "+Z+Y", "+X+Z", etc.
	virtual const char* GetForwardUpAxes() const = 0;

	virtual double GetUnitSizeInCentimeters() const = 0;

	virtual int GetNodeCount() const = 0;
	virtual const SNode* GetNode(int idx) const = 0;

	// Use GetMaterial(MeshUtils::Mesh::m_faceMatIds[i]) to access materials referenced by faces.
	virtual int GetMeshCount() const = 0;
	virtual const MeshUtils::Mesh* GetMesh(int idx) const = 0;
	virtual const SLinksInfo* GetMeshLinks(int idx) const = 0;

	virtual int GetMaterialCount() const = 0;
	virtual const SMaterial* GetMaterial(int idx) const = 0;

	virtual int GetAnimationStackCount() const  = 0;
	virtual const SAnimation* GetAnimationStack(int idx) const = 0;
	virtual bool SetCurrentAnimationStack(int idx) = 0;

	// Start and End time of the animation, expressed in frames(a constant frame rate of 30fps is assumed).
	virtual STrs EvaluateNodeLocalTransform(int nodeIdx, int animationFrameIndex) const = 0;
	virtual STrs EvaluateNodeGlobalTransform(int nodeIdx, int frameIndex) const  = 0;
};

// See: http://en.wikipedia.org/wiki/Tree_traversal
template<typename Visitor>
inline void TraverseDepthFirstPreOrder(const IScene* pScene, int nodeIdx, const Visitor &visitor)
{
	if (nodeIdx < 0 || !pScene)
	{
		return;
	}
	visitor(nodeIdx);
	for (size_t i = 0, n = pScene->GetNode(nodeIdx)->children.size(); i < n; ++i)
	{
		TraverseDepthFirstPreOrder(pScene, pScene->GetNode(nodeIdx)->children[i], visitor);
	}
}

// See: http://en.wikipedia.org/wiki/Tree_traversal
template<typename Visitor>
inline void TraverseDepthFirstPostOrder(const IScene* pScene, int nodeIdx, const Visitor &visitor)
{
	if (nodeIdx < 0 || !pScene)
	{
		return;
	}
	for (size_t i = 0, n = pScene->GetNode(nodeIdx)->children.size(); i < n; ++i)
	{
		TraverseDepthFirstPostOrder(pScene, pScene->GetNode(nodeIdx)->children[i], visitor);
	}
	visitor(nodeIdx);
}

// Visit parent nodes from root to the passed node
template<typename Visitor>
inline void TraversePathFromRoot(const IScene* pScene, int nodeIdx, const Visitor &visitor)
{
	if (nodeIdx < 0 || !pScene)
	{
		return;
	}
	TraversePathFromRoot(pScene, pScene->GetNode(nodeIdx)->parent, visitor);
	visitor(pScene->GetNode(nodeIdx));
}

// Visit parent nodes from passed node to the root
template<typename Visitor>
inline void TraversePathToRoot(const IScene* pScene, int nodeIdx, const Visitor &visitor)
{
	if (nodeIdx < 0 || !pScene)
	{
		return;
	}
	visitor(pScene->GetNode(nodeIdx));
	TraversePathToRoot(pScene, pScene->GetNode(nodeIdx)->parent, visitor);
}

// Full name contains node names separated by separatorChar,
// starting from the root node till the passed node.
// Note: the name of the root node is excluded because the root node
// assumed to be a helper that is invisible for the user.
inline string GetEncodedFullName(const IScene* pScene, int nodeIdx)
{
	if (nodeIdx < 0 || !pScene)
	{
		return string();
	}

	string name;

	TraversePathFromRoot(pScene, nodeIdx, [&name](const SNode *pNode)
	{
		if (pNode->parent >= 0)
		{
			static const char escapeChar = '^';
			static const char separatorChar = '/';
			if (!name.empty())
			{
				name += separatorChar;
			}
			const char* p = pNode->name.c_str();
			while (*p)
			{
				if (*p == escapeChar || *p == separatorChar)
				{
					name += escapeChar;
				}
				name += *p++;
			}
		}
	});

	return name;
}

// Returns path to the passed node in 'result'.
// Path to a node is a sequence of node names starting at the
// root node and ending at the passed node.
// Note: the name of the root node is excluded because the root node
// assumed to be a helper that is invisible to the user.
inline void GetPath(std::vector<string>& result, const IScene* pScene, int nodeIdx)
{
	result.clear();

	if (nodeIdx < 0 || !pScene)
	{
		return;
	}

	TraversePathFromRoot(pScene, nodeIdx, [&result](const SNode *pNode)
	{
		if (pNode->parent >= 0)
		{
			result.push_back(pNode->name);
		}
	});
}

// Having two nodes returns the lowest common ancestor. Returns collection of nodes forming the minimum subtree as a bit field.
// Assumes that parent index is always less than the child index.
inline int FindMinimumSubtree(std::vector<bool>& subtree, const IScene& scene, int i, int j)
{
	assert(subtree.size() == scene.GetNodeCount());

	subtree[i] = true;
	subtree[j] = true;

	while (i != j)
	{
		if (i > j)
		{
			assert(i > scene.GetNode(i)->parent);

			i = scene.GetNode(i)->parent;
			subtree[i] = true;
		}
		else
		{
			assert(j > scene.GetNode(j)->parent);

			j = scene.GetNode(j)->parent;
			subtree[j] = true;
		}
	}

	return i;
}

// Having a set of nodes returns the lowest common ancestor. Returns collection of nodes forming the minimum subtree as a bit field.
// Assumes that parent index is always less than the child index.
inline int FindMinimumSubtree(std::vector<bool>& subtree, const IScene& scene, const std::vector<bool>& nodes)
{
	assert(nodes.size() == scene.GetNodeCount());

	int i = scene.GetNodeCount();
	int j = -1;
	while (--i >= 0)
	{
		if (!nodes[i])
		{
			continue;
		}

		j = (j >= 0) ? FindMinimumSubtree(subtree, scene, i, j) : i;
	}
	return j;
}

// Returns collection of nodes forming path to the passed node as a bit field in 'nodes'.
// Returns index of the root node.
// Note: the scene the root node (index == 0) is excluded because the root node
// assumed to be a helper that is invisible for the user.
inline int MarkPath(std::vector<bool>& nodes, const IScene& scene, int nodeIndex)
{
	assert(nodes.size() == scene.GetNodeCount());

	int root = -1;
	while (nodeIndex > 0)
	{
		root = nodeIndex;
		nodes[nodeIndex] = true;
		nodeIndex = scene.GetNode(nodeIndex)->parent;
	}
	return root;
}

inline bool IsMesh(const IScene& scene, int nodeIndex)
{
	const SNode* pNode = scene.GetNode(nodeIndex);
	return pNode && std::any_of(pNode->attributes.begin(), pNode->attributes.end(), [](const Scene::SNode::Attribute& attribute)
	{ 
		return attribute.eType == Scene::SNode::eAttributeType_Mesh; 
	});
}

} // namespace Scene

