// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ModelAnimationSet.h" //embedded
#include "ModelMesh.h"         //embedded
#include "Skeleton.h"

class CFacialModel;

enum EJointFlag
{
	eJointFlag_NameHasForearm                    = BIT(1),
	eJointFlag_NameHasCalf                       = BIT(2),
	eJointFlag_NameHasPelvisOrHeadOrSpineOrThigh = BIT(3),
};

#define NODEINDEX_NONE (MAX_JOINT_AMOUNT - 1)
#define NO_ENTRY_FOUND -1
template<class T, int BUCKET_AMOUNT>
class CInt32HashMap
{
private:
	inline uint32 HashFunction(uint32 key) const
	{
		return (key * 1664525 + 1013904223) % BUCKET_AMOUNT;
	};

	struct Node
	{
		T           value;
		int32       key;
		JointIdType next;
		Node() : next(NODEINDEX_NONE) {};
	};

	struct Bucket
	{
		JointIdType firstNode;
		Bucket() : firstNode(NODEINDEX_NONE) {};
	};

	Bucket      m_buckets[BUCKET_AMOUNT];
	Node*       m_nodes;
	JointIdType m_nextFreeNode;
	int16       m_nodeAmount;
public:
	CInt32HashMap(int16 nodeAmount)
	{
		m_nodes = new Node[nodeAmount];
		m_nodeAmount = nodeAmount;
		m_nextFreeNode = 0;
	}
	~CInt32HashMap()
	{
		delete[] m_nodes;
	}

	void Clear()
	{
		for (int i = 0; i < BUCKET_AMOUNT; ++i)
		{
			m_buckets[i].firstNode = NODEINDEX_NONE;
		}
		for (int i = 0; i < m_nodeAmount; ++i)
		{
			m_nodes[i].next = NODEINDEX_NONE;
		}
		m_nextFreeNode = 0;
	}

	void Insert(uint32 key, T value)
	{
		uint32 index = HashFunction(key);
		assert(value != NODEINDEX_NONE);
		assert(m_nextFreeNode < m_nodeAmount);
		assert(index < BUCKET_AMOUNT);
		if (m_buckets[index].firstNode == NODEINDEX_NONE)
		{
			m_nodes[m_nextFreeNode].value = value;
			m_nodes[m_nextFreeNode].key = key;
			m_buckets[index].firstNode = m_nextFreeNode;
			m_nextFreeNode++;
			return;
		}
		else
		{
			JointIdType node = m_buckets[index].firstNode;
			while (1)
			{
				if (m_nodes[node].next == NODEINDEX_NONE)
				{
					m_nodes[m_nextFreeNode].value = value;
					m_nodes[m_nextFreeNode].key = key;
					m_nodes[node].next = m_nextFreeNode;
					m_nextFreeNode++;
					return;
				}
				node = m_nodes[node].next;
			}
		}
	}

	int32 Retrieve(uint32 key) const
	{
		uint32 index = HashFunction(key);
		JointIdType node = m_buckets[index].firstNode;
		while (node != NODEINDEX_NONE)
		{
			if (m_nodes[node].key == key)
			{
				return m_nodes[node].value;
			}
			node = m_nodes[node].next;
		}
		return NO_ENTRY_FOUND;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_buckets, sizeof(Bucket) * BUCKET_AMOUNT);
		pSizer->AddObject(m_nodes, sizeof(Node) * m_nodeAmount);
		pSizer->AddObject(m_nextFreeNode);
		pSizer->AddObject(m_nodeAmount);
	}
};

//----------------------------------------------------------------------

struct IdxAndName
{
	int32       m_idxJoint;
	const char* m_strJoint;
	IdxAndName()
	{
		m_idxJoint = -1;
		;
		m_strJoint = 0;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_strJoint);
	}
};
struct IKLimbType
{
	LimbIKDefinitionHandle m_nHandle;
	uint32                 m_nSolver;      //32-bit string
	uint32                 m_nInterations; //only need for iterative solvers
	f32                    m_fThreshold;   //only need for iterative solvers
	f32                    m_fStepSize;    //only need for iterative solvers
	DynArray<IdxAndName>   m_arrJointChain;
	DynArray<int16>        m_arrLimbChildren;
	DynArray<int16>        m_arrRootToEndEffector;
	IKLimbType()
	{
		m_nHandle = 0;
		m_nSolver = 0;
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_arrJointChain);
		pSizer->AddObject(m_arrLimbChildren);
		pSizer->AddObject(m_arrRootToEndEffector);
	}
};

struct SRecoilJoints
{
	const char* m_strJointName;
	int16       m_nIdx;
	int16       m_nArm;
	f32         m_fWeight;
	f32         m_fDelay;
	SRecoilJoints()
	{
		m_strJointName = 0;
		m_nIdx = -1;
		m_nArm = 0;
		m_fWeight = 0.0f;
		m_fDelay = 0.0f;
	};
	void GetMemoryUsage(ICrySizer* pSizer) const {}
};

struct ProcAdjust
{
	const char* m_strJointName;
	int16       m_nIdx;
	ProcAdjust()
	{
		m_strJointName = 0;
		m_nIdx = -1;
	};
	void GetMemoryUsage(ICrySizer* pSizer) const {}
};

struct RecoilDesc
{
	RecoilDesc()
	{
		m_weaponRightJointIndex = -1;
		m_weaponLeftJointIndex = -1;
	}
	DynArray<SRecoilJoints> m_joints;
	string                  m_ikHandleLeft;
	string                  m_ikHandleRight;
	int32                   m_weaponRightJointIndex;
	int32                   m_weaponLeftJointIndex;
};

struct PoseBlenderAimDesc
{
	PoseBlenderAimDesc()
	{
		m_error = 0;
	}

	uint32                      m_error;
	DynArray<DirectionalBlends> m_blends;
	DynArray<SJointsAimIK_Rot>  m_rotations;
	DynArray<SJointsAimIK_Pos>  m_positions;
	DynArray<ProcAdjust>        m_procAdjustments;
};

struct PoseBlenderLookDesc
{
	PoseBlenderLookDesc()
	{
		m_error = 0;
		m_eyeLimitHalfYawRadians = DEG2RAD(45);
		m_eyeLimitPitchRadiansUp = DEG2RAD(45);
		m_eyeLimitPitchRadiansDown = DEG2RAD(45);
	}

	uint32                      m_error;
	f32                         m_eyeLimitHalfYawRadians;
	f32                         m_eyeLimitPitchRadiansUp;
	f32                         m_eyeLimitPitchRadiansDown;
	string                      m_eyeAttachmentLeftName;
	string                      m_eyeAttachmentRightName;
	DynArray<DirectionalBlends> m_blends;
	DynArray<SJointsAimIK_Rot>  m_rotations;
	DynArray<SJointsAimIK_Pos>  m_positions;
};

// Animation Driven IK
struct ADIKTarget
{
	LimbIKDefinitionHandle m_nHandle;
	int32                  m_idxTarget;
	const char*            m_strTarget;
	int32                  m_idxWeight;
	const char*            m_strWeight;
	ADIKTarget()
	{
		m_nHandle = 0;
		m_idxTarget = -1;
		m_strTarget = 0;
		m_idxWeight = -1;
		m_strWeight = 0;
	}
	void GetMemoryUsage(ICrySizer* pSizer) const {}
};

//----------------------------------------------------------------------
// CDefault Skeleton
//----------------------------------------------------------------------
class CDefaultSkeleton : public IDefaultSkeleton
{
private:
	static uint s_guidLast;

	friend class CharacterManager;
public:
	CDefaultSkeleton(const char* pSkeletonFilePath, uint32 type, uint64 nCRC64 = 0);
	virtual ~CDefaultSkeleton();

	uint   GetGuid() const                         { return m_guid; }

	void   SetInstanceCounter(uint32 numInstances) { m_nInstanceCounter = numInstances; }
	void   SetKeepInMemory(bool nKiM)              { m_nKeepInMemory = nKiM; }
	uint32 GetKeepInMemory() const                 { return m_nKeepInMemory; }
	void   AddRef()
	{
		++m_nRefCounter;
		//this check will make sure that nobody can hijack a smart-pointer or manipulate the ref-counter through the interface
		if (m_nInstanceCounter != m_nRefCounter)
			CryFatalError("nRefCounter and registered Skel-Instances must be identical");
	}
	void Release()
	{
		--m_nRefCounter;
		//this check will make sure that nobody can hijack a smart-pointer or manipulate the ref-counter through the interface
		if (m_nInstanceCounter != m_nRefCounter)
			CryFatalError("CryAnimation: m_nRefCounter and m_nInstanceCounter must be identical");
		if (m_nKeepInMemory)
			return;
		if (m_nRefCounter == 0)
			delete this;
	}
	void DeleteIfNotReferenced()
	{
		if (m_nRefCounter <= 0)
			delete this;
	}
	int GetRefCounter() const
	{
		return m_nRefCounter;
	}

	virtual const char* GetModelFilePath() const                                        { return m_strFilePath.c_str(); }
	uint64              GetModelFilePathCRC64() const                                   { return m_nFilePathCRC64; }
	const char*         GetModelAnimEventDatabaseCStr() const                           { return m_strAnimEventFilePath.c_str(); }
	void                SetModelAnimEventDatabase(const string& sAnimEventDatabasePath) { m_strAnimEventFilePath = sAnimEventDatabasePath;  }
	uint32              SizeOfDefaultSkeleton() const;
	void                CopyAndAdjustSkeletonParams(const CDefaultSkeleton* pCDefaultSkeletonSrc);
	int32               RemapIdx(const CDefaultSkeleton* pCDefaultSkeletonSrc, const int32 idx);

	//-----------------------------------------------------------------------
	//-----       Interfaces to access the default-skeleton              -----
	//-----------------------------------------------------------------------
	struct SJoint
	{
		SJoint()
		{
			m_nOffsetChildren = 0;
			m_numChildren = 0;
			m_flags = 0;
			m_idxParent = -1;          //index of parent-joint
			m_numLevels = 0;
			m_nJointCRC32 = 0;
			m_nJointCRC32Lower = 0;

			m_CGAObject = 0;
			m_ObjectID = -1;
			m_NodeID = ~0;
			m_PhysInfo.pPhysGeom = 0;
			m_fMass = 0.0f;
		}

		void GetMemoryUsage(ICrySizer* pSizer) const { pSizer->AddObject(m_CGAObject); pSizer->AddObject(m_strJointName); }

		//--------------------------------------------------------------------------------

		string m_strJointName;     //the name of the joint
		uint32 m_nJointCRC32;      //case sensitive CRC32 of joint-name. Used to access case-sensitive controllers in in CAF files
		uint32 m_nJointCRC32Lower; //lower case CRC32 of joint-name.

		int32  m_nOffsetChildren;  //this is 0 if there are no children
		uint16 m_numChildren;      //how many children does this joint have
		uint16 m_flags;
		int16  m_idxParent;        //index of parent-joint. if the idx==-1 then this joint is the root. Usually this values are > 0
		uint16 m_numLevels;

		//#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-
		//--------> the rest is deprecated and will disappear sooner or later
		//#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-
		CryBonePhysics       m_PhysInfo;
		f32                  m_fMass;     //this doesn't need to be in every joint
		_smart_ptr<IStatObj> m_CGAObject; // Static object controlled by this joint.
		uint32               m_NodeID;    // CGA-node
		uint16               m_ObjectID;  // used by CGA
	};

	//! Compact helper structure used for efficient computation of joint<->controller mapping.
	struct SJointDescriptor
	{
		JointIdType id; //!< Index of the joint within its skeleton.
		uint32 crc32;   //!< Case-sensitive crc32 sum of the joint name.
	};

	virtual uint32 GetJointCount() const
	{
		return m_arrModelJoints.size();
	};

	virtual int32 GetJointIDByCRC32(uint32 crc32) const
	{
		if (m_pJointsCRCToIDMap)
			return m_pJointsCRCToIDMap->Retrieve(crc32);
		else
			return -1;
	}
	virtual uint32 GetJointCRC32ByID(int32 nJointID) const
	{
		int32 numJoints = m_arrModelJoints.size();
		if (nJointID >= 0 && nJointID < numJoints)
			return m_arrModelJoints[nJointID].m_nJointCRC32Lower;
		else
			return -1;
	}

	virtual const char* GetJointNameByID(int32 nJointID) const    // Return name of bone from bone table, return zero id nId is out of range
	{
		int32 numJoints = m_arrModelJoints.size();
		if (nJointID >= 0 && nJointID < numJoints)
			return m_arrModelJoints[nJointID].m_strJointName.c_str();
		assert("GetJointNameByID - Index out of range!");
		return ""; // invalid bone id
	}
	virtual int32 GetJointIDByName(const char* strJointName) const
	{
		uint32 crc32 = CCrc32::ComputeLowercase(strJointName);
		if (m_pJointsCRCToIDMap)
			return m_pJointsCRCToIDMap->Retrieve(crc32);
		else
			return -1;
	}

	virtual int32 GetJointParentIDByID(int32 nChildID) const
	{
		int32 numJoints = m_arrModelJoints.size();
		if (nChildID >= 0 && nChildID < numJoints)
			return m_arrModelJoints[nChildID].m_idxParent;
		assert(0);
		return -1;
	}
	virtual int32 GetControllerIDByID(int32 nJointID) const
	{
		int32 numJoints = m_arrModelJoints.size();
		if (nJointID >= 0 && nJointID < numJoints)
			return m_arrModelJoints[nJointID].m_nJointCRC32;
		else
			return -1;
	}

	virtual int32 GetJointChildrenCountByID(int32 id) const
	{
		const int32 numJoints = m_arrModelJoints.size();

		if (id >= 0 && id < numJoints)
		{
			return static_cast<int32>(m_arrModelJoints[id].m_numChildren);
		}

		return -1;
	}

	virtual int32 GetJointChildIDAtIndexByID(int32 id, uint32 childIndex) const
	{
		const int32 numChildren = GetJointChildrenCountByID(id);

		if (numChildren != -1 && childIndex < numChildren)
		{
			return id + m_arrModelJoints[id].m_nOffsetChildren + childIndex;
		}

		return -1;
	}

	virtual const QuatT& GetDefaultAbsJointByID(uint32 nJointIdx) const
	{
		uint32 jointCount = m_poseDefaultData.GetJointCount();
		if (nJointIdx < jointCount)
			return m_poseDefaultData.GetJointAbsolute(nJointIdx);
		assert(false);
		return g_IdentityQuatT;
	};
	virtual const QuatT& GetDefaultRelJointByID(uint32 nJointIdx) const
	{
		uint32 jointCount = m_poseDefaultData.GetJointCount();
		if (nJointIdx < jointCount)
			return m_poseDefaultData.GetJointRelative(nJointIdx);
		assert(false);
		return g_IdentityQuatT;
	};
	virtual const phys_geometry* GetJointPhysGeom(uint32 jointIndex) const
	{
		return m_arrModelJoints[jointIndex].m_PhysInfo.pPhysGeom;
	}
	virtual CryBonePhysics* GetJointPhysInfo(uint32 jointIndex)
	{
		return &m_arrModelJoints[jointIndex].m_PhysInfo;
	}
	virtual const DynArray<SBoneShadowCapsule>& GetShadowCapsules() const
	{
		return m_ShadowCapsulesList;
	}

	bool LoadNewSKEL(const char* szFilePath, uint32 nLoadingFlags);
	void LoadCHRPARAMS(const char* paramFileName);

	//--> setup of physical proxies
	bool                            SetupPhysicalProxies(const DynArray<PhysicalProxy>& arrPhyBoneMeshes, const DynArray<BONE_ENTITY>& arrBoneEntities, IMaterial* pIMaterial, const char* filename);

	static bool                     ParsePhysInfoProperties_ROPE(CryBonePhysics& pi, const DynArray<SJointProperty>& props);
	static DynArray<SJointProperty> GetPhysInfoProperties_ROPE(const CryBonePhysics& pi, int32 nRopeOrGrid);

	CDefaultSkeleton::SJoint*       GetParent(int32 i)
	{
		int32 pidx = m_arrModelJoints[i].m_idxParent;
		if (pidx < 0)
			return 0;
		return &m_arrModelJoints[pidx];
	}

	int32 GetLimbDefinitionIdx(LimbIKDefinitionHandle nHandle) const
	{
		uint32 numLimbTypes = m_IKLimbTypes.size();
		for (uint32 lt = 0; lt < numLimbTypes; lt++)
		{
			if (nHandle == m_IKLimbTypes[lt].m_nHandle)
				return lt;
		}
		return -1;
	}
	const IKLimbType* GetLimbDefinition(int32 index) const { return &m_IKLimbTypes[index]; }

	void              RebuildJointLookupCaches()
	{
		const int numBones = m_arrModelJoints.size();

		if (m_pJointsCRCToIDMap)
		{
			delete m_pJointsCRCToIDMap;
		}
		m_pJointsCRCToIDMap = new CInt32HashMap<JointIdType, 512>(numBones);

		for (int i = 0; i < numBones; ++i)
		{
			const uint32 crc32Lower = m_arrModelJoints[i].m_nJointCRC32Lower;
			m_pJointsCRCToIDMap->Insert(crc32Lower, i);
		}

		m_crcOrderedJointDescriptors.clear();
		m_crcOrderedJointDescriptors.reserve(numBones);
		for (int i = 0; i < numBones; ++i)
		{
			const uint32 crc32 = m_arrModelJoints[i].m_nJointCRC32;
			m_crcOrderedJointDescriptors.push_back({ JointIdType(i), crc32 });
		}
		std::sort(m_crcOrderedJointDescriptors.begin(), m_crcOrderedJointDescriptors.end(), [](const SJointDescriptor& lhs, const SJointDescriptor& rhs) { return lhs.crc32 < rhs.crc32; });
	}

	void                       InitializeHardcodedJointsProperty();
	const Skeleton::CPoseData& GetPoseData() const    { return m_poseDefaultData; }
	uint32                     SizeOfSkeleton() const { return 0; }
	void                       GetMemoryUsage(ICrySizer* pSizer) const;
	void                       VerifyHierarchy();

	DynArray<SJoint>                 m_arrModelJoints;    // This is the bone hierarchy. All the bones of the hierarchy are present in this array
	CInt32HashMap<JointIdType, 512>* m_pJointsCRCToIDMap; //this dramatically accelerates access to JointIDs by CRC - overall consumption should be less than 100 kb throughout the game (2-3 kb per ModelSkeleton)
	std::vector<SJointDescriptor>    m_crcOrderedJointDescriptors; //!< Array of joint descriptors, sorted by crc32.

	Skeleton::CPoseData              m_poseDefaultData;

	string                           m_strFeetLockIKHandle[MAX_FEET_AMOUNT];
	DynArray<IKLimbType>             m_IKLimbTypes;
	DynArray<ADIKTarget>             m_ADIKTargets;
	RecoilDesc                       m_recoilDesc;
	PoseBlenderLookDesc              m_poseBlenderLookDesc;
	PoseBlenderAimDesc               m_poseBlenderAimDesc;
	uint32                           m_usePhysProxyBBox;
	DynArray<int32>                  m_BBoxIncludeList;
	DynArray<SBoneShadowCapsule>     m_ShadowCapsulesList;
	AABB                             m_AABBExtension;
	AABB                             m_ModelAABB; //AABB of the model in default pose
	bool                             m_bHasPhysics2;
	DynArray<PhysicalProxy>          m_arrBackupPhyBoneMeshes; //collision proxi
	DynArray<BONE_ENTITY>            m_arrBackupBoneEntities;  //physical-bones
	DynArray<CryBonePhysics>         m_arrBackupPhysInfo;
	uint                             m_guid;

	//-----------------------------------------------------------------------
	// Returns the interface for animations applicable to this model
	//-----------------------------------------------------------------------
	DynArray<uint32> m_animListIDs;
	IAnimationSet*       GetIAnimationSet()       { return m_pAnimationSet; }
	const IAnimationSet* GetIAnimationSet() const { return m_pAnimationSet.get(); }
	bool LoadAnimations(class CParamLoader& paramLoader);   //loads animations by using the output of the ParamLoader
	const string         GetDefaultAnimDir();
	uint32               LoadAnimationFiles(CParamLoader& paramLoader, uint32 listID);  // load animation files that were parsed from the param file into memory (That are not in DBA)
	uint32               ReuseAnimationFiles(CParamLoader& paramLoader, uint32 listID); // files that are already in memory can be reused

	//! Returns sorted range of crc32 identifiers representing joints belonging to the given animation lod. If the returned range is empty, it signifies that LoD includes all joints.
	std::pair<const uint32*, const uint32*> FindClosestAnimationLod(const int lodValue) const;

	_smart_ptr<CAnimationSet>  m_pAnimationSet;
	DynArray<DynArray<uint32>> m_arrAnimationLOD;

	//////////////////////////////////////////////////////////////////////////
	void                CreateFacialInstance();// Called on a model after it was completely loaded.
	void                LoadFaceLib(const char* faceLibFile, const char* animDirName, CDefaultSkeleton* pDefaultSkeleton);
	CFacialModel*       GetFacialModel()       { return m_pFacialModel; }
	const CFacialModel* GetFacialModel() const { return m_pFacialModel; }

private:
	const string  m_strFilePath;
	const uint64  m_nFilePathCRC64;
	int           m_nRefCounter, m_nInstanceCounter;
	int           m_nKeepInMemory;
	CFacialModel* m_pFacialModel;
	string        m_strAnimEventFilePath;

	//#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-
	//----> CModelMeshm, CGAs and all interfaces to access the mesh and the IMaterials will be removed in the future
	//#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-
public:
	IMaterial* GetIMaterial() const
	{
		if (m_pCGA_Object)
			return m_pCGA_Object->GetMaterial();
		return m_ModelMesh.m_pIDefaultMaterial;
	}
	TRenderChunkArray* GetRenderMeshMaterials()
	{
		if (m_ModelMesh.m_pIRenderMesh)
			return &m_ModelMesh.m_pIRenderMesh->GetChunks();
		else
			return NULL;
	}
	uint32               GetModelMeshCount() const   { return 1; }
	CModelMesh*          GetModelMesh()              { return m_ModelMeshEnabled ? &m_ModelMesh : nullptr; }
	virtual void         PrecacheMesh(bool bFullUpdate, int nRoundId, int nLod);
	virtual Vec3         GetRenderMeshOffset() const { return m_ModelMesh.m_vRenderMeshOffset; };
	virtual IRenderMesh* GetIRenderMesh() const      { return m_ModelMesh.m_pIRenderMesh;  }
	virtual uint32       GetTextureMemoryUsage2(ICrySizer* pSizer = 0) const;
	virtual uint32       GetMeshMemoryUsage(ICrySizer* pSizer = 0) const;

public:

	uint32               m_ObjectType;
	_smart_ptr<IStatObj> m_pCGA_Object;

private:

	CModelMesh m_ModelMesh;
	bool       m_ModelMeshEnabled;
};
