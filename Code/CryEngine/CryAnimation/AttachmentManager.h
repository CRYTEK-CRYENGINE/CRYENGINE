// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

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
#include <CryEntitySystem/IEntity.h>
#include <unordered_map>
#include <CryCore/RingBuffer.h>
#include <queue>

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
		: m_Extents()
		, m_pSkelInstance(nullptr)
		, m_arrAttachments()
		, m_arrProxies()
		, m_nDrawProxies(0)
		, m_fTurbulenceGlobal(0.f)
		, m_fTurbulenceLocal(0.f)
		, m_attachedCharactersCache()
		, m_fZoomDistanceSq(0.f)
		, m_physAttachIds(1)
		, m_processingBuffer()
		, m_modificationCommandBuffer()
	{
		m_arrAttachments.reserve(0x20);
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

	bool NeedsHierarchicalUpdate();

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
	virtual void                SwapAttachmentObject(SAttachmentBase* pAttachment, SAttachmentBase* pNewAttachment);

	void                        PhysicalizeAttachment(int idx, int nLod, IPhysicalEntity* pent, const Vec3& offset);
	int                         UpdatePhysicalizedAttachment(int idx, IPhysicalEntity* pent, const QuatT& offset);
	int                         UpdatePhysAttachmentHideState(int idx, IPhysicalEntity* pent, const Vec3& offset);

	virtual void                PhysicalizeAttachment(int idx, IPhysicalEntity* pent = 0, int nLod = 0);
	virtual void                DephysicalizeAttachment(int idx, IPhysicalEntity* pent = 0);

	void                        Serialize(TSerialize ser);

	virtual ICharacterInstance* GetSkelInstance() const;

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

	float               GetExtent(EGeomForm eForm);
	void                GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const;
#if !defined(_RELEASE)
	float               DebugDrawAttachment(IAttachment* pAttachment, ISkin* pSkin, Vec3 drawLoc, IMaterial* pMaterial, float drawScale,const SRenderingPassInfo &passInfo);
#endif

public:

	void         ScheduleProcessingBufferRebuild() { m_processingBuffer.needsRebuild = true; }
	void         RebuildProcessingBuffer();

	size_t       SizeOfAllAttachments() const;
	void         GetMemoryUsage(ICrySizer* pSizer) const;

	template<typename TFunc>
	void ExecuteForNonemptyBoneAttachments(TFunc&& func) const { return m_processingBuffer.ExecuteForRange(SProcessingBuffer::eRange_BoneAttached, std::forward<TFunc>(func)); }

	template<typename TFunc>
	void ExecuteForSkinAttachments(TFunc&& func) const { return m_processingBuffer.ExecuteForRange(SProcessingBuffer::eRange_SkinMesh, std::forward<TFunc>(func)); }

	//! Performs a full traversal of the attachment hierarchy and generates a processing context for each encountered character instance.
	//! \return Total number of generated processing contexts.
	int GenerateAttachedCharactersContexts();

	CGeomExtents                             m_Extents;
	CCharInstance*                           m_pSkelInstance;
	std::vector<_smart_ptr<SAttachmentBase>> m_arrAttachments; //<! Attachment array accessible through the public interface.
	DynArray<CProxy>                         m_arrProxies;
	uint32 m_nDrawProxies;
	f32    m_fTurbulenceGlobal, m_fTurbulenceLocal;

	//! Returns a list of all character instances attached directly to this attachment manager.
	//! This method does not perform a full traversal of the attachment hierarchy - only the very first level will be inspected.
	const std::vector<CCharInstance*>& GetAttachedCharacterInstances();
	void ProcessAttachedCharactersChanges() { m_attachedCharactersCache.ProcessChanges(); }
private:
	struct SAttachedCharacterCache : public IEntityEventListener
	{
		SAttachedCharacterCache() = default;
		~SAttachedCharacterCache();
		SAttachedCharacterCache(const SAttachedCharacterCache&) = delete;
		SAttachedCharacterCache& operator=(const SAttachedCharacterCache&) = delete;

		virtual void OnEntityEvent(IEntity* pEntity, const SEntityEvent& event) override;

		void                               Insert(IAttachment* pAttachment);
		void                               Erase(IAttachment* pAttachment);
		const std::unordered_multimap<uint32, CCharInstance*>& GetAttachmentToCharacterMap() const { return m_attachmentCRCToCharacterMap; }
		const std::vector<CCharInstance*>& GetCharacters() const { return m_charactersCache; }

		void ProcessChanges();
	private:
		void QueueInsert(uint32 attachmentCRC, CCharInstance* pCharInstance);
		void QueueErase(uint32 attachmentCRC);
		void InsertEntityCharactersIntoCache(IEntity* pEntity);
		void RemoveEntityCharactersFromCache(EntityId entityId);
		void UpdateEntityCharactersInCache(IEntity* pEntity);

		std::unordered_multimap<uint32, CCharInstance*> m_attachmentCRCToCharacterMap;
		std::unordered_map<EntityId, uint32> m_entityToAttachmentCRCMap;
		std::vector<CCharInstance*> m_charactersCache;

		struct SCommand 
		{
			uint32 attachmentCRC = 0;
			CCharInstance* pChildInstance = nullptr;
		};

		CRingBuffer<SCommand, 8> m_commandQueue;
		std::queue<SCommand> m_fallbackCommandQueue;
	};
	
	SAttachedCharacterCache m_attachedCharactersCache; //!< Contains all of the characters that are attached to this character, either through bone attachments or the Entity system.

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
	uint32 m_physAttachIds; // bitmask for used physicalized attachment ids

	//! Internal attachment processing buffer.
	//! This structure snapshots m_arrAttachments at a synchronization point and is safe to access from job threads.
	struct SProcessingBuffer
	{
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

		struct SRange
		{
			uint16 begin;
			uint16 end;
			uint32 GetNumElements() const { return end - begin; };
		};

		void RebuildRanges(const CDefaultSkeleton& skeleton);

		template<typename TFunc>
		void ExecuteForRange(ERange range, TFunc func) const
		{
			for (size_t i = sortedRanges[range].begin; i < sortedRanges[range].end; ++i)
			{
				func(attachments[i].get());
			}
		}

		std::vector<_smart_ptr<SAttachmentBase>> attachments;
		SRange sortedRanges[eRange_COUNT];
		bool needsRebuild;

	} m_processingBuffer;

	CModificationCommandBuffer m_modificationCommandBuffer;
};
