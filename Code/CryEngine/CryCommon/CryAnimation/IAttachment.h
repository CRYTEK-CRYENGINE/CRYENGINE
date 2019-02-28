// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef IAttachment_h
#define IAttachment_h

#include <CryAnimation/ICryAnimation.h>
#include <CryParticleSystem/IParticles.h>
#include <CryString/CryName.h>

struct IStatObj;
struct IMaterial;
struct IPhysicalEntity;
struct ICharacterInstance;
struct IAttachmentSkin;
struct IAttachment;
struct IAttachmentObject;
struct IVertexAnimation;
struct IProxy;

//! Flags used by game. DO NOT REORDER.
enum AttachmentTypes
{
	CA_BONE,
	CA_FACE,
	CA_SKIN,
	CA_PROX,
	CA_PROW,
	CA_VCLOTH,
	CA_Invalid,
};

enum AttachmentFlags
{
	//Static Flags
	FLAGS_ATTACH_HIDE_ATTACHMENT          = BIT(0),  //!< Already stored in CDF, so don't change this.
	FLAGS_ATTACH_PHYSICALIZED_RAYS        = BIT(1),  //!< Already stored in CDF, so don't change this.
	FLAGS_ATTACH_PHYSICALIZED_COLLISIONS  = BIT(2),  //!< Already stored in CDF, so don't change this.
	FLAGS_ATTACH_SW_SKINNING              = BIT(3),  //!< Already stored in CDF, so don't change this.
	FLAGS_ATTACH_RENDER_ONLY_EXISTING_LOD = BIT(4),  //!< Already stored in CDF, so don't change this.
	FLAGS_ATTACH_LINEAR_SKINNING          = BIT(5),  //!< Already stored in CDF, so don't change this.
	FLAGS_ATTACH_PHYSICALIZED             = FLAGS_ATTACH_PHYSICALIZED_RAYS | FLAGS_ATTACH_PHYSICALIZED_COLLISIONS,

	// Geometry deformation using direct compute
	FLAGS_ATTACH_COMPUTE_SKINNING           = BIT(6),  //!< Already stored in CDF, so don't change this.
	FLAGS_ATTACH_COMPUTE_SKINNING_PREMORPHS = BIT(7),  //!< Already stored in CDF, so don't change this.
	FLAGS_ATTACH_COMPUTE_SKINNING_TANGENTS  = BIT(8),  //!< Already stored in CDF, so don't change this.

	FLAGS_ATTACH_EXCLUDE_FROM_NEAREST	    = BIT(9), //!< Already stored in CDF, so don't change this.

	// Dynamic Flags.
	FLAGS_ATTACH_VISIBLE            = BIT(13),    //!< We set this flag if we can render the object.
	FLAGS_ATTACH_PROJECTED          = BIT(14),    //!< We set this flag if we can attacht the object to a triangle.
	FLAGS_ATTACH_WAS_PHYSICALIZED   = BIT(15),    //!< The attachment actually was physicalized.
	FLAGS_ATTACH_HIDE_MAIN_PASS     = BIT(16),
	FLAGS_ATTACH_HIDE_SHADOW_PASS   = BIT(17),
	FLAGS_ATTACH_HIDE_RECURSION     = BIT(18),
	FLAGS_ATTACH_NEAREST_NOFOV      = BIT(19),
	FLAGS_ATTACH_NO_BBOX_INFLUENCE  = BIT(21),
	FLAGS_ATTACH_COMBINEATTACHMENT  = BIT(24),
	FLAGS_ATTACH_ID_MASK            = BIT(25) | BIT(26) | BIT(27) | BIT(28) | BIT(29),
	FLAGS_ATTACH_MERGED_FOR_SHADOWS = BIT(30),    //!< The attachment has been merged with other attachments for shadow rendering.
};

struct SVClothParams
{
	// Animation Control
	bool  forceSkinning;                   //!< If enabled, simulation is skipped and skinning is enforced.
	float forceSkinningFpsThreshold;       //!< If the framerate drops under the provided FPS, simulation is skipped and skinning is enforced.
	float forceSkinningTranslateThreshold; //!< If the translation is larger than this value, simulation is skipped and skinning is enforced.
	bool  checkAnimationRewind;            //!< Check for rewind in animation, if enabled, the cloth is re-initialized to collision-proxies in that case.
	float disableSimulationAtDistance;     //!< Disable simulation / enable skinning in dependence of camera distance.
	float disableSimulationTimeRange;      //!< Within this time range, the fading process (skinning vs. simulation) is done.
	float enableSimulationSSaxisSizePerc;  //!< If size of characters bounding box exceeds provided percentage of viewport size, simulation is enabled.

	// Simulation and Collision
	float timeStep;                     //!< The (pseudo)fixed time step for the simulator.
	int   timeStepsMax;                 //!< Number of maximum iterations for the time discretization in a single step.
	int   numIterations;                //!< Number of iterations for the positional stiffness & collision solver (contacts & edges).
	int   collideEveryNthStep;          //!< For stiffness & collision solver: collide only every Nth step.
	float collisionMultipleShiftFactor; //!< For collision solver: if a particle collides with more than one collider at the same time, the particle is shifted by this factor in the average direction.
	float gravityFactor;

	// Stiffness and Elasticity
	float stretchStiffness;             //!< Value from 0.0 to 1.0 - smaller for less stretch stiffness, bigger for over-relaxation.
	float shearStiffness;               //!< Tweak the shear constraints.
	float bendStiffness;                //!< Tweak the bend constraints.
	float bendStiffnessByTrianglesAngle;//!< Bend stiffness depending on triangle angles, thus, the stiffness is not affecting elasticity.
	float pullStiffness;                //!< The strength of pulling vertices towards the skinned position [0..1].

	// Friction and Damping
	float friction;                     //!< Global fricion, is reducing velocity by provided factor.
	float rigidDamping;                 //!< Damping stiffness into rigid-body. 0.0 represents no damping, 1.0 represents rigid cloth.
	float springDamping;                //!< Damping of springs.
	bool  springDampingPerSubstep;      //!< Also damp springs in substeps.
	float collisionDampingTangential;   //!< Tangential damping factor in case of collisions.

	// Nearest Neighbor Distance Constraints
	bool  useNearestNeighborDistanceConstraints; //!< Enables nndc
	float nndcAllowedExtension;			//!< Allowed extension, e.g. 0.1 = 10%.
	float nndcMaximumShiftFactor;		//!< Scales maximum shift per iteration to closest neighbor, e.g. 0.5 -> half way to closest neighbor.
	float nndcShiftCollisionFactor;		//!< Scales in case of shift the velocity.

	// Test Reset Damping
	int   resetDampingRange;            //!< No of frames, within resetDampingFactor is used within simulation for dampening the system.
	float resetDampingFactor;           //!< Strength of initial damping.

	// Additional
	float translationBlend;             //!< Blending factor between local and world space translation.
	float rotationBlend;                //!< Blending factor between local and world space rotation.
	float externalBlend;                //!< Blending with world space transformation.
	float maxAnimDistance;
	float filterLaplace;

	// Debug
	bool   isMainCharacter;
	string renderMeshName;
	string renderBinding;
	string simMeshName;
	string simBinding;
	string material;
	string materialLods[g_nMaxGeomLodLevels];
	float  debugDrawVerticesRadius;
	int    debugDrawCloth;
	int    debugDrawNndc;      //!< Debug Nearest Neighbor Distance Constraints (nndc).
	int    debugPrint;

	float* weights;            //!< Per vertex weights used for blending with animation.
	bool   hide;
	bool   disableSimulation;

public:

	SVClothParams() :

		// Animation Control
		forceSkinning(false)
		, forceSkinningFpsThreshold(25.0f)
		, forceSkinningTranslateThreshold(1.0f)
		, checkAnimationRewind(true)
		, disableSimulationAtDistance(10.0)
		, disableSimulationTimeRange(0.5f)
		, enableSimulationSSaxisSizePerc(0.25f)

		// Simulation and Collision
		, timeStep(0.007f)
		, timeStepsMax(50)
		, numIterations(5)
		, collideEveryNthStep(2)
		, collisionMultipleShiftFactor(0.0f)
		, gravityFactor(0.1f)

		// Stiffness and Elasticity
		, stretchStiffness(1)
		, shearStiffness(0.3f)
		, bendStiffness(0.3f)
		, bendStiffnessByTrianglesAngle(0)
		, pullStiffness(0)

		// Friction and Damping
		, friction(0.01f)
		, rigidDamping(0.01f)
		, springDamping(0)
		, springDampingPerSubstep(true)
		, collisionDampingTangential(0)

		// Nearest Neighbor Distance Constraints
		, useNearestNeighborDistanceConstraints(false)
		, nndcAllowedExtension(0.0)     // allowed extension, e.g. 0.1 = 10%
		, nndcMaximumShiftFactor(0.25)  // scales maximum shift per iteration to closest neighbor, e.g. 0.5 -> half way to closest neighbor
		, nndcShiftCollisionFactor(1.0) // scales in case of shift the velocity, 0.0=no shift, 1.0=no velocity change, -1=increase velocity by change

		// Test Reset Damping
		, resetDampingRange(3)
		, resetDampingFactor(0)

		// Additional
		, translationBlend(0)
		, rotationBlend(0)
		, externalBlend(0)
		, maxAnimDistance(0)
		, filterLaplace(0.0f)

		// Debug
		, isMainCharacter(true)
		, renderMeshName("")
		, renderBinding("")
		, simMeshName("")
		, simBinding("")
		, material("")
		, debugDrawVerticesRadius(0.01f)
		, debugDrawCloth(1)
		, debugDrawNndc(0) // Nearest Neighbor Distance Constraints
		, debugPrint(0)

		, weights(nullptr)
		, hide(false)
		, disableSimulation(false)
	{}
};


struct SVClothAttachmentParams
{
	SVClothAttachmentParams(const char* attachmentName, const SVClothParams& vclothParams, int skinLoadingFlags, int flags)
		: attachmentName(attachmentName)
		, vclothParams(vclothParams)
		, flags(flags)
		, skinLoadingFlags(skinLoadingFlags)
	{}

	const string attachmentName;
	const SVClothParams& vclothParams;
	const int flags;
	const int skinLoadingFlags;
};

struct SimulationParams
{
	enum { MaxCollisionProxies = 10 };

	enum ClampType
	{
		DISABLED                 = 0x00,

		PENDULUM_CONE            = 0x01, //!< For pendula.
		PENDULUM_HINGE_PLANE     = 0x02, //!< For pendula.
		PENDULUM_HALF_CONE       = 0x03, //!< For pendula.
		SPRING_ELLIPSOID         = 0x04, //!< For springs.
		TRANSLATIONAL_PROJECTION = 0x05  //!< Used to project joints out of proxies.
	};

	ClampType          m_nClampType;
	bool               m_useDebugSetup;
	bool               m_useDebugText;
	bool               m_useSimulation;
	bool               m_useRedirect;
	uint8              m_nSimFPS;

	f32                m_fMaxAngle;
	f32                m_fRadius;
	Vec2               m_vSphereScale;
	Vec2               m_vDiskRotation;

	f32                m_fMass;
	f32                m_fGravity;
	f32                m_fDamping;
	f32                m_fStiffness;

	Vec3               m_vPivotOffset;
	Vec3               m_vSimulationAxis;
	Vec3               m_vStiffnessTarget;
	Vec2               m_vCapsule;
	CCryName           m_strProcFunction;

	int32              m_nProjectionType;
	CCryName           m_strDirTransJoint;
	DynArray<CCryName> m_arrProxyNames; //!< Test capsules/sphere joint against these colliders.

	SimulationParams()
	{
		m_nClampType = DISABLED;
		m_useDebugSetup = 0;
		m_useDebugText = 0;
		m_useSimulation = 1;
		m_useRedirect = 0;
		m_nSimFPS = 10;                   //!< Don't set to 0.

		m_fMaxAngle = 45.0f;
		m_vDiskRotation.set(0, 0);

		m_fRadius = 0.5f;
		m_vSphereScale.set(1.0f, 1.0f);

		m_fMass = 1.0f;
		m_fGravity = 9.81f;
		m_fDamping = 1.0f;
		m_fStiffness = 0.0f;

		m_vPivotOffset.zero();
		m_vSimulationAxis = Vec3(0.0f, 0.5f, 0.0f);
		m_vStiffnessTarget.zero();
		m_vCapsule.set(0, 0);
		m_nProjectionType = 0;
	};
};

struct RowSimulationParams
{
	enum ClampMode
	{
		PENDULUM_CONE            = 0x00, //!< For pendula.
		PENDULUM_HINGE_PLANE     = 0x01, //!< For pendula.
		PENDULUM_HALF_CONE       = 0x02, //!< For pendula.
		TRANSLATIONAL_PROJECTION = 0x03  //!< Used to project joints out of proxies.
	};

	ClampMode          m_nClampMode;
	bool               m_useDebugSetup;
	bool               m_useDebugText;
	bool               m_useSimulation;
	uint8              m_nSimFPS;

	f32                m_fConeAngle;
	Vec3               m_vConeRotation;

	f32                m_fMass;
	f32                m_fGravity;
	f32                m_fDamping;
	f32                m_fJointSpring;
	f32                m_fRodLength;
	Vec2               m_vStiffnessTarget;
	Vec2               m_vTurbulence;
	f32                m_fMaxVelocity;

	bool               m_cycle;
	f32                m_fStretch;
	uint32             m_relaxationLoops;

	Vec3               m_vTranslationAxis;
	CCryName           m_strDirTransJoint;

	Vec2               m_vCapsule;
	int32              m_nProjectionType;
	DynArray<CCryName> m_arrProxyNames;  //!< Test capsules/sphere joint against these colliders.

	RowSimulationParams()
	{
		m_nClampMode = PENDULUM_CONE;
		m_useDebugSetup = 0;
		m_useDebugText = 0;
		m_useSimulation = 1;
		m_nSimFPS = 10;

		m_fConeAngle = 45.0f;
		m_vConeRotation = Vec3(0, 0, 0);

		m_fMass = 1.0f;
		m_fGravity = 9.81f;
		m_fDamping = 1.0f;
		m_fJointSpring = 0.0f;
		m_fRodLength = 0.0f;
		m_vStiffnessTarget = Vec2(0.0f, 0.0f);
		m_vTurbulence = Vec2(0.5f, 0.0f);
		m_fMaxVelocity = 8.0f;

		m_cycle = 0;
		m_relaxationLoops = 0;
		m_fStretch = 0.10f;

		m_vCapsule.set(0, 0);
		m_nProjectionType = 0;
		m_vTranslationAxis = Vec3(0.0f, 1.0f, 0.0f);
	};
};

//! Interface for a character instance's attachment manager, responsible for keeping track of the various object attachments tied to a character
struct IAttachmentManager
{
	// <interfuscator:shuffle>
	virtual ~IAttachmentManager(){}
	virtual uint32              LoadAttachmentList(const char* pathname) = 0;

	virtual IAttachment*        CreateAttachment(const char* szName, uint32 type, const char* szJointName = 0, bool bCallProject = true) = 0;
	virtual IAttachment*        CreateVClothAttachment(const SVClothAttachmentParams& params) = 0;

	virtual int32               RemoveAttachmentByInterface(const IAttachment* pAttachment, uint32 loadingFlags = 0) = 0;
	virtual int32               RemoveAttachmentByName(const char* szName, uint32 loadingFlags = 0) = 0;
	virtual int32               RemoveAttachmentByNameCRC(uint32 nameCrc, uint32 loadingFlags = 0) = 0;

	virtual IAttachment*        GetInterfaceByName(const char* szName) const = 0;
	virtual IAttachment*        GetInterfaceByIndex(uint32 c) const = 0;
	virtual IAttachment*        GetInterfaceByNameCRC(uint32 nameCRC) const = 0;
	virtual IAttachment*        GetInterfaceByPhysId(int id) const = 0;    //!< Takes physical partid as input.

	virtual int32               GetAttachmentCount() const = 0;
	virtual int32               GetIndexByName(const char* szName) const = 0;
	virtual int32               GetIndexByNameCRC(uint32 nameCRC) const = 0;

	virtual uint32              ProjectAllAttachment() = 0;

	virtual void                PhysicalizeAttachment(int idx, IPhysicalEntity* pent = 0, int nLod = 0) = 0;
	virtual void                DephysicalizeAttachment(int idx, IPhysicalEntity* pent = 0) = 0;

	virtual ICharacterInstance* GetSkelInstance() const = 0;

	virtual int32               GetProxyCount() const = 0;
	virtual IProxy*             CreateProxy(const char* szName, const char* szJointName) = 0;
	virtual IProxy*             GetProxyInterfaceByIndex(uint32 c) const = 0;
	virtual IProxy*             GetProxyInterfaceByName(const char* szName) const = 0;
	virtual IProxy*             GetProxyInterfaceByCRC(uint32 nameCRC) const = 0;
	virtual int32               GetProxyIndexByName(const char* szName) const = 0;
	virtual int32               RemoveProxyByInterface(const IProxy* ptr) = 0;
	virtual int32               RemoveProxyByName(const char* szName) = 0;
	virtual int32               RemoveProxyByNameCRC(uint32 nameCRC) = 0;
	virtual void                DrawProxies(uint32 enable) = 0;
	virtual uint32              GetProcFunctionCount() const = 0;
	virtual const char*         GetProcFunctionName(uint32 idx) const = 0;
	// </interfuscator:shuffle>
};

//! Represents an attachment attached to a character, usually loaded from the .CDF file - but can also be created at run-time
struct IAttachment
{
	// <interfuscator:shuffle>

	virtual void        AddRef() = 0;
	virtual void        Release() = 0;

	virtual const char* GetName() const = 0;
	virtual uint32      GetNameCRC() const = 0;
	virtual uint32      ReName(const char* szSocketName, uint32 crc) = 0;

	virtual uint32      GetType() const = 0;
	bool                IsMerged() const { return (GetFlags() & FLAGS_ATTACH_MERGED_FOR_SHADOWS) != 0; }

	virtual uint32      SetJointName(const char* szJointName) = 0;

	virtual void        SetFlags(uint32 flags) = 0;
	virtual uint32      GetFlags() const = 0;

	/**
	 * Sets model-space location of this attachment in the default pose.
	 * @param rot New attachment location.
	 */
	virtual void SetAttAbsoluteDefault(const QuatT& rot) = 0;

	/**
	 * Retrieves model-space location of this attachment in the default pose.
	 * @return Attachment location.
	 */
	virtual const QuatT& GetAttAbsoluteDefault() const = 0;

	/**
	 * Sets parent-space location of this attachment in the default pose.
	 * @param mat New attachment location.
	 * IMPORTANT: This feature is currently missing. The location set by this method is being overridden by the animation
	 * update. Please compute the location manually and use the SetAttAbsoluteDefault() method instead as a workaround.
	 */
	virtual void SetAttRelativeDefault(const QuatT& mat) = 0;

	/**
	 * Retrieves parent-space location of this attachment in the default pose.
	 * @return Attachment location.
	 */
	virtual const QuatT& GetAttRelativeDefault() const = 0;

	/**
	 * Retrieves current model-space location of this attachment.
	 * @return Attachment location.
	 */
	virtual const QuatT& GetAttModelRelative() const = 0;

	/**
	 * Retrieves current world-space location of this attachment.
	 * @return Attachment location.
	 */
	virtual const QuatTS GetAttWorldAbsolute() const = 0;

	/**
	 * Retrieves the additional transformation which is applied on top of the model-space location
	 * while rendering this attachment. Typically this returns QuatT(IDENTITY), except when simulation is running.
	 * @return Additional transformation
	 */
	virtual const QuatT&       GetAdditionalTransformation() const = 0;

	virtual void               UpdateAttModelRelative() = 0;

	virtual void               HideAttachment(uint32 x) = 0;
	virtual uint32             IsAttachmentHidden() const = 0;

	virtual void               HideInRecursion(uint32 x) = 0;
	virtual uint32             IsAttachmentHiddenInRecursion() const = 0;

	virtual void               HideInShadow(uint32 x) = 0;
	virtual uint32             IsAttachmentHiddenInShadow() const = 0;

	virtual void               AlignJointAttachment() = 0;

	virtual uint32             GetJointID() const = 0;

	virtual IAttachmentObject* GetIAttachmentObject() const = 0;
	virtual IAttachmentSkin*   GetIAttachmentSkin() = 0;

	// These functions modify the attachment object that is bound to an attachment (slot)
	// They are safe to call from the main thread at any time,
	// the actual execution is defered to a time when no animation job is running for
	// the character instance
	virtual void AddBinding(IAttachmentObject* pModel, ISkin* pISkin = 0, uint32 nLoadingFlags = 0) = 0;
	virtual void ClearBinding(uint32 nLoadingFlags = 0) = 0;
	virtual void SwapBinding(IAttachment* pNewAttachment) = 0;

	// TODO: get rid of the default implementation on the following methods:
	virtual SimulationParams&    GetSimulationParams()                                                                   { static SimulationParams ap; return ap; }
	virtual void                 PostUpdateSimulationParams(bool bAttachmentSortingRequired, const char* pJointName = 0) {}
	virtual RowSimulationParams& GetRowParams()                                                                          { static RowSimulationParams ap; return ap; }
	virtual void                 SetVClothParams(const SVClothParams& params)                                            {}

	virtual size_t               SizeOfThis() const = 0;
	virtual void                 Serialize(TSerialize ser) = 0; // TODO: should be const
	virtual void                 GetMemoryUsage(ICrySizer* pSizer) const = 0;
	virtual ~IAttachment() {}

	// </interfuscator:shuffle>
};

//! Interface to a skin-attachment.
struct IAttachmentSkin
{
	// <interfuscator:shuffle>
	virtual void              AddRef() = 0;
	virtual void              Release() = 0;
	virtual ISkin*            GetISkin() = 0;
	virtual IVertexAnimation* GetIVertexAnimation() = 0;
	virtual float             GetExtent(EGeomForm eForm) = 0;
	virtual void              GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const = 0;
	virtual void              GetMemoryUsage(class ICrySizer* pSizer) const = 0;
	virtual SMeshLodInfo      ComputeGeometricMean() const = 0;
	virtual ~IAttachmentSkin(){}
	// </interfuscator:shuffle>

#ifdef EDITOR_PCDEBUGCODE
	virtual void TriggerMeshStreaming(uint32 nDesiredRenderLOD, const SRenderingPassInfo& passInfo) = 0;
	virtual void DrawWireframeStatic(const Matrix34& m34, int nLOD, uint32 color) = 0;
#endif
};

//! Represents an instance of an attachment on a character, managing the updating of the contained object (for example an IStatObj).
struct IAttachmentObject
{
	enum EType
	{
		eAttachment_Unknown,
		eAttachment_StatObj,
		eAttachment_Skeleton,
		eAttachment_SkinMesh,
		eAttachment_Entity,
		eAttachment_Light,
		eAttachment_Effect,
	};

	// <interfuscator:shuffle>
	virtual ~IAttachmentObject(){}
	virtual EType GetAttachmentType() = 0;

	virtual void  ProcessAttachment(IAttachment* pIAttachment) = 0;
	virtual void  RenderAttachment(SRendParams& rParams, const SRenderingPassInfo& passInfo) {};

	//! \return Handled state.
	virtual bool                PhysicalizeAttachment(IAttachmentManager* pManager, int idx, int nLod, IPhysicalEntity* pent, const Vec3& offset) { return false; }
	virtual bool                UpdatePhysicalizedAttachment(IAttachmentManager* pManager, int idx, IPhysicalEntity* pent, const QuatT& offset)   { return false; }

	virtual AABB                GetAABB() const = 0;
	virtual float               GetRadiusSqr() const = 0;

	virtual IStatObj*           GetIStatObj() const;
	virtual ICharacterInstance* GetICharacterInstance() const;
	virtual IAttachmentSkin*    GetIAttachmentSkin() const { return 0; };
	virtual const char*         GetObjectFilePath() const
	{
		ICharacterInstance* pICharInstance = GetICharacterInstance();
		if (pICharInstance)
			return pICharInstance->GetFilePath();
		IStatObj* pIStaticObject = GetIStatObj();
		if (pIStaticObject)
			return pIStaticObject->GetFilePath();
		IAttachmentSkin* pIAttachmentSkin = GetIAttachmentSkin();
		if (pIAttachmentSkin)
		{
			ISkin* pISkin = pIAttachmentSkin->GetISkin();
			if (pISkin)
				return pISkin->GetModelFilePath();
		}
		return NULL;
	}
	virtual IMaterial* GetBaseMaterial(uint32 nLOD = 0) const = 0;
	virtual void       SetReplacementMaterial(IMaterial* pMaterial, uint32 nLOD = 0) = 0;
	virtual IMaterial* GetReplacementMaterial(uint32 nLOD = 0) const = 0;

	virtual void       OnRemoveAttachment(IAttachmentManager* pManager, int idx) {}
	virtual void       Release() = 0;
	// </interfuscator:shuffle>
};

inline ICharacterInstance* IAttachmentObject::GetICharacterInstance() const
{
	return 0;
}

inline IStatObj* IAttachmentObject::GetIStatObj() const
{
	return 0;
}

struct IAttachmentMerger
{
	virtual ~IAttachmentMerger() {};
	virtual uint GetAllocatedBytes() const = 0;
	virtual bool IsOutOfMemory() const = 0;
	virtual uint GetMergedAttachmentsCount() const = 0;
};

//

//! Represents a static (IStatObj) object instance tied to a character
struct CCGFAttachment : public IAttachmentObject
{
	virtual EType GetAttachmentType() override                          { return eAttachment_StatObj; };
	virtual void  ProcessAttachment(IAttachment* pIAttachment) override {}
	void          RenderAttachment(SRendParams& rParams, const SRenderingPassInfo& passInfo) override
	{
		rParams.pInstance = this;
		IMaterial* pPrev = rParams.pMaterial;
		rParams.pMaterial = pObj->GetMaterial();
		if (m_pReplacementMaterial)
			rParams.pMaterial = m_pReplacementMaterial;
		pObj->Render(rParams, passInfo);
		rParams.pMaterial = pPrev;
	};

	virtual AABB  GetAABB() const override      { return pObj->GetAABB(); };
	virtual float GetRadiusSqr() const override { return sqr(pObj->GetRadius()); };
	IStatObj*     GetIStatObj() const override;
	void          Release()  override           { delete this; }

	IMaterial*    GetBaseMaterial(uint32 nLOD) const override;
	void          SetReplacementMaterial(IMaterial* pMaterial, uint32 nLOD = 0) override { m_pReplacementMaterial = pMaterial; };
	IMaterial*    GetReplacementMaterial(uint32 nLOD) const override                     { return m_pReplacementMaterial; }

	_smart_ptr<IStatObj>  pObj;
	_smart_ptr<IMaterial> m_pReplacementMaterial;
};

inline IStatObj* CCGFAttachment::GetIStatObj() const
{
	return pObj;
}

inline IMaterial* CCGFAttachment::GetBaseMaterial(uint32 nLOD) const
{
	return m_pReplacementMaterial ? m_pReplacementMaterial.get() : pObj->GetMaterial();
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//! Represents a skeleton (.skel, .chr) attached to a character
struct CSKELAttachment : public IAttachmentObject
{
	CSKELAttachment()
	{}

	virtual EType GetAttachmentType() override { return eAttachment_Skeleton; };
	void          RenderAttachment(SRendParams& rParams, const SRenderingPassInfo& passInfo) override
	{
		rParams.pInstance = this;
		IMaterial* pPrev = rParams.pMaterial;
		rParams.pMaterial = (IMaterial*)(m_pCharInstance ? m_pCharInstance->GetIMaterial() : 0);
		if (m_pReplacementMaterial)
			rParams.pMaterial = m_pReplacementMaterial;
		m_pCharInstance->Render(rParams, passInfo);
		rParams.pMaterial = pPrev;
	};
	virtual void        ProcessAttachment(IAttachment* pIAttachment)  override {}
	virtual AABB        GetAABB() const override                               { return m_pCharInstance ? m_pCharInstance->GetAABB() : AABB(AABB::RESET);  };
	virtual float       GetRadiusSqr() const override                          { return m_pCharInstance ? m_pCharInstance->GetRadiusSqr() : 0.0f;    }
	ICharacterInstance* GetICharacterInstance() const override;

	void                Release() override
	{
		if (m_pCharInstance)
			m_pCharInstance->OnDetach();
		delete this;
	}

	IMaterial* GetBaseMaterial(uint32 nLOD) const override;
	void       SetReplacementMaterial(IMaterial* pMaterial, uint32 nLOD) override { m_pReplacementMaterial = pMaterial; }
	IMaterial* GetReplacementMaterial(uint32 nLOD) const override                 { return m_pReplacementMaterial; }

	_smart_ptr<ICharacterInstance> m_pCharInstance;
	_smart_ptr<IMaterial>          m_pReplacementMaterial;
};

inline ICharacterInstance* CSKELAttachment::GetICharacterInstance() const
{
	return m_pCharInstance;
}

inline IMaterial* CSKELAttachment::GetBaseMaterial(uint32 nLOD) const
{
	return m_pReplacementMaterial ? m_pReplacementMaterial.get() : (m_pCharInstance ? m_pCharInstance->GetIMaterial() : NULL);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//! Represents a skin file (.skin) attached to a character
struct CSKINAttachment : public IAttachmentObject
{
	CSKINAttachment()
	{}

	virtual EType    GetAttachmentType() override                          { return eAttachment_SkinMesh; };
	virtual AABB     GetAABB() const override                              { return AABB(AABB::RESET); };
	virtual float    GetRadiusSqr() const override                         { return 0.0f; }
	IAttachmentSkin* GetIAttachmentSkin() const override                   { return m_pIAttachmentSkin; }
	void             Release() override                                    { delete this; }
	virtual void     ProcessAttachment(IAttachment* pIAttachment) override {};

	IMaterial*       GetBaseMaterial(uint32 nLOD) const override;
	void             SetReplacementMaterial(IMaterial* pMaterial, uint32 nLOD) override
	{
		nLOD = clamp_tpl<uint32>(nLOD, 0, g_nMaxGeomLodLevels - 1);
		m_pReplacementMaterial[nLOD] = pMaterial;
	}
	IMaterial* GetReplacementMaterial(uint32 nLOD) const override
	{
		nLOD = clamp_tpl<uint32>(nLOD, 0, g_nMaxGeomLodLevels - 1);
		return m_pReplacementMaterial[nLOD];
	}

	_smart_ptr<IAttachmentSkin> m_pIAttachmentSkin;
	_smart_ptr<IMaterial>       m_pReplacementMaterial[g_nMaxGeomLodLevels];
};

inline IMaterial* CSKINAttachment::GetBaseMaterial(uint32 nLOD) const
{
	return m_pReplacementMaterial[nLOD] ? m_pReplacementMaterial[nLOD].get() : (m_pIAttachmentSkin ? m_pIAttachmentSkin->GetISkin()->GetIMaterial(nLOD) : NULL);
}

//! Used for attaching entities to attachment slots on a character, continuously overriding the child entities transformation in the scene to match the animated joint
struct CEntityAttachment : public IAttachmentObject
{
public:
	CEntityAttachment() : m_id(0), m_scale(1.0f, 1.0f, 1.0f) {}

	virtual EType GetAttachmentType() override { return eAttachment_Entity; }
	void          SetEntityId(EntityId id)     { m_id = id; }
	EntityId      GetEntityId()                { return m_id; }

	virtual void  ProcessAttachment(IAttachment* pIAttachment) override
	{
		const QuatTS quatTS = QuatTS(pIAttachment->GetAttWorldAbsolute());
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_id);
		if (pEntity)
			pEntity->SetPosRotScale(quatTS.t, quatTS.q, m_scale * quatTS.s, ENTITY_XFORM_NO_PROPOGATE);
	}

	virtual AABB GetAABB() const override
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_id);
		AABB aabb(Vec3(0, 0, 0), Vec3(0, 0, 0));

		if (pEntity) pEntity->GetLocalBounds(aabb);
		return aabb;
	};

	virtual float GetRadiusSqr() const override
	{
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_id);
		if (pEntity)
		{
			AABB aabb;
			pEntity->GetLocalBounds(aabb);
			return aabb.GetRadiusSqr();
		}
		else
		{
			return 0.0f;
		}
	};

	void       Release() override { delete this; }

	IMaterial* GetBaseMaterial(uint32 nLOD) const override;
	void       SetReplacementMaterial(IMaterial* pMaterial, uint32 nLOD) override {}
	IMaterial* GetReplacementMaterial(uint32 nLOD) const override                 { return 0; }
	void       SetScale(const Vec3& scale)
	{
		m_scale = scale;
	}

private:
	EntityId m_id;
	Vec3     m_scale;
};

inline IMaterial* CEntityAttachment::GetBaseMaterial(uint32 nLOD) const
{
	return 0;
}

//! Allows for attaching a light source to a character
struct CLightAttachment : public IAttachmentObject
{
public:
	CLightAttachment() : m_pLightSource(0) {};
	virtual ~CLightAttachment()
	{
		if (m_pLightSource)
		{
			gEnv->p3DEngine->DeleteRenderNode(m_pLightSource);
			m_pLightSource = NULL;
		}
	};

	virtual EType GetAttachmentType() override { return eAttachment_Light; }

	void          LoadLight(const SRenderLight& light)
	{
		m_pLightSource = gEnv->p3DEngine->CreateLightSource();
		if (m_pLightSource)
			m_pLightSource->SetLightProperties(light);
	}

	ILightSource* GetLightSource() { return m_pLightSource; }

	virtual void  ProcessAttachment(IAttachment* pIAttachment) override
	{
		if (m_pLightSource)
		{
			SRenderLight& light = m_pLightSource->GetLightProperties();
			Matrix34 worldMatrix = Matrix34(pIAttachment->GetAttWorldAbsolute());
			Vec3 origin = worldMatrix.GetTranslation();
			light.SetPosition(origin);
			light.SetMatrix(worldMatrix);
			light.m_sName = pIAttachment->GetName();
			m_pLightSource->SetMatrix(worldMatrix);
			f32 r = light.m_fRadius;
			m_pLightSource->SetBBox(AABB(Vec3(origin.x - r, origin.y - r, origin.z - r), Vec3(origin.x + r, origin.y + r, origin.z + r)));
			gEnv->p3DEngine->RegisterEntity(m_pLightSource);
		}
	}

	virtual AABB GetAABB() const override
	{
		f32 r = m_pLightSource->GetLightProperties().m_fRadius;
		return AABB(Vec3(-r, -r, -r), Vec3(+r, +r, +r));
	};

	virtual float GetRadiusSqr() const override { return sqr(m_pLightSource->GetLightProperties().m_fRadius); }

	void          Release() override            { delete this; }

	IMaterial*    GetBaseMaterial(uint32 nLOD) const override;
	void          SetReplacementMaterial(IMaterial* pMaterial, uint32 nLOD) override {}
	IMaterial*    GetReplacementMaterial(uint32 nLOD) const override                 { return 0; }

private:
	ILightSource* m_pLightSource;
};

inline IMaterial* CLightAttachment::GetBaseMaterial(uint32 nLOD) const
{
	return 0;
}

//! Allows for attaching particle effects to characters
struct CEffectAttachment : public IAttachmentObject
{
	struct SParameter
	{
		SParameter() : name(), value(0) { }
		SParameter(const string& name, const IParticleAttributes::TValue& value) : name(name), value(value) { }
		string name;
		IParticleAttributes::TValue value;
	};

public:

	virtual EType GetAttachmentType() override { return eAttachment_Effect; }

	CEffectAttachment(const char* effectName, const Vec3& offset, const Vec3& dir, f32 scale = 1.0f, bool prime = false)
		: m_loc(IParticleEffect::ParticleLoc(offset, dir, scale)), m_bPrime(prime)
	{
		m_pEffect = gEnv->pParticleManager->FindEffect(effectName, "Character Attachment", true);
	}

	CEffectAttachment(IParticleEffect* pParticleEffect, const Vec3& offset, const Vec3& dir, f32 scale = 1.0f, bool prime = false)
		: m_loc(IParticleEffect::ParticleLoc(offset, dir, scale)), m_bPrime(prime)
	{
		m_pEffect = pParticleEffect;
	}

	virtual ~CEffectAttachment()
	{
		if (m_pEmitter)
			m_pEmitter->Activate(false);
	}

	IParticleEmitter* GetEmitter()
	{
		return m_pEmitter;
	}

	virtual void ProcessAttachment(IAttachment* pIAttachment) override
	{
		if (!pIAttachment->IsAttachmentHidden())
		{
			QuatTS loc = pIAttachment->GetAttWorldAbsolute() * m_loc;
			if (!m_pEmitter && m_pEffect != 0)
			{
				SpawnParams sp;
				sp.bPrime = m_bPrime;
				m_pEmitter = m_pEffect->Spawn(loc, sp);
				ApplyAttribsToEmitter(m_pEmitter);
				FreeAttribs(); // just to conserve some memory, attributes are not needed any more
			}
			else if (m_pEmitter)
				m_pEmitter->SetLocation(loc);
		}
		else
		{
			if (m_pEmitter)
			{
				m_pEmitter->Activate(false);
				m_pEmitter = 0;
			}
		}
	}

	virtual AABB GetAABB() const override
	{
		if (m_pEmitter)
		{
			AABB bb;
			m_pEmitter->GetLocalBounds(bb);
			return bb;
		}
		else
		{
			return AABB(Vec3(-0.1f), Vec3(0.1f));
		}
	};

	virtual float GetRadiusSqr() const override
	{
		if (m_pEmitter)
		{
			AABB bb;
			m_pEmitter->GetLocalBounds(bb);
			return bb.GetRadiusSqr();
		}
		else
		{
			return 0.1f;
		}
	}

	void       Release() override { delete this; }

	IMaterial* GetBaseMaterial(uint32 nLOD) const override;
	void       SetReplacementMaterial(IMaterial* pMaterial, uint32 nLOD) override {}
	IMaterial* GetReplacementMaterial(uint32 nLOD) const override                 { return 0; }

	void       SetSpawnParams(const SpawnParams& params)
	{
		if (m_pEmitter)
			m_pEmitter->SetSpawnParams(params);
	}

	void ClearParticleAttributes() { m_particleAttribs.clear(); }
	void AppendParticleAttribute(const string& name, const IParticleAttributes::TValue& value) { m_particleAttribs.emplace_back(name, value); }

private:
	void FreeAttribs()
	{
		m_particleAttribs.clear();
		m_particleAttribs.shrink_to_fit();
	}

	void ApplyAttribsToEmitter(IParticleEmitter* pEmitter) const
	{
		if (!pEmitter)
			return;

		IParticleAttributes& particleAttributes = pEmitter->GetAttributes();
		for (const SParameter& param : m_particleAttribs)
		{
			particleAttributes.SetValue(param.name.c_str(), param.value);
		}
	}

private:
	_smart_ptr<IParticleEmitter> m_pEmitter;
	_smart_ptr<IParticleEffect>  m_pEffect;
	QuatTS                       m_loc;
	bool                         m_bPrime;
	std::vector<SParameter>      m_particleAttribs;
};

inline IMaterial* CEffectAttachment::GetBaseMaterial(uint32 nLOD) const
{
	return 0;
}

struct IProxy
{
	virtual const char*  GetName() const = 0;
	virtual uint32       GetNameCRC() const = 0;
	virtual uint32       ReName(const char* szSocketName, uint32 crc) = 0;
	virtual uint32       SetJointName(const char* szJointName = 0) = 0;
	virtual uint32       GetJointID() const = 0;

	virtual const QuatT& GetProxyAbsoluteDefault() const = 0;
	virtual const QuatT& GetProxyRelativeDefault() const = 0;
	virtual const QuatT& GetProxyModelRelative() const = 0;
	virtual void         SetProxyAbsoluteDefault(const QuatT& rot) = 0;

	virtual uint32       ProjectProxy() = 0;

	virtual void         AlignProxyWithJoint() = 0;
	virtual Vec4         GetProxyParams() const = 0;
	virtual void         SetProxyParams(const Vec4& params) = 0;
	virtual int8         GetProxyPurpose() const = 0;
	virtual void         SetProxyPurpose(int8 p) = 0;
	virtual void         SetHideProxy(uint8 nHideProxy) = 0;

	virtual ~IProxy(){}
};

#endif // IAttachment_h
