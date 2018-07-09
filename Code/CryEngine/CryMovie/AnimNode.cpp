// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AnimNode.h"
#include "AnimTrack.h"
#include "AnimSequence.h"
#include "CharacterTrack.h"
#include "AnimSplineTrack.h"
#include "BoolTrack.h"
#include "LookAtTrack.h"
#include "CompoundSplineTrack.h"
#include "GotoTrack.h"
#include "ScreenFaderTrack.h"
#include "TimeRangesTrack.h"
#include "Tracks.h"

#include <Cry3DEngine/I3DEngine.h>
#include <ctime>

#define APARAM_CHARACTER4   (eAnimParamType_User + 0x10)
#define APARAM_CHARACTER5   (eAnimParamType_User + 0x11)
#define APARAM_CHARACTER6   (eAnimParamType_User + 0x12)
#define APARAM_CHARACTER7   (eAnimParamType_User + 0x13)
#define APARAM_CHARACTER8   (eAnimParamType_User + 0x14)
#define APARAM_CHARACTER9   (eAnimParamType_User + 0x15)
#define APARAM_CHARACTER10  (eAnimParamType_User + 0x16)

#define APARAM_EXPRESSION4  (eAnimParamType_User + 0x20)
#define APARAM_EXPRESSION5  (eAnimParamType_User + 0x21)
#define APARAM_EXPRESSION6  (eAnimParamType_User + 0x22)
#define APARAM_EXPRESSION7  (eAnimParamType_User + 0x23)
#define APARAM_EXPRESSION8  (eAnimParamType_User + 0x24)
#define APARAM_EXPRESSION9  (eAnimParamType_User + 0x25)
#define APARAM_EXPRESSION10 (eAnimParamType_User + 0x26)

// Old serialization values that are no longer
// defined in IMovieSystem.h, but needed for conversion:
static const int OLD_APARAM_USER = 100;
static const int OLD_ACURVE_GOTO = 21;
static const int OLD_APARAM_PARTICLE_COUNT_SCALE = 95;
static const int OLD_APARAM_PARTICLE_PULSE_PERIOD = 96;
static const int OLD_APARAM_PARTICLE_SCALE = 97;
static const int OLD_APARAM_PARTICLE_SPEED_SCALE = 98;
static const int OLD_APARAM_PARTICLE_STRENGTH = 99;

void CAnimNode::Activate(bool bActivate)
{
}

int CAnimNode::GetTrackCount() const
{
	return m_tracks.size();
}

const char* CAnimNode::GetParamName(const CAnimParamType& paramType) const
{
	SParamInfo info;

	if (GetParamInfoFromType(paramType, info))
	{
		return info.name;
	}

	return "Unknown";
}

EAnimValue CAnimNode::GetParamValueType(const CAnimParamType& paramType) const
{
	SParamInfo info;

	if (GetParamInfoFromType(paramType, info))
	{
		return info.valueType;
	}

	return eAnimValue_Unknown;
}

IAnimNode::ESupportedParamFlags CAnimNode::GetParamFlags(const CAnimParamType& paramType) const
{
	SParamInfo info;

	if (GetParamInfoFromType(paramType, info))
	{
		return info.flags;
	}

	return IAnimNode::ESupportedParamFlags(0);
}

IAnimTrack* CAnimNode::GetTrackForParameter(const CAnimParamType& paramType) const
{
	for (int i = 0, num = (int)m_tracks.size(); i < num; i++)
	{
		if (m_tracks[i]->GetParameterType() == paramType)
		{
			return m_tracks[i];
		}

		// Search the sub-tracks also if any.
		for (int k = 0; k < m_tracks[i]->GetSubTrackCount(); ++k)
		{
			if (m_tracks[i]->GetSubTrack(k)->GetParameterType() == paramType)
			{
				return m_tracks[i]->GetSubTrack(k);
			}
		}
	}

	return 0;
}

IAnimTrack* CAnimNode::GetTrackForParameter(const CAnimParamType& paramType, uint32 index) const
{
	SParamInfo paramInfo;
	GetParamInfoFromType(paramType, paramInfo);

	if ((paramInfo.flags & IAnimNode::eSupportedParamFlags_MultipleTracks) == 0)
	{
		return GetTrackForParameter(paramType);
	}

	uint32 count = 0;

	for (int i = 0, num = (int)m_tracks.size(); i < num; i++)
	{
		if (m_tracks[i]->GetParameterType() == paramType && count++ == index)
		{
			return m_tracks[i];
		}

		// For this case, no subtracks are considered.
	}

	return 0;
}

uint32 CAnimNode::GetTrackParamIndex(const IAnimTrack* pTrack) const
{
	assert(pTrack);
	uint32 index = 0;
	CAnimParamType paramType = pTrack->GetParameterType();

	SParamInfo paramInfo;
	GetParamInfoFromType(paramType, paramInfo);

	if ((paramInfo.flags & IAnimNode::eSupportedParamFlags_MultipleTracks) == 0)
	{
		return 0;
	}

	for (int i = 0, num = (int)m_tracks.size(); i < num; i++)
	{
		if (m_tracks[i] == pTrack)
		{
			return index;
		}

		if (m_tracks[i]->GetParameterType() == paramType)
		{
			++index;
		}

		// For this case, no subtracks are considered.
	}

	assert(!"CAnimNode::GetTrackParamIndex() called with an invalid argument!");
	return 0;
}

IAnimTrack* CAnimNode::GetTrackByIndex(int nIndex) const
{
	if (nIndex >= (int)m_tracks.size())
	{
		assert("nIndex>=m_tracks.size()" && false);
		return NULL;
	}

	return m_tracks[nIndex];
}

void CAnimNode::SetTrack(const CAnimParamType& paramType, IAnimTrack* pTrack)
{
	if (pTrack)
	{
		for (unsigned int i = 0; i < m_tracks.size(); i++)
		{
			if (m_tracks[i]->GetParameterType() == paramType)
			{
				m_tracks[i] = pTrack;
				return;
			}
		}

		AddTrack(pTrack);
	}
	else
	{
		// Remove track at this id.
		for (unsigned int i = 0; i < m_tracks.size(); i++)
		{
			if (m_tracks[i]->GetParameterType() == paramType)
			{
				m_tracks.erase(m_tracks.begin() + i);
			}
		}
	}
}

bool CAnimNode::TrackOrder(const _smart_ptr<IAnimTrack>& left, const _smart_ptr<IAnimTrack>& right)
{
	return left->GetParameterType() < right->GetParameterType();
}

void CAnimNode::AddTrack(IAnimTrack* pTrack)
{
	pTrack->SetTimeRange(GetSequence()->GetTimeRange());
	m_tracks.push_back(pTrack);
	std::stable_sort(m_tracks.begin(), m_tracks.end(), TrackOrder);
}

bool CAnimNode::RemoveTrack(IAnimTrack* pTrack)
{
	for (unsigned int i = 0; i < m_tracks.size(); i++)
	{
		if (m_tracks[i] == pTrack)
		{
			m_tracks.erase(m_tracks.begin() + i);
			return true;
		}
	}

	return false;
}

IAnimTrack* CAnimNode::CreateTrackInternal(const CAnimParamType& paramType, EAnimValue valueType)
{
	if (valueType == eAnimValue_Unknown)
	{
		SParamInfo info;

		// Try to get info from paramType, else we can't determine the track data type
		if (!GetParamInfoFromType(paramType, info))
		{
			return 0;
		}

		valueType = info.valueType;
	}

	IAnimTrack* pTrack = NULL;

	switch (paramType.GetType())
	{
	// Create sub-classed tracks
	case eAnimParamType_Event:
		pTrack = new CEventTrack;
		break;

	case eAnimParamType_AudioTrigger:
		pTrack = new CAudioTriggerTrack;
		break;

	case eAnimParamType_AudioFile:
		pTrack = new CAudioFileTrack;
		break;

	case eAnimParamType_AudioParameter:
		pTrack = new CAudioParameterTrack;
		break;

	case eAnimParamType_AudioSwitch:
		pTrack = new CAudioSwitchTrack;
		break;

	case eAnimParamType_DynamicResponseSignal:
		pTrack = new CDynamicResponseSignalTrack;
		break;

	case eAnimParamType_Animation:
		pTrack = new CCharacterTrack;
		break;

	case eAnimParamType_Mannequin:
		pTrack = new CMannequinTrack;
		break;

	case eAnimParamType_Expression:
		pTrack = new CExprTrack;
		break;

	case eAnimParamType_Console:
		pTrack = new CConsoleTrack;
		break;

	case eAnimParamType_LookAt:
		pTrack = new CLookAtTrack;
		break;

	case eAnimParamType_TrackEvent:
		pTrack = new CTrackEventTrack;
		break;

	case eAnimParamType_Sequence:
		pTrack = new CSequenceTrack;
		break;

	case eAnimParamType_Capture:
		pTrack = new CCaptureTrack;
		break;

	case eAnimParamType_CommentText:
		pTrack = new CCommentTrack;
		break;

	case eAnimParamType_ScreenFader:
		pTrack = new CScreenFaderTrack;
		break;

	case eAnimParamType_Goto:
		pTrack = new CGotoTrack;
		break;

	case eAnimParamType_TimeRanges:
		pTrack = new CTimeRangesTrack;
		break;

	case eAnimParamType_Camera:
		pTrack = new CCameraTrack;
		break;

	case eAnimParamType_FaceSequence:
		pTrack = new CFaceSequenceTrack;
		break;

	case eAnimParamType_Float:
		pTrack = CreateTrackInternalFloat(paramType);
		break;

	default:
		// Create standard tracks
		switch (valueType)
		{
		case eAnimValue_Float:
			pTrack = CreateTrackInternalFloat(paramType);
			break;

		case eAnimValue_RGB:
		case eAnimValue_Vector:
			pTrack = CreateTrackInternalVector(paramType, valueType);
			break;

		case eAnimValue_Quat:
			pTrack = CreateTrackInternalQuat(paramType);
			break;

		case eAnimValue_Bool:
			pTrack = new CBoolTrack(paramType);
			break;

		case eAnimValue_Vector4:
			pTrack = CreateTrackInternalVector4(paramType);
			break;
		}
	}

	if (pTrack)
	{
		AddTrack(pTrack);
	}

	return pTrack;
}

IAnimTrack* CAnimNode::CreateTrack(const CAnimParamType& paramType)
{
	IAnimTrack* pTrack = CreateTrackInternal(paramType, eAnimValue_Unknown);
	InitializeTrackDefaultValue(pTrack, paramType);
	return pTrack;
}

void CAnimNode::SerializeAnims(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Animation, 0, "CAnimNode");

	if (bLoading)
	{
		// Delete all tracks.
		stl::free_container(m_tracks);

		CAnimNode::SParamInfo info;
		// Loading.
		int paramTypeVersion = 0;
		xmlNode->getAttr("paramIdVersion", paramTypeVersion);
		CAnimParamType paramType;
		int num = xmlNode->getChildCount();

		for (int i = 0; i < num; i++)
		{
			XmlNodeRef trackNode = xmlNode->getChild(i);
			paramType.Serialize(trackNode, bLoading, paramTypeVersion);

			MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Animation, 0, gEnv->pMovieSystem->GetParamTypeName(paramType));

			if (paramTypeVersion == 0) // for old version with sound and animation param ids swapped
			{
				const int APARAM_ANIMATION_OLD = eAnimParamType_AudioTrigger;
				const int APARAM_SOUND_OLD = eAnimParamType_Animation;

				if (paramType.GetType() == APARAM_ANIMATION_OLD)
				{
					paramType = eAnimParamType_Animation;
				}
				else if (paramType.GetType() == APARAM_SOUND_OLD)
				{
					paramType = eAnimParamType_AudioTrigger;
				}
			}

			if (paramTypeVersion <= 3 && paramType.GetType() >= OLD_APARAM_USER)
			{
				// APARAM_USER 100 => 100000
				paramType = paramType.GetType() + (eAnimParamType_User - OLD_APARAM_USER);
			}

			if (paramTypeVersion <= 4)
			{
				// In old versions there was special code for particles
				// that is now handles by generic entity node code
				switch (paramType.GetType())
				{
				case OLD_APARAM_PARTICLE_COUNT_SCALE:
					paramType = CAnimParamType("ScriptTable:Properties/CountScale");
					break;

				case OLD_APARAM_PARTICLE_PULSE_PERIOD:
					paramType = CAnimParamType("ScriptTable:Properties/PulsePeriod");
					break;

				case OLD_APARAM_PARTICLE_SCALE:
					paramType = CAnimParamType("ScriptTable:Properties/Scale");
					break;

				case OLD_APARAM_PARTICLE_SPEED_SCALE:
					paramType = CAnimParamType("ScriptTable:Properties/SpeedScale");
					break;

				case OLD_APARAM_PARTICLE_STRENGTH:
					paramType = CAnimParamType("ScriptTable:Properties/Strength");
					break;
				}
			}

			if (paramTypeVersion <= 5 && !(GetSequence()->GetFlags() & IAnimSequence::eSeqFlags_LightAnimationSet))
			{
				// In old versions there was special code for lights that is now handled
				// by generic entity node code if this is not a light animation set sequence
				switch (paramType.GetType())
				{
				case eAnimParamType_LightDiffuse:
					paramType = CAnimParamType("ScriptTable:Properties/Color/clrDiffuse");
					break;

				case eAnimParamType_LightRadius:
					paramType = CAnimParamType("ScriptTable:Properties/Radius");
					break;

				case eAnimParamType_LightDiffuseMult:
					paramType = CAnimParamType("ScriptTable:Properties/Color/fDiffuseMultiplier");
					break;

				case eAnimParamType_LightHDRDynamic:
					paramType = CAnimParamType("ScriptTable:Properties/Color/fHDRDynamic");
					break;

				case eAnimParamType_LightSpecularMult:
					paramType = CAnimParamType("ScriptTable:Properties/Color/fSpecularMultiplier");
					break;

				case eAnimParamType_LightSpecPercentage:
					paramType = CAnimParamType("ScriptTable:Properties/Color/fSpecularPercentage");
					break;
				}
			}

			if (paramTypeVersion <= 7 && paramType.GetType() == eAnimParamType_Physics)
			{
				paramType = eAnimParamType_PhysicsDriven;
			}

			int valueType = eAnimValue_Unknown;
			trackNode->getAttr("ValueType", valueType);

			IAnimTrack* pTrack = CreateTrackInternal(paramType, (EAnimValue)valueType);

			if (pTrack)
			{
				if (!pTrack->Serialize(trackNode, bLoading, bLoadEmptyTracks))
				{
					// Boolean tracks must always be loaded even if empty.
					if (pTrack->GetValueType() != eAnimValue_Bool)
					{
						RemoveTrack(pTrack);
					}
				}
			}

			if (gEnv->IsEditor())
			{
				InitializeTrackDefaultValue(pTrack, paramType);
			}
		}
	}
	else
	{
		// Saving.
		xmlNode->setAttr("paramIdVersion", CAnimParamType::kParamTypeVersion);

		for (unsigned int i = 0; i < m_tracks.size(); i++)
		{
			IAnimTrack* pTrack = m_tracks[i];

			if (pTrack)
			{
				CAnimParamType paramType = m_tracks[i]->GetParameterType();
				XmlNodeRef trackNode = xmlNode->newChild("Track");
				paramType.Serialize(trackNode, bLoading);
				pTrack->Serialize(trackNode, bLoading);

				int valueType = pTrack->GetValueType();
				if (valueType != eAnimValue_Unknown)
				{
					trackNode->setAttr("ValueType", valueType);
				}
			}
		}
	}
}

void CAnimNode::SetTimeRange(TRange<SAnimTime> timeRange)
{
	for (unsigned int i = 0; i < m_tracks.size(); i++)
	{
		if (m_tracks[i])
		{
			m_tracks[i]->SetTimeRange(timeRange);
		}
	}
}

CAnimNode::CAnimNode(const int id) : m_id(id)
{
	m_guid = CryGUID::Create();
	m_pOwner = 0;
	m_pSequence = 0;
	m_flags = 0;
	m_bIgnoreSetParam = false;
	m_pParentNode = 0;
	m_nLoadedParentNodeId = 0;
}

void CAnimNode::SetFlags(int flags)
{
	m_flags = flags;
}

int CAnimNode::GetFlags() const
{
	return m_flags;
}

bool CAnimNode::IsParamValid(const CAnimParamType& paramType) const
{
	SParamInfo info;

	if (GetParamInfoFromType(paramType, info))
	{
		return true;
	}

	return false;
}

void CAnimNode::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks)
{
	if (bLoading)
	{
		xmlNode->getAttr("Id", m_id);
		xmlNode->getAttr("GUIDLo", m_guid.lopart);
		xmlNode->getAttr("GUIDHi", m_guid.hipart);
		const char* name = xmlNode->getAttr("Name");
		int flags;

		if (xmlNode->getAttr("Flags", flags))
		{
			// Don't load expanded or selected flags
			SetFlags(flags);
		}

		SetName(name);

		m_nLoadedParentNodeId = 0;
		xmlNode->getAttr("ParentNode", m_nLoadedParentNodeId);
	}
	else
	{
		m_nLoadedParentNodeId = 0;
		xmlNode->setAttr("Id", m_id);
		xmlNode->setAttr("GUIDLo", m_guid.lopart);
		xmlNode->setAttr("GUIDHi", m_guid.hipart);

		EAnimNodeType nodeType = GetType();
		static_cast<CMovieSystem*>(gEnv->pMovieSystem)->SerializeNodeType(nodeType, xmlNode, bLoading, IAnimSequence::kSequenceVersion, m_flags);

		xmlNode->setAttr("Name", GetName());
		xmlNode->setAttr("Flags", GetFlags());

		if (m_pParentNode)
		{
			xmlNode->setAttr("ParentNode", static_cast<CAnimNode*>(m_pParentNode)->GetId());
		}
	}

	SerializeAnims(xmlNode, bLoading, bLoadEmptyTracks);
}

void CAnimNode::SetNodeOwner(IAnimNodeOwner* pOwner)
{
	m_pOwner = pOwner;

	if (pOwner)
	{
		pOwner->OnNodeAnimated(this);
	}
}

void CAnimNode::PostLoad()
{
	if (m_nLoadedParentNodeId)
	{
		IAnimNode* pParentNode = ((CAnimSequence*)m_pSequence)->FindNodeById(m_nLoadedParentNodeId);
		m_pParentNode = pParentNode;
		m_nLoadedParentNodeId = 0;
	}
}

IAnimTrack* CAnimNode::CreateTrackInternalFloat(const CAnimParamType& paramType) const
{
	return new CAnimSplineTrack(paramType);
}

IAnimTrack* CAnimNode::CreateTrackInternalVector(const CAnimParamType& paramType, const EAnimValue animValue) const
{
	CAnimParamType subTrackParamTypes[MAX_SUBTRACKS];

	for (unsigned int i = 0; i < MAX_SUBTRACKS; ++i)
	{
		subTrackParamTypes[i] = eAnimParamType_Float;
	}

	if (paramType == eAnimParamType_Position)
	{
		subTrackParamTypes[0] = eAnimParamType_PositionX;
		subTrackParamTypes[1] = eAnimParamType_PositionY;
		subTrackParamTypes[2] = eAnimParamType_PositionZ;
	}
	else if (paramType == eAnimParamType_Scale)
	{
		subTrackParamTypes[0] = eAnimParamType_ScaleX;
		subTrackParamTypes[1] = eAnimParamType_ScaleY;
		subTrackParamTypes[2] = eAnimParamType_ScaleZ;
	}
	else if (paramType == eAnimParamType_Rotation)
	{
		subTrackParamTypes[0] = eAnimParamType_RotationX;
		subTrackParamTypes[1] = eAnimParamType_RotationY;
		subTrackParamTypes[2] = eAnimParamType_RotationZ;
		IAnimTrack* pTrack = new CCompoundSplineTrack(paramType, 3, eAnimValue_Quat, subTrackParamTypes);
		return pTrack;
	}
	else if (paramType == eAnimParamType_DepthOfField)
	{
		subTrackParamTypes[0] = eAnimParamType_FocusDistance;
		subTrackParamTypes[1] = eAnimParamType_FocusRange;
		subTrackParamTypes[2] = eAnimParamType_BlurAmount;
		IAnimTrack* pTrack = new CCompoundSplineTrack(paramType, 3, eAnimValue_Vector, subTrackParamTypes);
		pTrack->SetSubTrackName(0, "FocusDist");
		pTrack->SetSubTrackName(1, "FocusRange");
		pTrack->SetSubTrackName(2, "BlurAmount");
		return pTrack;
	}
	else if (animValue == eAnimValue_RGB || paramType == eAnimParamType_LightDiffuse ||
	         paramType == eAnimParamType_MaterialDiffuse || paramType == eAnimParamType_MaterialSpecular
	         || paramType == eAnimParamType_MaterialEmissive)
	{
		subTrackParamTypes[0] = eAnimParamType_ColorR;
		subTrackParamTypes[1] = eAnimParamType_ColorG;
		subTrackParamTypes[2] = eAnimParamType_ColorB;
		IAnimTrack* pTrack = new CCompoundSplineTrack(paramType, 3, eAnimValue_RGB, subTrackParamTypes);
		pTrack->SetSubTrackName(0, "Red");
		pTrack->SetSubTrackName(1, "Green");
		pTrack->SetSubTrackName(2, "Blue");
		return pTrack;
	}

	return new CCompoundSplineTrack(paramType, 3, eAnimValue_Vector, subTrackParamTypes);
}

IAnimTrack* CAnimNode::CreateTrackInternalQuat(const CAnimParamType& paramType) const
{
	CAnimParamType subTrackParamTypes[MAX_SUBTRACKS];

	if (paramType == eAnimParamType_Rotation)
	{
		subTrackParamTypes[0] = eAnimParamType_RotationX;
		subTrackParamTypes[1] = eAnimParamType_RotationY;
		subTrackParamTypes[2] = eAnimParamType_RotationZ;
	}
	else
	{
		// Unknown param type
		assert(0);
	}

	return new CCompoundSplineTrack(paramType, 3, eAnimValue_Quat, subTrackParamTypes);
}

IAnimTrack* CAnimNode::CreateTrackInternalVector4(const CAnimParamType& paramType) const
{
	IAnimTrack* pTrack;

	CAnimParamType subTrackParamTypes[MAX_SUBTRACKS];

	for (unsigned int i = 0; i < MAX_SUBTRACKS; ++i)
	{
		subTrackParamTypes[i] = eAnimParamType_Float;
	}

	if (paramType == eAnimParamType_TransformNoise
	    || paramType == eAnimParamType_ShakeMultiplier)
	{
		subTrackParamTypes[0] = eAnimParamType_ShakeAmpAMult;
		subTrackParamTypes[1] = eAnimParamType_ShakeAmpBMult;
		subTrackParamTypes[2] = eAnimParamType_ShakeFreqAMult;
		subTrackParamTypes[3] = eAnimParamType_ShakeFreqBMult;

		pTrack = new CCompoundSplineTrack(paramType, 4, eAnimValue_Vector4, subTrackParamTypes);

		if (paramType == eAnimParamType_TransformNoise)
		{
			pTrack->SetSubTrackName(0, "Pos Noise Amp");
			pTrack->SetSubTrackName(1, "Pos Noise Freq");
			pTrack->SetSubTrackName(2, "Rot Noise Amp");
			pTrack->SetSubTrackName(3, "Rot Noise Freq");
		}
		else
		{
			pTrack->SetSubTrackName(0, "Amplitude A");
			pTrack->SetSubTrackName(1, "Amplitude B");
			pTrack->SetSubTrackName(2, "Frequency A");
			pTrack->SetSubTrackName(3, "Frequency B");
		}
	}
	else
	{
		pTrack = new CCompoundSplineTrack(paramType, 4, eAnimValue_Vector4, subTrackParamTypes);
	}

	return pTrack;
}

IAnimNode* CAnimNode::HasDirectorAsParent() const
{
	IAnimNode* pParent = GetParent();

	while (pParent)
	{
		if (pParent->GetType() == eAnimNodeType_Director)
		{
			return pParent;
		}

		// There are some invalid data.
		if (pParent->GetParent() == pParent)
		{
			pParent->SetParent(NULL);
			return NULL;
		}

		pParent = pParent->GetParent();
	}

	return NULL;
}
