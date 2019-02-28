// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef Animation_PoseAligner_h
#define Animation_PoseAligner_h

#include <CryAnimation/ICryAnimation.h>
#include <CryExtension/ClassWeaver.h>

namespace PoseAligner {

class CChain;
class CPose;

class CVars
{
public:
	static const CVars& GetInstance()
	{
		static CVars instance;
		return instance;
	}

public:
	CVars();

public:
	int m_enable;
	int m_debugDraw;

	int m_forceTargetSmoothing;
	int m_forceNoRootOffset;
	int m_forceNoIntersections;
	int m_forceWeightOne;
	int m_forceLock;
};

class CContactReporter :
	public _i_reference_target<int>
{
public:
	virtual bool Update(Vec3& position, Vec3& normal) = 0;
};
typedef _smart_ptr<CContactReporter> CContactReporterPtr;

class CContactRaycast :
	public CContactReporter
{
public:
	CContactRaycast(IEntity& entity);

public:
	void SetLength(float length) { m_length = length; }

	// CContactReporter
public:
	virtual bool Update(Vec3& position, Vec3& normal);

private:
	IEntity* m_pEntity;
	float    m_length;
};
typedef _smart_ptr<CContactRaycast> CContactRaycastPtr;

struct SChainDesc
{
public:
	const char*                       name;
	IAnimationPoseAlignerChain::EType eType;
	LimbIKDefinitionHandle            solver;
	int                               targetBlendJointIndex;

	Vec3                              offsetMin;
	Vec3                              offsetMax;

	bool                              bBlendProcedural;
	bool                              bForceNoIntersection;
	bool                              bTargetSmoothing;

	CContactReporterPtr               pContactReporter;
	int                               contactJointIndex;

public:
	SChainDesc() :
		name(NULL),
		eType(IAnimationPoseAlignerChain::eType_Limb),
		solver(NULL),
		targetBlendJointIndex(-1),
		offsetMin(ZERO),
		offsetMax(ZERO),
		bBlendProcedural(false),
		bForceNoIntersection(false),
		bTargetSmoothing(false),
		contactJointIndex(-1)
	{
	}

public:
	ILINE bool IsValid() const
	{
		if (!solver)
			return false;
		if (!pContactReporter)
			return false;
		if (contactJointIndex < 0)
			return false;
		return true;
	}
};

typedef _smart_ptr<CChain> CChainPtr;
class CChain :
	public _i_reference_target<int>
{
public:
	static CChainPtr Create(const SChainDesc& desc);

private:
	CChain();

public:
	void        Reset();

	void        SetTargetForceWeightOne(bool bForce) { m_bTargetForceWeightOne = bForce; }

	const Vec3& GetTargetPosition() const            { return m_targetPosition; }
	const Vec3& GetTargetPositionFiltered() const    { return m_targetPositionFiltered; }

	void        UpdateFromAnimations(ICharacterInstance& character, const QuatT& location, const float time);
	void        FindContact(const QuatT& location);
	void        FilterTargetLocation(const float time);
	float       ComputeTargetBlendValue(ISkeletonPose& skeletonPose, const float time, const float weight);
	bool        ComputeRootOffsetExtents(float& offsetMin, float& offsetMax);

	bool        SetupStoreOperators(IAnimationOperatorQueue& operatorQueue);
	bool        SetupTargetPoseModifiers(const QuatT& location, const Vec3& limitOffset, ISkeletonAnim& skeletonAnim);

	void        DrawDebug(const QuatT& location);

private:
	SChainDesc                    m_desc;

	Vec3                          m_targetPosition;
	Vec3                          m_targetPositionFiltered;
	Vec3                          m_targetPositionFilteredSmoothRate;

	Vec3                          m_targetNormal;
	Vec3                          m_targetNormalFiltered;
	Vec3                          m_targetNormalFilteredSmoothRate;

	float                         m_targetBlendWeight;
	float                         m_targetBlendWeightAnimated;
	float                         m_targetBlendWeightFiltered;
	float                         m_targetBlendWeightFilteredSmoothRate;

	Vec3                          m_targetDelta;

	bool                          m_bTargetIntersecting;
	bool                          m_bTargetLock;
	bool                          m_bTargetForceWeightOne;

	IAnimationPoseAlignerChainPtr m_pPoseAlignerChain;
	IAnimationPoseModifierPtr     m_pTargetLocation;
	QuatT                         m_targetPositionAnimation;

	Vec3                          m_animationSlopeNormal;
	Vec3                          m_animationSlopeNormalFiltered;
	Vec3                          m_animationSlopeNormalFilteredSmoothRate;
};

class CPose :
	public IAnimationPoseAligner
{
public:
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IAnimationPoseAligner)
	CRYINTERFACE_END()

public:
	CPose();
	virtual ~CPose();

public:
	bool         Initialize(IEntity& entity, ICharacterInstance* pCharacter, int rootJointIndex);
	virtual bool Initialize(IEntity& entity, ICharacterInstance* pCharacter) { return false; }
	void         Reset();
	void         Clear();

	CChainPtr    CreateChain(const SChainDesc& desc);
	const uint   GetChainCount() const { return uint(m_chains.size()); }
	CChainPtr    GetChain(uint index)  { return m_chains[index]; }
	void         RemoveAllChains();

	void         SetRootOffsetEnable(bool bEnable)                     { m_bRootOffset = bEnable; }
	void         SetRootOffsetMinMax(float offsetMin, float offsetMax) { m_rootOffsetMin = offsetMin; m_rootOffsetMax = offsetMax; }
	void         SetRootOffsetAdditional(const Vec3& offset)           { m_rootOffsetAdditional = offset; }
	void         SetRootOffsetAverage(bool bRootOffsetAverage)         { m_bRootOffsetAverage = bRootOffsetAverage; }

	void         SetBlendWeight(float weight)                          { m_blendWeight = weight; }

	void         Update(ICharacterInstance* pCharacter, const QuatT& location, const float time);

private:
	void SetupPoseModifiers(const QuatT& location);

	void DrawDebug(const QuatT& location, const Vec3& groundNormal);

public:
	IEntity*                   m_pEntity;
	ISkeletonAnim*             m_pSkeletonAnim;
	ISkeletonPose*             m_pSkeletonPose;
	IAnimationOperatorQueuePtr m_operatorQueue;

	int                        m_rootJointIndex;

	bool                       m_bRootOffset;
	bool                       m_bRootOffsetAverage;
	Vec3                       m_rootOffsetDirection;
	Vec3                       m_rootOffsetAdditional;
	float                      m_rootOffsetMin;
	float                      m_rootOffsetMax;
	float                      m_rootOffsetSmoothed;
	float                      m_rootOffsetSmoothedRate;

	DynArray<CChainPtr>        m_chains;

	float                      m_blendWeight;

	// TEMP
	bool m_bInitialized;
};

} // namespace PoseAligner

class CPoseAlignerC3 :
	PoseAligner::CPose
{
	CRYGENERATE_CLASS_GUID(CPoseAlignerC3, "AnimationPoseAlignerC3", "f5381a4c-1374-ff00-8de1-9ba730cf572b"_cry_guid)

		virtual ~CPoseAlignerC3() {}

public:
	virtual bool Initialize(IEntity& entity, ICharacterInstance* pCharacter) override;
};

#endif // Animation_PoseAligner_h
