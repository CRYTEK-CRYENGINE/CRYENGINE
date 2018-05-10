// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Vertex/VertexData.h"
#include "Vertex/VertexAnimation.h"
#include "ModelSkin.h"
#include "Skeleton.h"
#include "AttachmentFace.h"
#include "AttachmentBone.h"
#include "AttachmentSkin.h"
#include "AttachmentProxy.h"
#include "AttachmentMerged.h"
#include <CryMath/GeomQuery.h>

struct CharacterAttachment;
namespace Command {
class CBuffer;
}

class CAttachmentManager : public IAttachmentManager
{
	friend class CAttachmentBONE;
	friend class CAttachmentFACE;
	friend class CAttachmentSKIN;

public:
	CAttachmentManager()
	{
		m_pSkelInstance = NULL;
		m_nHaveEntityAttachments = 0;
		m_numRedirectionWithAttachment = 0;
		m_fZoomDistanceSq = 0;
		m_arrAttachments.reserve(0x20);
		m_TypeSortingRequired = 0;
		m_attachmentMergingRequired = 0;
		m_nDrawProxies = 0;
		memset(&m_sortedRanges, 0, sizeof(m_sortedRanges));
		m_fTurbulenceGlobal = 0;
		m_fTurbulenceLocal = 0;
		m_physAttachIds = 1;
	};

	uint32        LoadAttachmentList(const char* pathname);
	static uint32 ParseXMLAttachmentList(CharacterAttachment* parrAttachments, uint32 numAttachments, XmlNodeRef nodeAttachements);
	void          InitAttachmentList(const CharacterAttachment* parrAttachments, uint32 numAttachments, const string pathname, uint32 nLoadingFlags, int nKeepModelInMemory);

	IAttachment*  CreateAttachment(const char* szName, uint32 type, const char* szJointName = 0, bool bCallProject = true);
	IAttachment*  CreateVClothAttachment(const SVClothAttachmentParams& params);

	void          MergeCharacterAttachments();
	void          RequestMergeCharacterAttachments() { ++m_attachmentMergingRequired; }

	int32         GetAttachmentCount() const         { return m_arrAttachments.size(); };
	int32         GetExtraBonesCount() const         { return m_extraBones.size(); }

	int32         AddExtraBone(IAttachment* pAttachment);
	void          RemoveExtraBone(IAttachment* pAttachment);
	int32         FindExtraBone(IAttachment* pAttachment);

	void          UpdateBindings();

	IAttachment*  GetInterfaceByIndex(uint32 c) const;
	IAttachment*  GetInterfaceByName(const char* szName) const;
	IAttachment*  GetInterfaceByNameCRC(uint32 nameCRC) const;
	IAttachment*  GetInterfaceByPhysId(int id) const;

	int32         GetIndexByName(const char* szName) const;
	int32         GetIndexByNameCRC(uint32 nameCRC) const;

	void          AddEntityAttachment()
	{
		m_nHaveEntityAttachments++;
	}
	void RemoveEntityAttachment()
	{
		if (m_nHaveEntityAttachments > 0)
		{
			m_nHaveEntityAttachments--;
		}
#ifndef _RELEASE
		else
		{
			CRY_ASSERT_MESSAGE(0, "Removing too many entity attachments");
		}
#endif
	}

	bool NeedsHierarchicalUpdate();
#ifdef EDITOR_PCDEBUGCODE
	void Verification();
#endif

	void UpdateAllRemapTables();
	void PrepareAllRedirectedTransformations(Skeleton::CPoseData& pPoseData);
	void GenerateProxyModelRelativeTransformations(Skeleton::CPoseData& pPoseData);

	// Updates all attachment objects except for those labeled "execute".
	// They all are safe to update in the animation job.
	// These updates get skipped if the character is not visible (as a performance optimization).
	void UpdateLocationsExceptExecute(Skeleton::CPoseData& pPoseData);

	// for attachment objects labeled "execute", we have to distinguish between attachment
	// objects which are safe to update in the animation job and those who are not (e. g. entities)
	// Updates only "execute" attachment objects which are safe to update in the animation job
	// "execute" attachment objects are always updated (regardless of visibility)
	void           UpdateLocationsExecute(Skeleton::CPoseData& pPoseData);
	// Updates only "execute" attachments which are unsafe to update in the animation job
	void           UpdateLocationsExecuteUnsafe(Skeleton::CPoseData& pPoseData);

	void           ProcessAllAttachedObjectsFast();

	void           DrawAttachments(SRendParams& rRendParams, const Matrix34& m, const SRenderingPassInfo& passInfo, const f32 fZoomFactor, const f32 fZoomDistanceSq);
	void           DrawMergedAttachments(SRendParams& rRendParams, const Matrix34& m, const SRenderingPassInfo& passInfo, const f32 fZoomFactor, const f32 fZoomDistanceSq);

	virtual int32  RemoveAttachmentByInterface(const IAttachment* pAttachment, uint32 loadingFlags = 0);
	virtual int32  RemoveAttachmentByName(const char* szName, uint32 loadingFlags = 0);
	virtual int32  RemoveAttachmentByNameCRC(uint32 nameCrc, uint32 loadingFlags = 0);
	virtual uint32 ProjectAllAttachment();

	void           RemoveAttachmentByIndex(uint32 index, uint32 loadingFlags = 0);
	uint32         RemoveAllAttachments();

	void           CreateCommands(Command::CBuffer& buffer);

	// Attachment Object Binding
	virtual void                ClearAttachmentObject(SAttachmentBase* pAttachment, uint32 nLoadingFlags);
	virtual void                AddAttachmentObject(SAttachmentBase* pAttachment, IAttachmentObject* pModel, ISkin* pISkin = 0, uint32 nLoadingFlags = 0);
	virtual void                SwapAttachmentObject(SAttachmentBase* pAttachment, IAttachment* pNewAttachment);

	void                        PhysicalizeAttachment(int idx, int nLod, IPhysicalEntity* pent, const Vec3& offset);
	int                         UpdatePhysicalizedAttachment(int idx, IPhysicalEntity* pent, const QuatT& offset);
	int                         UpdatePhysAttachmentHideState(int idx, IPhysicalEntity* pent, const Vec3& offset);

	virtual void                PhysicalizeAttachment(int idx, IPhysicalEntity* pent = 0, int nLod = 0);
	virtual void                DephysicalizeAttachment(int idx, IPhysicalEntity* pent = 0);

	void                        OnHideAttachment(const IAttachment* pAttachment, uint32 nHideType, bool bHide);
	void                        Serialize(TSerialize ser);

	virtual ICharacterInstance* GetSkelInstance() const;
	ILINE bool                  IsFastUpdateType(IAttachmentObject::EType eAttachmentType) const
	{
		if (eAttachmentType == IAttachmentObject::eAttachment_Entity)
			return true;
		if (eAttachmentType == IAttachmentObject::eAttachment_Effect)
			return true;
		return false;
	}

	virtual IProxy*     CreateProxy(const char* szName, const char* szJointName);
	virtual int32       GetProxyCount() const { return m_arrProxies.size(); }

	virtual IProxy*     GetProxyInterfaceByIndex(uint32 c) const;
	virtual IProxy*     GetProxyInterfaceByName(const char* szName) const;
	virtual IProxy*     GetProxyInterfaceByCRC(uint32 nameCRC) const;

	virtual int32       GetProxyIndexByName(const char* szName) const;
	virtual int32       GetProxyIndexByCRC(uint32 nameCRC) const;

	virtual int32       RemoveProxyByInterface(const IProxy* ptr);
	virtual int32       RemoveProxyByName(const char* szName);
	virtual int32       RemoveProxyByNameCRC(uint32 nameCRC);
	void                RemoveProxyByIndex(uint32 n);
	virtual void        DrawProxies(uint32 enable) { (enable & 0x80) ? m_nDrawProxies &= enable : m_nDrawProxies |= enable; }
	void                VerifyProxyLinks();

	virtual uint32      GetProcFunctionCount() const { return 5; }
	virtual const char* GetProcFunctionName(uint32 idx) const;
	const char*         ExecProcFunction(uint32 nCRC32, Skeleton::CPoseData* pPoseData, const char* pstrFunction = 0) const;

	float               GetExtent(EGeomForm eForm);
	void                GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const;
#if !defined(_RELEASE)
	float               DebugDrawAttachment(IAttachment* pAttachment, ISkin* pSkin, Vec3 drawLoc, IMaterial* pMaterial, float drawScale,const SRenderingPassInfo &passInfo);
#endif

public:
	uint32 m_TypeSortingRequired;
	void         SortByType();
	size_t       SizeOfAllAttachments() const;
	void         GetMemoryUsage(ICrySizer* pSizer) const;
	ILINE uint32 GetMinJointAttachments() const  { return m_sortedRanges[eRange_BoneStatic].begin; }
	ILINE uint32 GetMaxJointAttachments() const  { return m_sortedRanges[eRange_BoneExecute].end; }
	ILINE uint32 GetRedirectedJointCount() const { return m_sortedRanges[eRange_BoneRedirect].GetNumElements(); }

	// Generates a context in the Character Manager for each attached instance
	// and returns the number of instance contexts generated
	int GenerateAttachedInstanceContexts();

	CGeomExtents                            m_Extents;
	CCharInstance*                          m_pSkelInstance;
	DynArray<_smart_ptr<SAttachmentBase>>   m_arrAttachments;
	DynArray<_smart_ptr<CAttachmentMerged>> m_mergedAttachments;
	DynArray<IAttachment*>                  m_extraBones;
	DynArray<CProxy>                        m_arrProxies;
	uint32 m_nDrawProxies;
	f32    m_fTurbulenceGlobal, m_fTurbulenceLocal;
private:
	class CModificationCommand
	{
	public:
		CModificationCommand(SAttachmentBase* pAttachment) : m_pAttachment(pAttachment) {}
		virtual ~CModificationCommand() {}
		virtual void Execute() = 0;
	protected:
		_smart_ptr<SAttachmentBase> m_pAttachment;
	};

	class CModificationCommandBuffer
	{
	public:
		static const uint32 kMaxOffsets = 4096;
		static const uint32 kMaxMemory = 4096;

		CModificationCommandBuffer() { Clear(); }
		~CModificationCommandBuffer();
		void  Execute();
		template<class T>
		T*    Alloc() { return static_cast<T*>(Alloc(sizeof(T), alignof(T))); }
	private:
		void* Alloc(size_t size, size_t align);
		void  Clear();
		stl::aligned_vector<char, 32> m_memory;
		std::vector<uint32>           m_commandOffsets;
	};

#define CMD_BUF_PUSH_COMMAND(buf, command, ...) \
  new((buf).Alloc<command>())command(__VA_ARGS__)

	f32    m_fZoomDistanceSq;
	uint32 m_nHaveEntityAttachments;
	uint32 m_numRedirectionWithAttachment;
	uint32 m_physAttachIds; // bitmask for used physicalized attachment ids
	uint32 m_attachmentMergingRequired;

	struct SRange
	{
		uint16 begin;
		uint16 end;
		uint32 GetNumElements() const { return end - begin; };
	};

	// The eRange_BoneExecute and eRange_BoneExecuteUnsafe need to stay next
	// to each other.
	// Generally, the algorithms make some assumptions on the order of
	// the ranges, so it is best not to touch them, currently.
	enum ERange
	{
		eRange_BoneRedirect,
		eRange_BoneEmpty,
		eRange_BoneStatic,
		eRange_BoneExecute,
		eRange_BoneExecuteUnsafe,
		eRange_FaceEmpty,
		eRange_FaceStatic,
		eRange_FaceExecute,
		eRange_FaceExecuteUnsafe,
		eRange_SkinMesh,
		eRange_VertexClothOrPendulumRow,

		eRange_COUNT
	};

	SRange                     m_sortedRanges[eRange_COUNT];

	CModificationCommandBuffer m_modificationCommandBuffer;
};
