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
		m_nDrawProxies = 0;
		memset(&m_sortedRanges, 0, sizeof(m_sortedRanges));
		m_fTurbulenceGlobal = 0;
		m_fTurbulenceLocal = 0;
		m_physAttachIds = 1;
	};

	//! This has been introduced as a proxy for IAttachment::ProcessAttachment(), implementations of which reside in ICryAnimation.h header, to speed up iteration times.
	//! TODO: Should be removed before shipping out to main.
	void          ProcessAttachment(IAttachment* pSocket);

	uint32        LoadAttachmentList(const char* pathname);
	static uint32 ParseXMLAttachmentList(CharacterAttachment* parrAttachments, uint32 numAttachments, XmlNodeRef nodeAttachements);
	void          InitAttachmentList(const CharacterAttachment* parrAttachments, uint32 numAttachments, const string pathname, uint32 nLoadingFlags, int nKeepModelInMemory);

	IAttachment*  CreateAttachment(const char* szName, uint32 type, const char* szJointName = 0, bool bCallProject = true);
	IAttachment*  CreateVClothAttachment(const SVClothAttachmentParams& params);

	int32         GetAttachmentCount() const         { return m_arrAttachments.size(); };

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
	void Verification();

	void UpdateAllRemapTables();
	void PrepareAllRedirectedTransformations(Skeleton::CPoseData& pPoseData);
	void GenerateProxyModelRelativeTransformations(Skeleton::CPoseData& pPoseData);

	//! Updates attachment sockets.
	//! This part of attachment update is performed in an animation job thread and handles primarily socket transform updates (including simulation).
	void           UpdateSockets(Skeleton::CPoseData& pPoseData);

	//! Updates objects attached to sockets.
	//! This part of attachment update is performed in the main thread and lets IAttachmentObject implementations consume results of UpdateAttachments().
	void           UpdateAttachedObjects();

	void           DrawAttachments(SRendParams& rRendParams, const Matrix34& m, const SRenderingPassInfo& passInfo, const f32 fZoomFactor, const f32 fZoomDistanceSq);

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
	ILINE uint32 GetMinJointAttachments() const  { return m_sortedRanges[eRange_BoneAttached].begin; }
	ILINE uint32 GetMaxJointAttachments() const  { return m_sortedRanges[eRange_BoneAttached].end; }
	ILINE uint32 GetRedirectedJointCount() const { return m_sortedRanges[eRange_BoneRedirect].GetNumElements(); }

	//! Performs a full traversal of the attachment hierarchy and generates a processing context for each encountered character instance.
	//! \return Total number of generated processing contexts.
	int GenerateAttachedCharactersContexts();

	CGeomExtents                            m_Extents;
	CCharInstance*                          m_pSkelInstance;
	DynArray<_smart_ptr<SAttachmentBase>>   m_arrAttachments;
	DynArray<CProxy>                        m_arrProxies;
	uint32 m_nDrawProxies;
	f32    m_fTurbulenceGlobal, m_fTurbulenceLocal;

	//! Returns a list of all character instances attached directly to this attachment manager.
	//! This method does not perform a full traversal of the attachment hierarchy - only the very first level will be inspected.
	const std::vector<CCharInstance*>& GetAttachedCharacterInstances();

private:

	//! Performs rebuild of m_attachedCharactersCache, if necessary.
	void RebuildAttachedCharactersCache();

	struct sAttachedCharactersCache
	{
		void Push(CCharInstance* character, IAttachment* attachment);
		void Clear();
		void Erase(IAttachment* attachment);
		uint32 FrameId() const { return m_frameId; }
		void SetFrameId(uint32 frameId) { m_frameId = frameId; }
		const std::vector<CCharInstance*>& Characters() const { return m_characters; }
		const std::vector<IAttachment*>& Attachments() const { return m_attachments; }
		IAttachment* Attachment(int idx) const { return m_attachments[idx]; }
		CCharInstance* Characters(int idx) const { return m_characters[idx]; }
	private:
		std::vector<CCharInstance*> m_characters;                 //!< List of character instances attached directly to this attachment manager.
		std::vector<IAttachment*> m_attachments;                  //!< List of attachments containing character instances stored in the characters vector above. These two vectors match by index.
		uint32 m_frameId = 0xffffffff;                            //!< Frame identifier of the last update, used to determine if a cache rebuild is needed.
	};
	sAttachedCharactersCache m_attachedCharactersCache;

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

	struct SRange
	{
		uint16 begin;
		uint16 end;
		uint32 GetNumElements() const { return end - begin; };
	};

	// Take precautions when modifying, certain systems make assumptions on the relative order of these ranges.
	enum ERange
	{
		eRange_BoneRedirect,
		eRange_BoneEmpty,
		eRange_BoneAttached,
		eRange_FaceEmpty,
		eRange_FaceAttached,
		eRange_SkinMesh,
		eRange_VertexClothOrPendulumRow,

		eRange_COUNT
	};

	SRange                     m_sortedRanges[eRange_COUNT];

	CModificationCommandBuffer m_modificationCommandBuffer;
};
