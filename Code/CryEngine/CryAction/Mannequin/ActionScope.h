// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////
#ifndef __ACTIONSCOPE_H__
#define __ACTIONSCOPE_H__

#include "ICryMannequin.h"
#include "ActionController.h"

struct SScopeContext
{
	SScopeContext()
		: id(0)
		, mask(0)
		, pDatabase(NULL)
		, pCharInst(NULL)
		, entityId(0)
		, pCachedEntity(NULL)
		, pEnslavedController(NULL)
	{
	}

	void Reset(uint32 scopeContextId)
	{
		id = scopeContextId;
		mask = (1 << scopeContextId);
		pDatabase = NULL;
		pCharInst = NULL;
		entityId = 0;
		pCachedEntity = NULL;
		pEnslavedController = NULL;
	}

	// HasInvalidCharInst returns true when
	//   character instance is not NULL
	// and
	//   the character instance is not part of the entity
	bool HasInvalidCharInst() const;

	// HasInvalidEntity returns true when
	//   entityId is not 0
	// and
	//   the cachedEntity is that entity
	bool HasInvalidEntity() const;

	uint32                         id;
	uint32                         mask;
	const CAnimationDatabase*      pDatabase;
	_smart_ptr<ICharacterInstance> pCharInst;
	EntityId                       entityId;
	IEntity*                       pCachedEntity;
	CActionController*             pEnslavedController;
	TagState                       sharedTags;
	TagState                       setTags;
};

class CActionScope : public IScope
{
public:
	friend class CActionController;

	CActionScope(const char* _name, uint32 scopeID, CActionController& actionController, SAnimationContext& _context, SScopeContext& _scopeContext, int layer, int numLayers, const TagState& additionalTags);

	// -- IScope implementation -------------------------------------------------
	virtual ~CActionScope();

	virtual const char* GetName()
	{
		return m_name.c_str();
	}

	virtual uint32 GetID()
	{
		return m_id;
	}

	virtual ICharacterInstance* GetCharInst()
	{
		return m_scopeContext.pCharInst;
	}

	virtual IActionController& GetActionController() const
	{
		return (IActionController&)m_actionController;
	}

	virtual SAnimationContext& GetContext() const
	{
		return m_context;
	}

	virtual uint32 GetContextID() const
	{
		return m_scopeContext.id;
	}

	virtual const IAnimationDatabase& GetDatabase() const
	{
		CRY_ASSERT(m_scopeContext.pDatabase);
		return (const IAnimationDatabase&)*m_scopeContext.pDatabase;
	}

	virtual bool HasDatabase() const
	{
		return(m_scopeContext.pDatabase != NULL);
	}

	virtual IEntity& GetEntity() const
	{
		CRY_ASSERT(m_scopeContext.pCachedEntity);
		return *m_scopeContext.pCachedEntity;
	}

	virtual EntityId GetEntityId() const
	{
		return m_scopeContext.entityId;
	}

	virtual uint32 GetTotalLayers() const
	{
		return m_numLayers;
	}
	virtual uint32 GetBaseLayer() const
	{
		return m_layer;
	}
	virtual IAction* GetAction() const
	{
		return m_pAction.get();
	}

	virtual void              IncrementTime(const CTimeValue& timeDelta);

	virtual const CAnimation* GetTopAnim(int layer) const;

	virtual CAnimation*       GetTopAnim(int layer);

	virtual void              ApplyAnimWeight(uint32 layer, float weight);

	virtual bool              IsDifferent(const FragmentID aaID, const TagState& fragmentTags, const TagID subContext = TAG_ID_INVALID) const;

	virtual ILINE FragmentID  GetLastFragmentID() const
	{
		return m_lastFragmentID;
	}

	virtual ILINE const SFragTagState& GetLastTagState() const
	{
		return m_lastFragSelection.tagState;
	}

	virtual const CTimeValue    CalculateFragmentTimeRemaining() const;

	virtual const CTimeValue    CalculateFragmentDuration(const CFragment& fragment) const;

	virtual void     _FlushFromEditor()          { Flush(FM_Normal); }

	virtual const CTimeValue&    GetFragmentDuration() const { return m_fragmentDuration; }

	virtual const CTimeValue&    GetFragmentTime() const     { return m_fragmentTime; }

	virtual TagState GetAdditionalTags() const   { return m_additionalTags; }

	virtual void     MuteLayers(uint32 mutedAnimLayerMask, uint32 mutedProcLayerMask);

	// -- ~IScope implementation ------------------------------------------------

	bool NeedsInstall(uint32 currentContextMask) const
	{
		return (m_additionalTags != TAG_STATE_EMPTY) || ((currentContextMask & m_scopeContext.mask) == 0);
	}

	bool  InstallAnimation(int animID, const CryCharAnimationParams& animParams);
	bool  InstallAnimation(const SAnimationEntry& animEntry, int layer, const SAnimBlend& animBlend);
	void  StopAnimationOnLayer(uint32 layer, const CTimeValue& blendTime);
	const CTimeValue GetFragmentStartTime() const;
	bool  CanInstall(EPriorityComparison priorityComparison, FragmentID fragID, const SFragTagState& fragTagState, bool isRequeue, CTimeValue& timeRemaining) const;
	void  Install(IAction& action)
	{
		m_pAction = &action;
		m_speedBias = action.GetSpeedBias();
		m_animWeight = action.GetAnimWeight();
	}
	void         UpdateSequencers(const CTimeValue& timePassed);
	void         Update(const CTimeValue& timePassed);
	void         ClearSequencers();
	void         Flush(EFlushMethod flushMethod);
	void         QueueAnimFromSequence(uint32 layer, uint32 pos, bool isPersistent);
	void         QueueProcFromSequence(uint32 layer, uint32 pos);
	int          GetNumAnimsInSequence(uint32 layer) const;
	bool         PlayPendingAnim(uint32 layer, const CTimeValue& timePassed = 0);
	bool         PlayPendingProc(uint32 layer);
	bool         QueueFragment(FragmentID aaID, const SFragTagState& fragTagState, uint32 optionIdx = OPTION_IDX_RANDOM, const CTimeValue& startTime = 0, uint32 userToken = 0, bool isRootScope = true, bool isHigherPriority = false, bool principleContext = true);
	void         BlendOutFragments();

	ILINE uint32 GetContextMask() const
	{
		return m_scopeContext.mask;
	}

	ILINE uint32 GetLastOptionIdx() const
	{
		return m_lastFragSelection.optionIdx;
	}

	ILINE bool HasFragment() const
	{
		return ((m_sequenceFlags & eSF_Fragment) != 0);
	}
	ILINE bool HasTransition() const
	{
		return ((m_sequenceFlags & eSF_Transition) != 0);
	}
	ILINE bool HasOutroTransition() const
	{
		return ((m_sequenceFlags & eSF_TransitionOutro) != 0);
	}
	ILINE const CTimeValue& GetTransitionDuration() const
	{
		return m_transitionDuration;
	}

	ILINE const CTimeValue& GetTransitionOutroDuration() const
	{
		return m_transitionOutroDuration;
	}
	ILINE const IActionPtr& GetPlayingAction()
	{
		return m_pExitingAction ? m_pExitingAction : m_pAction;
	}

	ILINE IActionController* GetEnslavedActionController() const
	{
		return m_scopeContext.pEnslavedController;
	}

	void InstallProceduralClip(const SProceduralEntry& proc, int layer, const SAnimBlend& blend, const CTimeValue& duration);

	void Pause();
	void Resume(const CTimeValue& forcedBlendOutTime, uint32 resumeFlags);

private:
	void InitAnimationParams(const SAnimationEntry& animEntry, const uint32 sequencerLayer, const SAnimBlend& animBlend, CryCharAnimationParams& paramsOut);
	void FillBlendQuery(SBlendQuery& query, FragmentID fragID, const SFragTagState& fragTagState, bool isHigherPriority, CTimeValue* pLoopDuration) const;
	void ClipInstalled(uint8 clipType);

private:
	CActionScope();
	CActionScope(const CActionScope&);
	CActionScope& operator=(const CActionScope&);

private:

	enum ESequencerFlags
	{
		eSF_Queued      = BIT(0),
		eSF_BlendingOut = BIT(1)
	};

	struct SSequencer
	{

		SSequencer()
			:
			installTime(-1),
			referenceTime(-1),
			savedAnimNormalisedTime(-1),
			pos(0),
			flags(0)
		{
		}

		TAnimClipSequence sequence;
		SAnimBlend        blend;
		CTimeValue        installTime; // time in seconds until installation
		CTimeValue        referenceTime;
		nTime             savedAnimNormalisedTime;
		uint8             pos;
		uint8             flags;
	};

	struct SProcSequencer
	{
		SProcSequencer()
			:
			installTime(-1),
			pos(0),
			flags(0)
		{
		}

		TProcClipSequence                sequence;
		SAnimBlend                       blend;
		CTimeValue                       installTime;
		std::shared_ptr<IProceduralClip> proceduralClip;
		uint8                            pos;
		uint8                            flags;
	};

	string                      m_name;
	uint32                      m_id;
	SAnimationContext&          m_context;
	SScopeContext&              m_scopeContext;
	CActionController&          m_actionController;
	int                         m_layer;
	uint32                      m_numLayers;
	SSequencer*                 m_layerSequencers;
	std::vector<SProcSequencer> m_procSequencers;
	mpfloat                     m_speedBias;
	float                       m_animWeight;
	CTimeValue                  m_timeIncrement;
	TagState                    m_additionalTags;
	mutable TagState            m_cachedFragmentTags;
	mutable TagState            m_cachedContextStateMask;
	mutable FragmentID          m_cachedaaID;
	mutable uint32              m_cachedTagSetIdx;
	FragmentID                  m_lastFragmentID;
	SFragmentSelection          m_lastFragSelection;
	SFragTagState               m_lastQueueTagState;
	uint32                      m_sequenceFlags;
	CTimeValue                  m_fragmentTime;
	CTimeValue                  m_fragmentDuration;
	CTimeValue                  m_transitionOutroDuration;
	CTimeValue                  m_transitionDuration;
	CTimeValue                  m_blendOutDuration;
	EClipType                   m_partTypes[SFragmentData::PART_TOTAL];

	nTime                       m_lastNormalisedTime;
	nTime                       m_normalisedTime;

	IActionPtr                  m_pAction;
	IActionPtr                  m_pExitingAction;
	uint32                      m_userToken; // token that will be passed when installing animations

	uint32                      m_mutedAnimLayerMask;
	uint32                      m_mutedProcLayerMask;

	bool                        m_isOneShot;
	bool                        m_fragmentInstalled;
};

#endif //!__ACTIONSCOPE_H__
