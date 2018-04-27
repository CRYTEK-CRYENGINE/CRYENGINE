// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
// Utilities for FbxTool.
#pragma once
#if !defined(BUILDING_FBX_TOOL)
	#error This header must not be included directly, #include FbxTool.h instead
#endif
#include "FbxSdkInclude.h"
#include <vector> // std::vector

namespace FbxTool
{
// Visits all nodes in an FBX scene (in depth-first order).
// The functor should have the signature "void functor(FbxNode* pNode)".
template<typename TFunctor>
inline void VisitNodes(FbxScene* pScene, TFunctor& functor)
{
	struct SVisitor   // Can't be a lambda without type-erasure (like std::function)
	{
		SVisitor(TFunctor& functor) : functor(functor) {}

		void operator()(FbxNode* pParent, FbxNode* pNode)
		{
			if (pNode)
			{
				const int numChildren = pNode->GetChildCount();
				functor(pParent, pNode);
				for (int i = 0; i < numChildren; ++i)
				{
					(*this)(pNode, pNode->GetChild(i));
				}
			}
		}

		TFunctor& functor;
	};
	SVisitor visitor(functor);
	visitor(nullptr, pScene->GetRootNode());
}

// Visits all node attributes of the given type TNode in the scene.
// The functor should have the signature "void functor(TAttribute* pAttribute, FbxNode* pNode)".
template<typename TAttribute, typename TFunctor>
inline void VisitNodeAttributesOfType(FbxScene* pScene, TFunctor& functor, bool bPrimaryAttributesOnly = false)
{
	auto selector = [&functor](FbxNodeAttribute* pAttribute, FbxNode* pNode)
	{
		if (pAttribute && pAttribute->Is<TAttribute>())
		{
			functor(static_cast<TAttribute*>(pAttribute), pNode);
		}
		return true;
	};
	auto visitor = [&selector, bPrimaryAttributesOnly](FbxNode* pParent, FbxNode* pNode)
	{
		if (bPrimaryAttributesOnly)
		{
			selector(pNode->GetNodeAttribute(), pNode);
		}
		else
		{
			const int numAttributes = pNode->GetNodeAttributeCount();
			for (int i = 0; i < numAttributes; ++i)
			{
				selector(pNode->GetNodeAttributeByIndex(i), pNode);
			}
		}
		return true;
	};
	VisitNodes(pScene, visitor);
}

// Visit all textures in some FbxObject, taking into account layering.
// The functor should have the signature "void functor(FbxTexture* pTexture)".
template<typename TFunctor>
inline void VisitTextures(FbxObject* pObject, TFunctor& functor)
{
	FbxTexture* const pBaseTexture = FbxCast<FbxTexture>(pObject);
	if (pBaseTexture)
	{
		functor(pBaseTexture);
		FbxLayeredTexture* const pLayeredTexture = FbxCast<FbxLayeredTexture>(pBaseTexture);
		if (pLayeredTexture)
		{
			const int numObjects = pLayeredTexture->GetSrcObjectCount();
			for (int i = 0; i < numObjects; ++i)
			{
				VisitTextures(pLayeredTexture->GetSrcObject(i), functor);
			}
		}
	}
}

// Returns the path to a node, last node can refer to the name of a mesh/material etc.
// Note: This is for user-interface/logging purposes, no escaping is performed
string GetNodePath(FbxNode* pNode, const char* szLastName = nullptr);

// Returns the path to a node, pObject can refer to a mesh/material/texture etc attached to the node
// Note: This is for user-interface/logging purposes, no escaping is performed
inline string GetNodePath(FbxNode* pNode, FbxObject* pObject)
{
	return GetNodePath(pNode, pObject->GetName());
}

template<typename T>
inline void StoreTransform(T* pResult, const FbxAMatrix& transform)
{
	FbxVector4 t = transform.GetT();
	FbxQuaternion r = transform.GetQ();
	FbxVector4 s = transform.GetS();
	pResult->translation[0] = t[0];
	pResult->translation[1] = t[1];
	pResult->translation[2] = t[2];
	pResult->translation[3] = 0.0;
	pResult->rotation[0] = r[0];
	pResult->rotation[1] = r[1];
	pResult->rotation[2] = r[2];
	pResult->rotation[3] = r[3];
	pResult->scale[0] = s[0];
	pResult->scale[1] = s[1];
	pResult->scale[2] = s[2];
	pResult->scale[3] = 0.0;
}
}

