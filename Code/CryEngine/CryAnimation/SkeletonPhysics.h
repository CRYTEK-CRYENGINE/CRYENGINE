// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Memory/Memory.h"
#include "Memory/Pool.h"
#include "Model.h"
#include "Skeleton.h"

struct CCGAJoint;
class CSkeletonPose;
class CSkeletonAnim;
class CCharInstance;

class CSkeletonPhysicsNull
{
public:
	CSkeletonPhysicsNull()
	{
		m_pCharPhysics = NULL;
		m_ppBonePhysics = NULL;
		m_timeStandingUp = 0.0f;
		;

		m_bHasPhysics = false;
		m_bHasPhysicsProxies = false;
		m_bPhysicsAwake = false;
		m_bPhysicsWasAwake = false;
		;
		m_bPhysicsRelinquished = false;
	}

public:
	IPhysicalEntity*          GetPhysEntOnJoint(int32)                                                                                                                                     { return NULL; }
	const IPhysicalEntity*    GetPhysEntOnJoint(int32) const                                                                                                                               { return NULL; }
	void                      SetPhysEntOnJoint(int32, IPhysicalEntity*)                                                                                                                   {}
	int                       GetPhysIdOnJoint(int32) const                                                                                                                                { return 0; }
	void                      BuildPhysicalEntity(IPhysicalEntity*, f32, int, f32, int, int, const Matrix34&)                                                                              {}
	IPhysicalEntity*          CreateCharacterPhysics(IPhysicalEntity*, f32, int, f32, int, const Matrix34&)                                                                                { return NULL; }
	int                       CreateAuxilaryPhysics(IPhysicalEntity*, const Matrix34&, int lod = 0)                                                                                        { return 0; }
	IPhysicalEntity*          GetCharacterPhysics(int) const                                                                                                                               { return NULL; }
	IPhysicalEntity*          GetCharacterPhysics(const char*) const                                                                                                                       { return NULL; }
	IPhysicalEntity*          GetCharacterPhysics(void) const                                                                                                                              { return NULL; }
	void                      SetCharacterPhysics(IPhysicalEntity*)                                                                                                                        {}
	void                      SynchronizeWithPhysicalEntity(IPhysicalEntity*, const Vec3&, const Quat&)                                                                                    {}
	IPhysicalEntity*          RelinquishCharacterPhysics(const Matrix34&, f32, bool, const Vec3&)                                                                                          { return NULL; }
	void                      DestroyCharacterPhysics(int)                                                                                                                                 {}
	bool                      AddImpact(int, Vec3, Vec3)                                                                                                                                   { return false; }
	int                       TranslatePartIdToDeadBody(int)                                                                                                                               { return 0; }
	int                       GetAuxPhysicsBoneId(int, int) const                                                                                                                          { return 0; }
	bool                      BlendFromRagdoll(QuatTS& location, IPhysicalEntity*& pPhysicalEntity)                                                                                        { return false; }
	int                       GetFallingDir(void) const                                                                                                                                    { return 0; }
	int                       getBonePhysParentOrSelfIndex(int, int) const                                                                                                                 { return 0; }
	int                       GetBoneSurfaceTypeId(int, int) const                                                                                                                         { return 0; }

	bool                      Initialize(CSkeletonPose& skeletonPose)                                                                                                                      { return true; }
	int                       getBonePhysParentIndex(int nBoneIndex, int nLod = 0)                                                                                                         { return 0; }
	CDefaultSkeleton::SJoint* GetModelJointPointer(int nBone)                                                                                                                              { return NULL; }
	Vec3                      GetOffset()                                                                                                                                                  { return Vec3(0.0f, 0.0f, 0.0f); }

	void                      InitPhysicsSkeleton()                                                                                                                                        {}
	void                      InitializeAnimToPhysIndexArray()                                                                                                                             {}
	int                       CreateAuxilaryPhysics(IPhysicalEntity* pHost, const Matrix34& mtx, f32 scale, Vec3 offset, int nLod)                                                         { return 0; }
	int                       FillRopeLenArray(float* arr, int i0, int sz)                                                                                                                 { return 0; }
	void                      DestroyPhysics()                                                                                                                                             {}
	void                      SetAuxParams(pe_params* pf)                                                                                                                                  {}

	void                      SynchronizeWithPhysics(Skeleton::CPoseData& poseData)                                                                                                        {}
	void                      SynchronizeWithPhysicalEntity(Skeleton::CPoseData& poseData, IPhysicalEntity* pent, const Vec3& posMaster, const Quat& qMaster, QuatT offset, int iDir = -1) {}
	void                      SynchronizeWithPhysicsPost()                                                                                                                                 {}

public:
	IPhysicalEntity*  m_pCharPhysics;
	IPhysicalEntity** m_ppBonePhysics;
	float             m_timeStandingUp;

	bool              m_bHasPhysics          : 1;
	bool              m_bHasPhysicsProxies   : 1;
	bool              m_bPhysicsAwake        : 1;
	bool              m_bPhysicsWasAwake     : 1;
	bool              m_bPhysicsRelinquished : 1;
};

struct CPhysicsJoint
{
	CPhysicsJoint() :
		m_DefaultRelativeQuat(IDENTITY),
		m_qRelPhysParent(IDENTITY),
		m_qRelFallPlay(IDENTITY)
	{
	}

	void GetMemoryUsage(ICrySizer* pSizer) const {}

	QuatT m_DefaultRelativeQuat;  //default relative joint (can be different for every instance)
	Quat  m_qRelPhysParent;       // default orientation relative to the physicalized parent
	Quat  m_qRelFallPlay;
};

struct aux_bone_info
{
	quaternionf quat0;
	Vec3        dir0;
	int         iBone;
	f32         rlen0;
};

struct aux_phys_data
{
	IPhysicalEntity* pPhysEnt;
	Vec3*            pVtx;
	Vec3*            pSubVtx;
	const char*      strName;
	aux_bone_info*   pauxBoneInfo;
	int              nChars;
	int              nBones;
	int              iBoneTiedTo[2];
	int              nSubVtxAlloc;
	bool             bPhysical;
	bool             bTied0, bTied1;
	int              iBoneStiffnessController;

	void             GetMemoryUsage(ICrySizer* pSizer) const {}
};

struct SBatchUpdateValidator : pe_action_batch_parts_update::Validator
{
	SBatchUpdateValidator()
	{
		bValid = 1;
		nRefCount = 1;
		lock = 0;
		WriteLock glock(g_lockList);
		next = prev = &g_firstValidator;
		next = g_firstValidator.next;
		g_firstValidator.next->prev = this;
		g_firstValidator.next = this;
	}
	~SBatchUpdateValidator() { WriteLock glock(g_lockList); prev->next = next; next->prev = prev; }
	int                             bValid;
	int                             nRefCount;
	volatile int                    lock;
	volatile SBatchUpdateValidator* next, * prev;
	static SBatchUpdateValidator    g_firstValidator;
	static volatile int             g_lockList;

	virtual bool Lock()
	{
		if (!bValid) { Release(); return false; }
		CryReadLock(&lock);
		return true;
	}
	virtual void Unlock()  { CryReleaseReadLock(&lock); Release(); }
	int          AddRef()  { return CryInterlockedIncrement(&nRefCount); }
	void         Release() { if (CryInterlockedDecrement(&nRefCount) <= 0) delete this; }
};

class CSkeletonPhysics
{
public:
	CSkeletonPhysics();
	~CSkeletonPhysics();

public:
	bool Initialize(CSkeletonPose& skeletonPose);

	// Helper
public:
	IPhysicalEntity* GetCharacterPhysics() const
	{
		return m_pCharPhysics;
	}
	IPhysicalEntity* GetCharacterPhysics(const char* pRootBoneName) const;
	IPhysicalEntity* GetCharacterPhysics(int iAuxPhys) const;
	void             SetCharacterPhysics(IPhysicalEntity* pent)
	{
		m_pCharPhysics = pent;
		m_timeStandingUp = -1.0f;
	}
	int                             getBonePhysParentIndex(int nBoneIndex, int nLod = 0);

	CDefaultSkeleton::SJoint*       GetModelJointPointer(int nBone);
	const CDefaultSkeleton::SJoint* GetModelJointPointer(int nBone) const;

	Vec3                            GetOffset()                        { return m_vOffset; }

	IPhysicalEntity*                GetPhysEntOnJoint(int32 nId)       { return m_ppBonePhysics ? m_ppBonePhysics[nId] : 0; }
	const IPhysicalEntity*          GetPhysEntOnJoint(int32 nId) const { return const_cast<CSkeletonPhysics*>(this)->GetPhysEntOnJoint(nId); }
	void                            SetPhysEntOnJoint(int32 nId, IPhysicalEntity* pPhysEnt);
	int                             GetPhysIdOnJoint(int32 nId) const;
	int                             GetAuxPhysicsBoneId(int iAuxPhys, int iBone = 0) const
	{
		PREFAST_SUPPRESS_WARNING(6385)
		return (iAuxPhys < m_nAuxPhys && iBone < m_auxPhys[iAuxPhys].nBones) ? m_auxPhys[iAuxPhys].pauxBoneInfo[iBone].iBone : -1;
	}
	int TranslatePartIdToDeadBody(int partid);
	int getBonePhysParentOrSelfIndex(int nBoneIndex, int nLod = 0) const;
	int GetBoneSurfaceTypeId(int nBoneIndex, int nLod) const;

private:
	int    getBonePhysChildIndex(int nBoneIndex, int nLod = 0) const;
	uint32 getBoneParentIndex(uint32 nBoneIndex) const;
	int    GetPhysRoot() const;

	int    GetModelJointChildIndex(int nBone, int i) const
	{
		int32 numChildren = int32(GetModelJointPointer(nBone)->m_numChildren);
		assert(i >= 0 && i < numChildren);
		return nBone + GetModelJointPointer(nBone)->m_nOffsetChildren + i;
	}
	int  GetPhysicsLod() const { return m_bHasPhysicsProxies ? 1 : 0; }

	void ResetNonphysicalBoneRotations(Skeleton::CPoseData& poseData, int nLod, float fBlend);
	void UnconvertBoneGlobalFromRelativeForm(Skeleton::CPoseData& poseData, bool bNonphysicalOnly, int nLod = 0, bool bRopeTipsOnly = false);

	void FindSpineBones() const;

	void ForceReskin();

	void SetOffset(Vec3 offset) { m_vOffset = offset; }

	// Initialization/creation
public:
	void                     InitPhysicsSkeleton();
	void                     InitializeAnimToPhysIndexArray();
	int                      CreateAuxilaryPhysics(IPhysicalEntity* pHost, const Matrix34& mtx, int nLod = 0);
	int                      CreateAuxilaryPhysics(IPhysicalEntity* pHost, const Matrix34& mtx, f32 scale, Vec3 offset, int nLod);
	int                      FillRopeLenArray(float* arr, int i0, int sz);
	void                     DestroyPhysics();
	void                     SetAuxParams(pe_params* pf);
	void                     BuildPhysicalEntity(IPhysicalEntity* pent, f32 mass, int surface_idx, f32 stiffness_scale, int nLod = 0, int partid0 = -1, const Matrix34& mtxloc = Matrix34(QuatT(IDENTITY)));
	IPhysicalEntity*         CreateCharacterPhysics(IPhysicalEntity* pHost, f32 mass, int surface_idx, f32 stiffness_scale, int nLod = 0, const Matrix34& mtxloc = Matrix34(QuatT(IDENTITY)));
	IPhysicalEntity*         RelinquishCharacterPhysics(const Matrix34& mtx, float stiffness, bool bCopyJointVelocities, const Vec3& velHost);
	void                     DestroyCharacterPhysics(int iMode = 0);
	bool                     AddImpact(int partid, Vec3 point, Vec3 impact);
	void                     RequestForcedPostSynchronization();

	void                     SetJointPhysInfo(uint32 iJoint, const CryBonePhysics& pi, int nLod);
	const CryBonePhysics&    GetJointPhysInfo(uint32 iJoint, int nLod) const;
	DynArray<SJointProperty> GetJointPhysProperties_ROPE(uint32 jointIndex, int nLod) const;
	bool                     SetJointPhysProperties_ROPE(uint32 jointIndex, int nLod, const DynArray<SJointProperty>& props);

	void                     SetLocation(const QuatTS& location) { m_location = location; }

private:
	void CreateRagdollDefaultPose(Skeleton::CPoseData& poseData);

public:
	bool BlendFromRagdoll(QuatTS& location, IPhysicalEntity*& pPhysicalEntity);
	int  GetFallingDir() const;

	// Execution
public:
	void Job_SynchronizeWithPhysicsPrepare(Memory::CPool& memoryPool);
	void Job_Physics_SynchronizeFrom(Skeleton::CPoseData& poseData, float timeDelta);

	void SynchronizeWithPhysics(Skeleton::CPoseData& poseData);
	void SynchronizeWithPhysicalEntity(IPhysicalEntity* pent, const Vec3& posMaster, const Quat& qMaster);
	void SynchronizeWithPhysicalEntity(Skeleton::CPoseData& poseData, IPhysicalEntity* pPhysicalEntity, const Vec3& posMaster, const Quat& qMaster, QuatT offset, int iDir = -1);
	void SynchronizeWithPhysicsPost();

private:
	void ProcessPhysics(Skeleton::CPoseData& poseData, float timeDelta);

	void Physics_SynchronizeToAux(const Skeleton::CPoseData& poseData);
	void Physics_SynchronizeToEntity(IPhysicalEntity& physicalEntity, QuatT offset);
	void Physics_SynchronizeToEntityArticulated(float fDeltaTimePhys);
	void Physics_SynchronizeToImpact(float timeDelta);

	void Job_Physics_SynchronizeFromEntityPrepare(Memory::CPool& memoryPool, IPhysicalEntity* pPhysicalEntity);
	void Job_Physics_SynchronizeFromEntity(Skeleton::CPoseData& poseData, IPhysicalEntity* pPhysicalEntity, QuatT offset);
	void Job_Physics_SynchronizeFromEntityArticulated(Skeleton::CPoseData& poseData, float timeDelta);
	void Job_Physics_SynchronizeFromAuxPrepare(Memory::CPool& memoryPool);
	void Job_Physics_SynchronizeFromAux(Skeleton::CPoseData& poseData);
	void Job_Physics_SynchronizeFromImpactPrepare(Memory::CPool& memoryPool);
	void Job_Physics_SynchronizeFromImpact(Skeleton::CPoseData& poseData, float timeDelta);

	void Job_Physics_BlendFromRagdoll(Skeleton::CPoseData& poseData, float timeDelta);
	void SetPrevHost(IPhysicalEntity* pNewHost = 0)
	{
		if (m_pPrevCharHost == pNewHost)
			return;
		if (m_pPrevCharHost)
		{
			m_pPrevCharHost->Release();
			if (m_pPrevCharHost->GetForeignData(PHYS_FOREIGN_ID_RAGDOLL) == (void*)(ICharacterInstance*)m_pInstance)
				gEnv->pPhysicalWorld->DestroyPhysicalEntity(m_pPrevCharHost);
		}
		if (m_pPrevCharHost = pNewHost)
			pNewHost->AddRef();
	}

public:
	IPhysicalEntity*  m_pCharPhysics;
	IPhysicalEntity** m_ppBonePhysics;
	float             m_timeStandingUp;

	bool              m_bBlendFromRagdollFlip : 1;
	bool              m_bHasPhysics           : 1;
	bool              m_bHasPhysicsProxies    : 1;
	bool              m_bPhysicsAwake         : 1;
	bool              m_bPhysicsWasAwake      : 1;
	bool              m_bPhysicsRelinquished  : 1;

private:
	QuatTS                  m_location;

	IPhysicalEntity*        m_pPrevCharHost;
	DynArray<CPhysicsJoint> m_arrPhysicsJoints;
	Vec3                    m_physicsJointRootPosition;

	struct PhysData
	{
		QuatT location;
		Quat  rotation;
		Ang3  angles;

		bool  bSet  : 1;
		bool  bSet2 : 1;
	}*   m_pPhysBuffer;
	bool m_bPhysicsSynchronize;
	bool m_bPhysicsSynchronizeFromEntity;
	bool m_bPhysBufferFilled;
	int  m_idLastSyncFrame = 0;

	struct PhysAuxData
	{
		QuatT matrix;
		Vec3* pPoints;

		bool  bSet : 1;
	}*   m_pPhysAuxBuffer;
	bool m_bPhysicsSynchronizeAux;

	struct PhysImpactData
	{
		Ang3 angles; // in-out
		Vec3 pivot;  // out
		Quat q0;     // out

		bool bSet : 1;
	}*                      m_pPhysImpactBuffer;

	Vec3                    m_vOffset;
	mutable int             m_iSpineBone[3];
	mutable uint32          m_nSpineBones;
	int                     m_nAuxPhys;
	int                     m_iSurfaceIdx;
	DynArray<aux_phys_data> m_auxPhys;
	f32                     m_fPhysBlendTime;
	f32                     m_fPhysBlendMaxTime;
	f32                     m_frPhysBlendMaxTime;
	float                   m_stiffnessScale;
	f32                     m_fScale;
	f32                     m_fMass;
	Vec3                    m_prevPosPivot;
	Vec3                    m_velPivot;
	DynArray<QuatT>         m_physJoints;
	DynArray<int>           m_physJointsIdx;
	int                     m_nPhysJoints;
	SBatchUpdateValidator*  m_pPhysUpdateValidator;
	struct SExtraPhysInfo
	{
		uint32         iJoint;
		int            nLod;
		CryBonePhysics info;
	};
	DynArray<SExtraPhysInfo> m_extraPhysInfo;

	// From SkeletonPose
private:
	CCharInstance*       m_pInstance;
	CSkeletonPose*       m_pSkeletonPose;
	CSkeletonAnim*       m_pSkeletonAnim;

	bool                 m_bLimpRagdoll              : 1;
	bool                 m_bSetDefaultPoseExecute    : 1;

	bool                 m_bFullSkeletonUpdate       : 1;
	bool                 m_bForcePostSynchronization : 1;

	DynArray<CCGAJoint>* m_arrCGAJoints;

	const Skeleton::CPoseData& GetPoseData() const;
	Skeleton::CPoseData&       GetPoseDataExplicitWriteable();
	Skeleton::CPoseData&       GetPoseDataForceWriteable();
	Skeleton::CPoseData*       GetPoseDataWriteable();
	const Skeleton::CPoseData& GetPoseDataDefault() const;

	int16                      GetJointIDByName(const char* szJointName) const;

public:
	size_t SizeOfThis()
	{
		size_t TotalSize = 0;
		TotalSize += m_arrPhysicsJoints.get_alloc_size();
		if (m_ppBonePhysics)
			TotalSize += GetPoseData().GetJointCount() * sizeof(IPhysicalEntity*);
		return TotalSize;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddContainer(m_arrPhysicsJoints);
		pSizer->AddContainer(m_auxPhys);
	}
};
