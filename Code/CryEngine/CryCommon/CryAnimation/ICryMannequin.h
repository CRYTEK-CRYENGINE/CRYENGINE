// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifdef CRYACTION_EXPORTS
#define CRYMANNEQUIN_API DLL_EXPORT
#else
#define CRYMANNEQUIN_API DLL_IMPORT
#endif

#include <CryAnimation/ICryAnimation.h>
#include <CryExtension/ICryUnknown.h>
#include <CryExtension/CryCreateClassInstance.h>
#include <CrySerialization/Forward.h>
#include "ICryMannequinDefs.h"
#include "ICryMannequinProceduralClipFactory.h"

#define MannGenCRC CCrc32::ComputeLowercase

#include <CryCore/BitFiddling.h>
#include <CryMath/MTPseudoRandom.h>
#include "ICryMannequinTagDefs.h"

struct IMannequinEditorManager;
struct IMannequinGameListener;
class CAnimationDatabase;
struct AnimEventInstance;
struct IMannequinWriter;

struct IProceduralParams;
DECLARE_SHARED_POINTERS(IProceduralParams);

const uint32 MANN_NUMBER_BLEND_CHANNELS = 4;

struct SAnimationEntry
{
	SAnimationEntry()
		:
		flags(0),
		playbackSpeed(1.0f),
		playbackWeight(1.0f),
		weightList(0)
	{
		memset(blendChannels, 0, sizeof(float) * MANN_NUMBER_BLEND_CHANNELS);
	}

	ILINE bool IsEmpty() const
	{
		return animRef.IsEmpty();
	}

	friend bool operator==(const SAnimationEntry& entryA, const SAnimationEntry& entryB)
	{
		return (entryA.animRef.crc == entryB.animRef.crc)
			&& (entryA.flags == entryB.flags)
			&& (entryA.playbackSpeed == entryB.playbackSpeed)
			&& (entryA.playbackWeight == entryB.playbackWeight)
			&& (entryA.weightList == entryB.weightList);
	}

	SAnimRef animRef;
	uint32   flags;
	float    playbackSpeed;
	float    playbackWeight;
	float    blendChannels[MANN_NUMBER_BLEND_CHANNELS];
	uint8    weightList;
};

struct IProceduralParamsComparer;
typedef std::shared_ptr<IProceduralParamsComparer> IProceduralParamsComparerPtr;

struct IProceduralParamsComparer
	: public ICryUnknown
{
	CRYINTERFACE_DECLARE_GUID(IProceduralParamsComparer, "dde3edc3-dca8-4794-b71e-e1c21047c654"_cry_guid);

	virtual bool                        Equal(const IProceduralParams& lhs, const IProceduralParams& rhs) const = 0;

	static IProceduralParamsComparerPtr CreateDefaultProceduralParamsComparer()
	{
		IProceduralParamsComparerPtr pProceduralParamsComparer;
		CryCreateClassInstanceForInterface<IProceduralParamsComparer>(cryiidof<IProceduralParamsComparer>(), pProceduralParamsComparer);
		return pProceduralParamsComparer;
	}
};

struct IProceduralParamsEditor
{
	virtual ~IProceduralParamsEditor() {}

	struct SEditorCreateTransformGizmoResult
	{
		bool         createGizmo;   // Specifies if the editor should create a transform gizmo or not. Defaults to false.
		QuatT        gizmoLocation; // Specifies the initial location of the transform gizmo (Relative to the supplied attachment or joint id, if any).
		IAttachment* pAttachment;   // If not null, the gizmo's location will be relative to the attachment. Do not specify both an attachment and a jointId.
		int          jointId;       // If positive, the gizmo's location will be relative to the joint with this Id. Do not specify both an attachment and a jointId.
		uint32       paramCRC;      // If not 0, the gizmo's location will be relative to the parameter location. Do not specify both an attachment and a jointId.
		string       helperName;

		SEditorCreateTransformGizmoResult()
			: createGizmo(false)
			, gizmoLocation(IDENTITY)
			, pAttachment(NULL)
			, jointId(-1)
			, paramCRC(0)
		{
		}

		SEditorCreateTransformGizmoResult(IEntity& entity, const QuatT& location, const uint32 jointOrAttachmentNameCrc, const char* _helperName = NULL)
			: createGizmo(true)
			, gizmoLocation(location)
			, pAttachment(NULL)
			, jointId(-1)
			, paramCRC(0)
			, helperName(_helperName)
		{
			ICharacterInstance* const pCharacterInstance = entity.GetCharacter(0);
			if (pCharacterInstance)
			{
				pAttachment = pCharacterInstance->GetIAttachmentManager()->GetInterfaceByNameCRC(jointOrAttachmentNameCrc);
				if (!pAttachment)
				{
					jointId = pCharacterInstance->GetIDefaultSkeleton().GetJointIDByCRC32(jointOrAttachmentNameCrc);
					if (jointId < 0)
					{
						paramCRC = jointOrAttachmentNameCrc;
					}
				}
			}
		}
	};

	// OnEditorCreateTransformGizmo is an editor callback for when the editor wants to query if/how to create a transformation gizmo in the mannequin viewport and associate it
	// with this instance of a procedural clip's parameters to facilitate editing of locations in space.
	// The return value is SEditorCreateTransformGizmoResult with details of how that locator should be created. See structure for details on the parameters.
	// This function might be called more than once on a specific instance (e.g. When the procedural clip parameters change), but it always refers to the same transform
	// gizmo: At most a single transform gizmo will be created.
	// This function should disappear once there is support for a transform gizmo/location decorators in the serialization framework UI components.
	virtual SEditorCreateTransformGizmoResult OnEditorCreateTransformGizmo(IEntity& entity)
	{
		return SEditorCreateTransformGizmoResult();
	}

	// OnEditorMovedTransformGizmo is an editor callback for when the transform gizmo created after OnEditorCreateTransformGizmo and associated to the current procedural clip is moved.
	// The location in gizmoLocation is relative to the attachment or joint specified in the SEditorCreateTransformGizmoResult returned in the last call to OnEditorCreateTransformGizmo.
	// This function should disappear once there is support for a transform gizmo/location decorators in the serialization framework UI components.
	virtual void OnEditorMovedTransformGizmo(const QuatT& gizmoLocation)
	{
	}

	// GetEditorDefaultBlendDuration is called by the editor to know what value to use as the blend duration when creating a procedural clip.
	virtual float GetEditorDefaultBlendDuration() const
	{
		return 0.3f;
	}
};

struct IProceduralParams
	: public IProceduralParamsEditor
{
	virtual ~IProceduralParams() {}

	virtual void Serialize(Serialization::IArchive& ar) = 0;

	// Important: Calling this function with two IProceduralParams of different types will result in undefined behavior.
	// There is no internal check for this requirement, so the check needs to happen at a higher level.
	bool operator==(const IProceduralParams& rhs) const
	{
		IProceduralParamsComparerPtr pComparer = IProceduralParamsComparer::CreateDefaultProceduralParamsComparer();
		if (pComparer)
		{
			return pComparer->Equal(*this, rhs);
		}
		return false;
	}

	template<typename TChar>
	struct StringWrapperImpl
	{
		StringWrapperImpl()
		{
			SetValue(NULL);
		}

		explicit StringWrapperImpl(const TChar* const s)
		{
			SetValue(s);
		}

		const TChar* c_str() const
		{
			return &m_buffer[0];
		}

		const StringWrapperImpl& operator=(const TChar* const s)
		{
			SetValue(s);
			return *this;
		}

		const size_t GetLength() const
		{
			const size_t bufferSize = m_buffer.size();
			assert(0 < bufferSize);
			return bufferSize - 1;
		}

		const bool IsEmpty() const
		{
			return (GetLength() == 0);
		}

	private:
		void SetValue(const TChar* const s)
		{
			m_buffer.clear();
			if (s)
			{
				const TChar* it = s;
				while (*it)
				{
					m_buffer.push_back(*it);
					++it;
				}
			}
			m_buffer.push_back(0);
		}

	private:
		DynArray<TChar> m_buffer;
	};

	typedef StringWrapperImpl<char> StringWrapper;

	// GetExtraDebugInfo is called to retrieve a string containing extra information about the procedural parameters.
	// This function is called both by the editor and the runtime.
	// The editor may call it to display information on top of tracks in the fragment editor, preview editor, etc.
	// The runtime may call it in debug functions (e.g mn_debug) to display data associated to procedural clips.
	virtual void GetExtraDebugInfo(StringWrapper& extraInfoOut) const
	{
	}
};

struct SNoProceduralParams
	: public IProceduralParams
{
	virtual void Serialize(Serialization::IArchive& ar)
	{
	}
};

struct SMannParameter
{
	uint32 crc;
	QuatT  value;

	SMannParameter()
		:
		crc(0),
		value(IDENTITY)
	{
	}
};

typedef DynArray<SMannParameter> TMannParamList;

//---------------------------------------------------------------------------------------------------------
// SAnimBlend - Specific blend information for a clip on a layer
//---------------------------------------------------------------------------------------------------------
// ExitTime				- Time to start the following clip relative to the previous one
//                   -ve values mean to align to the end of the previous clip
// StartTime			- Time in secs for the following clip to start off advanced to
// Duration				- Duration of the blend in secs
// flags					- Specific playback flags for timeAlignment etc.
// terminal				- Has this clip been explicitly setup to blend into the next fragment?
//---------------------------------------------------------------------------------------------------------
struct SAnimBlend
{
	SAnimBlend(float _exitTime = 0.0f, float _startTime = 0.0f, float _duration = 0.2f, uint32 _flags = 0)
		:
		exitTime(_exitTime),
		startTime(_startTime),
		duration(_duration),
		flags(_flags),
		terminal(false)
	{
	}

	friend bool operator==(const SAnimBlend& entryA, const SAnimBlend& entryB)
	{
		return (entryA.exitTime == entryB.exitTime)
			&& (entryA.startTime == entryB.startTime)
			&& (entryA.duration == entryB.duration)
			&& (entryA.flags == entryB.flags)
			&& (entryA.terminal == entryB.terminal);
	}

	float  exitTime;
	float  startTime;
	float  duration;
	uint32 flags;
	bool   terminal;
};

enum EClipType
{
	eCT_Normal,
	eCT_Transition,
	eCT_TransitionOutro,
};

//---------------------------------------------------------------------------------------------------------
// SAnimClip - Single animation clip used by the sequencer
//---------------------------------------------------------------------------------------------------------
// blend					- Blend data to use for this clip
// animation			- Pure animation data
// blendPart			- The fragment part which the blend came from
// part						- The fragment part which the animation data came from
//---------------------------------------------------------------------------------------------------------
struct SAnimClip
{
	SAnimClip()
		: referenceLength(0.0f)
		, blendPart(0)
		, part(0)
		, isVariableLength(false)
	{
	}

	friend bool operator==(const SAnimClip& entryA, const SAnimClip& entryB)
	{
		return (entryA.blend == entryB.blend)
			&& (entryA.animation == entryB.animation)
			&& (entryA.blendPart == entryB.blendPart)
			&& (entryA.part == entryB.part);
	}

	SAnimBlend      blend;
	SAnimationEntry animation;
	float           referenceLength;
	uint8           blendPart;
	uint8           part;
	bool            isVariableLength;
};

typedef DynArray<SAnimClip> TAnimClipSequence;

struct SProceduralEntry
{
	SProceduralEntry()
		: blendPart(0)
		, part(0)
	{
	}

	static bool ProceduralParamsMatch(const IProceduralParamsPtr& p1, const IProceduralParamsPtr& p2)
	{
		return ((!p1 && !p2) || (p1 && p2 && *p1 == *p2));
	}

	friend bool operator==(const SProceduralEntry& entryA, const SProceduralEntry& entryB)
	{
		return (entryA.typeNameHash == entryB.typeNameHash)
			&& (entryA.blend == entryB.blend)
			&& (entryA.blendPart == entryB.blendPart)
			&& (entryA.part == entryB.part)
			&& SProceduralEntry::ProceduralParamsMatch(entryA.pProceduralParams, entryB.pProceduralParams);
	}

	bool IsNoneType() const
	{
		return typeNameHash.IsEmpty();
	}

	IProceduralClipFactory::THash typeNameHash;

	SAnimBlend                    blend;
	uint8                         blendPart;
	uint8                         part;

	IProceduralParamsPtr          pProceduralParams;
};

typedef DynArray<SProceduralEntry> TProcClipSequence;

struct SFragmentData
{
	enum
	{
		PART_FRAGMENT,
		PART_TRANSITION1,
		PART_TRANSITION2,
		PART_TOTAL
	};

	SFragmentData()
		: isOneShot(false)
		, blendOutDuration(0.0f)
	{
		for (uint32 p = 0; p < PART_TOTAL; p++)
		{
			duration[p] = 0.0f;
			transitionType[p] = eCT_Normal;
		}
	}

	float                       blendOutDuration;
	DynArray<TAnimClipSequence> animLayers;
	DynArray<TProcClipSequence> procLayers;
	float                       duration[PART_TOTAL];
	EClipType                   transitionType[PART_TOTAL];
	bool                        isOneShot;
};

class CFragment
{
public:
	CFragment() : m_blendOutDuration(0.2f) {}

	float                       m_blendOutDuration;
	DynArray<TAnimClipSequence> m_animLayers;
	DynArray<TProcClipSequence> m_procLayers;
};

// Unique id for SFragmentBlend
struct SFragmentBlendUid
{
	enum
	{
		INVALID_UID = 0,
	};

	enum ENewUid
	{
		NEW
	};

	SFragmentBlendUid()
		: m_uid(INVALID_UID)
	{
	}

	explicit SFragmentBlendUid(ENewUid)
		: m_uid(GenerateUid())
	{
	}

	SFragmentBlendUid(const SFragmentBlendUid& other)
		: m_uid(other.m_uid)
	{
	}

	bool operator==(const SFragmentBlendUid& other) const
	{
		return m_uid == other.m_uid;
	}

	bool operator!=(const SFragmentBlendUid& other) const
	{
		return m_uid != other.m_uid;
	}

	SFragmentBlendUid& operator=(const SFragmentBlendUid& other)
	{
		m_uid = other.m_uid;
		return *this;
	}

	bool operator<(const SFragmentBlendUid& other) const
	{
		assert(IsValid());
		assert(other.IsValid());
		return m_uid < other.m_uid;
	}

	bool IsValid() const
	{
		return m_uid != INVALID_UID;
	}

	void Serialize(XmlNodeRef& xmlNode, bool bLoading)
	{
		if (bLoading)
		{
			xmlNode->getAttr("uid", m_uid);
		}
		else
		{
			xmlNode->setAttr("uid", m_uid);
		}
	}

private:
	static uint32 GenerateUid()
	{
		return gEnv->pSystem->GetRandomGenerator().GenerateUint32();
	}

private:
	uint32 m_uid;
};

//---------------------------------------------------------------------------------------------------------
// SFragmentBlend - High level blend information
//---------------------------------------------------------------------------------------------------------
// SelectTime					- Time value used for selecting the best transition
// StartTime					- Time to begin the transition relative to the end of the previous fragment
// EnterTime					- Time delta to apply to the following fragment, allows snipping of the start of the entire fragment
// pFragment					- Transition fragment: includes additional tween clips and blends into the intial fragment clips.
//										  This is merged onto the front of the following fragment on installation.
// uid								- Unique id
//---------------------------------------------------------------------------------------------------------
struct SFragmentBlend
{
	enum EFlags
	{
		Cyclic = BIT(0),
		CycleLocked = BIT(1),
		ExitTransition = BIT(2)
	};

	SFragmentBlend()
		:
		selectTime(0.0f),
		startTime(0.0f),
		enterTime(0.0f),
		pFragment(NULL),
		flags(0),
		uid(SFragmentBlendUid::NEW)
	{

	}

	bool operator<(const SFragmentBlend& blend) const
	{
		return selectTime < blend.selectTime;
	}

	ILINE const bool IsExitTransition() const
	{
		return (flags & ExitTransition) != 0;
	}

	float             selectTime;
	float             startTime;
	float             enterTime;
	CFragment*        pFragment;
	uint8             flags;
	SFragmentBlendUid uid;
};

typedef TTagSortedList<ActionScopes> TTagListActionScope;

struct SFragmentDef
{
	enum EFlags
	{
		PERSISTENT = BIT(0),
		AUTO_REINSTALL_BEST_MATCH = BIT(1)
	};

	SFragmentDef()
		: flags(0)
	{
		scopeMaskList.Insert(SFragTagState(), ACTION_SCOPES_ALL);
	}

	TTagListActionScope scopeMaskList;
	uint8               flags;
};

struct SScopeDef
{
	SScopeDef()
		: context(SCOPE_CONTEXT_ID_INVALID)
		, layer(0)
		, numLayers(0)
		, additionalTags(TAG_STATE_EMPTY)
	{
	}

	uint32    context;
	uint32    layer;
	uint32    numLayers;
	TagState  additionalTags;
	SScopeRef scopeAlias;
};

struct SScopeContextDef
{
	SScopeContextDef()
		: sharedTags(TAG_STATE_FULL)
		, additionalTags(TAG_STATE_EMPTY)
	{
	}

	TagState sharedTags;
	TagState additionalTags;
};

struct SSubContext
{
	ActionScopes scopeMask;
	TagState     additionalTags;
};

struct SControllerDef
{
	SControllerDef(const CTagDefinition& tags, const CTagDefinition& fragmentIDs, const CTagDefinition& scopeIDs)
		:
		m_tags(tags),
		m_fragmentIDs(fragmentIDs),
		m_scopeIDs(scopeIDs)
	{
	}

	SControllerDef(const SControllerDef& rhs)
		: m_filename(rhs.m_filename)
		, m_tags(rhs.m_tags)
		, m_fragmentIDs(rhs.m_fragmentIDs)
		, m_scopeIDs(rhs.m_scopeIDs)
		, m_subContextIDs(rhs.m_subContextIDs)
		, m_scopeContexts(rhs.m_scopeContexts)
		, m_fragmentDef(rhs.m_fragmentDef)
		, m_scopeDef(rhs.m_scopeDef)
		, m_scopeContextDef(rhs.m_scopeContextDef)
		, m_subContext(rhs.m_subContext)
	{
	}

	const SFragmentDef& GetFragmentDef(FragmentID fragment) const
	{
		IF_LIKELY(m_fragmentIDs.IsValidTagID(fragment))
		{
			return m_fragmentDef[fragment];
		}
		else
		{
			CRY_ASSERT(false);
			return m_fragmentDef[0];
		}
	}

	const CTagDefinition* GetFragmentTagDef(FragmentID fragment) const
	{
		return m_fragmentIDs.GetSubTagDefinition(fragment);
	}

	const ActionScopes GetScopeMask(FragmentID fragID, const SFragTagState& fragTagState, TagID subContext = TAG_ID_INVALID) const
	{
		ActionScopes scopeMask = ACTION_SCOPES_NONE;
		IF_LIKELY(m_fragmentIDs.IsValidTagID(fragID))
		{
			const CTagDefinition* pFragTagDef = m_fragmentIDs.GetSubTagDefinition(fragID);
			scopeMask = *m_fragmentDef[fragID].scopeMaskList.GetBestMatch(fragTagState, &m_tags, pFragTagDef);

			if (subContext != TAG_ID_INVALID)
			{
				scopeMask |= m_subContext[subContext].scopeMask;
			}
		}

		return scopeMask;
	}

	TDefPathString             m_filename;
	const CTagDefinition&      m_tags;
	const CTagDefinition&      m_fragmentIDs;
	const CTagDefinition       m_scopeIDs;
	CTagDefinition             m_subContextIDs;
	CTagDefinition             m_scopeContexts;

	DynArray<SFragmentDef>     m_fragmentDef;
	DynArray<SScopeDef>        m_scopeDef;
	DynArray<SScopeContextDef> m_scopeContextDef;
	DynArray<SSubContext>      m_subContext;
};

struct SAnimationContext
{
	SAnimationContext(const SControllerDef& _controllerDef)
		:
		controllerDef(_controllerDef),
		state(_controllerDef.m_tags)
	{
		subStates.resize(_controllerDef.m_subContextIDs.GetNum(), state);

		if (gEnv->pTimer)
		{
			const uint32 seed = static_cast<uint32>(gEnv->pTimer->GetAsyncTime().GetValue());
			randGenerator.Seed(seed);
		}
	}

	const SControllerDef& controllerDef;
	CTagState             state;
	DynArray<CTagState>   subStates;
	CMTRand_int32         randGenerator;
};

struct SMannHistoryItem
{
	SMannHistoryItem()
		:
		time(-1.0f),
		tagState(TAG_STATE_EMPTY),
		scopeMask(0),
		fragment(FRAGMENT_ID_INVALID),
		optionIdx(0),
		trumpsPrevious(false),
		type(None)
	{
	}

	SMannHistoryItem(ActionScopes _scopeMask, FragmentID _fragment, const TagState& _tagState, uint32 _optionIdx, bool _trumpsPrevious = false)
		:
		time(-1.0f),
		tagState(_tagState),
		scopeMask(_scopeMask),
		fragment(_fragment),
		optionIdx(_optionIdx),
		trumpsPrevious(_trumpsPrevious),
		type(Fragment)
	{
	}
	SMannHistoryItem(const TagState& _tagState)
		:
		time(-1.0f),
		tagState(_tagState),
		scopeMask(0),
		fragment(FRAGMENT_ID_INVALID),
		optionIdx(0),
		trumpsPrevious(false),
		type(Tag)
	{
	}

	enum EType
	{
		Fragment,
		Tag,
		None
	};
	float        time;
	TagState     tagState;
	ActionScopes scopeMask;
	FragmentID   fragment;
	uint32       optionIdx;
	bool         trumpsPrevious;
	uint8        type;
};

struct IMannequinListener
{
	virtual ~IMannequinListener() {}

	virtual void OnEvent(const SMannHistoryItem& historyItem, const class IActionController& actionController) = 0;
};

struct SMannequinErrorReport
{
	SMannequinErrorReport()
		:
		errorType(None),
		fragID(FRAGMENT_ID_INVALID),
		fragIDTo(FRAGMENT_ID_INVALID),
		fragOptionID(0)
	{
		error[0] = '\0';
	}
	enum ErrorType
	{
		None,
		Fragment,
		Blend
	};

	char          error[1024];
	ErrorType     errorType;
	FragmentID    fragID;
	SFragTagState tags;
	FragmentID    fragIDTo;
	SFragTagState tagsTo;
	uint32        fragOptionID;
};

struct SAnimAssetReport
{
	SAnimAssetReport()
		:
		pAnimName(NULL),
		pAnimPath(NULL),
		animID(-1),
		fragID(FRAGMENT_ID_INVALID),
		fragIDTo(FRAGMENT_ID_INVALID),
		fragOptionID(0)
	{
	}

	const char*   pAnimName;
	const char*   pAnimPath;
	int           animID;
	FragmentID    fragID;
	SFragTagState tags;
	FragmentID    fragIDTo;
	SFragTagState tagsTo;
	uint32        fragOptionID;
};

struct SBlendQueryResult
{
	SBlendQueryResult()
		:
		fragmentFrom(FRAGMENT_ID_INVALID),
		fragmentTo(FRAGMENT_ID_INVALID),
		pFragmentBlend(NULL),
		blendIdx(0),
		selectTime(0.0f),
		duration(0.0f)
	{
	}

	FragmentID            fragmentFrom;
	FragmentID            fragmentTo;
	SFragTagState         tagStateFrom;
	SFragTagState         tagStateTo;
	const SFragmentBlend* pFragmentBlend;
	uint32                blendIdx;
	SFragmentBlendUid     blendUid;
	float                 selectTime;
	float                 duration;
};

struct SBlendQuery
{
	enum EFlags
	{
		fromInstalled = BIT(0),
		toInstalled = BIT(1),
		higherPriority = BIT(2),
		noTransitions = BIT(3)
	};

	SBlendQuery()
		:
		fragmentFrom(FRAGMENT_ID_INVALID),
		fragmentTo(FRAGMENT_ID_INVALID),
		additionalTags(TAG_STATE_EMPTY),
		fragmentTime(0.0f),
		prevNormalisedTime(0.0f),
		normalisedTime(0.0f),
		flags(0)
	{
	}

	bool IsFlagSet(uint32 flag) const
	{
		return (flags & flag) == flag;
	}
	void SetFlag(uint32 flag, bool set)
	{
		if (set)
			flags |= flag;
		else
			flags &= ~flag;
	}

	FragmentID        fragmentFrom;
	FragmentID        fragmentTo;
	SFragTagState     tagStateFrom;
	SFragTagState     tagStateTo;
	TagState          additionalTags;
	float             fragmentTime;
	float             prevNormalisedTime;
	float             normalisedTime;
	uint32            flags;
	SFragmentBlendUid forceBlendUid;
};

struct SFragmentQuery
{
	SFragmentQuery(const CTagDefinition& fragDef, FragmentID _fragID = FRAGMENT_ID_INVALID, SFragTagState _tagState = SFragTagState(), TagState _requiredTags = TAG_STATE_EMPTY, uint32 _optionIdx = OPTION_IDX_RANDOM)
		:
		fragID(_fragID),
		tagState(_tagState),
		requiredTags(_requiredTags),
		optionIdx(_optionIdx)
	{
		tagState.globalTags = fragDef.GetUnion(tagState.globalTags, _requiredTags);
	}

	SFragmentQuery(FragmentID _fragID = FRAGMENT_ID_INVALID, SFragTagState _tagState = SFragTagState(), uint32 _optionIdx = OPTION_IDX_RANDOM)
		:
		fragID(_fragID),
		tagState(_tagState),
		requiredTags(TAG_STATE_EMPTY),
		optionIdx(_optionIdx)
	{
	}

	FragmentID    fragID;
	SFragTagState tagState;
	TagState      requiredTags;
	uint32        optionIdx;
};

struct SFragmentSelection
{
	SFragmentSelection(SFragTagState _tagState = SFragTagState(), uint32 _optionIdx = OPTION_IDX_RANDOM)
		:
		tagState(_tagState),
		tagSetIdx(TAG_SET_IDX_INVALID),
		optionIdx(_optionIdx)
	{
	}

	SFragTagState tagState;
	uint32        tagSetIdx;
	uint32        optionIdx;
};

struct SMiniSubADB
{
	SMiniSubADB()
		: tags(TAG_STATE_EMPTY)
		, pFragDef(NULL)
	{
	}

	TagState              tags;
	string                filename;
	const CTagDefinition* pFragDef;

	typedef DynArray<FragmentID> TFragIDArray;
	TFragIDArray vFragIDs;

	typedef DynArray<SMiniSubADB> TSubADBArray;
	TSubADBArray vSubADBs;
};

typedef void(*MannErrorCallback)(const SMannequinErrorReport& errorReport, void* _context);
typedef void(*MannAssetCallback)(const SAnimAssetReport& assetReport, void* _context);

class IAnimationDatabase
{
public:
	virtual ~IAnimationDatabase() {}

	virtual bool        Validate(const struct IAnimationSet* animSet, MannErrorCallback errorCallback = NULL, MannErrorCallback warningCallback = NULL, void* errorCallbackContext = NULL) const = 0;
	virtual void        EnumerateAnimAssets(const IAnimationSet* animSet, MannAssetCallback assetCallback, void* callbackContext) const = 0;

	virtual const char* GetFilename() const = 0;
	virtual bool        Query(SFragmentData& outFragmentData, const SFragmentQuery& inFragQuery, SFragmentSelection* outFragSelection = NULL) const = 0;

	//-----------------------------------------------------------------
	// Main Query function, expands queried fragment and transition data out into the target fragData buffer
	// Returns the following flags to say which sources are contributing to the data
	// From eSequenceFlags: eSF_Fragment, eSF_TransitionOutro, eSF_Transition
	//-----------------------------------------------------------------
	virtual uint32                Query(SFragmentData& outFragmentData, const SBlendQuery& inBlendQuery, uint32 inOptionIdx, const IAnimationSet* inAnimSet, SFragmentSelection* outFragSelection = NULL) const = 0;

	virtual uint32                FindBestMatchingTag(const SFragmentQuery& inFragQuery, SFragTagState* matchingTagState = NULL, uint32* tagSetIdx = NULL) const = 0;
	virtual const CTagDefinition& GetTagDefs() const = 0;
	virtual FragmentID            GetFragmentID(const char* szActionName) const = 0;
	virtual const CTagDefinition& GetFragmentDefs() const = 0;

	virtual uint32                GetTotalTagSets(FragmentID fragmentID) const = 0;
	virtual uint32                GetTagSetInfo(FragmentID fragmentID, uint32 tagSetID, SFragTagState& fragTagState) const = 0;

	virtual const CFragment*      GetBestEntry(const SFragmentQuery& fragQuery, SFragmentSelection* fragSelection = NULL) const = 0;
	virtual const CFragment*      GetEntry(FragmentID fragmentID, uint32 tagSetID, uint32 optionIdx) const = 0;

	virtual void                  FindBestBlends(const SBlendQuery& blendQuery, SBlendQueryResult& result1, SBlendQueryResult& result2) const = 0;
	virtual uint32                GetNumBlends(FragmentID fragmentIDFrom, FragmentID fragmentIDTo, const SFragTagState& tagFrom, const SFragTagState& tagTo) const = 0;
	virtual const SFragmentBlend* GetBlend(FragmentID fragmentIDFrom, FragmentID fragmentIDTo, const SFragTagState& tagFrom, const SFragTagState& tagTo, uint32 blendNum) const = 0;
	virtual const SFragmentBlend* GetBlend(FragmentID fragmentIDFrom, FragmentID fragmentIDTo, const SFragTagState& tagFrom, const SFragTagState& tagTo, SFragmentBlendUid uid) const = 0;

	virtual const char*           FindSubADBFilenameForID(FragmentID fragmentID) const = 0;
	virtual bool                  RemoveSubADBFragmentFilter(FragmentID fragmentID) = 0;
	virtual bool                  AddSubADBFragmentFilter(const string& sADBFileName, FragmentID fragmentID) = 0;
	virtual void                  GetSubADBFragmentFilters(SMiniSubADB::TSubADBArray& outList) const = 0;

	virtual bool                  AddSubADBTagFilter(const string& sParentFilename, const string& sADBFileName, const TagState& tag) = 0;
	virtual bool                  MoveSubADBFilter(const string& sADBFileName, const bool bMoveUp) = 0;
	virtual bool                  DeleteSubADBFilter(const string& sADBFileName) = 0;
	virtual bool                  ClearSubADBFilter(const string& sADBFileName) = 0;

	virtual void                  QueryUsedTags(const FragmentID fragmentID, const SFragTagState& filter, SFragTagState& usedTags) const = 0;
};

class IAnimationDatabaseManager
{
public:
	virtual ~IAnimationDatabaseManager() {}

	virtual int                       GetTotalDatabases() const = 0;
	virtual const IAnimationDatabase* GetDatabase(int idx) const = 0;

	virtual const IAnimationDatabase* FindDatabase(const uint32 crcFilename) const = 0;
	virtual const IAnimationDatabase* Load(const char* filename) = 0;
	virtual const SControllerDef*     LoadControllerDef(const char* filename) = 0;
	virtual const CTagDefinition*     LoadTagDefs(const char* filename, bool isTags) = 0;
	virtual void					  SaveAll(IMannequinWriter* pWriter) const = 0;

	virtual const SControllerDef*     FindControllerDef(const uint32 crcFilename) const = 0;
	virtual const SControllerDef*     FindControllerDef(const char* filename) const = 0;

	virtual const CTagDefinition*     FindTagDef(const uint32 crcFilename) const = 0;
	virtual const CTagDefinition*     FindTagDef(const char* filename) const = 0;

	virtual IAnimationDatabase*       Create(const char* filename, const char* defFilename) = 0;
	virtual CTagDefinition*           CreateTagDefinition(const char* filename) = 0;

	virtual void                      ReloadAll() = 0;
	virtual void                      UnloadAll() = 0;
};

enum EPriorityComparison
{
	Lower,
	Equal,
	Higher
};

class IActionController;
class IAction;
typedef _smart_ptr<IAction> IActionPtr;
class CAnimation;
struct SGameObjectEvent;

//! Represents a scope in which fragments can be played.
//! A common use case is to separate first and third person into different scopes.
class IScope
{
public:
	virtual ~IScope() {}

	virtual const char*               GetName() = 0;
	virtual uint32                    GetID() = 0;
	virtual ICharacterInstance*       GetCharInst() = 0;
	virtual IActionController&        GetActionController() const = 0;
	virtual SAnimationContext&        GetContext() const = 0;
	virtual uint32                    GetContextID() const = 0;
	virtual const IAnimationDatabase& GetDatabase() const = 0;
	virtual IEntity&                  GetEntity() const = 0;
	virtual EntityId                  GetEntityId() const = 0;
	virtual IAction*                  GetAction() const = 0;
	virtual bool                      HasDatabase() const = 0;

	virtual IActionController*        GetEnslavedActionController() const = 0;

	virtual uint32                    GetTotalLayers() const = 0;
	virtual uint32                    GetBaseLayer() const = 0;

	virtual void                      IncrementTime(float timeDelta) = 0;

	virtual const CAnimation*         GetTopAnim(int layer) const = 0;
	virtual CAnimation*               GetTopAnim(int layer) = 0;

	virtual void                      ApplyAnimWeight(uint32 layer, float weight) = 0;

	virtual bool                      IsDifferent(const FragmentID aaID, const TagState& fragmentTags, const TagID subContext = TAG_ID_INVALID) const = 0;
	virtual FragmentID                GetLastFragmentID() const = 0;
	virtual const SFragTagState& GetLastTagState() const = 0;
	virtual float                CalculateFragmentTimeRemaining() const = 0;
	virtual float                CalculateFragmentDuration(const CFragment& fragment) const = 0;

	virtual float                GetFragmentDuration() const = 0;
	virtual float                GetFragmentTime() const = 0;

	virtual TagState             GetAdditionalTags() const = 0;

	virtual void                 _FlushFromEditor() = 0; // needs to be moved into an editor-only interface

	virtual void                 MuteLayers(uint32 mutedAnimLayerMask, uint32 mutedProcLayerMask) = 0;

	template<typename PODTYPE>
	bool GetParam(const char* paramName, PODTYPE& value) const;
	template<typename PODTYPE>
	bool GetParam(uint32 paramNameCRC, PODTYPE& value) const;
};

enum EActionControllerFlags
{
	AC_PausedUpdate = BIT(0),
	AC_DebugDraw = BIT(1),
	AC_DumpState = BIT(2),
	AC_IsInUpdate = BIT(3),
	AC_NoTransitions = BIT(4),
};

enum EActionFailure
{
	AF_QueueFull,
	AF_InvalidContext,
};

class CMannequinParams
{
public:

	void Reset()
	{
		m_paramList.clear();
	}

	const SMannParameter* FindParam(const char* paramName) const
	{
		const uint32 crc = CCrc32::ComputeLowercase(paramName);
		return FindParam(crc);
	}

	const SMannParameter* FindParam(uint32 paramNameCRC) const
	{
		const uint32 numParams = m_paramList.size();
		for (uint32 i = 0; i < numParams; i++)
		{
			const SMannParameter& param = m_paramList[i];
			if (param.crc == paramNameCRC)
			{
				return &param;
			}
		}

		return NULL;
	}

	template<typename PODTYPE>
	bool GetParam(const char* paramName, PODTYPE& value) const
	{
		const uint32 crc = CCrc32::ComputeLowercase(paramName);
		return GetParam(crc, value);
	}

	template<typename PODTYPE>
	bool GetParam(uint32 paramNameCRC, PODTYPE& value) const
	{
		static_assert(sizeof(PODTYPE) <= sizeof(QuatT), "Invalid type size!");
		const SMannParameter* pParam = FindParam(paramNameCRC);
		if (pParam)
		{
			value = *alias_cast<const PODTYPE*>(&pParam->value);
			return true;
		}

		return false;
	}

	bool RemoveParam(const char* paramName)
	{
		const uint32 crc = CCrc32::ComputeLowercase(paramName);
		return RemoveParam(crc);
	}

	bool RemoveParam(uint32 paramNameCRC)
	{
		const TMannParamList::iterator iEnd = m_paramList.end();
		for (TMannParamList::iterator i = m_paramList.begin(); i != iEnd; ++i)
		{
			const SMannParameter& param = *i;
			if (param.crc == paramNameCRC)
			{
				m_paramList.erase(i);
				return true;
			}
		}
		return false;
	}

	template<typename PODTYPE>
	void SetParam(const char* paramName, const PODTYPE& value)
	{
		const uint32 crc = CCrc32::ComputeLowercase(paramName);
		SetParam(crc, value);
	}

	template<typename PODTYPE>
	void SetParam(uint32 paramNameCRC, const PODTYPE& value)
	{
		static_assert(sizeof(PODTYPE) <= sizeof(QuatT), "Invalid type size!");
		const uint32 numParams = m_paramList.size();
		for (uint32 i = 0; i < numParams; i++)
		{
			SMannParameter& param = m_paramList[i];
			if (param.crc == paramNameCRC)
			{
				*alias_cast<PODTYPE*>(&param.value) = value;
				return;
			}
		}

		m_paramList.resize(numParams + 1);
		SMannParameter& param = m_paramList[numParams];
		param.crc = paramNameCRC;
		*alias_cast<PODTYPE*>(&param.value) = value;
	}

private:
	TMannParamList m_paramList;
};

//! Main interface into an action controller, managing fragment playback on a specific entity
class IActionController
{
public:
	virtual void OnEvent(const SGameObjectEvent& event) = 0;
	//! Should be called when the specified character encounters an animation event
	//! \par Example
	//! \include CryAnimation/Examples/ControllerAnimationEvents.cpp
	virtual void OnAnimationEvent(ICharacterInstance* pCharacter, const AnimEventInstance& event) = 0;

	// Completely resets the state of the action controller
	virtual void Reset() = 0;

	// Flushes all currently playing and queued actions
	virtual void                     Flush() = 0;

	virtual uint32                   GetTotalScopes() const = 0;
	virtual void                     SetScopeContext(uint32 scopeContextID, IEntity& entity, ICharacterInstance* pCharacter, const IAnimationDatabase* animDatabase) = 0;
	virtual void                     ClearScopeContext(uint32 scopeContextID, bool flushAnimations = true) = 0;

	virtual bool                     IsScopeActive(uint32 scopeID) const = 0;
	virtual ActionScopes             GetActiveScopeMask() const = 0;

	virtual IEntity&                 GetEntity() const = 0;
	virtual EntityId                 GetEntityId() const = 0;

	virtual IScope*                  GetScope(uint32 scopeID) = 0;
	virtual const IScope*            GetScope(uint32 scopeID) const = 0;

	virtual uint32                   GetScopeID(const char* name) const = 0;

	virtual FragmentID               GetFragID(uint32 crc) const = 0;
	virtual TagID                    GetGlobalTagID(uint32 crc) const = 0;
	virtual TagID                    GetFragTagID(FragmentID fragID, uint32 crc) const = 0;
	virtual const CTagDefinition*    GetTagDefinition(FragmentID fragID) const = 0;

	//! Queues the specified action / fragment for playback
	//! \par Example
	//! \include CryAnimation/Examples/QueueFragment.cpp
	virtual void                     Queue(IAction& action, float time = -1.0f) = 0;
	virtual void                     Requeue(IAction& action) = 0;

	//! Updates the action controller, should be called by the user each frame for fragments to be played
	virtual void                     Update(float timePassed) = 0;

	virtual SAnimationContext&       GetContext() = 0;
	virtual const SAnimationContext& GetContext() const = 0;

	virtual void                     Pause() = 0;
	enum EResumeFlags
	{
		ERF_RestartAnimations = BIT(0),
		ERF_RestoreLoopingAnimationTime = BIT(1),
		ERF_RestoreNonLoopingAnimationTime = BIT(2),

		ERF_Default = ERF_RestartAnimations | ERF_RestoreLoopingAnimationTime | ERF_RestoreNonLoopingAnimationTime
	};
	virtual void  Resume(uint32 resumeFlags = ERF_Default) = 0;

	virtual void  SetFlag(EActionControllerFlags flag, bool enable) = 0;

	virtual void  SetTimeScale(float timeScale) = 0;
	virtual float GetTimeScale() const = 0;

	// Only needed for animationgraph?
	virtual bool                            IsActionPending(uint32 userToken) const = 0;

	virtual bool                            IsDifferent(const FragmentID fragID, const TagState& fragmentTags, const ActionScopes& scopeMask) const = 0;

	virtual bool                            CanInstall(const IAction& action, const ActionScopes& scopeMask, float timeStep, float& timeTillInstall) const = 0;

	virtual bool                            QueryDuration(IAction& action, float& fragmentDuration, float& transitionDuration) const = 0;

	virtual void                            SetSlaveController(IActionController& target, uint32 targetContext, bool enslave, const IAnimationDatabase* piOptionTargetDatabase) = 0;

	virtual void                            RegisterListener(IMannequinListener* listener) = 0;
	virtual void                            UnregisterListener(IMannequinListener* listener) = 0;

	virtual class IProceduralContext*       FindOrCreateProceduralContext(const CryClassID& contextId) = 0;
	virtual const class IProceduralContext* FindProceduralContext(const CryClassID& contextId) const = 0;
	virtual class IProceduralContext*       FindProceduralContext(const CryClassID& contextId) = 0;
	virtual class IProceduralContext*       CreateProceduralContext(const CryClassID& contextId) = 0;

	virtual QuatT                           ExtractLocalAnimLocation(FragmentID fragID, TagState fragTags, uint32 scopeID, uint32 optionIdx) = 0;

	virtual void                            Release() = 0;

	virtual const SMannParameter*           GetParam(const char* paramName) const = 0;
	virtual const SMannParameter*           GetParam(uint32 paramNameCRC) const = 0;
	virtual bool                            RemoveParam(const char* paramName) = 0;
	virtual bool                            RemoveParam(uint32 paramNameCRC) = 0;
	virtual void                            SetParam(const char* paramName, const SMannParameter& param) = 0;
	virtual void                            SetParam(const SMannParameter& param) = 0;
	virtual void                            ResetParams() = 0;

	template<typename PODTYPE>
	bool GetParam(const char* paramName, PODTYPE& value) const
	{
		const SMannParameter* pParam = GetParam(paramName);
		if (pParam)
		{
			value = *alias_cast<const PODTYPE*>(&pParam->value);
			return true;
		}
		return false;
	}

	template<typename PODTYPE>
	bool GetParam(uint32 paramNameCRC, PODTYPE& value) const
	{
		const SMannParameter* pParam = GetParam(paramNameCRC);
		if (pParam)
		{
			value = *alias_cast<const PODTYPE*>(&pParam->value);
			return true;
		}
		return false;
	}

	template<typename PODTYPE>
	void SetParam(const char* paramName, const PODTYPE& value)
	{
		SMannParameter param;
		param.crc = MannGenCRC(paramName);
		*alias_cast<PODTYPE*>(&param.value) = value;
		return SetParam(paramName, param);
	}

	template<typename PODTYPE>
	void SetParam(uint32 paramNameCRC, const PODTYPE& value)
	{
		SMannParameter param;
		param.crc = paramNameCRC;
		*alias_cast<PODTYPE*>(&param.value) = value;
		return SetParam(param);
	}

protected:
	virtual ~IActionController() {}
};

#define DEFINE_ACTION(name)                                     \
  virtual const char* GetName() const override { return name; } \
  virtual void DoDelete() override { delete this; }

//! Main interface for a Mannequin fragment
class IAction
{
public:
	friend class CActionController;
	friend class CActionScope;

	//---------------------------------------------------------------------------------------------------------
	// Action Status
	//---------------------------------------------------------------------------------------------------------
	// None					- Not installed
	// Pending			- In the pending action queue
	// Installed		- Installed on a scope
	// Exiting			- Action is finishing up, via an exit transition
	// Finished			- Finished. Will be removed from its scope on the the next update
	//---------------------------------------------------------------------------------------------------------
	enum EStatus
	{
		None,
		Pending,
		Installed,
		Exiting,
		Finished
	};

	//---------------------------------------------------------------------------------------------------------
	// Action Flags
	//---------------------------------------------------------------------------------------------------------
	// BlendOut								- Can the action now blend out into a new action?
	// NoAutoBlendOut					- The action should not allow itself to be blended out on completion
	// Interruptable					- The action can be interrupted and so is pushed onto the pending action queue
	//													rather than deleted
	// Installing							- This action is in the process of being installed (Enter is not called yet & action is still in the queue)
	// Requeued								- This action is installed for a requeue on the pending queue
	// TrumpSelf							- This action should be treated as a higher priority when compared to itself
	// Transitioning					- This action is transitioning in
	// PlayingFragment				- This action is playing its core fragment
	// TransitioningOut				- This action is transitioning out
	// TransitionPending			- This action is going to transition off soon
	// FragmentIsOneShot			- This action is a one-shot & so will end itself at the end of the sequence
	// Stopping								- This action is marked for stopping
	//---------------------------------------------------------------------------------------------------------
	enum EFlags
	{
		BlendOut = BIT(0),
		NoAutoBlendOut = BIT(1),
		Interruptable = BIT(2),
		Installing = BIT(4),
		Started = BIT(5),
		Requeued = BIT(6),
		TrumpSelf = BIT(7),
		Transitioning = BIT(8),
		PlayingFragment = BIT(9),
		TransitioningOut = BIT(10),
		TransitionPending = BIT(11),
		FragmentIsOneShot = BIT(12),
		Stopping = BIT(13),
		PlaybackStateMask = (Transitioning | PlayingFragment | TransitioningOut)
	};

	virtual ~IAction()
	{
#ifndef _RELEASE
		if ((m_flags & Started))
		{
			__debugbreak();
		}
#endif //_RELEASE
	}

	IAction(int priority, FragmentID fragmentID = FRAGMENT_ID_INVALID, const TagState& fragTags = TAG_STATE_EMPTY, uint32 flags = 0, ActionScopes scopeMask = 0, uint32 userToken = 0)
		: m_context(NULL)
		, m_activeTime(0.0f)
		, m_queueTime(-1.0f)
		, m_forcedScopeMask(scopeMask)
		, m_installedScopeMask(0)
		, m_subContext(TAG_ID_INVALID)
		, m_priority(priority)
		, m_eStatus(None)
		, m_flags(flags)
		, m_rootScope(NULL)
		, m_fragmentID(fragmentID)
		, m_fragTags(fragTags)
		, m_optionIdx(OPTION_IDX_RANDOM)
		, m_userToken(userToken)
		, m_refCount(0)
		, m_speedBias(1.0f)
		, m_animWeight(1.0f)
	{
	}

	void AddRef()
	{
		m_refCount++;
	}
	void Release()
	{
		m_refCount--;
		if (m_refCount <= 0)
		{
			CRY_ASSERT((m_flags & Started) == 0);

			DoDelete();
		}
	}

	FragmentID GetFragmentID() const
	{
		return m_fragmentID;
	}
	TagState GetFragTagState() const
	{
		return m_fragTags;
	}
	uint32 GetOptionIdx() const
	{
		return m_optionIdx;
	}
	uint32 GetUserToken() const
	{
		return m_userToken;
	}
	int GetPriority() const
	{
		return m_priority;
	}
	void SetOptionIdx(uint32 optionIdx)
	{
		m_optionIdx = optionIdx;
	}

	bool CanBlendOut(EPriorityComparison priorityComparison) const
	{
		switch (priorityComparison)
		{
			case Higher:
				return true;
				break;
			case Lower:
			case Equal:
			default:
				return ((m_flags & (FragmentIsOneShot | NoAutoBlendOut)) == FragmentIsOneShot)
					|| ((m_flags & BlendOut) != 0)
					|| (m_eStatus == Finished)
					|| (m_eStatus == Exiting);
				break;
		}
	}
	ActionScopes GetInstalledScopeMask() const
	{
		return m_installedScopeMask;
	}
	ActionScopes GetForcedScopeMask() const
	{
		return m_forcedScopeMask;
	}
	EStatus GetStatus() const
	{
		return m_eStatus;
	}
	const IScope& GetRootScope() const
	{
		CRY_ASSERT_MESSAGE(m_rootScope, "Action not installed or queued into actionStack!");
		return *m_rootScope;
	}
	IScope& GetRootScope()
	{
		CRY_ASSERT_MESSAGE(m_rootScope, "Action not installed or queued into actionStack!");
		return *m_rootScope;
	}
	uint32 GetFlags() const
	{
		return m_flags;
	}
	bool IsOneShot() const
	{
		return 0 != (m_flags & FragmentIsOneShot);
	}
	float GetActiveTime() const
	{
		return m_activeTime;
	}
	virtual void Install()
	{
		if (m_eStatus != Finished)
		{
			m_eStatus = Installed;
		}
		m_flags &= ~PlaybackStateMask;
	}
	virtual void Enter()
	{
		m_flags |= Started;
	}
	virtual void Fail(EActionFailure actionFailure)
	{
		m_eStatus = None;
		m_flags &= ~Started;
	}
	virtual void Exit()
	{
		m_eStatus = None;
		m_flags &= ~Started;
	}
	virtual EStatus UpdatePending(float timePassed)
	{
		const float oldActiveTime = m_activeTime;
		m_activeTime += timePassed;

		//--- If we have a passed a limited queue time and have ticked at least once, then exit
		if ((m_queueTime >= 0.0f) && (oldActiveTime > 0.0f) && (m_activeTime > m_queueTime))
		{
			m_eStatus = Finished;
		}

		return m_eStatus;
	}
	virtual EStatus Update(float timePassed)
	{
		m_activeTime += timePassed;

		return m_eStatus;
	}

	virtual void OnResolveActionInstallations()
	{
		if (m_fragmentID != FRAGMENT_ID_INVALID)
		{
			const SFragmentDef& fragmentDef = m_context->controllerDef.GetFragmentDef(m_fragmentID);
			if ((fragmentDef.flags & SFragmentDef::AUTO_REINSTALL_BEST_MATCH) != 0)
			{
				if (IsDifferent(m_fragmentID, m_fragTags))
				{
					SetFragment(m_fragmentID, m_fragTags, m_optionIdx, m_userToken, false);
				}
			}
		}
	}

	bool Interrupt()
	{
		if (IsInstalling() || ((m_flags & Started) == 0))
			m_eStatus = None;
		else
			Exit();

		return (m_flags & Interruptable) != 0;
	}

	virtual EPriorityComparison ComparePriority(const IAction& actionCurrent) const
	{
		return Equal;
	}

	virtual void OnRequestBlendOut(EPriorityComparison priorityComp)
	{
	}

	bool IsDifferent(const FragmentID fragID, const TagState& fragmentTags) const;

	void SetSpeedBias(float speedBias)
	{
		m_speedBias = speedBias;
	}
	float GetSpeedBias() const
	{
		return m_speedBias;
	}

	TagID GetSubContext() const
	{
		return m_subContext;
	}
	void SetSubContext(const TagID subContext)
	{
		m_subContext = subContext;
	}

	void SetAnimWeight(float animWeight)
	{
		m_animWeight = animWeight;
	}
	float GetAnimWeight() const
	{
		return m_animWeight;
	}

	virtual void OnSequenceFinished(int layer, uint32 scopeId)
	{
	}

	void Stop()
	{
		m_flags |= (BlendOut | Stopping);

		const uint32 numSlaves = m_slaveActions.size();
		for (uint32 i = 0; i < numSlaves; i++)
		{
			m_slaveActions[i]->Stop();
		}
	}

	void ForceFinish()
	{
		m_eStatus = Finished;
		m_flags &= ~Interruptable;

		ForceFinishSlaveActions();
	}

	void ForceFinishSlaveActions()
	{
		const uint32 numSlaves = m_slaveActions.size();
		for (uint32 i = 0; i < numSlaves; i++)
		{
			m_slaveActions[i]->ForceFinish();
		}
	}

	void BeginInstalling()
	{
		CRY_ASSERT(!IsInstalling());
		m_flags |= Installing;
		m_flags &= ~(PlayingFragment | Transitioning);
	}
	void EndInstalling()
	{
		CRY_ASSERT(IsInstalling());
		m_flags &= ~Installing;
	}
	bool IsInstalling() const
	{
		return ((m_flags & Installing) != 0);
	}

	template<typename PODTYPE>
	bool GetParam(const char* paramName, PODTYPE& value) const
	{
		if (!m_mannequinParams.GetParam(paramName, value))
		{
			if (m_rootScope)
			{
				const IActionController& actionController = m_rootScope->GetActionController();
				return actionController.GetParam(paramName, value);
			}
			else
			{
				return false;
			}
		}
		return true;
	}

	template<typename PODTYPE>
	bool GetParam(uint32 paramNameCRC, PODTYPE& value) const
	{
		if (!m_mannequinParams.GetParam(paramNameCRC, value))
		{
			if (m_rootScope)
			{
				const IActionController& actionController = m_rootScope->GetActionController();
				return actionController.GetParam(paramNameCRC, value);
			}
			else
			{
				return false;
			}
		}
		return true;
	}

	template<typename PODTYPE>
	void SetParam(const char* paramName, const PODTYPE& value)
	{
		return m_mannequinParams.SetParam(paramName, value);
	}

	template<typename PODTYPE>
	void SetParam(uint32 paramNameCRC, const PODTYPE& value)
	{
		return m_mannequinParams.SetParam(paramNameCRC, value);
	}

	void ResetParams()
	{
		m_mannequinParams.Reset();
	}

	bool IsTransitioning() const
	{
		return (m_flags & Transitioning) != 0;
	}
	bool IsPlayingFragment() const
	{
		return (m_flags & PlayingFragment) != 0;
	}
	bool IsTransitioningOut() const
	{
		return (m_flags & TransitioningOut) != 0;
	}

	bool IsStarted() const
	{
		return (m_flags & Started) != 0;
	}

	virtual IAction*    CreateSlaveAction(FragmentID slaveFragID, const TagState& fragTags, SAnimationContext& context) = 0;

	virtual void        OnTransitionStarted() {}
	virtual void        OnFragmentStarted() {}
	virtual void        OnTransitionOutStarted() {}
	virtual void        OnInitialise() {}
	virtual void        OnActionFinished() {}

	virtual void        OnEvent(const SGameObjectEvent& event) {}
	virtual void        OnAnimationEvent(ICharacterInstance* pCharacter, const AnimEventInstance& event) {}
	virtual void        OnActionEvent(const uint32 eventCRC) {}

	virtual const char* GetName() const = 0;
	virtual void        DoDelete() = 0;

	IAction*            GetSlaveAction(uint32 i)
	{
		return m_slaveActions[i].get();
	}
	uint32 GetTotalSlaveActions() const
	{
		return m_slaveActions.size();
	}

protected:

	void SetFragment(const FragmentID fragmentID, const TagState& tagState = TAG_STATE_EMPTY, uint32 optionIdx = OPTION_IDX_RANDOM, const uint32 userToken = 0, bool trumpSelf = true)
	{
		m_fragmentID = fragmentID;
		m_fragTags = tagState;
		m_optionIdx = optionIdx;
		m_userToken = userToken;
		if (trumpSelf)
			m_flags |= TrumpSelf;
		else
			m_flags &= ~TrumpSelf;

		if (m_eStatus == Installed)
		{
			if ((m_flags & Requeued) == 0)
			{
				m_flags |= Requeued;

				IActionController& actionController = m_rootScope->GetActionController();

				actionController.Requeue(*this);
			}
		}
	}

	EPriorityComparison DoComparePriority(const IAction& actionCurrent) const
	{
		if ((&actionCurrent == this) && ((m_flags & TrumpSelf) != 0))
		{
			return Higher;
		}
		else if (m_priority > actionCurrent.m_priority)
		{
			return Higher;
		}
		else if (m_priority == actionCurrent.m_priority)
		{
			return ComparePriority(actionCurrent);
		}
		return Lower;
	}

	SAnimationContext* m_context;
	float              m_activeTime;
	float              m_queueTime;
	ActionScopes       m_forcedScopeMask;
	ActionScopes       m_installedScopeMask;
	TagID              m_subContext;
	int                m_priority;
	EStatus            m_eStatus;
	uint32             m_flags;
	IScope*            m_rootScope;
	FragmentID         m_fragmentID;
	TagState           m_fragTags;
	uint32             m_optionIdx;
	uint32             m_userToken;
	int                m_refCount;
	float              m_speedBias;
	float              m_animWeight;

	CMannequinParams   m_mannequinParams;

private:
	void TransitionStarted()
	{
		m_flags &= ~PlaybackStateMask;
		m_flags |= Transitioning;

		OnTransitionStarted();
	}

	void FragmentStarted()
	{
		m_flags &= ~PlaybackStateMask;
		m_flags |= PlayingFragment;

		OnFragmentStarted();
	}

	void TransitionOutStarted()
	{
		m_flags &= ~PlaybackStateMask;
		m_flags |= TransitioningOut;

		OnTransitionOutStarted();
	}

	void Initialise(SAnimationContext& context)
	{
		m_context = &context;
		m_eStatus = Pending;
		m_rootScope = NULL;
		m_flags &= ~Started;

		m_activeTime = 0.0f;

		OnInitialise();
	}

	DynArray<_smart_ptr<IAction>> m_slaveActions;
};

template<class CONTEXT>
class TAction : public IAction
{
public:

	DEFINE_ACTION("BaseAction");

	TAction(int priority, FragmentID fragmentID = FRAGMENT_ID_INVALID, const TagState& fragTags = TAG_STATE_EMPTY, uint32 flags = 0, ActionScopes scopeMask = 0, uint32 userToken = 0)
		:
		IAction(priority, fragmentID, fragTags, flags, scopeMask, userToken)
	{
	}

	virtual IAction* CreateSlaveAction(FragmentID slaveFragID, const TagState& fragTags, SAnimationContext& context) override
	{
		ActionScopes forceScopeMask = (slaveFragID == FRAGMENT_ID_INVALID) ? ACTION_SCOPES_ALL : ACTION_SCOPES_NONE;
		return new TAction<CONTEXT>(GetPriority(), slaveFragID, fragTags, 0, forceScopeMask);
	}

	CONTEXT& GetContext()
	{
		return *((CONTEXT*)m_context);
	}
	const CONTEXT& GetContext() const
	{
		return *((const CONTEXT*)m_context);
	}
};

template<typename PODTYPE>
bool IScope::GetParam(const char* paramName, PODTYPE& value) const
{
	IActionPtr pAction = GetAction();
	return pAction ? pAction->GetParam(paramName, value) : false;
}

template<typename PODTYPE>
bool IScope::GetParam(uint32 paramNameCRC, PODTYPE& value) const
{
	IAction* pAction = GetAction();
	return pAction ? pAction->GetParam(paramNameCRC, value) : false;
}

class CMannequinUserParamsManager;
struct IProceduralClipFactory;

//! Main interface to the Mannequin implementation
struct IMannequin
{
	virtual ~IMannequin() {}

	virtual void                         UnloadAll() = 0;
	virtual void                         ReloadAll() = 0;

	virtual IAnimationDatabaseManager&   GetAnimationDatabaseManager() = 0;
	//! Creates an action controller for the specified entity
	//! \par Example
	//! \include CryAnimation/Examples/CreateActionController.cpp
	virtual IActionController*           CreateActionController(IEntity* pEntity, SAnimationContext& context) = 0;
	virtual IActionController*           FindActionController(const IEntity& entity) = 0;
	virtual IMannequinEditorManager*     GetMannequinEditorManager() = 0;
	virtual CMannequinUserParamsManager& GetMannequinUserParamsManager() = 0;
	virtual IProceduralClipFactory&      GetProceduralClipFactory() = 0;

	virtual void                         AddMannequinGameListener(IMannequinGameListener* pListener) = 0;
	virtual void                         RemoveMannequinGameListener(IMannequinGameListener* pListener) = 0;
	virtual uint32                       GetNumMannequinGameListeners() = 0;
	virtual IMannequinGameListener*      GetMannequinGameListener(uint32 idx) = 0;
	// Indicates if the mouse is doing something (select / drag / move / etc...)
	virtual void                         SetSilentPlaybackMode(bool bSilentPlaybackMode) = 0;
	virtual bool                         IsSilentPlaybackMode() const = 0;
};

bool ILINE IAction::IsDifferent(const FragmentID fragID, const TagState& fragmentTags) const
{
	IActionController& actionController = GetRootScope().GetActionController();
	return actionController.IsDifferent(fragID, fragmentTags, m_installedScopeMask);
}

class IProceduralClip;
DECLARE_SHARED_POINTERS(IProceduralClip);

class IProceduralClip
{
public:
	IProceduralClip()
		:
		m_entity(NULL),
		m_charInstance(NULL),
		m_scope(NULL),
		m_action(NULL)
	{
	}

	virtual ~IProceduralClip() {}

	virtual void Initialise(IEntity& entity, ICharacterInstance& charInstance, IScope& scope, IAction& action)
	{
		m_entity = &entity;
		m_charInstance = &charInstance;
		m_scope = &scope;
		m_action = &action;
	}

	virtual void        INTERNAL_OnEnter(float blendTime, float duration, const IProceduralParamsPtr& pProceduralParams) = 0;
	virtual void        OnFail() {}
	virtual void        OnExit(float blendTime) = 0;
	virtual void        Update(float timePassed) = 0;
	virtual const CryClassID GetContextID() const
	{
		static CryClassID null = CryClassID::Null();
		return null;
	}
	virtual void        SetContext(class IProceduralContext* procContext) { CRY_ASSERT(0); }

protected:
	template<typename PODTYPE>
	bool GetParam(const char* paramName, PODTYPE& value) const
	{
		CRY_ASSERT(m_action != 0);
		return m_action->GetParam(paramName, value);
	}

	template<typename PODTYPE>
	bool GetParam(uint32 paramNameCRC, PODTYPE& value) const
	{
		CRY_ASSERT(m_action != 0);
		return m_action->GetParam(paramNameCRC, value);
	}

	bool IsRootEntity() const
	{
		CRY_ASSERT(m_action != 0);
		return (m_scope == &m_action->GetRootScope()) || (m_scope->GetEntityId() != m_action->GetRootScope().GetEntityId());
	}

	void SendActionEvent(const uint32 eventCRC) const
	{
		CRY_ASSERT(m_action != 0);

		if (eventCRC != 0 && IsRootEntity())
		{
			m_action->OnActionEvent(eventCRC);
		}
	}

	ActionScopes GetActionInstalledScopeMask()
	{
		return m_action->GetInstalledScopeMask();
	}

protected:
	IEntity*            m_entity;
	ICharacterInstance* m_charInstance;
	IScope*             m_scope;

private:
	IActionPtr m_action;
};

#define PROCEDURAL_CONTEXT(className, name, guid) \
  CRYINTERFACE_BEGIN()                            \
  CRYINTERFACE_ADD(IProceduralContext)            \
  CRYINTERFACE_END()                              \
  CRYGENERATE_CLASS_GUID(className, name, guid)

class IProceduralContext : public ICryUnknown
{
public:
	IProceduralContext()
		:
		m_entity(NULL),
		m_actionController(NULL)
	{
	}

	CRYINTERFACE_DECLARE_GUID(IProceduralContext, "cc61bc28-4b52-43e0-aae3-950b2a7f7dcb"_cry_guid);

	virtual void Initialise(IEntity& entity, IActionController& actionController)
	{
		m_entity = &entity;
		m_actionController = &actionController;
	}

	virtual void Update(float timePassed) = 0;

protected:
	IEntity*           m_entity;
	IActionController* m_actionController;
};

template<class PARAMS = SNoProceduralParams>
class TProceduralClip : public IProceduralClip
{
public:
	typedef PARAMS TParamsType;

	virtual void INTERNAL_OnEnter(float blendTime, float duration, const IProceduralParamsPtr& pProceduralParams)
	{
		CRY_ASSERT(pProceduralParams.get());
		m_params = *(static_cast<const PARAMS*>(pProceduralParams.get()));
		OnEnter(blendTime, duration, m_params);
	}

	ILINE const PARAMS& GetParams() const
	{
		return m_params;
	}

	virtual void OnEnter(float blendTime, float duration, const PARAMS& proceduralParams) = 0;

private:
	PARAMS m_params;
};

template<class CONTEXT, class PARAMS = SNoProceduralParams>
class TProceduralContextualClip : public TProceduralClip<PARAMS>
{
public:

	virtual const CryClassID GetContextID() const
	{
		return CONTEXT::GetCID();
	}
	virtual void SetContext(class IProceduralContext* procContext)
	{
		m_context = (CONTEXT*)procContext;
	}

protected:
	CONTEXT* m_context;
};

//////////////////////////////////////////////////////////////////////////
// Specify a SFragmentQuery to get all animations cached that are contained in
// fragments that match the query.
class CFragmentCache
{
public:
	// This will not precache anything, you must call AddAllAnimsFromAllScopes or AddAnimCRCs.
	explicit CFragmentCache(const SFragmentQuery& fragmentQuery)
		: m_fragmentQuery(fragmentQuery)
		, m_numOptions(OPTION_IDX_INVALID)
	{
	}
	// Automatically precaches everything in the actioncontroller that matches the query.
	CFragmentCache(const SFragmentQuery& fragmentQuery, const IActionController* piActionController)
		: m_fragmentQuery(fragmentQuery)
		, m_numOptions(OPTION_IDX_INVALID)
	{
		PrecacheAnimsFromAllDatabases(piActionController);
	}
	// Copies an existing fragment cache but allows you to update the option - useful for round robins.
	CFragmentCache(const CFragmentCache& fragmentCache, const IActionController* piActionController, const uint32 optionIdx)
		: m_fragmentQuery(fragmentCache.m_fragmentQuery)
		, m_numOptions(OPTION_IDX_INVALID)
	{
		m_fragmentQuery.optionIdx = optionIdx;

		PrecacheAnimsFromAllDatabases(piActionController);
	}
	~CFragmentCache()
	{
		Release();
	}
	void PrecacheAnimsFromAllDatabases(const IActionController* piActionController)
	{
		const uint32 numScopes = piActionController->GetTotalScopes();
		for (uint32 i = 0; i < numScopes; ++i)
		{
			// TODO: We should defend adding the same ADB more than once.
			const IScope* piScope = piActionController->GetScope(i);
			if (piScope->HasDatabase())
			{
				const IAnimationDatabase& animationDB = piScope->GetDatabase();
				PrecacheAnimsFromDatabase(&animationDB, piActionController);
			}
		}
	}
	void PrecacheAnimsFromDatabase(const IAnimationDatabase* pAnimDB, const IActionController* piActionController)
	{
		SFragTagState fragTagStateMatch;

		// TODO: this should use Union, see for example CActionScope::FillBlendQuery.
		SFragTagState fragTagStateQuery(m_fragmentQuery.tagState.globalTags, m_fragmentQuery.tagState.fragmentTags);
		uint32 tagSetID;
		const uint32 numOptions = pAnimDB->FindBestMatchingTag(SFragmentQuery(m_fragmentQuery.fragID, fragTagStateQuery), &fragTagStateMatch, &tagSetID);

		if ((numOptions > 0))
		{
			if (m_numOptions == OPTION_IDX_INVALID)
			{
				m_numOptions = numOptions;
			}
			else
			{
				m_numOptions = max(m_numOptions, numOptions);
			}
		}

		if (m_numOptions != OPTION_IDX_INVALID)
		{
			const CTagDefinition* pTagDef = pAnimDB->GetFragmentDefs().GetSubTagDefinition(m_fragmentQuery.fragID);
			if (pTagDef && pTagDef->Contains(m_fragmentQuery.tagState.fragmentTags, fragTagStateMatch.fragmentTags))
			{
				// TODO: get the charinstance from the scope.
				ICharacterInstance* piCharacterInstance = piActionController->GetEntity().GetCharacter(0);
				const IAnimationSet* piAnimationSet = piCharacterInstance->GetIAnimationSet();

				if (m_fragmentQuery.optionIdx == OPTION_IDX_RANDOM)
				{
					for (uint i = 0; i < m_numOptions; ++i)
					{
						const CFragment* pFragment = pAnimDB->GetEntry(m_fragmentQuery.fragID, tagSetID, i);
						if (pFragment)
						{
							AddFragment(*pFragment, piAnimationSet);
						}
					}
				}
				else if (m_fragmentQuery.optionIdx < m_numOptions)
				{
					const CFragment* pFragment = pAnimDB->GetEntry(m_fragmentQuery.fragID, tagSetID, m_fragmentQuery.optionIdx);
					if (pFragment)
					{
						AddFragment(*pFragment, piAnimationSet);
					}
				}
			}
		}
	}
	void Release()
	{
		m_animsCached.clear();
		stl::free_container(m_animsCached);
	}

	bool IsLoaded() const
	{
		return(m_animsCached.end() == std::find_if(m_animsCached.begin(), m_animsCached.end(), FPredNotLoaded()));
	}

	uint32 GetNumOptions() const { return m_numOptions; }
	uint32 GetCurrentOption() const { return m_fragmentQuery.optionIdx; }

private:

	struct SCacheAnims
	{
		explicit SCacheAnims(const uint32 crc)
			: m_crc(crc)
		{
			gEnv->pCharacterManager->CAF_AddRef(m_crc);
		}
		SCacheAnims(const SCacheAnims& rhs)
			: m_crc(rhs.m_crc)
		{
			gEnv->pCharacterManager->CAF_AddRef(m_crc);
		}
		~SCacheAnims()
		{
			gEnv->pCharacterManager->CAF_Release(m_crc);
		}

		SCacheAnims& operator=(const SCacheAnims& anim)
		{
			if (m_crc != anim.m_crc)
			{
				gEnv->pCharacterManager->CAF_Release(m_crc);
				m_crc = anim.m_crc;
				gEnv->pCharacterManager->CAF_AddRef(m_crc);
			}
			return *this;
		}

		bool IsLoaded() const { return gEnv->pCharacterManager->CAF_IsLoaded(m_crc); }

		uint32 m_crc;
	};

	struct FPredNotLoaded
	{
		bool operator()(const SCacheAnims& anim) const { return !anim.IsLoaded(); }
	};

	void AddFragment(const CFragment& fragment, const IAnimationSet* piAnimationSet)
	{
		const DynArray<TAnimClipSequence>& animLayers = fragment.m_animLayers;
		for (int i = 0; i < animLayers.size(); ++i)
		{
			const TAnimClipSequence& sequence = animLayers[i];
			for (int j = 0; j < sequence.size(); ++j)
			{
				const SAnimClip& animClip = sequence[j];
				const int animID = piAnimationSet->GetAnimIDByCRC(animClip.animation.animRef.crc);
				const uint32 filePathCRC = piAnimationSet->GetFilePathCRCByAnimID(animID);

				const char* pAnimPath = piAnimationSet->GetFilePathByID(animID);

				// CharacterManager needs the filePathCRC
				m_animsCached.push_back(SCacheAnims(filePathCRC));
			}
		}
	}

	SFragmentQuery m_fragmentQuery;
	uint32         m_numOptions;

	typedef std::vector<SCacheAnims> TAnimsCached;
	TAnimsCached m_animsCached;
};

#include "ICryMannequinUserParams.h"