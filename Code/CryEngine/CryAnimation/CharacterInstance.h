// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAnimation/ICryAnimation.h>
#include "AttachmentManager.h"
#include "Model.h"
#include "SkeletonEffectManager.h"
#include "SkeletonAnim.h"
#include "SkeletonPose.h"

struct AnimData;
class CCamera;
class CFacialInstance;
class CModelMesh;
class CSkeletonAnim;
class CSkeletonPose;
class CryCharManager;
struct CryParticleSpawnInfo;
namespace CharacterInstanceProcessing {
struct SStartAnimationProcessing;
}

extern f32 g_YLine;

class CRY_ALIGN(128) CCharInstance final : public ICharacterInstance
{
public:
	friend CharacterInstanceProcessing::SStartAnimationProcessing;

	~CCharInstance();

	CCharInstance(const string &strFileName, CDefaultSkeleton * pDefaultSkeleton);

	//////////////////////////////////////////////////////////
	// ICharacterInstance implementation
	//////////////////////////////////////////////////////////
	virtual int AddRef() override;
	virtual int Release() override;
	virtual int GetRefCount() const override;
	virtual ISkeletonAnim*            GetISkeletonAnim() override                 { return &m_SkeletonAnim; }
	virtual const ISkeletonAnim*      GetISkeletonAnim() const override           { return &m_SkeletonAnim; }
	virtual ISkeletonPose*            GetISkeletonPose() override                 { return &m_SkeletonPose; }
	virtual const ISkeletonPose*      GetISkeletonPose() const override           { return &m_SkeletonPose; }
	virtual IAttachmentManager*       GetIAttachmentManager() override            { return &m_AttachmentManager; }
	virtual const IAttachmentManager* GetIAttachmentManager() const override      { return &m_AttachmentManager; }
	virtual IDefaultSkeleton&       GetIDefaultSkeleton() override              { return *m_pDefaultSkeleton; }
	virtual const IDefaultSkeleton& GetIDefaultSkeleton() const override        { return *m_pDefaultSkeleton; }
	virtual IAnimationSet*          GetIAnimationSet() override                 { return m_pDefaultSkeleton->GetIAnimationSet(); }
	virtual const IAnimationSet*    GetIAnimationSet() const override           { return m_pDefaultSkeleton->GetIAnimationSet(); }
	virtual const char*             GetModelAnimEventDatabase() const override  { return m_pDefaultSkeleton->GetModelAnimEventDatabaseCStr(); }
	virtual void                    EnableStartAnimation(bool bEnable) override { m_bEnableStartAnimation = bEnable; }
	virtual void StartAnimationProcessing(const SAnimationProcessParams &params) override;
	virtual AABB                    GetAABB() const override                    { return m_SkeletonPose.GetAABB(); }
	virtual float GetExtent(EGeomForm eForm) override;
	virtual void GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const override;
	virtual CLodValue ComputeLod(int wantedLod, const SRenderingPassInfo &passInfo) override;
	virtual void Render(const SRendParams &rParams, const SRenderingPassInfo &passInfo) override;
	virtual void                   SetFlags(int nFlags) override                        { m_rpFlags = nFlags; }
	virtual int                    GetFlags() const override                            { return m_rpFlags; }
	virtual int                    GetObjectType() const override                       { return m_pDefaultSkeleton->m_ObjectType; }
	virtual const char*            GetFilePath() const override                         { return m_strFilePath.c_str(); }
	virtual SMeshLodInfo           ComputeGeometricMean() const override;
	virtual bool                   HasVertexAnimation() const override                  { return m_bHasVertexAnimation; }
	virtual IMaterial*             GetMaterial() const override                         { return GetIMaterial(); }
	virtual IMaterial*             GetIMaterial() const override                        { return m_pInstanceMaterial ? m_pInstanceMaterial.get() : m_pDefaultSkeleton->GetIMaterial(); }
	virtual void                   SetIMaterial_Instance(IMaterial* pMaterial) override { m_pInstanceMaterial = pMaterial; }
	virtual IMaterial*             GetIMaterial_Instance() const override               { return m_pInstanceMaterial; }
	virtual IRenderMesh*           GetRenderMesh() const override                       { return m_pDefaultSkeleton ? m_pDefaultSkeleton->GetIRenderMesh() : nullptr; }
	virtual phys_geometry*         GetPhysGeom(int nType) const override;
	virtual IPhysicalEntity*       GetPhysEntity() const override;
	virtual IFacialInstance*       GetFacialInstance() override;
	virtual const IFacialInstance* GetFacialInstance() const override;
	virtual void EnableFacialAnimation(bool bEnable) override;
	virtual void EnableProceduralFacialAnimation(bool bEnable) override;
	virtual void   SetPlaybackScale(f32 speed) override { m_fPlaybackScale = max(0.0f, speed); }
	virtual f32    GetPlaybackScale() const override    { return m_fPlaybackScale; }
	virtual uint32 IsCharacterVisible() const override  { return m_SkeletonPose.m_bInstanceVisible; };
	virtual void SpawnSkeletonEffect(const AnimEventInstance& animEvent, const QuatTS &entityLoc) override;
	virtual void KillAllSkeletonEffects() override;
	virtual void  SetViewdir(const Vec3& rViewdir) override                                     { m_Viewdir = rViewdir; }
	virtual float GetUniformScale() const override                                              { return m_location.s; }
	virtual void CopyPoseFrom(const ICharacterInstance &instance) override;
	virtual void  FinishAnimationComputations() override                                        { m_SkeletonAnim.FinishAnimationComputations(); }
	virtual void  SetAttachmentLocation_DEPRECATED(const QuatTS& newCharacterLocation) override { m_location = newCharacterLocation; } // TODO: Resolve this issue (has been described as "This is a hack to keep entity attachments in sync.").
	virtual void OnDetach() override;
	virtual void  HideMaster(uint32 h) override                                                 { m_bHideMaster = (h > 0); };
	virtual void GetMemoryUsage(ICrySizer * pSizer) const override;
	virtual void Serialize(TSerialize ser) override;
#ifdef EDITOR_PCDEBUGCODE
	virtual uint32 GetResetMode() const override        { return m_ResetMode; }
	virtual void   SetResetMode(uint32 rm) override     { m_ResetMode = rm > 0; }
	virtual f32    GetAverageFrameTime() const override { return g_AverageFrameTime; }
	virtual void   SetCharEditMode(uint32 m) override   { m_CharEditMode = m; }
	virtual uint32 GetCharEditMode() const override     { return m_CharEditMode; }
	virtual void DrawWireframeStatic(const Matrix34 &m34, int nLOD, uint32 color) override;
	virtual void ReloadCHRPARAMS() override;
#endif
	//////////////////////////////////////////////////////////

	CharacterInstanceProcessing::SContext* GetProcessingContext();
	void                                   ClearProcessingContext() { m_processingContext = -1; };

	void WaitForSkinningJob() const;

	void RuntimeInit(CDefaultSkeleton * pExtDefaultSkeleton);

	SSkinningData* GetSkinningData(const SRenderingPassInfo& passInfo);

	void           SetFilePath(const string& filePath) { m_strFilePath = filePath; }

	void           SetHasVertexAnimation(bool state)   { m_bHasVertexAnimation = state; }

	void SkinningTransformationsComputation(SSkinningData * pSkinningData, CDefaultSkeleton * pDefaultSkeleton, int nRenderLod, const Skeleton::CPoseData * pPoseData, f32 * pSkinningTransformationsMovement);

	void  SetWasVisible(bool wasVisible) { m_bWasVisible = wasVisible; }

	bool  GetWasVisible() const          { return m_bWasVisible; };

	int32 GetAnimationLOD() const        { return m_nAnimationLOD; }

	bool  FacialAnimationEnabled() const { return m_bFacialAnimationEnabled; }

	void SkeletonPostProcess();

	void UpdatePhysicsCGA(Skeleton::CPoseData & poseData, f32 fScale, const QuatTS &rAnimLocationNext);

	void ApplyJointVelocitiesToPhysics(IPhysicalEntity * pent, const Quat& qrot = Quat(IDENTITY), const Vec3& velHost = Vec3(ZERO));

	size_t SizeOfCharInstance() const;

	void RenderCGA(const struct SRendParams& rParams, const Matrix34 &RotTransMatrix34, const SRenderingPassInfo &passInfo);

	void RenderCHR(const struct SRendParams& rParams, const Matrix34 &RotTransMatrix34, const SRenderingPassInfo &passInfo);

	bool MotionBlurMotionCheck(uint64 nObjFlags) const;

	uint32 GetSkinningTransformationCount() const { return m_skinningTransformationsCount; }

	void BeginSkinningTransformationsComputation(SSkinningData * pSkinningData);

	void PerFrameUpdate();

private:
	// Functions that are called from Character Instance Processing
	void SetupThroughParent(const CCharInstance * pParent);
	void SetupThroughParams(const SAnimationProcessParams * pParams);

	CCharInstance(const CCharInstance &) /*= delete*/;
	void operator=(const CCharInstance&) /*= delete*/;

public:

	CAttachmentManager m_AttachmentManager;

	CSkeletonEffectManager m_skeletonEffectManager;

	CSkeletonAnim m_SkeletonAnim;

	CSkeletonPose m_SkeletonPose;

	QuatTS m_location;
	Vec3 m_Viewdir;

	string m_strFilePath;

	struct
	{
		SSkinningData* pSkinningData;
		int            nFrameID;
	} arrSkinningRendererData[3];                                                      // Triple buffered for motion blur. (These resources are owned by the renderer)

	_smart_ptr<CDefaultSkeleton> m_pDefaultSkeleton;

	_smart_ptr<CFacialInstance> m_pFacialInstance;

	uint32 m_CharEditMode;

	uint32 m_rpFlags;

	uint32 m_LastUpdateFrameID_Pre;
	uint32 m_LastUpdateFrameID_Post;

	float m_fPostProcessZoomAdjustedDistanceFromCamera;

	f32 m_fDeltaTime;
	f32 m_fOriginalDeltaTime;

	// This is the scale factor that affects the animation speed of the character.
	// All the animations are played with the constant real-time speed multiplied by this factor.
	// So, 0 means still animations (stuck at some frame), 1 - normal, 2 - twice as fast, 0.5 - twice slower than normal.
	f32 m_fPlaybackScale;

	bool m_bEnableStartAnimation;

private:

	_smart_ptr<IMaterial> m_pInstanceMaterial;

	CGeomExtents m_Extents;

	int m_nRefCounter;

	uint32 m_LastRenderedFrameID;     // Can be accessed from main thread only!

	int32 m_nAnimationLOD;

	uint32 m_ResetMode;

	uint32 m_skinningTransformationsCount;

	f32 m_skinningTransformationsMovement;

	f32 m_fZoomDistanceSq;

	bool m_bHasVertexAnimation;

	bool m_bWasVisible;

	bool m_bPlayedPhysAnim;

	bool m_bHideMaster;

	bool m_bFacialAnimationEnabled;

	int m_processingContext;

};

inline int CCharInstance::AddRef()
{
	return ++m_nRefCounter;
}

inline int CCharInstance::GetRefCount() const
{
	return m_nRefCounter;
}

inline int CCharInstance::Release()
{
	if (--m_nRefCounter == 0)
	{
		m_AttachmentManager.RemoveAllAttachments();
		delete this;
		return 0;
	}
	else if (m_nRefCounter < 0)
	{
		// Should never happens, someone tries to release CharacterInstance pointer twice.
		assert(0 && "CSkelInstance::Release");
		CryFatalError("CSkelInstance::Release");
	}
	return m_nRefCounter;
}

inline void CCharInstance::SpawnSkeletonEffect(const AnimEventInstance& animEvent, const QuatTS &entityLoc)
{
	m_skeletonEffectManager.SpawnEffect(this, animEvent, entityLoc);
}

inline void CCharInstance::KillAllSkeletonEffects()
{
	m_skeletonEffectManager.KillAllEffects();
}
