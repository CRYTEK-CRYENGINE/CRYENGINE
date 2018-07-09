// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MannequinBase_h__
#define __MannequinBase_h__

#include "ICryMannequin.h"
#include <CryInput/IInput.h>

class IAnimationDatabase;
class CSequencerTrack;
class CSequencerNode;
class CSequencerSequence;
struct IMannequinEditorManager;

const uint32 HISTORY_ITEM_INVALID = 0xffffffff;
const uint32 CONTEXT_DATA_NONE = 0xffffffff;

enum EMannequinEditorMode
{
	eMEM_FragmentEditor = 0,
	eMEM_Previewer,
	eMEM_TransitionEditor,
	eMEM_Max
};

struct SScopeContextViewData
{
	SScopeContextViewData() :
		charInst(NULL),
		pStatObj(NULL),
		entity(NULL),
		oldAttachmentLoc(IDENTITY),
		m_pAnimContext(NULL),
		m_pActionController(NULL),
		enabled(false)
	{
	}

	_smart_ptr<ICharacterInstance>  charInst;
	IStatObj*                       pStatObj;
	IEntity*                        entity;
	QuatT                           oldAttachmentLoc;
	struct SEditorAnimationContext* m_pAnimContext;
	class IActionController*        m_pActionController;
	bool                            enabled;
};

struct SScopeContextData
{
	SScopeContextData() :
		database(NULL),
		animSet(NULL),
		pControllerDef(NULL),
		pSlaveDatabase(NULL),
		dataID(0),
		contextID(CONTEXT_DATA_NONE),
		startLocation(IDENTITY),
		startRotationEuler(ZERO),
		tags(TAG_STATE_EMPTY),
		changeCount(0),
		startActive(false)
	{
	}

	void SetCharacter(EMannequinEditorMode editorMode, ICharacterInstance* charInst)
	{
		viewData[editorMode].charInst = charInst;
		viewData[editorMode].charInst->SetCharEditMode(CA_CharacterTool);
		animSet = charInst->GetIAnimationSet();
	}

	bool CreateCharInstance(const char* modelName)
	{
		for (uint32 i = 0; i < eMEM_Max; i++)
		{
			SScopeContextViewData& data = viewData[i];

			data.pStatObj = NULL;
			data.charInst = gEnv->pCharacterManager->CreateInstance(modelName, CA_CharEditModel);

			if (data.charInst)
			{
				data.charInst->SetCharEditMode(CA_CharacterTool);
				animSet = data.charInst->GetIAnimationSet();
			}
			else
			{
				return false;
			}
		}

		return true;
	}

	bool CreateStatObj(const char* modelName)
	{
		for (uint32 i = 0; i < eMEM_Max; i++)
		{
			SScopeContextViewData& data = viewData[i];

			data.pStatObj = gEnv->p3DEngine->LoadStatObj(modelName);
			data.charInst = NULL;

			if (!data.pStatObj)
				return false;
		}
		return true;
	}

	bool operator==(const SScopeContextData& rhs) const
	{
		return rhs.dataID == dataID;
	}

	CString                   name;
	CString                   scopes;
	IAnimationDatabase*       database;
	IAnimationSet*            animSet;
	const SControllerDef*     pControllerDef;
	const IAnimationDatabase* pSlaveDatabase;
	uint32                    dataID;
	uint32                    contextID;
	string                    attachment;
	QuatT                     startLocation;
	Ang3                      startRotationEuler; // The rotations from file restored after previewing
	string                    attachmentContext;
	string                    attachmentHelper;
	TagState                  tags;
	string                    fragTags;
	uint32                    changeCount;
	bool                      startActive; // should the anim start active (default: false)
	SScopeContextViewData     viewData[eMEM_Max];
};

struct SScopeData
{
	SScopeData()
		: scopeID(0)
		, contextID(CONTEXT_DATA_NONE)
		, layer(0)
		, numLayers(0)
		, mannContexts(NULL)
	{
		for (uint32 i = 0; i < eMEM_Max; i++)
		{
			context[i] = NULL;
			fragTrack[i] = NULL;
			animNode[i] = NULL;
		}
	}

	CString                    name;
	uint32                     scopeID;
	uint32                     contextID;
	int                        layer;
	int                        numLayers;
	SScopeContextData*         context[eMEM_Max];
	class CFragmentTrack*      fragTrack[eMEM_Max];
	CSequencerNode*            animNode[eMEM_Max];
	struct SMannequinContexts* mannContexts;
};

struct SMannequinContextViewData
{
	typedef std::vector<_smart_ptr<CBaseObject>> TBackgroundObjects;

	SMannequinContextViewData()
		: m_pEntity(NULL)
		, m_pAnimContext(NULL)
		, m_pActionController(NULL)
	{
	}

	IEntity*                        m_pEntity;
	struct SEditorAnimationContext* m_pAnimContext;
	class IActionController*        m_pActionController;
	TBackgroundObjects              backgroundObjects;
	std::vector<int>                backgroundProps;
};

struct SMannequinContexts
{
	struct SProp
	{
		CString name;
		CString entity;
		int     entityID[eMEM_Max];
	};

	SMannequinContexts()
		: m_controllerDef(NULL)
		, potentiallyActiveScopes(ACTION_SCOPES_ALL)
		, backgroundRootTran(IDENTITY)
	{
	}

	SScopeContextData*       GetContextDataForID(const SFragTagState& fragTagState, FragmentID fragID, uint32 contextID, EMannequinEditorMode editorMode);
	SScopeContextData*       GetContextDataForID(TagState state, uint32 contextID, EMannequinEditorMode editorMode);

	const SScopeContextData* GetContextDataForID(TagState state, uint32 contextID, EMannequinEditorMode editorMode) const
	{
		return const_cast<SMannequinContexts*>(this)->GetContextDataForID(state, contextID, editorMode);
	}

	string                         previewFilename;
	const SControllerDef*          m_controllerDef;
	std::vector<SScopeData>        m_scopeData;
	std::vector<SScopeContextData> m_contextData;
	ActionScopes                   potentiallyActiveScopes;
	XmlNodeRef                     backGroundObjects;
	std::vector<SProp>             backgroundProps;
	QuatT                          backgroundRootTran;
	SMannequinContextViewData      viewData[eMEM_Max];
};

class CFragmentHistory
{
public:
	CFragmentHistory(IActionController& actionController)
		:
		m_firstTime(0.0f),
		m_startTime(0.0f),
		m_endTime(0.0f),
		m_actionController(actionController)
	{
	}

	void LoadSequence(const char* sequence);
	void LoadSequence(XmlNodeRef root);
	void LoadSequence(const int scopeContextIdx, const FragmentID fromID, const FragmentID toID, const SFragTagState fromFragTag, const SFragTagState toFragTag, const SFragmentBlendUid blendUid, const TagState& tagState);
	void SaveSequence(const char* filename);

	void FillInTagStates()
	{
		TagState tags = TAG_STATE_EMPTY;
		for (THistoryBuffer::iterator iter = m_items.begin(); iter != m_items.end(); iter++)
		{
			SHistoryItem& item = *iter;
			if (item.type == SHistoryItem::Tag)
			{
				tags = item.tagState.globalTags;
			}
			else
			{
				item.tagState.globalTags = tags;
			}
		}
	}

	void UpdateScopeMasks(const SMannequinContexts* contexts, const TagState& globalTags);

	struct SHistoryItem
	{
		enum EType
		{
			Fragment,
			Tag,
			Param,
			None
		};

		SHistoryItem()
			:
			time(-1.0f),
			type(None),
			loadedScopeMask(0),
			fragment(FRAGMENT_ID_INVALID),
			fragOptionIdx(OPTION_IDX_RANDOM),
			fragOptionIdxSel(OPTION_IDX_RANDOM),
			installedScopeMask(0),
			startTime(-1.0f),
			trumpsPrevious(false),
			isLocation(false)
		{
		}

		SHistoryItem(ActionScopes _scopeMask, FragmentID _fragment)
			:
			time(-1.0f),
			type(Fragment),
			loadedScopeMask(_scopeMask),
			fragment(_fragment),
			fragOptionIdx(OPTION_IDX_RANDOM),
			fragOptionIdxSel(OPTION_IDX_RANDOM),
			installedScopeMask(0),
			startTime(-1.0f),
			trumpsPrevious(false),
			isLocation(false)
		{
		}

		SHistoryItem(const TagState& _tagState)
			:
			time(-1.0f),
			type(Tag),
			tagState(_tagState),
			loadedScopeMask(0),
			fragment(FRAGMENT_ID_INVALID),
			fragOptionIdx(OPTION_IDX_RANDOM),
			fragOptionIdxSel(OPTION_IDX_RANDOM),
			installedScopeMask(0),
			startTime(-1.0f),
			trumpsPrevious(false),
			isLocation(false)
		{
		}

		SHistoryItem(const char* _paramName, const SMannParameter& _mannParam)
			:
			time(-1.0f),
			type(Param),
			loadedScopeMask(0),
			fragment(FRAGMENT_ID_INVALID),
			param(_mannParam),
			paramName(_paramName),
			installedScopeMask(0),
			startTime(-1.0f),
			trumpsPrevious(false),
			isLocation(false)
		{
		}
		float          time;
		EType          type;
		SFragTagState  tagState;
		ActionScopes   loadedScopeMask;
		FragmentID     fragment;
		uint32         fragOptionIdx;
		uint32         fragOptionIdxSel;
		SMannParameter param;
		CString        paramName;
		ActionScopes   installedScopeMask;
		float          startTime;
		bool           trumpsPrevious;
		bool           isLocation;
	};

	const SHistoryItem* FindLastByType(SHistoryItem::EType type, float endTime) const
	{
		const SHistoryItem* ret = NULL;
		for (THistoryBuffer::const_iterator iter = m_items.begin(); (iter != m_items.end()) && (iter->time <= endTime); iter++)
		{
			const SHistoryItem& item = *iter;
			if (item.type == type)
			{
				ret = &item;
			}
		}

		return ret;
	}

	typedef std::vector<SHistoryItem> THistoryBuffer;

	THistoryBuffer     m_items;
	float              m_firstTime;
	float              m_startTime;
	float              m_endTime;

	IActionController& m_actionController;
};

class CFragmentTrack;

struct SFragmentHistoryContext
{
	SFragmentHistoryContext(IActionController& actionController)
		:
		m_history(actionController)
	{
	}

	CFragmentHistory              m_history;
	std::vector<CSequencerTrack*> m_tracks;
};

struct SClipTrackContext
{
	SClipTrackContext()
		:
		context(NULL),
		historyItem(0xffffffff),
		scopeID(0)
	{
	}
	SScopeContextData* context;
	SFragTagState      tagState;
	uint32             historyItem;
	uint32             scopeID;
};

class CTagControl
{
public:
	CTagControl()
		:
		m_tagDef(NULL)
	{
	}

	void     Init(CVarBlock* pVarBlock, const CTagDefinition& tagDef, TagState enabledControls = TAG_STATE_FULL);
	void     Set(const TagState& tagState) const;
	TagState Get() const;

	void     SetCallback(IVariable::OnSetCallback func);

	const CTagDefinition*                m_tagDef;
	std::vector<CSmartVariable<bool>>    m_tagVarList;
	std::vector<CSmartVariableEnum<int>> m_tagGroupList;
};

struct SLastChange
{
	SLastChange()
		:
		lastChangeCount(0),
		changeTime(0.0f),
		dirty(0)
	{}

	uint32 lastChangeCount;
	float  changeTime;
	bool   dirty;
};

namespace MannUtils
{
bool                     InsertClipTracksToNode(CSequencerNode* node, const SFragmentData& fragData, float startTime, float endTime, float& maxTime, const SClipTrackContext& trackContext);
void                     GetFragmentFromClipTracks(CFragment& fragment, CSequencerNode* animNode, uint32 historyNode = HISTORY_ITEM_INVALID, float fragStartTime = 0.0f, int fragPart = 0);

void                     InsertFragmentTrackFromHistory(CSequencerNode* animNode, SFragmentHistoryContext& historyContext, float startTime, float endTime, float& maxTime, SScopeData& scopeData);
void                     GetHistoryFromTracks(SFragmentHistoryContext& historyContext);

void                     FlagsToTagList(char* tagList, uint32 bufferSize, const SFragTagState& tagState, FragmentID fragID, const SControllerDef& def, const char* emptyString = NULL);
void                     SerializeFragTagState(SFragTagState& tagState, const SControllerDef& contDef, const FragmentID fragID, XmlNodeRef& xmlNode, bool bLoading);

bool                     IsSequenceDirty(SMannequinContexts* contexts, CSequencerSequence* sequence, SLastChange& lastChange, EMannequinEditorMode editorMode);

void                     SaveSplitterToXml(const XmlNodeRef& xmlLayout, const CString& name, const CSplitterWnd& splitter);
void                     LoadSplitterFromXml(const XmlNodeRef& xmlLayout, const CString& name, CSplitterWnd& splitter, int min = 0);
void                     SaveCheckboxToXml(const XmlNodeRef& xmlLayout, const CString& name, const CButton& checkbox);
void                     LoadCheckboxFromXml(const XmlNodeRef& xmlLayout, const CString& name, CButton& checkbox);
void                     SaveDockingPaneToXml(const XmlNodeRef& xmlLayout, const CString& name, const CXTPDockingPane& dockingPane);
void                     LoadDockingPaneFromXml(const XmlNodeRef& xmlLayout, const CString& name, CXTPDockingPane& dockingPane, CXTPDockingPaneManager* dockingPaneManager);
void                     SaveWindowSizeToXml(const XmlNodeRef& xmlLayout, const CString& name, const CRect& windowRect);
void                     LoadWindowSizeFromXml(const XmlNodeRef& xmlLayout, const CString& name, CRect& windowRect);

EMotionParamID           GetMotionParam(const char* paramName);
void                     AdjustFragDataForVEGs(SFragmentData& fragData, SScopeContextData& context, EMannequinEditorMode editorMode, float motionParams[eMotionParamID_COUNT]);

const char*              GetEmptyTagString();

const char*              GetMotionParamByIndex(size_t index);

void                     AppendTagsFromAnimName(const string& animName, SFragTagState& tagState);

IMannequinEditorManager& GetMannequinEditorManager();

size_t                   GetParamNameCount();
const char*              GetParamNameByIndex(size_t index);
};

struct SEditorAnimationContext : public SAnimationContext
{
	SEditorAnimationContext(class CMannequinDialog* mannequinDialog, const SControllerDef& controllerDef)
		:
		SAnimationContext(controllerDef),
		mannequinDialog(mannequinDialog)
	{
	}
	class CMannequinDialog* mannequinDialog;
};

typedef TAction<SEditorAnimationContext> CBasicAction;

class CActionInputHandler : public IInputEventListener
{
public:
	CActionInputHandler() :
		m_leftThumb(0.0f, 0.0f),
		m_rightThumb(0.0f, 0.0f),
		m_speedMax(10.0f),
		m_turnAngle(0.0f),
		m_moveSpeed(1.0f),
		m_stanceChangeSign(1.f),
		m_stanceChange(false)
	{
		gEnv->pInput->AddEventListener(this);
	}

	~CActionInputHandler()
	{
		gEnv->pInput->RemoveEventListener(this);
	}

	virtual bool OnInputEvent(const SInputEvent& event)
	{
		EKeyId inputKey = event.keyId;
		if (event.deviceType == eIDT_Gamepad)
		{
			switch (inputKey)
			{
			case eKI_XI_ThumbLY:
				m_leftThumb.y = event.value;
				m_moveSpeed = m_leftThumb.GetLength() * m_speedMax;
				m_turnAngle = atan2_tpl(m_leftThumb.x, -m_leftThumb.y);
				break;

			case eKI_XI_ThumbLX:
				m_leftThumb.x = event.value;
				m_moveSpeed = m_leftThumb.GetLength() * m_speedMax;
				m_turnAngle = atan2_tpl(m_leftThumb.x, -m_leftThumb.y);
				break;

			case eKI_XI_ThumbRX:
				m_rightThumb.x = event.value;
				break;

			case eKI_XI_ThumbRY:
				m_rightThumb.y = event.value;
				break;

			case eKI_XI_B:
				if (event.state & eIS_Pressed)
				{
					m_stanceChange = true;
					m_stanceChangeSign *= -1.f;
				}
				break;
			}
		}

		return false;
	}

	void ResetLockedValues()
	{
		for (uint32 i = 0; i < eMotionParamID_COUNT; i++)
		{
			m_locked[i] = false;
		}
	}

	void LockMotionParam(EMotionParamID paramID, float value)
	{
		m_locked[paramID] = true;
		m_lockedValues[paramID] = value;
	}

	void SetMotionParam(ISkeletonAnim& skelAnim, EMotionParamID paramID, float value)
	{
		if (!m_locked[paramID])
		{
			skelAnim.SetDesiredMotionParam(paramID, value, 0.0f);
		}
	}

	void UpdateInput(ICharacterInstance* pChar, float timePassed, IActionController* pAction)
	{
		if (pChar)
		{
			ISkeletonAnim& skelAnim = *pChar->GetISkeletonAnim();
			SetMotionParam(skelAnim, eMotionParamID_TravelSpeed, m_moveSpeed);
			SetMotionParam(skelAnim, eMotionParamID_TravelAngle, m_turnAngle);

			for (int32 i = 0; i < eMotionParamID_COUNT; i++)
			{
				if (m_locked[i])
				{
					skelAnim.SetDesiredMotionParam((EMotionParamID)i, m_lockedValues[i], 0.0f);
				}
			}
		}

		pAction->SetParam("aim_direction", Vec3(0.f, 1.f, 0.f));
		pAction->SetParam("input_rot", Ang3(m_rightThumb.y * gf_PI, 0.f, m_rightThumb.x * gf_PI));

		Vec3 velocity(0.f, 0.f, 0.f);

		if (!m_leftThumb.IsZero())
		{
			velocity.Set(m_leftThumb.x, m_leftThumb.y, 0.f);
			velocity.Normalize();
		}

		pAction->SetParam("velocity", velocity * m_moveSpeed);
		pAction->SetParam("input_move", velocity);
		pAction->SetParam("runfactor", m_leftThumb.GetLength());
		pAction->SetParam("stanceChanged", m_stanceChange ? m_stanceChangeSign : 0.f);

		m_stanceChange = false;
	}

public:
	Vec2  m_leftThumb;
	Vec2  m_rightThumb;
	float m_speedMax;
	float m_turnAngle;
	float m_moveSpeed;
	float m_stanceChangeSign;
	bool  m_stanceChange;

	float m_lockedValues[eMotionParamID_COUNT];
	bool  m_locked[eMotionParamID_COUNT];
};

#endif //!__MannequinBase_h__

