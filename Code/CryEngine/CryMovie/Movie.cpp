// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   Movie.cpp
//  Created:     23/4/2002 by Timur.
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "Movie.h"
#include "AnimSplineTrack.h"
#include "AnimSequence.h"
#include "EntityNode.h"
#include "CVarNode.h"
#include "ScriptVarNode.h"
#include "AnimCameraNode.h"
#include "SceneNode.h"
#include "MaterialNode.h"
#include "EventNode.h"
#include "AnimPostFXNode.h"
#include "AnimScreenFaderNode.h"
#include "CommentNode.h"
#include "LayerNode.h"
#include "ShadowsSetupNode.h"
#include "AudioNode.h"

#include <CryCore/StlUtils.h>

#include <CrySystem/ISystem.h>
#include <CrySystem/ILog.h>
#include <CrySystem/IConsole.h>
#include <CrySystem/ITimer.h>
#include <CryRenderer/IRenderer.h>
#include <CryGame/IGameFramework.h>
#include <CryMovie/AnimKey_impl.h>
#include <../CryAction/IViewSystem.h>

int CMovieSystem::m_mov_NoCutscenes = 0;
float CMovieSystem::m_mov_cameraPrecacheTime = 1.f;
#if !defined(_RELEASE)
int CMovieSystem::m_mov_debugCamShake = 0;
int CMovieSystem::m_mov_debugSequences = 0;
#endif

#if !defined(_RELEASE)
struct SMovieSequenceAutoComplete : public IConsoleArgumentAutoComplete
{
	virtual int GetCount() const
	{
		return gEnv->pMovieSystem->GetNumSequences();
	}

	virtual const char* GetValue(int nIndex) const
	{
		IAnimSequence* pSequence = gEnv->pMovieSystem->GetSequence(nIndex);

		if (pSequence)
		{
			return pSequence->GetName();
		}

		return "";
	}
};
#endif //#if !defined(_RELEASE)

#if !defined(_RELEASE)
static SMovieSequenceAutoComplete s_movieSequenceAutoComplete;
#endif

//////////////////////////////////////////////////////////////////////////
// Serialization for anim nodes & param types
#define REGISTER_NODE_TYPE(name, defaultName) assert(g_animNodeEnumToStringMap.find(eAnimNodeType_ ## name) == g_animNodeEnumToStringMap.end()); \
  g_animNodeEnumToStringMap[eAnimNodeType_ ## name] = STRINGIFY(name);                                                                           \
  g_animNodeEnumToDefaultNameMap[eAnimNodeType_ ## name] = defaultName;                                                                          \
  g_animNodeStringToEnumMap[STRINGIFY(name)] = eAnimNodeType_ ## name;

#define REGISTER_PARAM_TYPE(name)             assert(g_animParamEnumToStringMap.find(eAnimParamType_ ## name) == g_animParamEnumToStringMap.end()); \
  g_animParamEnumToStringMap[eAnimParamType_ ## name] = STRINGIFY(name);                                                                            \
  g_animParamStringToEnumMap[STRINGIFY(name)] = eAnimParamType_ ## name;

namespace
{
std::unordered_map<int, string> g_animNodeEnumToStringMap;
std::unordered_map<int, string> g_animNodeEnumToDefaultNameMap;
std::map<string, EAnimNodeType, stl::less_stricmp<string>> g_animNodeStringToEnumMap;

std::unordered_map<int, string> g_animParamEnumToStringMap;
std::map<string, EAnimParamType, stl::less_stricmp<string>> g_animParamStringToEnumMap;

// If you get an assert in this function, it means two node types have the same enum value.
void RegisterNodeTypes()
{
	REGISTER_NODE_TYPE(Entity, "Entity")
	REGISTER_NODE_TYPE(Director, "Director")
	REGISTER_NODE_TYPE(Camera, "Camera")
	REGISTER_NODE_TYPE(CVar, "CVar")
	REGISTER_NODE_TYPE(ScriptVar, "Script Variable")
	REGISTER_NODE_TYPE(Material, "Material")
	REGISTER_NODE_TYPE(Event, "Event")
	REGISTER_NODE_TYPE(Group, "Group")
	REGISTER_NODE_TYPE(Layer, "Layer")
	REGISTER_NODE_TYPE(Comment, "Comment")
	REGISTER_NODE_TYPE(RadialBlur, "Radial Blur")
	REGISTER_NODE_TYPE(ColorCorrection, "Color Correction")
	REGISTER_NODE_TYPE(DepthOfField, "Depth of Field")
	REGISTER_NODE_TYPE(ScreenFader, "Screen Fader")
	REGISTER_NODE_TYPE(Light, "Light Animation")
	REGISTER_NODE_TYPE(HDRSetup, "HDR Setup")
	REGISTER_NODE_TYPE(ShadowSetup, "Shadow Setup")
	REGISTER_NODE_TYPE(Alembic, "Alembic")
	REGISTER_NODE_TYPE(GeomCache, "Geom Cache")
	REGISTER_NODE_TYPE(Environment, "Environment")
	REGISTER_NODE_TYPE(Audio, "Audio");
}

// If you get an assert in this function, it means two param types have the same enum value.
void RegisterParamTypes()
{
	REGISTER_PARAM_TYPE(FOV)
	REGISTER_PARAM_TYPE(Position)
	REGISTER_PARAM_TYPE(Rotation)
	REGISTER_PARAM_TYPE(Scale)
	REGISTER_PARAM_TYPE(Event)
	REGISTER_PARAM_TYPE(Visibility)
	REGISTER_PARAM_TYPE(Camera)
	REGISTER_PARAM_TYPE(Animation)
	REGISTER_PARAM_TYPE(AudioTrigger)
	REGISTER_PARAM_TYPE(AudioFile)
	REGISTER_PARAM_TYPE(AudioParameter)
	REGISTER_PARAM_TYPE(AudioSwitch)
	REGISTER_PARAM_TYPE(DynamicResponseSignal)
	REGISTER_PARAM_TYPE(Sequence)
	REGISTER_PARAM_TYPE(Expression)
	REGISTER_PARAM_TYPE(Console)
	REGISTER_PARAM_TYPE(Float)
	REGISTER_PARAM_TYPE(FaceSequence)
	REGISTER_PARAM_TYPE(LookAt)
	REGISTER_PARAM_TYPE(TrackEvent)
	REGISTER_PARAM_TYPE(ShakeAmplitudeA)
	REGISTER_PARAM_TYPE(ShakeAmplitudeB)
	REGISTER_PARAM_TYPE(ShakeFrequencyA)
	REGISTER_PARAM_TYPE(ShakeFrequencyB)
	REGISTER_PARAM_TYPE(ShakeMultiplier)
	REGISTER_PARAM_TYPE(ShakeNoise)
	REGISTER_PARAM_TYPE(ShakeWorking)
	REGISTER_PARAM_TYPE(ShakeAmpAMult)
	REGISTER_PARAM_TYPE(ShakeAmpBMult)
	REGISTER_PARAM_TYPE(ShakeFreqAMult)
	REGISTER_PARAM_TYPE(ShakeFreqBMult)
	REGISTER_PARAM_TYPE(DepthOfField)
	REGISTER_PARAM_TYPE(FocusDistance)
	REGISTER_PARAM_TYPE(FocusRange)
	REGISTER_PARAM_TYPE(BlurAmount)
	REGISTER_PARAM_TYPE(Capture)
	REGISTER_PARAM_TYPE(TransformNoise)
	REGISTER_PARAM_TYPE(TimeWarp)
	REGISTER_PARAM_TYPE(FixedTimeStep)
	REGISTER_PARAM_TYPE(NearZ)
	REGISTER_PARAM_TYPE(Goto)
	REGISTER_PARAM_TYPE(PositionX)
	REGISTER_PARAM_TYPE(PositionY)
	REGISTER_PARAM_TYPE(PositionZ)
	REGISTER_PARAM_TYPE(RotationX)
	REGISTER_PARAM_TYPE(RotationY)
	REGISTER_PARAM_TYPE(RotationZ)
	REGISTER_PARAM_TYPE(ScaleX)
	REGISTER_PARAM_TYPE(ScaleY)
	REGISTER_PARAM_TYPE(ScaleZ)
	REGISTER_PARAM_TYPE(ColorR)
	REGISTER_PARAM_TYPE(ColorG)
	REGISTER_PARAM_TYPE(ColorB)
	REGISTER_PARAM_TYPE(CommentText)
	REGISTER_PARAM_TYPE(ScreenFader)
	REGISTER_PARAM_TYPE(LightDiffuse)
	REGISTER_PARAM_TYPE(LightRadius)
	REGISTER_PARAM_TYPE(LightDiffuseMult)
	REGISTER_PARAM_TYPE(LightHDRDynamic)
	REGISTER_PARAM_TYPE(LightSpecularMult)
	REGISTER_PARAM_TYPE(LightSpecPercentage)
	REGISTER_PARAM_TYPE(MaterialDiffuse)
	REGISTER_PARAM_TYPE(MaterialSpecular)
	REGISTER_PARAM_TYPE(MaterialEmissive)
	REGISTER_PARAM_TYPE(MaterialOpacity)
	REGISTER_PARAM_TYPE(MaterialSmoothness)
	REGISTER_PARAM_TYPE(TimeRanges)
	REGISTER_PARAM_TYPE(Physics)
	REGISTER_PARAM_TYPE(GameCameraInfluence)
	REGISTER_PARAM_TYPE(GSMCache)
	REGISTER_PARAM_TYPE(ShutterSpeed)
	REGISTER_PARAM_TYPE(Physicalize)
	REGISTER_PARAM_TYPE(PhysicsDriven)
	REGISTER_PARAM_TYPE(SunLongitude)
	REGISTER_PARAM_TYPE(SunLatitude)
	REGISTER_PARAM_TYPE(MoonLongitude)
	REGISTER_PARAM_TYPE(MoonLatitude)
	REGISTER_PARAM_TYPE(ProceduralEyes)
	REGISTER_PARAM_TYPE(Mannequin)
}
}

//////////////////////////////////////////////////////////////////////////
CMovieSystem::CMovieSystem(ISystem* pSystem)
{
	m_pSystem = pSystem;
	m_pCallback = NULL;
	m_pUser = NULL;
	m_bPaused = false;
	m_bEnableCameraShake = true;
	m_sequenceStopBehavior = eSSB_GotoEndTime;
	m_lastUpdateTime.SetValue(0);
	m_captureSeq = NULL;
	m_bStartCapture = false;
	m_bEndCapture = false;
	m_bPreEndCapture = false; // this flag indicates the period of time when "Capture Started" was finished, but "Capture End" has not yet begun.
	m_bIsInGameCutscene = false;
	m_fixedTimeStepBackUp = 0;
	m_cvar_capture_file_format = NULL;
	m_cvar_capture_frame_once = NULL;
	m_cvar_capture_folder = NULL;
	m_cvar_t_FixedStep = NULL;
	m_cvar_capture_frames = NULL;
	m_cvar_capture_file_prefix = NULL;
	m_cvar_capture_misc_render_buffers = NULL;
	m_bPhysicsEventsEnabled = true;
	m_bBatchRenderMode = false;

	m_nextSequenceId = 1;

	REGISTER_CVAR2("mov_NoCutscenes", &m_mov_NoCutscenes, 0, 0, "Disable playing of Cut-Scenes");
	REGISTER_CVAR2("mov_cameraPrecacheTime", &m_mov_cameraPrecacheTime, 1.f, VF_NULL, "");
#if !defined(_RELEASE)
	REGISTER_CVAR2("mov_debugSequences", &m_mov_debugSequences, 0, VF_NULL, "Sequence debug info (0 = off, 1 = only active sequences, 2 = all sequences, 3 = detailed node-in-use info per sequence");
#endif
	m_mov_overrideCam = REGISTER_STRING("mov_overrideCam", "", VF_NULL, "Set the camera used for the sequence which overrides the camera track info in the sequence.");

	DoNodeStaticInitialisation();

	RegisterNodeTypes();
	RegisterParamTypes();
}

//////////////////////////////////////////////////////////////////////////
CMovieSystem::~CMovieSystem()
{
	RemoveAllSequences();

	IConsole* pConsole = gEnv->pConsole;
	if (pConsole != nullptr)
	{
		pConsole->UnregisterVariable("mov_NoCutscenes");
		pConsole->UnregisterVariable("mov_cameraPrecacheTime");
#if !defined(_RELEASE)
		pConsole->UnregisterVariable("mov_debugSequences");
#endif
		pConsole->UnregisterVariable("mov_overrideCam");
		m_mov_overrideCam = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
void CMovieSystem::DoNodeStaticInitialisation()
{
	CAnimCameraNode::Initialize();
	CAnimEntityNode::Initialize();
	CAnimMaterialNode::Initialize();
	CAnimPostFXNode::Initialize();
	CAnimSceneNode::Initialize();
	CAnimScreenFaderNode::Initialize();
	CCommentNode::Initialize();
	CLayerNode::Initialize();
}

//////////////////////////////////////////////////////////////////////////
bool CMovieSystem::Load(const char* pszFile, const char* pszMission)
{
	INDENT_LOG_DURING_SCOPE(true, "Movie system is loading the file '%s' (mission='%s')", pszFile, pszMission);
	LOADING_TIME_PROFILE_SECTION(GetISystem());

	XmlNodeRef rootNode = m_pSystem->LoadXmlFromFile(pszFile);

	if (!rootNode)
	{
		return false;
	}

	XmlNodeRef Node = NULL;

	for (int i = 0; i < rootNode->getChildCount(); i++)
	{
		XmlNodeRef missionNode = rootNode->getChild(i);
		XmlString sName;

		if (!(sName = missionNode->getAttr("Name")))
		{
			continue;
		}

		if (stricmp(sName.c_str(), pszMission))
		{
			continue;
		}

		Node = missionNode;
		break;
	}

	if (!Node)
	{
		return false;
	}

	Serialize(Node, true, true, false);
	return true;
}

//////////////////////////////////////////////////////////////////////////
IAnimSequence* CMovieSystem::CreateSequence(const char* pSequenceName, bool bLoad, uint32 id)
{
	if (!bLoad)
	{
		id = m_nextSequenceId++;
	}

	IAnimSequence* pSequence = new CAnimSequence(this, id);
	pSequence->SetName(pSequenceName);
	m_sequences.push_back(pSequence);
	return pSequence;
}

//////////////////////////////////////////////////////////////////////////
IAnimSequence* CMovieSystem::LoadSequence(const char* pszFilePath)
{
	XmlNodeRef sequenceNode = m_pSystem->LoadXmlFromFile(pszFilePath);

	if (sequenceNode)
	{
		return LoadSequence(sequenceNode);
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
IAnimSequence* CMovieSystem::LoadSequence(XmlNodeRef& xmlNode, bool bLoadEmpty)
{
	IAnimSequence* pSequence = new CAnimSequence(this, 0);
	pSequence->Serialize(xmlNode, true, bLoadEmpty);

	// Delete previous sequence with the same name.
	const char* pFullName = pSequence->GetName();

	IAnimSequence* pPrevSeq = FindSequence(pFullName);

	if (pPrevSeq)
	{
		RemoveSequence(pPrevSeq);
	}

	m_sequences.push_back(pSequence);
	return pSequence;
}

//////////////////////////////////////////////////////////////////////////
IAnimSequence* CMovieSystem::FindSequence(const char* pSequenceName) const
{
	assert(pSequenceName);

	if (!pSequenceName)
	{
		return NULL;
	}

	for (Sequences::const_iterator it = m_sequences.begin(); it != m_sequences.end(); ++it)
	{
		IAnimSequence* pCurrentSequence = *it;
		const char* fullname = pCurrentSequence->GetName();

		if (stricmp(fullname, pSequenceName) == 0)
		{
			return pCurrentSequence;
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
IAnimSequence* CMovieSystem::FindSequenceById(uint32 id) const
{
	if (id == 0 || id >= m_nextSequenceId)
	{
		return NULL;
	}

	for (Sequences::const_iterator it = m_sequences.begin(); it != m_sequences.end(); ++it)
	{
		IAnimSequence* pCurrentSequence = *it;

		if (id == pCurrentSequence->GetId())
		{
			return pCurrentSequence;
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
IAnimSequence* CMovieSystem::FindSequenceByGUID(CryGUID guid) const
{
	for (Sequences::const_iterator it = m_sequences.begin(); it != m_sequences.end(); ++it)
	{
		IAnimSequence* pCurrentSequence = *it;
		if (guid == pCurrentSequence->GetGUID())
		{
			return pCurrentSequence;
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////
bool CMovieSystem::FindSequence(IAnimSequence* pSequence, PlayingSequences::const_iterator& sequenceIteratorOut) const
{
	PlayingSequences::const_iterator itend = m_playingSequences.end();

	for (sequenceIteratorOut = m_playingSequences.begin(); sequenceIteratorOut != itend; ++sequenceIteratorOut)
	{
		if (sequenceIteratorOut->sequence == pSequence)
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CMovieSystem::FindSequence(IAnimSequence* pSequence, PlayingSequences::iterator& sequenceIteratorOut)
{
	PlayingSequences::const_iterator itend = m_playingSequences.end();

	for (sequenceIteratorOut = m_playingSequences.begin(); sequenceIteratorOut != itend; ++sequenceIteratorOut)
	{
		if (sequenceIteratorOut->sequence == pSequence)
		{
			return true;
		}
	}

	return false;
}

IAnimSequence* CMovieSystem::GetSequence(int i) const
{
	assert(i >= 0 && i < GetNumSequences());

	if (i < 0 || i >= GetNumSequences())
	{
		return NULL;
	}

	return m_sequences[i];
}

int CMovieSystem::GetNumSequences() const
{
	return m_sequences.size();
}

IAnimSequence* CMovieSystem::GetPlayingSequence(int i) const
{
	assert(i >= 0 && i < GetNumPlayingSequences());

	if (i < 0 || i >= GetNumPlayingSequences())
	{
		return NULL;
	}

	return m_playingSequences[i].sequence;
}

int CMovieSystem::GetNumPlayingSequences() const
{
	return m_playingSequences.size();
}

void CMovieSystem::AddSequence(IAnimSequence* pSequence)
{
	m_sequences.push_back(pSequence);
}

bool CMovieSystem::IsCutScenePlaying() const
{
	const uint numPlayingSequences = m_playingSequences.size();

	for (uint i = 0; i < numPlayingSequences; ++i)
	{
		const IAnimSequence* pAnimSequence = m_playingSequences[i].sequence;

		if (pAnimSequence && (pAnimSequence->GetFlags() & IAnimSequence::eSeqFlags_CutScene) != 0)
		{
			return true;
		}
	}

	return false;
}

void CMovieSystem::RemoveSequence(IAnimSequence* pSequence)
{
	assert(pSequence != 0);

	if (pSequence)
	{
		IMovieCallback* pCallback = GetCallback();
		SetCallback(NULL);
		StopSequence(pSequence);

		for (Sequences::iterator it = m_sequences.begin(); it != m_sequences.end(); ++it)
		{
			if (pSequence == *it)
			{
				m_movieListenerMap.erase(pSequence);
				m_sequences.erase(it);
				break;
			}
		}

		SetCallback(pCallback);
	}
}

void CMovieSystem::RemoveAllSequences()
{
	IMovieCallback* pCallback = GetCallback();
	SetCallback(NULL);
	InternalStopAllSequences(true, false);
	stl::free_container(m_sequences);

	for (TMovieListenerMap::iterator it = m_movieListenerMap.begin(); it != m_movieListenerMap.end(); )
	{
		if (it->first)
		{
			m_movieListenerMap.erase(it++);
		}
		else
		{
			++it;
		}
	}

	SetCallback(pCallback);
}

void CMovieSystem::PlaySequence(const char* pSequenceName, IAnimSequence* pParentSeq, bool bResetFx, bool bTrackedSequence, SAnimTime startTime, SAnimTime endTime)
{
	IAnimSequence* pSequence = FindSequence(pSequenceName);

	if (pSequence)
	{
		PlaySequence(pSequence, pParentSeq, bResetFx, bTrackedSequence, startTime, endTime);
	}
	else
	{
		gEnv->pLog->Log("CMovieSystem::PlaySequence: Error: Sequence \"%s\" not found", pSequenceName);
	}
}

void CMovieSystem::PlaySequence(IAnimSequence* pSequence, IAnimSequence* parentSeq,
                                bool bResetFx, bool bTrackedSequence, SAnimTime startTime, SAnimTime endTime)
{
	assert(pSequence != 0);

	if (!pSequence || IsPlaying(pSequence))
	{
		return;
	}

	if ((pSequence->GetFlags() & IAnimSequence::eSeqFlags_CutScene) || (pSequence->GetFlags() & IAnimSequence::eSeqFlags_NoUI))
	{
		// Don't play cut-scene if this console variable set.
		if (m_mov_NoCutscenes != 0)
		{
			return;
		}
	}

	// If this sequence is cut scene disable player.
	if (pSequence->GetFlags() & IAnimSequence::eSeqFlags_CutScene)
	{
		OnCameraCut();

		pSequence->SetParentSequence(parentSeq);

		if (!gEnv->IsEditing() && m_pUser)
		{
			m_bIsInGameCutscene = true;
			m_pUser->BeginCutScene(pSequence, pSequence->GetCutSceneFlags(), bResetFx);
		}
	}

	pSequence->Activate();
	pSequence->Resume();
	static_cast<CAnimSequence*>(pSequence)->OnStart();

	PlayingSequence ps;
	ps.sequence = pSequence;
	ps.startTime = startTime == SAnimTime::Min() ? pSequence->GetTimeRange().start : startTime;
	ps.endTime = endTime == SAnimTime::Min() ? pSequence->GetTimeRange().end : endTime;
	ps.currentTime = startTime == SAnimTime::Min() ? pSequence->GetTimeRange().start : startTime;
	ps.currentSpeed = 1.0f;
	ps.trackedSequence = bTrackedSequence;
	ps.bSingleFrame = false;
	// Make sure all members are initialized before pushing.
	m_playingSequences.push_back(ps);

	// tell all interested listeners
	NotifyListeners(pSequence, IMovieListener::eMovieEvent_Started);
}

void CMovieSystem::NotifyListeners(IAnimSequence* pSequence, IMovieListener::EMovieEvent event)
{
	TMovieListenerMap::iterator found(m_movieListenerMap.find(pSequence));

	if (found != m_movieListenerMap.end())
	{
		TMovieListenerVec listForSeq = (*found).second;
		TMovieListenerVec::iterator iter(listForSeq.begin());

		while (iter != listForSeq.end())
		{
			(*iter)->OnMovieEvent(event, pSequence);
			++iter;
		}
	}

	// 'NULL' ones are listeners interested in every sequence. Do not send "update" here
	if (event != IMovieListener::eMovieEvent_Updated)
	{
		TMovieListenerMap::iterator found2(m_movieListenerMap.find((IAnimSequence*)0));

		if (found2 != m_movieListenerMap.end())
		{
			TMovieListenerVec listForSeq = (*found2).second;
			TMovieListenerVec::iterator iter(listForSeq.begin());

			while (iter != listForSeq.end())
			{
				(*iter)->OnMovieEvent(event, pSequence);
				++iter;
			}
		}
	}
}

bool CMovieSystem::StopSequence(const char* pSequenceName)
{
	IAnimSequence* pSequence = FindSequence(pSequenceName);

	if (pSequence)
	{
		return StopSequence(pSequence);
	}

	return false;
}

bool CMovieSystem::StopSequence(IAnimSequence* pSequence)
{
	return InternalStopSequence(pSequence, false, true);
}

void CMovieSystem::InternalStopAllSequences(bool bAbort, bool bAnimate)
{
	while (!m_playingSequences.empty())
	{
		InternalStopSequence(m_playingSequences.begin()->sequence, bAbort, bAnimate);
	}

	stl::free_container(m_playingSequences);
}

bool CMovieSystem::InternalStopSequence(IAnimSequence* pSequence, bool bAbort, bool bAnimate)
{
	assert(pSequence != 0);

	bool bRet = false;
	PlayingSequences::iterator it;

	if (FindSequence(pSequence, it))
	{
		if (bAnimate)
		{
			if (m_sequenceStopBehavior == eSSB_GotoEndTime)
			{
				SAnimContext ac;
				ac.bSingleFrame = true;
				ac.time = pSequence->GetTimeRange().end;
				pSequence->Activate();
				pSequence->Animate(ac);
			}
			else if (m_sequenceStopBehavior == eSSB_GotoStartTime)
			{
				SAnimContext ac;
				ac.bSingleFrame = true;
				ac.time = pSequence->GetTimeRange().start;
				pSequence->Activate();
				pSequence->Animate(ac);
			}

			pSequence->Deactivate();
		}

		// If this sequence is cut scene end it.
		if (pSequence->GetFlags() & IAnimSequence::eSeqFlags_CutScene)
		{
			if (m_bIsInGameCutscene && m_pUser)
			{
				m_bIsInGameCutscene = false;
				m_pUser->EndCutScene(pSequence, pSequence->GetCutSceneFlags(true));
			}

			pSequence->SetParentSequence(NULL);
		}

		if (pSequence->GetFlags() & IAnimSequence::eSeqFlags_Capture)
		{
			EndCapture();
			ControlCapture();
		}

		// tell all interested listeners
		NotifyListeners(pSequence, bAbort ? IMovieListener::eMovieEvent_Aborted : IMovieListener::eMovieEvent_Stopped);

		// erase the sequence after notifying listeners so if they choose to they can get the ending time of this sequence
		if (FindSequence(pSequence, it))
		{
			m_playingSequences.erase(it);
		}

		pSequence->Resume();
		static_cast<CAnimSequence*>(pSequence)->OnStop();
		bRet = true;
	}

	return bRet;
}

bool CMovieSystem::AbortSequence(IAnimSequence* pSequence, bool bLeaveTime)
{
	assert(pSequence);

	if (pSequence->GetFlags() & IAnimSequence::eSeqFlags_NoAbort)
	{
		return false;
	}

	// to avoid any camera blending after aborting a cut scene
	IViewSystem* pViewSystem = gEnv->pGameFramework->GetIViewSystem();

	if (pViewSystem)
	{
		pViewSystem->SetBlendParams(0, 0, 0);
		IView* pView = pViewSystem->GetActiveView();

		if (pView)
		{
			pView->ResetBlending();
		}
	}

	return InternalStopSequence(pSequence, true, !bLeaveTime);
}

void CMovieSystem::StopAllSequences()
{
	InternalStopAllSequences(false, true);
}

void CMovieSystem::StopAllCutScenes()
{
	bool bAnyStoped;
	PlayingSequences::iterator next;

	do
	{
		bAnyStoped = false;

		for (PlayingSequences::iterator it = m_playingSequences.begin(); it != m_playingSequences.end(); it = next)
		{
			next = it;
			++next;
			IAnimSequence* pCurrentSequence = it->sequence;

			if (pCurrentSequence->GetFlags() & IAnimSequence::eSeqFlags_CutScene)
			{
				bAnyStoped = true;
				StopSequence(pCurrentSequence);
				break;
			}
		}
	}
	while (bAnyStoped);

	if (m_playingSequences.empty())
	{
		stl::free_container(m_playingSequences);
	}
}

bool CMovieSystem::IsPlaying(IAnimSequence* pSequence) const
{
	for (PlayingSequences::const_iterator it = m_playingSequences.begin(); it != m_playingSequences.end(); ++it)
	{
		if (it->sequence == pSequence)
		{
			return true;
		}
	}

	return false;
}

void CMovieSystem::Reset(bool bPlayOnReset, bool bSeekToStart)
{
	InternalStopAllSequences(true, false);

	// Reset all sequences.
	for (Sequences::iterator iter = m_sequences.begin(); iter != m_sequences.end(); ++iter)
	{
		IAnimSequence* pCurrentSequence = *iter;
		NotifyListeners(pCurrentSequence, IMovieListener::eMovieEvent_Started);
		pCurrentSequence->Reset(bSeekToStart);
		NotifyListeners(pCurrentSequence, IMovieListener::eMovieEvent_Stopped);
	}

	if (bPlayOnReset && !m_bStartCapture)
	{
		for (Sequences::iterator iter = m_sequences.begin(); iter != m_sequences.end(); ++iter)
		{
			IAnimSequence* pCurrentSequence = *iter;

			if (pCurrentSequence->GetFlags() & IAnimSequence::eSeqFlags_PlayOnReset)
			{
				PlaySequence(pCurrentSequence);
			}
		}
	}

	// un-pause the movie system
	m_bPaused = false;

	// Reset camera.
	SCameraParams CamParams = GetCameraParams();
	CamParams.cameraEntityId = 0;
	CamParams.fFOV = 0.0f;
	CamParams.justActivated = true;
	SetCameraParams(CamParams);
}

void CMovieSystem::PlayOnLoadSequences()
{
	for (Sequences::iterator sit = m_sequences.begin(); sit != m_sequences.end(); ++sit)
	{
		IAnimSequence* pSequence = *sit;

		if (pSequence->GetFlags() & IAnimSequence::eSeqFlags_PlayOnReset)
		{
			PlaySequence(pSequence);
		}
	}

	// Reset camera.
	SCameraParams CamParams = GetCameraParams();
	CamParams.cameraEntityId = 0;
	CamParams.fFOV = 0.0f;
	CamParams.justActivated = true;
	SetCameraParams(CamParams);
}

void CMovieSystem::StillUpdate()
{
	if (!gEnv->IsEditor())
	{
		return;
	}

	for (PlayingSequences::iterator it = m_playingSequences.begin(); it != m_playingSequences.end(); ++it)
	{
		PlayingSequence& playingSequence = *it;

		playingSequence.sequence->StillUpdate();
	}
}

void CMovieSystem::PreUpdate(float deltaTime)
{
	UpdateInternal(deltaTime, true);
}

void CMovieSystem::PostUpdate(float deltaTime)
{
	UpdateInternal(deltaTime, false);
}

void CMovieSystem::UpdateInternal(const float deltaTime, const bool bPreUpdate)
{
	SAnimContext animContext;

	if (m_bPaused)
	{
		return;
	}

	if (m_bPaused)
	{
		return;
	}

	// don't update more than once if dt==0.0
	CTimeValue curTime = gEnv->pTimer->GetFrameStartTime();

	if (deltaTime == 0.0f && curTime == m_lastUpdateTime && !gEnv->IsEditor())
	{
		return;
	}

	m_lastUpdateTime = curTime;

	std::vector<IAnimSequence*> stopSequences;

	const size_t numPlayingSequences = m_playingSequences.size();

	for (size_t i = 0; i < numPlayingSequences; ++i)
	{
		PlayingSequence& playingSequence = m_playingSequences[i];

		if (playingSequence.sequence->IsPaused())
		{
			continue;
		}

		const SAnimTime scaledTimeDelta = SAnimTime(deltaTime * playingSequence.currentSpeed);

		// Increase play time in pre-update
		if (bPreUpdate)
		{
			playingSequence.currentTime += scaledTimeDelta;
		}

		// Skip sequence if current update does not apply
		const bool bSequenceEarlyUpdate = (playingSequence.sequence->GetFlags() & IAnimSequence::eSeqFlags_EarlyMovieUpdate) != 0;

		if (bPreUpdate && !bSequenceEarlyUpdate || !bPreUpdate && bSequenceEarlyUpdate)
		{
			continue;
		}

		int nSeqFlags = playingSequence.sequence->GetFlags();

		if ((nSeqFlags& IAnimSequence::eSeqFlags_CutScene) && m_mov_NoCutscenes != 0)
		{
			// Don't play cut-scene if no cut scenes console variable set.
			stopSequences.push_back(playingSequence.sequence);
			continue;
		}

		animContext.time = playingSequence.currentTime;
		animContext.pSequence = playingSequence.sequence;
		animContext.dt = scaledTimeDelta;
		animContext.startTime = playingSequence.startTime;

		// Check time out of range.
		if (playingSequence.currentTime > playingSequence.endTime)
		{
			int seqFlags = playingSequence.sequence->GetFlags();

			if (seqFlags & IAnimSequence::eSeqFlags_OutOfRangeLoop)
			{
				// Time wrap's back to the start of the time range.
				playingSequence.currentTime = playingSequence.startTime; // should there be a fmodf here?
			}
			else if (seqFlags & IAnimSequence::eSeqFlags_OutOfRangeConstant)
			{
				// Time just continues normally past the end of time range.
			}
			else
			{
				// If no out-of-range type specified sequence stopped when time reaches end of range.
				// Que sequence for stopping.
				if (playingSequence.trackedSequence == false)
				{
					if (seqFlags & IAnimSequence::eSeqFlags_Capture)
					{
						EndCapture();
					}
					stopSequences.push_back(playingSequence.sequence);
				}

				continue;
			}
		}
		else
		{
			NotifyListeners(playingSequence.sequence, IMovieListener::eMovieEvent_Updated);
		}

		animContext.bSingleFrame = playingSequence.bSingleFrame;
		playingSequence.bSingleFrame = false;

		// Animate sequence. (Can invalidate iterator)
		playingSequence.sequence->Animate(animContext);
	}

#if !defined(_RELEASE)
	if (m_mov_debugSequences)
	{
		ShowPlayedSequencesDebug();
	}
#endif //#if !defined(_RELEASE)

	// Stop queued sequences.
	for (int i = 0; i < (int)stopSequences.size(); i++)
	{
		StopSequence(stopSequences[i]);
	}
}

void CMovieSystem::Render()
{
	for (PlayingSequences::iterator it = m_playingSequences.begin(); it != m_playingSequences.end(); ++it)
	{
		PlayingSequence& playingSequence = *it;
		playingSequence.sequence->Render();
	}
}

void CMovieSystem::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bRemoveOldNodes, bool bLoadEmpty)
{
	if (bLoading)
	{
		XmlNodeRef seqNode = xmlNode->findChild("SequenceData");

		if (seqNode)
		{
			RemoveAllSequences();

			INDENT_LOG_DURING_SCOPE(true, "SequenceData tag contains %u sequences", seqNode->getChildCount());

			for (int i = 0; i < seqNode->getChildCount(); i++)
			{
				XmlNodeRef childNode = seqNode->getChild(i);

				if (!LoadSequence(childNode, bLoadEmpty))
				{
					return;
				}
			}
		}
	}
	else
	{
		XmlNodeRef sequencesNode = xmlNode->newChild("SequenceData");

		for (int i = 0; i < GetNumSequences(); ++i)
		{
			IAnimSequence* pSequence = GetSequence(i);
			XmlNodeRef sequenceNode = sequencesNode->newChild("Sequence");
			pSequence->Serialize(sequenceNode, false);
		}
	}
}

void CMovieSystem::SetCameraParams(const SCameraParams& Params)
{
	m_ActiveCameraParams = Params;

	if (m_pUser)
	{
		m_pUser->SetActiveCamera(m_ActiveCameraParams);
	}

	if (m_pCallback)
	{
		m_pCallback->OnSetCamera(m_ActiveCameraParams);
	}
}

void CMovieSystem::SendGlobalEvent(const char* pszEvent)
{
	if (m_pUser)
	{
		m_pUser->SendGlobalEvent(pszEvent);
	}
}

void CMovieSystem::Pause()
{
	m_bPaused = true;
}

void CMovieSystem::Resume()
{
	m_bPaused = false;
}

SAnimTime CMovieSystem::GetPlayingTime(IAnimSequence* pSequence)
{
	if (!pSequence || !IsPlaying(pSequence))
	{
		return SAnimTime::Min();
	}

	PlayingSequences::const_iterator it;

	if (FindSequence(pSequence, it))
	{
		return it->currentTime;
	}

	return SAnimTime::Min();
}

float CMovieSystem::GetPlayingSpeed(IAnimSequence* pSequence)
{
	if (!pSequence || !IsPlaying(pSequence))
	{
		return -1.0f;
	}

	PlayingSequences::const_iterator it;

	if (FindSequence(pSequence, it))
	{
		return it->currentSpeed;
	}

	return -1.0f;
}

bool CMovieSystem::SetPlayingTime(IAnimSequence* pSequence, SAnimTime fTime)
{
	if (!pSequence || !IsPlaying(pSequence))
	{
		return false;
	}

	PlayingSequences::iterator it;

	if (FindSequence(pSequence, it) && !(pSequence->GetFlags() & IAnimSequence::eSeqFlags_NoSeek))
	{
		it->currentTime = fTime;
		it->bSingleFrame = true;
		NotifyListeners(pSequence, IMovieListener::eMovieEvent_Updated);
		return true;
	}

	return false;
}

bool CMovieSystem::SetPlayingSpeed(IAnimSequence* pSequence, float fSpeed)
{
	if (!pSequence)
	{
		return false;
	}

	PlayingSequences::iterator it;

	if (FindSequence(pSequence, it) && !(pSequence->GetFlags() & IAnimSequence::eSeqFlags_NoSpeed))
	{
		NotifyListeners(pSequence, IMovieListener::eMovieEvent_Updated);
		it->currentSpeed = fSpeed;
		return true;
	}

	return false;
}

bool CMovieSystem::GetStartEndTime(IAnimSequence* pSequence, SAnimTime& fStartTime, SAnimTime& fEndTime)
{
	fStartTime = SAnimTime(0);
	fEndTime = SAnimTime(0);

	if (!pSequence || !IsPlaying(pSequence))
	{
		return false;
	}

	PlayingSequences::const_iterator it;

	if (FindSequence(pSequence, it))
	{
		fStartTime = it->startTime;
		fEndTime = it->endTime;
		return true;
	}

	return false;
}

bool CMovieSystem::SetStartEndTime(IAnimSequence* pSeq, const SAnimTime fStartTime, const SAnimTime fEndTime)
{
	if (!pSeq || !IsPlaying(pSeq))
	{
		return false;
	}

	PlayingSequences::iterator it;

	if (FindSequence(pSeq, it))
	{
		it->startTime = fStartTime;
		it->endTime = fEndTime;
		return true;
	}

	return false;
}

void CMovieSystem::SetSequenceStopBehavior(ESequenceStopBehavior behavior)
{
	m_sequenceStopBehavior = behavior;
}

IMovieSystem::ESequenceStopBehavior CMovieSystem::GetSequenceStopBehavior()
{
	return m_sequenceStopBehavior;
}

bool CMovieSystem::AddMovieListener(IAnimSequence* pSequence, IMovieListener* pListener)
{
	assert(pListener != 0);
	if (pSequence != nullptr && std::find(m_sequences.begin(), m_sequences.end(), pSequence) == m_sequences.end())
	{
		gEnv->pLog->Log("CMovieSystem::AddMovieListener: Sequence %p unknown to CMovieSystem", pSequence);
		return false;
	}
	return stl::push_back_unique(m_movieListenerMap[pSequence], pListener);
}

bool CMovieSystem::RemoveMovieListener(IAnimSequence* pSequence, IMovieListener* pListener)
{
	assert(pListener != 0);
	if (pSequence != nullptr && std::find(m_sequences.begin(), m_sequences.end(), pSequence) == m_sequences.end())
	{
		gEnv->pLog->Log("CMovieSystem::AddMovieListener: Sequence %p unknown to CMovieSystem", pSequence);
		return false;
	}
	return stl::find_and_erase(m_movieListenerMap[pSequence], pListener);
}

#if !defined(_RELEASE)
void CMovieSystem::GoToFrameCmd(IConsoleCmdArgs* pArgs)
{
	if (pArgs->GetArgCount() < 3)
	{
		gEnv->pLog->LogError("GoToFrame failed! You should provide two arguments of 'sequence name' & 'frame time'.");
		return;
	}

	const char* pSeqName = pArgs->GetArg(1);
	float targetFrame = (float)atof(pArgs->GetArg(2));

	((CMovieSystem*)gEnv->pMovieSystem)->GoToFrame(pSeqName, targetFrame);
}
#endif //#if !defined(_RELEASE)

#if !defined(_RELEASE)
void CMovieSystem::ListSequencesCmd(IConsoleCmdArgs* pArgs)
{
	int numSequences = gEnv->pMovieSystem->GetNumSequences();

	for (int i = 0; i < numSequences; i++)
	{
		IAnimSequence* pSeq = gEnv->pMovieSystem->GetSequence(i);

		if (pSeq)
		{
			CryLogAlways("%s", pSeq->GetName());
		}
	}
}
#endif //#if !defined(_RELEASE)

#if !defined(_RELEASE)
void CMovieSystem::PlaySequencesCmd(IConsoleCmdArgs* pArgs)
{
	const char* sequenceName = pArgs->GetArg(1);
	gEnv->pMovieSystem->PlaySequence(sequenceName, NULL, false, false);
}
#endif //#if !defined(_RELEASE)

#if !defined(_RELEASE)
void CMovieSystem::ShowPlayedSequencesDebug()
{
	f32 green[4] = { 0, 1, 0, 1 };
	f32 purple[4] = { 1, 0, 1, 1 };
	f32 white[4] = { 1, 1, 1, 1 };
	float y = 10.0f;
	std::vector<const char*> names;

	IRenderAuxText::Draw2dLabel(1.0f, y, 1.5f, purple, false, "mov_debugSequences = %i", m_mov_debugSequences);
	y += 20.0f;

	IRenderAuxText::Draw2dLabel(1.0f, y, 1.5f, purple, false, "Name:");
	IRenderAuxText::Draw2dLabel(200.0f, y, 1.5f, purple, false, "Speed:");
	IRenderAuxText::Draw2dLabel(300.0f, y, 1.5f, purple, false, "Time:");

	if (m_mov_debugSequences == 3)
	{
		IRenderAuxText::Draw2dLabel(400.0f, y, 1.5f, purple, false, "Nodes:");
	}
	y += 20.0f;

	for (Sequences::iterator it = m_sequences.begin(); it != m_sequences.end(); ++it)
	{
		IAnimSequence* currentSequence = *it;
		const char* szSeqName = currentSequence->GetName();
		const bool bIsRunning = IsPlaying(currentSequence);

		if (!bIsRunning && m_mov_debugSequences == 1)
			continue;

		const SAnimTime playingTime = bIsRunning ? GetPlayingTime(currentSequence) : SAnimTime(0);

		float x = 1.0f;
		IRenderAuxText::Draw2dLabel(x, y, 1.2f, bIsRunning ? green : white, false, "%s", szSeqName);

		x += 200.0f;
		IRenderAuxText::Draw2dLabel(x, y, 1.2f, bIsRunning ? green : white, false, "x %1.1f", GetPlayingSpeed(currentSequence));

		x += 100.0f;
		IRenderAuxText::Draw2dLabel(x, y, 1.2f, bIsRunning ? green : white, false, "%u", playingTime.GetTicks());

		if (m_mov_debugSequences == 3)
		{
			x += 100.0f;

			for (int i = 0; i < currentSequence->GetNodeCount(); ++i)
			{
				// Checks nodes which happen to be in several sequences.
				// Those can be a bug, since several sequences may try to control the same entity.
				IAnimNode* pAnimNode = currentSequence->GetNode(i);
				if (!pAnimNode || pAnimNode->GetType() == eAnimNodeType_Director)
					continue;

				bool bAlreadyThere = false;
				const char* szNodeName = pAnimNode->GetName();
				for (size_t k = 0; k < names.size(); ++k)
				{
					if (strcmp(names[k], szNodeName) == 0)
					{
						bAlreadyThere = true;
						break;
					}
				}

				if (!bAlreadyThere)
				{
					names.push_back(szNodeName);
				}

				IRenderAuxText::Draw2dLabel(x, y, 1.2f, bAlreadyThere ? purple : white, false, "%s", szNodeName);
				y += 10.0f;
			}
		}
		y += 20.0f;
	}
}
#endif //#if !defined(_RELEASE)

void CMovieSystem::GoToFrame(const char* seqName, float targetFrame)
{
	assert(seqName != NULL);

	if (gEnv->IsEditor() && gEnv->IsEditorGameMode() == false)
	{
		string editorCmd;
		editorCmd.Format("mov_goToFrameEditor %s %f", seqName, targetFrame);
		gEnv->pConsole->ExecuteString(editorCmd.c_str());
		return;
	}

	for (PlayingSequences::iterator it = m_playingSequences.begin();
	     it != m_playingSequences.end(); ++it)
	{
		PlayingSequence& ps = *it;

		const char* fullname = ps.sequence->GetName();

		if (strcmp(fullname, seqName) == 0)
		{
			assert(ps.sequence->GetTimeRange().start <= SAnimTime(targetFrame) && SAnimTime(targetFrame) <= ps.sequence->GetTimeRange().end);
			ps.currentTime = SAnimTime(targetFrame);
			ps.bSingleFrame = true;
			break;
		}
	}
}

void CMovieSystem::StartCapture(IAnimSequence* seq, const SCaptureKey& key)
{
	m_bStartCapture = true;
	m_bEndCapture = false;
	m_bPreEndCapture = false;
	m_captureSeq = seq;
	m_captureKey = key;
}

void CMovieSystem::EndCapture()
{
	if (m_bPreEndCapture)
	{
		m_bPreEndCapture = false;
		m_bEndCapture = true;
	}
}

void CMovieSystem::ControlCapture()
{
	if (!gEnv->IsEditor())
	{
		return;
	}

	bool bBothStartAndEnd = m_bStartCapture && m_bEndCapture;
	assert(!bBothStartAndEnd);

	if (m_bStartCapture)
	{
		bool bAllCVarsReady = m_cvar_capture_file_format && m_cvar_capture_frame_once
		                      && m_cvar_capture_folder && m_cvar_t_FixedStep && m_cvar_capture_frames
		                      && m_cvar_capture_file_prefix;

		if (!bAllCVarsReady)
		{
			m_cvar_capture_file_format = gEnv->pConsole->GetCVar("capture_file_format");
			m_cvar_capture_frame_once = gEnv->pConsole->GetCVar("capture_frame_once");
			m_cvar_capture_folder = gEnv->pConsole->GetCVar("capture_folder");
			m_cvar_t_FixedStep = gEnv->pConsole->GetCVar("t_FixedStep");
			m_cvar_capture_frames = gEnv->pConsole->GetCVar("capture_frames");
			m_cvar_capture_file_prefix = gEnv->pConsole->GetCVar("capture_file_prefix");
		}

		bAllCVarsReady = m_cvar_capture_file_format && m_cvar_capture_frame_once
		                 && m_cvar_capture_folder && m_cvar_t_FixedStep && m_cvar_capture_frames
		                 && m_cvar_capture_file_prefix;

		assert(bAllCVarsReady);

		m_cvar_capture_file_format->Set(SCaptureFormatInfo::GetCaptureFormatExtension(m_captureKey.m_captureFormat));
		m_cvar_capture_frame_once->Set(m_captureKey.m_bOnce ? 1 : 0);
		m_cvar_capture_folder->Set(m_captureKey.m_folder);
		m_cvar_capture_file_prefix->Set(m_captureKey.m_prefix);

		m_fixedTimeStepBackUp = m_cvar_t_FixedStep->GetFVal();
		m_cvar_t_FixedStep->Set(m_captureKey.m_timeStep);
		m_cvar_capture_frames->Set(m_captureKey.GetStartTimeInFrames() + 1);

		m_bStartCapture = false;
		m_bPreEndCapture = true;

		if (m_captureSeq)
		{
			m_captureSeq->SetFlags(m_captureSeq->GetFlags() | IAnimSequence::eSeqFlags_Capture);

			PlaySequence(m_captureSeq);
			NotifyListeners(m_captureSeq, IMovieListener::eMovieEvent_CaptureStarted);
			return;
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_MOVIE, VALIDATOR_WARNING, "StartCapture failed, can't find a valid Sequence to capture!");
		}
	}

	if (m_cvar_capture_frames && (!m_bStartCapture && !m_bEndCapture))
	{
		if (m_cvar_capture_frames->GetIVal() > m_captureKey.GetEndTimeInFrames() + 1)
		{
			// EndCapture() should be invoked carefully from here, because StartCapture()
			// invoked from AnimationNode. The flag m_bPreEndCapture should help
			// to resolve the case when EndCapture() invoked from here and
			// and AnimationNode twice.
			EndCapture();
		}
	}

	if (m_bEndCapture)
	{
		m_cvar_capture_frames->Set(0);
		m_cvar_t_FixedStep->Set(m_fixedTimeStepBackUp);

		m_bEndCapture = false;

		if (m_captureSeq)
		{
			m_captureSeq->SetFlags(m_captureSeq->GetFlags() & ~IAnimSequence::eSeqFlags_Capture);
			NotifyListeners(m_captureSeq, IMovieListener::eMovieEvent_CaptureStopped);
			m_captureSeq = NULL;
			return;
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_MOVIE, VALIDATOR_WARNING, "EndCapture failed, passed sequence doesn't seem to be valid!");
		}
	}
}

void CMovieSystem::SerializeNodeType(EAnimNodeType& animNodeType, XmlNodeRef& xmlNode, bool bLoading, const uint version, int flags)
{
	static const char* kType = "Type";

	if (bLoading)
	{
		// Old serialization values that are no longer
		// defined in IMovieSystem.h, but needed for conversion:
		static const int kOldParticleNodeType = 0x18;

		animNodeType = eAnimNodeType_Invalid;

		// In old versions there was special code for particles
		// that is now handles by generic entity node code
		if (version == 0 && animNodeType == kOldParticleNodeType)
		{
			animNodeType = eAnimNodeType_Entity;
			return;
		}

		// Convert light nodes that are not part of a light
		// animation set to common entity nodes
		if (version <= 1 && animNodeType == eAnimNodeType_Light && !(flags & IAnimSequence::eSeqFlags_LightAnimationSet))
		{
			animNodeType = eAnimNodeType_Entity;
			return;
		}

		if (version <= 2)
		{
			int type;

			if (xmlNode->getAttr(kType, type))
			{
				animNodeType = (EAnimNodeType)type;
			}

			return;
		}
		else
		{
			XmlString nodeTypeString;

			if (xmlNode->getAttr(kType, nodeTypeString))
			{
				assert(g_animNodeStringToEnumMap.find(nodeTypeString.c_str()) != g_animNodeStringToEnumMap.end());
				animNodeType = stl::find_in_map(g_animNodeStringToEnumMap, nodeTypeString.c_str(), eAnimNodeType_Invalid);
			}
		}
	}
	else
	{
		assert(g_animNodeEnumToStringMap.find(animNodeType) != g_animNodeEnumToStringMap.end());
		const char* pTypeString = g_animNodeEnumToStringMap[animNodeType];
		xmlNode->setAttr(kType, pTypeString);
	}
}

void CMovieSystem::SerializeParamType(CAnimParamType& animParamType, XmlNodeRef& xmlNode, bool bLoading, const uint version)
{
	static const char* kByNameAttrName = "paramIdIsName";
	static const char* kParamUserValue = "paramUserValue";

	if (bLoading)
	{
		animParamType.m_type = eAnimParamType_Invalid;

		if (version <= 6)
		{
			static const char* kParamId = "paramId";

			if (xmlNode->haveAttr(kByNameAttrName))
			{
				XmlString name;

				if (xmlNode->getAttr(kParamId, name))
				{
					animParamType.m_type = eAnimParamType_ByString;
					animParamType.m_name = name.c_str();
				}
			}
			else
			{
				int type;
				xmlNode->getAttr(kParamId, type);
				animParamType.m_type = (EAnimParamType)type;
			}
		}
		else
		{
			static const char* kParamType = "paramType";

			XmlString paramTypeString;

			if (xmlNode->getAttr(kParamType, paramTypeString))
			{
				if (paramTypeString == "ByString")
				{
					animParamType.m_type = eAnimParamType_ByString;

					XmlString userValue;
					xmlNode->getAttr(kParamUserValue, userValue);
					animParamType.m_name = userValue;
				}
				else if (paramTypeString == "User")
				{
					animParamType.m_type = eAnimParamType_User;

					int type;
					xmlNode->getAttr(kParamUserValue, type);
					animParamType.m_type = (EAnimParamType)type;
				}
				else if (paramTypeString == "Sound")
				{
					// Since the paramType "Sound" got renamed to "AudioTrigger", we have to
					// make sure that we can still load old references. Those old references
					// will then be updated accordingly during the next layer serialization.
					animParamType.m_type = eAnimParamType_AudioTrigger;
				}
				else
				{
					assert(g_animParamStringToEnumMap.find(paramTypeString.c_str()) != g_animParamStringToEnumMap.end());
					animParamType.m_type = stl::find_in_map(g_animParamStringToEnumMap, paramTypeString.c_str(), eAnimParamType_Invalid);
				}
			}
		}
	}
	else
	{
		static const char* kParamType = "paramType";
		const char* pTypeString = "Invalid";

		if (animParamType.m_type == eAnimParamType_ByString)
		{
			pTypeString = "ByString";
			xmlNode->setAttr(kParamUserValue, animParamType.m_name.c_str());
		}
		else if (animParamType.m_type >= eAnimParamType_User)
		{
			pTypeString = "User";
			xmlNode->setAttr(kParamUserValue, (int)animParamType.m_type);
		}
		else
		{
			assert(g_animParamEnumToStringMap.find(animParamType.m_type) != g_animParamEnumToStringMap.end());
			pTypeString = g_animParamEnumToStringMap[animParamType.m_type];
		}

		xmlNode->setAttr(kParamType, pTypeString);
	}
}

const char* CMovieSystem::GetParamTypeName(const CAnimParamType& animParamType)
{
	if (animParamType.m_type == eAnimParamType_ByString)
	{
		return animParamType.GetName();
	}
	else if (animParamType.m_type >= eAnimParamType_User)
	{
		return "User";
	}
	else
	{
		if (g_animParamEnumToStringMap.find(animParamType.m_type) != g_animParamEnumToStringMap.end())
		{
			return g_animParamEnumToStringMap[animParamType.m_type];
		}
	}

	return "Invalid";
}

void CMovieSystem::OnCameraCut()
{
}

ILightAnimWrapper* CMovieSystem::CreateLightAnimWrapper(const char* szName) const
{
	return CLightAnimWrapper::Create(szName);
}

CLightAnimWrapper::LightAnimWrapperCache CLightAnimWrapper::ms_lightAnimWrapperCache;
_smart_ptr<IAnimSequence> CLightAnimWrapper::ms_pLightAnimSet = 0;

CLightAnimWrapper::CLightAnimWrapper(const char* szName)
	: ILightAnimWrapper(szName)
{
	m_nRefCounter = 1;
}

CLightAnimWrapper::~CLightAnimWrapper()
{
	RemoveCachedLightAnim(m_name.c_str());
}

bool CLightAnimWrapper::Resolve()
{
	if (m_pNode == nullptr)
	{
		IAnimSequence* pSeq = GetLightAnimSet();
		m_pNode = pSeq ? pSeq->FindNodeByName(m_name.c_str(), 0) : 0;
	}

	return m_pNode != nullptr;
}

CLightAnimWrapper* CLightAnimWrapper::Create(const char* szName)
{
	CLightAnimWrapper* pWrapper = FindLightAnim(szName);

	if (pWrapper != nullptr)
	{
		pWrapper->AddRef();
	}
	else
	{
		pWrapper = new CLightAnimWrapper(szName);
		CacheLightAnim(szName, pWrapper);
	}

	return pWrapper;
}

void CLightAnimWrapper::ReconstructCache()
{
#if !defined(_RELEASE)

	if (!ms_lightAnimWrapperCache.empty())
	{
		__debugbreak();
	}

#endif

	stl::reconstruct(ms_lightAnimWrapperCache);
	SetLightAnimSet(0);
}

IAnimSequence* CLightAnimWrapper::GetLightAnimSet()
{
	return ms_pLightAnimSet;
}

void CLightAnimWrapper::SetLightAnimSet(IAnimSequence* pLightAnimSet)
{
	ms_pLightAnimSet = pLightAnimSet;
}

void CLightAnimWrapper::InvalidateAllNodes()
{
#if !defined(_RELEASE)

	if (!gEnv->IsEditor())
	{
		__debugbreak();
	}

#endif
	// !!! Will only work in Editor as the renderer runs in single threaded mode !!!
	// Invalidate all node pointers before the light anim set can get destroyed via SetLightAnimSet(0).
	// They'll get re-resolved in the next frame via Resolve(). Needed for Editor undo, import, etc.
	LightAnimWrapperCache::iterator it = ms_lightAnimWrapperCache.begin();
	LightAnimWrapperCache::iterator itEnd = ms_lightAnimWrapperCache.end();

	for (; it != itEnd; ++it)
	{
		(*it).second->m_pNode = 0;
	}
}

CLightAnimWrapper* CLightAnimWrapper::FindLightAnim(const char* name)
{
	LightAnimWrapperCache::const_iterator it = ms_lightAnimWrapperCache.find(CONST_TEMP_STRING(name));
	return it != ms_lightAnimWrapperCache.end() ? (*it).second : 0;
}

void CLightAnimWrapper::CacheLightAnim(const char* name, CLightAnimWrapper* p)
{
	ms_lightAnimWrapperCache.insert(LightAnimWrapperCache::value_type(string(name), p));
}

void CLightAnimWrapper::RemoveCachedLightAnim(const char* name)
{
	ms_lightAnimWrapperCache.erase(CONST_TEMP_STRING(name));
}

#ifdef MOVIESYSTEM_SUPPORT_EDITING
EAnimNodeType CMovieSystem::GetNodeTypeFromString(const char* pString) const
{
	return stl::find_in_map(g_animNodeStringToEnumMap, pString, eAnimNodeType_Invalid);
}

CAnimParamType CMovieSystem::GetParamTypeFromString(const char* pString) const
{
	const EAnimParamType paramType = stl::find_in_map(g_animParamStringToEnumMap, pString, eAnimParamType_Invalid);

	if (paramType != eAnimParamType_Invalid)
	{
		return CAnimParamType(paramType);
	}

	return CAnimParamType(pString);
}

DynArray<EAnimNodeType> CMovieSystem::GetAllNodeTypes() const
{
	DynArray<EAnimNodeType> types;
	for (std::unordered_map<int, string>::const_iterator iter = g_animNodeEnumToDefaultNameMap.begin(); iter != g_animNodeEnumToDefaultNameMap.end(); ++iter)
	{
		types.push_back(static_cast<EAnimNodeType>(iter->first));
	}

	return types;
}

const char* CMovieSystem::GetDefaultNodeName(EAnimNodeType type) const
{
	const string name = stl::find_in_map(g_animNodeEnumToDefaultNameMap, type, "Unknown");
	return name.c_str();
}
#endif
