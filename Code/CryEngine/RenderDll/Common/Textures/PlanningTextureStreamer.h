// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef PLANNINGTEXTURESTREAMER_H
#define PLANNINGTEXTURESTREAMER_H

#include "ITextureStreamer.h"

//#define TEXSTRM_DEFER_UMR

struct SPlanningMemoryState
{
	ptrdiff_t nMemStreamed;
	ptrdiff_t nStaticTexUsage;
	ptrdiff_t nPhysicalLimit;
	ptrdiff_t nTargetPhysicalLimit;
	ptrdiff_t nMemLimit;
	ptrdiff_t nMemFreeSlack;
	ptrdiff_t nUnknownPoolUsage;
	ptrdiff_t nMemBoundStreamed;
	ptrdiff_t nMemBoundStreamedPers;
	ptrdiff_t nMemTemp;
	ptrdiff_t nMemFreeLower;
	ptrdiff_t nMemFreeUpper;
	ptrdiff_t nStreamLimit;
	ptrdiff_t nStreamMid;
	ptrdiff_t nStreamDelta;
};

struct SPlanningUMRState
{
	int32 arrRoundIds[MAX_PREDICTION_ZONES];
};

struct SPlanningAction
{
	enum
	{
		Unknown,
		Relink,
		Abort,
	};

	SPlanningAction()
		: eAction(Unknown)
	{
	}

	SPlanningAction(int eAction, size_t nTexture, uint8 nMip = 0)
		: nTexture((uint16)nTexture)
		, nMip(nMip)
		, eAction((uint8)eAction)
	{
	}

	uint16 nTexture;
	uint8  nMip;
	uint8  eAction;
};

typedef DynArray<std::pair<CTexture*, int>> TPlanningTextureReqVec;
typedef DynArray<SPlanningAction>           TPlanningActionVec;

struct SPlanningSortState
{
	// In
	TStreamerTextureVec* pTextures;
	size_t               nStreamLimit;
	int32                arrRoundIds[MAX_PREDICTION_ZONES];
	int                  nFrameId;
	int                  nBias;
	int                  fpMinBias;
	int                  fpMaxBias;
	int                  fpMinMip;
	SPlanningMemoryState memState;

	// In/Out
	size_t nTextures;

	// Out
	size_t                  nBalancePoint;
	size_t                  nOnScreenPoint;
	size_t                  nPrecachedTexs;
	size_t                  nListSize;
	TPlanningTextureReqVec* pRequestList;
	TStreamerTextureVec*    pTrimmableList;
	TStreamerTextureVec*    pUnlinkList;
	TPlanningActionVec*     pActionList;
};

struct SPlanningScheduleState
{
	int                    nFrameId;

	int                    nBias;
	SPlanningMemoryState   memState;

	TPlanningTextureReqVec requestList;
	TStreamerTextureVec    trimmableList;
	TStreamerTextureVec    unlinkList;
	TPlanningActionVec     actionList;
	size_t                 nBalancePoint;
	size_t                 nOnScreenPoint;
};

struct SPlanningUpdateMipRequest
{
	CTexture* pTexture;
	float     fMipFactor;
	int       nFlags;
	int       nUpdateId;
};

struct SPlanningTextureOrderKey
{
	// 1 force stream high res
	// 1 high priority
	// 1 is visible
	// 1 is in zone[0]
	// 1 is in zone[1]
	// 16 fp min mip cur, biased
	CTexture* pTexture;
	uint32    nKey;

	uint16    nWidth;
	uint16    nHeight;

	uint8     nMips           : 4;
	uint8     nMipsPersistent : 4;
	uint8     nCurMip;
	uint8     nFormatCode;
	uint8     eTF;

	uint32    nPersistentSize  : 31;
	uint32    bIsStreaming     : 1;

	uint32    bUnloaded        : 1;
	uint32    nStreamPrio      : 3;
	uint32    nSlicesMinus1    : 9;
	uint32    nSlicesPotMinus1 : 9;

	enum
	{
		InBudgetMask   = 0xffffffff ^ ((1 << 30) | (1 << 29)),
		OverBudgetMask = 0xffffffff,
		PackedFpBias   = 0x7f00
	};

	bool   IsForceStreamHighRes() const { return (nKey & (1 << 31)) == 0; }
	bool   IsHighPriority() const       { return (nKey & (1 << 30)) == 0; }
	bool   IsVisible() const            { return (nKey & (1 << 29)) == 0; }
	bool   IsInZone(int z) const        { return (nKey & (1 << (28 - z))) == 0; }
	bool   IsPrecached() const          { return (nKey & ((1 << 31) | (1 << 28) | (1 << 27))) != ((1 << 31) | (1 << 28) | (1 << 27)); }
	int    GetFpMinMipCur() const       { return static_cast<int16>(static_cast<int>(nKey & 0xffff) - PackedFpBias); }
	uint16 GetFpMinMipCurBiased() const { return (uint16)nKey; }

	SPlanningTextureOrderKey() {}
	SPlanningTextureOrderKey(CTexture* pTex, int nFrameId, const int nZoneIds[])
	{
		nKey =
		  (pTex->IsForceStreamHighRes() ? 0 : (1 << 31)) |
		  (pTex->IsStreamHighPriority() ? 0 : (1 << 30)) |
		  (pTex->GetAccessFrameId() >= nFrameId ? 0 : (1 << 29)) |
		  (pTex->GetStreamRoundInfo(0).nRoundUpdateId >= nZoneIds[0] ? 0 : (1 << 28)) |
		  (pTex->GetStreamRoundInfo(1).nRoundUpdateId >= nZoneIds[1] ? 0 : (1 << 27)) |
		  static_cast<uint16>(pTex->GetRequiredMipFP() + PackedFpBias);

		pTexture = pTex;

		nWidth = pTex->GetWidth();
		nHeight = pTex->GetHeight();
		nMips = pTex->GetNumMips();
		nMipsPersistent = pTex->IsForceStreamHighRes() ?  pTex->GetNumMips() : pTex->GetNumPersistentMips();
		nFormatCode = pTex->StreamGetFormatCode();

		uint32 nSlices = pTex->StreamGetNumSlices();
		nSlicesMinus1 = nSlices - 1;
		nSlicesPotMinus1 = (1u << (32 - (nSlices > 1 ? countLeadingZeros32(nSlices - 1) : 32))) - 1;

		nCurMip = pTex->StreamGetLoadedMip();
		eTF = pTex->GetDstFormat();
		nPersistentSize = pTex->GetPersistentSize();
		bIsStreaming = pTex->IsStreaming();
		bUnloaded = pTex->IsUnloaded();
		nStreamPrio = pTex->StreamGetPriority();
	}
};

struct SPlanningRequestIdent
{
	SPlanningRequestIdent() {}
	SPlanningRequestIdent(uint32 nSortKey, int nKey, int nMip)
		: nSortKey(nSortKey)
		, nKey(nKey)
		, nMip(nMip)
	{
	}

	uint32 nSortKey;
	int    nKey : 27;
	int    nMip : 5;
};

class CPlanningTextureStreamer : public ITextureStreamer
{
public:
	CPlanningTextureStreamer();

public:
	virtual void  BeginUpdateSchedule();
	virtual void  ApplySchedule(EApplyScheduleFlags asf);

	virtual bool  BeginPrepare(CTexture* pTexture, const char* sFilename, uint32 nFlags);
	virtual void  EndPrepare(STexStreamPrepState*& pState);

	virtual void  Precache(CTexture* pTexture);
	virtual void  UpdateMip(CTexture* pTexture, const float fMipFactor, const int nFlags, const int nUpdateId, const int nCounter);

	virtual void  OnTextureDestroy(CTexture* pTexture);

	virtual void  FlagOutOfMemory();
	virtual void  Flush();

	virtual bool  IsOverflowing() const;
	virtual float GetBias() const { return m_nBias / 256.0f; }

public: // Job entry points - do not call directly!
	void Job_UpdateEntry();

private:
	enum State
	{
		S_Idle,
		S_QueuedForUpdate,
		S_Updating,
		S_QueuedForSync,
		S_QueuedForSchedule,
		S_QueuedForScheduleDiscard,
	};

	typedef DynArray<SPlanningUpdateMipRequest> UpdateMipRequestVec;

private:
	SPlanningMemoryState GetMemoryState();

	void                 StartUpdateJob();

	void                 Job_UpdateMip(CTexture* pTexture, const float fMipFactor, const int nFlags, const int nUpdateId);
	void                 Job_Sort();
	int                  Job_Bias(SPlanningSortState& sortState, SPlanningTextureOrderKey* pKeys, size_t nNumPrecachedTexs, size_t nStreamLimit);
	size_t               Job_Plan(SPlanningSortState& sortState, const SPlanningTextureOrderKey* pKeys, size_t nTextures, size_t nNumPrecachedTexs, size_t nBalancePoint, int nMinMip, int fpSortStateBias);
	void                 Job_InitKeys(SPlanningTextureOrderKey* pKeys, CTexture** pTexs, size_t nTextures, int nFrameId, const int nZoneIds[]);
	void                 Job_CommitKeys(CTexture** pTextures, SPlanningTextureOrderKey* pKeys, size_t nTextures);
	void                 Job_ConfigureSchedule();
	void                 SyncWithJob_Locked();

private:

	bool TryBegin_FromDisk(CTexture* pTex, uint32 nTexPersMip, uint32 nTexWantedMip, uint32 nTexAvailMip, int nBias, int nBalancePoint,
	                       TStreamerTextureVec& textures, TStreamerTextureVec& trimmable, ptrdiff_t& nMemFreeLower, ptrdiff_t& nMemFreeUpper, int& nKickIdx,
	                       int& nNumSubmittedLoad, size_t& nAmtSubmittedLoad);

#if defined(TEXSTRM_TEXTURECENTRIC_MEMORY)
	bool      TrimTexture(int nBias, TStreamerTextureVec& trimmable, STexPool* pPrioritise);
#endif
	ptrdiff_t TrimTextures(ptrdiff_t nRequired, int nBias, TStreamerTextureVec& trimmable);
	ptrdiff_t KickTextures(CTexture** pTextures, ptrdiff_t nRequired, int nBalancePoint, int& nKickIdx);
	void      Job_CheckEnqueueForStreaming(CTexture* pTexture, const float fMipFactor, bool bHighPriority);

private:
	CryCriticalSection                    m_lock;
	std::vector<SPlanningTextureOrderKey> m_keys;
	int                   m_nRTList;
	int                   m_nJobList;

	volatile State        m_state;

	JobManager::SJobState m_JobState;
	SPlanningUMRState     m_umrState;
	SPlanningSortState    m_sortState;

#if defined(TEXSTRM_DEFER_UMR)
	UpdateMipRequestVec m_updateMipRequests[2];
#endif

	SPlanningScheduleState m_schedule;

	int                    m_nBias;
	int                    m_nStreamAllocFails;
	bool                   m_bOverBudget;
	size_t                 m_nPrevListSize;
};

struct SPlanningTextureRequestOrder
{
	bool operator()(const SPlanningRequestIdent& a, const SPlanningRequestIdent& b) const
	{
		return a.nSortKey < b.nSortKey;
	}
};

#endif
