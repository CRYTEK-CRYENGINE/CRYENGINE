// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "DialogCommon.h"  // CSceneModel
#include "SceneModelSkeleton.h"  // CSkeleton
#include "FbxScene.h"

class CSceneCHR : public MeshImporter::CSceneModel<CSceneCHR>
{
public:
	enum EAxisLimit
	{
		eAxisLimit_MinX,
		eAxisLimit_MinY,
		eAxisLimit_MinZ,
		eAxisLimit_MaxX,
		eAxisLimit_MaxY,
		eAxisLimit_MaxZ
	};

	struct SPhysicsProxy
	{
		const FbxTool::SNode* pProxyNode;
		bool bSnapToJoint;

		SPhysicsProxy()
			: pProxyNode(nullptr)
			, bSnapToJoint(false)
		{
		}
	};

	struct SJointLimits
	{
		float limits[6];
		char limitMask;

		SJointLimits()
			: limitMask(0)
		{
			for (int i = 0; i < 6; ++i)
			{
				limits[i] = 0.0f;
			}
			limitMask = 63;
		}
	};

	CSceneCHR()
	{
		m_skeleton.SetCallback([this]()
		{
			Notify();
		});
	}

	void Initialize(const FbxTool::CScene& scene)
	{
		m_skeleton.SetScene(&scene);

		m_physicsProxies.clear();
		m_physicsProxies.resize(scene.GetNodeCount());

		m_jointTransforms.clear();
		m_jointTransforms.resize(scene.GetNodeCount(), QuatT(IDENTITY));

		m_jointLimits.clear();
		m_jointLimits.resize(scene.GetNodeCount());

		AutoAssignProxies(scene);
	}

	void SetPhysicsProxyNode(const FbxTool::SNode* pJointNode, const FbxTool::SNode* pMeshNode)
	{
		CRY_ASSERT(!pMeshNode || pMeshNode->numMeshes);

		if (m_physicsProxies[pJointNode->id].pProxyNode == pMeshNode)
		{
			return;
		}

		// A particular mesh node is the proxy node of at most one joint.
		for (auto& p : m_physicsProxies)
		{
			if (p.pProxyNode == pMeshNode)
			{
				p.pProxyNode = nullptr;
			}
		}

		m_physicsProxies[pJointNode->id].pProxyNode = pMeshNode;

		sigJointProxyChanged();
	}

	const FbxTool::SNode* GetPhysicsProxyNode(const FbxTool::SNode* pJointNode) const
	{
		return m_physicsProxies[pJointNode->id].pProxyNode;
	}

	void SetPhysicsProxySnapToJoint(const FbxTool::SNode* pJointNode, bool bSnapToJoint)
	{
		if (m_physicsProxies[pJointNode->id].bSnapToJoint != bSnapToJoint)
		{
			m_physicsProxies[pJointNode->id].bSnapToJoint = bSnapToJoint;
			sigJointProxyChanged();
		}
	}

	bool GetPhysicsProxySnapToJoint(const FbxTool::SNode* pJointNode) const
	{
		return m_physicsProxies[pJointNode->id].bSnapToJoint;
	}

	QuatT GetJointTransform(const FbxTool::SNode* pJointNode) const
	{
		return m_jointTransforms[pJointNode->id];
	}

	Vec3 GetJointRotationXYZ(const FbxTool::SNode* pJointNode) const
	{
		return Vec3(RAD2DEG(Ang3::GetAnglesXYZ(Matrix33(GetJointTransform(pJointNode).q))));
	}

	void SetJointRotationXYZ(const FbxTool::SNode* pJointNode, const Vec3& v)
	{
		const QuatT qt(Quat(Ang3(DEG2RAD(v))), GetJointTransform(pJointNode).t);
		SetJointTransform(pJointNode, qt);
	}

	void SetJointTransform(const FbxTool::SNode* pJointNode, const QuatT& transform)
	{
		if (IsEquivalent(GetJointTransform(pJointNode), transform))
		{
			return;
		}

		m_jointTransforms[pJointNode->id] = transform;
	}

	bool GetJointLimit(const FbxTool::SNode* pJointNode, EAxisLimit axisLimit, float* pValue = nullptr) const
	{
		const SJointLimits& limits = m_jointLimits[pJointNode->id];
		const bool bHasLimit = limits.limitMask & (1 << axisLimit);
		if (bHasLimit && pValue)
		{
			*pValue = limits.limits[axisLimit];
		}
		return bHasLimit;
	}

	void SetJointLimit(const FbxTool::SNode* pJointNode, EAxisLimit axisLimit, float value)
	{
		float oldValue;
		if (GetJointLimit(pJointNode, axisLimit, &oldValue) && IsEquivalent(oldValue, value))
		{
			return;
		}

		SJointLimits& limits = m_jointLimits[pJointNode->id];
		limits.limitMask |= 1 << axisLimit;
		limits.limits[axisLimit] = value;

		sigJointLimitsChanged();
	}

	void ClearJointLimit(const FbxTool::SNode* pJointNode, EAxisLimit axisLimit)
	{
		if (!GetJointLimit(pJointNode, axisLimit))
		{
			return;
		}

		m_jointLimits[pJointNode->id].limitMask &= ~(1 << axisLimit);

		const float unconstrained = 1000.0f;  // A sufficiently large value is considered unconstrained.
		m_jointLimits[pJointNode->id].limits[axisLimit] = unconstrained;
		if (axisLimit <= eAxisLimit_MinZ)
		{
			m_jointLimits[pJointNode->id].limits[axisLimit] *= -1.0f;
		}

		sigJointLimitsChanged();
	}

	void AutoAssignProxies(const FbxTool::CScene& scene);

	CCrySignal<void()> sigJointProxyChanged;
	CCrySignal<void()> sigJointLimitsChanged;

	CSkeleton m_skeleton;
	std::vector<SPhysicsProxy> m_physicsProxies;  // Maps FBX scene nodes to physics proxies. Indexed by node ids.
	std::vector<QuatT> m_jointTransforms;
	std::vector<SJointLimits> m_jointLimits;
};

struct SBlockSignals
{
	SBlockSignals(CSceneCHR& sceneUserData)
		: m_sceneUserData(sceneUserData)
	{
		Swap();
	}

	~SBlockSignals()
	{
		Swap();
	}

	void Swap()
	{
		std::swap(m_oldJointProxyChanged, m_sceneUserData.sigJointProxyChanged);
		std::swap(m_oldJointDataChanged, m_sceneUserData.sigJointLimitsChanged);
	}

	CSceneCHR& m_sceneUserData;

	CCrySignal<void()> m_oldJointProxyChanged;
	CCrySignal<void()> m_oldJointDataChanged;
};


