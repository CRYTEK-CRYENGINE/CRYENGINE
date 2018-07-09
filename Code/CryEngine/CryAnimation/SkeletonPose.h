// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Skeleton.h"
#include "ModelAnimationSet.h"
#include <CryMath/GeomQuery.h>
#include "Memory/Pool.h"
#include "SkeletonPhysics.h"

class CDefaultSkeleton;
class CCharInstance;
class CSkeletonAnim;
class CSkeletonPose;
struct Locator;

struct CCGAJoint
{
	CCGAJoint() : m_CGAObjectInstance(0), m_qqqhasPhysics(~0), m_pMaterial(0) {}
	~CCGAJoint()
	{
	};
	void GetMemoryUsage(ICrySizer* pSizer) const {}

	_smart_ptr<IStatObj>  m_CGAObjectInstance; //Static object controlled by this joint (this can be different for every instance).
	int                   m_qqqhasPhysics;     //>=0 if have physics (don't make it int16!!)
	_smart_ptr<IMaterial> m_pMaterial;         // custom material override
};

struct SPostProcess
{
	QuatT  m_RelativePose;
	uint32 m_JointIdx;

	SPostProcess(){};

	SPostProcess(int idx, const QuatT& qp)
	{
		m_JointIdx = idx;
		m_RelativePose = qp;
	}
	void GetMemoryUsage(ICrySizer* pSizer) const {}
};

#include "PoseModifier/DirectionalBlender.h"
#include "PoseModifier/PoseBlenderAim.h"
#include "PoseModifier/PoseBlenderLook.h"
#include "PoseModifier/FeetLock.h"

class CRY_ALIGN(128) CSkeletonPose: public ISkeletonPose
{

public:
	CSkeletonPose();
	~CSkeletonPose();

	size_t SizeOfThis();
	void GetMemoryUsage(ICrySizer * pSizer) const;

	//-----------------------------------------------------------------------------------
	//---- interface functions to access bones ----
	//-----------------------------------------------------------------------------------
	virtual const QuatT& GetAbsJointByID(int32 index) const override
	{
		const Skeleton::CPoseData& poseData = GetPoseData();
		if (index >= 0 && (uint32)index < poseData.GetJointCount())
		{
			return poseData.GetJointAbsolute(index);
		}
		else
		{
			assert(false);
			return g_IdentityQuatT;
		}
	}

	virtual const QuatT& GetRelJointByID(int32 index) const override
	{
		const Skeleton::CPoseData& poseData = GetPoseData();
		if (index >= 0 && (uint32)index < poseData.GetJointCount())
		{
			return poseData.GetJointRelative(index);
		}
		else
		{
			assert(false);
			return g_IdentityQuatT;
		}
	}

	virtual Diag33 GetAbsJointScalingByID(int32 index) const override
	{
		const Skeleton::CPoseData& poseData = GetPoseData();
		if (index >= 0 && (uint32)index < poseData.GetJointCount())
		{
			return poseData.GetJointAbsoluteS(index);
		}
		else
		{
			assert(false);
			return Diag33(1.0);
		}
	}

	virtual Diag33 GetRelJointScalingByID(int32 index) const override
	{
		const Skeleton::CPoseData& poseData = GetPoseData();
		if (index >= 0 && (uint32)index < poseData.GetJointCount())
		{
			return poseData.GetJointRelativeS(index);
		}
		else
		{
			assert(false);
			return Diag33(1.0);
		}
	}

	void InitSkeletonPose(CCharInstance * pInstance, CSkeletonAnim * pSkeletonAnim);
	void InitCGASkeleton();

	void SetDefaultPosePerInstance(bool bDataPoseForceWriteable);
	void SetDefaultPoseExecute(bool bDataPoseForceWriteable);
	void SetDefaultPose() override;

	//------------------------------------------------------------------------------------
	void SkeletonPostProcess(Skeleton::CPoseData & poseData);

	int32 m_nForceSkeletonUpdate;
	void SetForceSkeletonUpdate(int32 i) override { m_nForceSkeletonUpdate = i; };

	void UpdateBBox(uint32 update = 0);

public:

	// -------------------------------------------------------------------------
	// Implicit Pose Modifiers
	// -------------------------------------------------------------------------
	// LimbIk
	uint32 SetHumanLimbIK(const Vec3 &vWorldPos, const char* strLimb) override;
	IAnimationPoseModifierPtr m_limbIk;

	// Recoil
	void ApplyRecoilAnimation(f32 fDuration, f32 fImpact, f32 fKickIn, uint32 arms = 3) override;
	IAnimationPoseModifierPtr m_recoil;

	// FeetLock
	CFeetLock m_feetLock;

	// Look and Aim
	IAnimationPoseBlenderDirPtr m_PoseBlenderLook;
	IAnimationPoseBlenderDir*       GetIPoseBlenderLook() override       { return m_PoseBlenderLook.get(); }
	const IAnimationPoseBlenderDir* GetIPoseBlenderLook() const override { return m_PoseBlenderLook.get(); }

	IAnimationPoseBlenderDirPtr m_PoseBlenderAim;
	IAnimationPoseBlenderDir*       GetIPoseBlenderAim() override       { return m_PoseBlenderAim.get(); }
	const IAnimationPoseBlenderDir* GetIPoseBlenderAim() const override { return m_PoseBlenderAim.get(); }

public:
	int (* m_pPostProcessCallback)(ICharacterInstance*, void*);
	void* m_pPostProcessCallbackData;
	void SetPostProcessCallback(CallBackFuncType func, void* pdata) override
	{
		m_pPostProcessCallback = func;
		m_pPostProcessCallbackData = pdata;
	}

	IStatObj*       GetStatObjOnJoint(int32 nId) override;
	const IStatObj* GetStatObjOnJoint(int32 nId) const override;

	void SetStatObjOnJoint(int32 nId, IStatObj * pStatObj) override;
	void SetMaterialOnJoint(int32 nId, IMaterial* pMaterial)  override
	{
		int32 numJoints = int32(m_arrCGAJoints.size());
		if (nId >= 0 && nId < numJoints)
			m_arrCGAJoints[nId].m_pMaterial = pMaterial;
	}
	const IMaterial* GetMaterialOnJoint(int32 nId) const override
	{
		int32 numJoints = int32(m_arrCGAJoints.size());
		if (nId >= 0 && nId < numJoints)
			return m_arrCGAJoints[nId].m_pMaterial;
		return 0;
	}
	IMaterial* GetMaterialOnJoint(int32 nId) override
	{
		const CSkeletonPose* pConstThis = this;
		return const_cast<IMaterial*>(pConstThis->GetMaterialOnJoint(nId));
	}

	int GetInstanceVisible() const { return m_bFullSkeletonUpdate; };

	//just for debugging
	void DrawPose(const Skeleton::CPoseData & pose, const Matrix34 &rRenderMat34);
	void DrawBBox(const Matrix34 &rRenderMat34);
	void DrawSkeleton(const Matrix34 &rRenderMat34, uint32 shift = 0) override;
	f32 SecurityCheck();
	uint32 IsSkeletonValid();
	const AABB& GetAABB() const { return m_AABB; }
	void ExportSkeleton();

	// -------------------------------------------------------------------------
	// Physics
	// -------------------------------------------------------------------------
	IPhysicalEntity*         GetPhysEntOnJoint(int32 nId) override                                                                                                                                                  { return m_physics.GetPhysEntOnJoint(nId); }
	const IPhysicalEntity*   GetPhysEntOnJoint(int32 nId) const override                                                                                                                                            { return m_physics.GetPhysEntOnJoint(nId); }
	void                     SetPhysEntOnJoint(int32 nId, IPhysicalEntity* pPhysEnt) override                                                                                                                       { m_physics.SetPhysEntOnJoint(nId, pPhysEnt); }
	int                      GetPhysIdOnJoint(int32 nId) const override                                                                                                                                             { return m_physics.GetPhysIdOnJoint(nId); }
	void                     BuildPhysicalEntity(IPhysicalEntity* pent, f32 mass, int surface_idx, f32 stiffness_scale = 1.0f, int nLod = 0, int partid0 = 0, const Matrix34& mtxloc = Matrix34(IDENTITY)) override { m_physics.BuildPhysicalEntity(pent, mass, surface_idx, stiffness_scale, nLod, partid0, mtxloc); }
	IPhysicalEntity*         CreateCharacterPhysics(IPhysicalEntity* pHost, f32 mass, int surface_idx, f32 stiffness_scale, int nLod = 0, const Matrix34& mtxloc = Matrix34(IDENTITY)) override                     { return m_physics.CreateCharacterPhysics(pHost, mass, surface_idx, stiffness_scale, nLod, mtxloc); }
	int                      CreateAuxilaryPhysics(IPhysicalEntity* pHost, const Matrix34& mtx, int nLod = 0) override                                                                                              { return m_physics.CreateAuxilaryPhysics(pHost, mtx, nLod); }
	IPhysicalEntity*         GetCharacterPhysics() const override                                                                                                                                                   { return m_physics.GetCharacterPhysics(); }
	IPhysicalEntity*         GetCharacterPhysics(const char* pRootBoneName) const override                                                                                                                          { return m_physics.GetCharacterPhysics(pRootBoneName); }
	IPhysicalEntity*         GetCharacterPhysics(int iAuxPhys) const override                                                                                                                                       { return m_physics.GetCharacterPhysics(iAuxPhys); }
	void                     SetCharacterPhysics(IPhysicalEntity* pent) override                                                                                                                                    { m_physics.SetCharacterPhysics(pent); }
	void                     SynchronizeWithPhysicalEntity(IPhysicalEntity* pent, const Vec3& posMaster = Vec3(ZERO), const Quat& qMaster = Quat(1, 0, 0, 0)) override                                              { m_physics.SynchronizeWithPhysicalEntity(pent, posMaster, qMaster); }
	IPhysicalEntity*         RelinquishCharacterPhysics(const Matrix34& mtx, f32 stiffness = 0.0f, bool bCopyJointVelocities = false, const Vec3& velHost = Vec3(ZERO)) override                                    { return m_physics.RelinquishCharacterPhysics(mtx, stiffness, bCopyJointVelocities, velHost); }
	void                     DestroyCharacterPhysics(int iMode = 0) override                                                                                                                                        { m_physics.DestroyCharacterPhysics(iMode); }
	bool                     AddImpact(int partid, Vec3 point, Vec3 impact) override                                                                                                                                { return m_physics.AddImpact(partid, point, impact); }
	int                      TranslatePartIdToDeadBody(int partid) override                                                                                                                                         { return m_physics.TranslatePartIdToDeadBody(partid); }
	int                      GetAuxPhysicsBoneId(int iAuxPhys, int iBone = 0) const override                                                                                                                        { return m_physics.GetAuxPhysicsBoneId(iAuxPhys, iBone); }
	bool                     BlendFromRagdoll(QuatTS& location, IPhysicalEntity*& pPhysicalEntity, bool b3dof) override                                                                                             { return m_physics.BlendFromRagdoll(location, pPhysicalEntity); }
	int                      GetFallingDir() const override                                                                                                                                                         { return m_physics.GetFallingDir(); }
	int                      getBonePhysParentOrSelfIndex(int nBoneIndex, int nLod = 0) const override                                                                                                              { return m_physics.getBonePhysParentOrSelfIndex(nBoneIndex, nLod = 0); }
	int                      GetBoneSurfaceTypeId(int nBoneIndex, int nLod = 0) const override                                                                                                                      { return m_physics.GetBoneSurfaceTypeId(nBoneIndex, nLod); }
	DynArray<SJointProperty> GetJointPhysProperties_ROPE(uint32 jointIndex, int nLod) const override                                                                                                                { return m_physics.GetJointPhysProperties_ROPE(jointIndex, nLod); }
	bool                     SetJointPhysProperties_ROPE(uint32 jointIndex, int nLod, const DynArray<SJointProperty>& props) override                                                                               { return m_physics.SetJointPhysProperties_ROPE(jointIndex, nLod, props); }

	float GetExtent(EGeomForm eForm);
	void GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const;

	CSkeletonPhysics m_physics;

public:
	CCharInstance* m_pInstance;

public:
	ILINE const Skeleton::CPoseData& GetPoseData() const            { return m_poseData; }
	ILINE Skeleton::CPoseData&       GetPoseDataExplicitWriteable() { return m_poseData; }
	Skeleton::CPoseData&             GetPoseDataForceWriteable();
	Skeleton::CPoseData*             GetPoseDataWriteable();
	ILINE const Skeleton::CPoseData& GetPoseDataDefault() const { return *m_pPoseDataDefault; }

	bool PreparePoseDataAndLocatorWriteables(Memory::CPool & memoryPool);
	void SynchronizePoseDataAndLocatorWriteables();

	//private:
	f32 m_fDisplaceRadiant;
	Skeleton::CPoseData m_poseData;
	Skeleton::CPoseData m_poseDataWriteable;
	Skeleton::CPoseData* m_pPoseDataWriteable;
	Skeleton::CPoseData* m_pPoseDataDefault;

public:
	DynArray<Vec3> m_FaceAnimPosSmooth;
	DynArray<Vec3> m_FaceAnimPosSmoothRate;
	DynArray<CCGAJoint> m_arrCGAJoints;
	CGeomExtents m_Extents;
	AABB m_AABB;

	bool m_bInstanceVisible : 1;
	bool m_bFullSkeletonUpdate : 1;
	uint32 m_bAllNodesValid : 1; //True if this animation was already played once.
	bool m_bSetDefaultPoseExecute : 1;

	CSkeletonAnim* m_pSkeletonAnim;
};
