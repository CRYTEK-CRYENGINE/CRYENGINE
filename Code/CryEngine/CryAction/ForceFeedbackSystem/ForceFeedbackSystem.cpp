// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------

   Implementation for force feedback system

* Effect definition (shape, time, ...) are defined in xml
* Effects are invoked by name, and updated here internally, feeding
   input system in a per frame basis

   -------------------------------------------------------------------------
   History:
   - 18-02-2010:	Created by Benito Gangoso Rodriguez
   - 20-09-2012: Updated by Dario Sancho (added support for Durango Triggers feedback)

*************************************************************************/

#include "StdAfx.h"
#include "ForceFeedbackSystem.h"
#include "GameXmlParamReader.h"

#include <CryString/StringUtils.h>

#if !defined(_RELEASE)
	#define DEBUG_FORCEFEEDBACK_SYSTEM
#endif

#define MAX_FORCE_FEEDBACK_EFFECTS 8
//#define MAX_FORCE_FEEDBACK_EFFECTS 100  //For stress testing

#ifdef DEBUG_FORCEFEEDBACK_SYSTEM
	#include <CryAction/IDebugHistory.h>

class CForceFeedBackSystemDebug
{
public:
	static void Init()
	{
		assert(s_pDebugHistoryManager == NULL);

		if (s_pDebugHistoryManager == NULL)
		{
			s_pDebugHistoryManager = CCryAction::GetCryAction()->CreateDebugHistoryManager();
		}
	}

	static void Shutdown()
	{
		SAFE_RELEASE(s_pDebugHistoryManager);
	}

	static void DebugFFOutput(bool debugEnabled, float motorA, float motorB, const SFFTriggerOutputData& triggers)
	{

		if (!debugEnabled)
		{
			if (s_pDebugHistoryManager)
			{
				s_pDebugHistoryManager->Clear();
			}
			return;
		}

		if (s_pDebugHistoryManager)
		{
			s_pDebugHistoryManager->LayoutHelper("FFMotorA_HighFreq", NULL, true, -20, 20, 0, 1, 0.0f, 0.0f);
			s_pDebugHistoryManager->LayoutHelper("FFMotorB_LowFreq", NULL, true, -20, 20, 0, 1, 1.0f, 0.0f);
			s_pDebugHistoryManager->LayoutHelper("FFLeftTriggerGain", NULL, true, 0, 1, 0, 1.f, 2.0f, 0.0f);
			s_pDebugHistoryManager->LayoutHelper("FFRightTriggerGain", NULL, true, 0, 1, 0, 1.f, 2.5f, 0.0f);
			s_pDebugHistoryManager->LayoutHelper("FFLeftTriggerEnv", NULL, true, 0, 2000, 0, 2000.f, 3.0f, 0.0f);
			s_pDebugHistoryManager->LayoutHelper("FFRightTriggerEnv", NULL, true, 0, 2000, 0, 2000.f, 3.5f, 0.0f);

			IDebugHistory* pDHMotorA = s_pDebugHistoryManager->GetHistory("FFMotorA_HighFreq");
			if (pDHMotorA != NULL)
			{
				pDHMotorA->AddValue(motorA);
			}

			IDebugHistory* pDHMotorB = s_pDebugHistoryManager->GetHistory("FFMotorB_LowFreq");
			if (pDHMotorB != NULL)
			{
				pDHMotorB->AddValue(motorB);
			}

			IDebugHistory* pDHLeftTriggerGain = s_pDebugHistoryManager->GetHistory("FFLeftTriggerGain");
			if (pDHLeftTriggerGain != NULL)
			{
				pDHLeftTriggerGain->AddValue(triggers.leftGain);
			}

			IDebugHistory* pDHRightTriggerGain = s_pDebugHistoryManager->GetHistory("FFRightTriggerGain");
			if (pDHRightTriggerGain != NULL)
			{
				pDHRightTriggerGain->AddValue(triggers.rightGain);
			}

			IDebugHistory* pDHLeftTriggerEnv = s_pDebugHistoryManager->GetHistory("FFLeftTriggerEnv");
			if (pDHLeftTriggerEnv != NULL)
			{
				pDHLeftTriggerEnv->AddValue(triggers.leftEnv);
			}

			IDebugHistory* pDHRightTriggerEnv = s_pDebugHistoryManager->GetHistory("FFRightTriggerEnv");
			if (pDHRightTriggerEnv != NULL)
			{
				pDHRightTriggerEnv->AddValue(triggers.rightEnv);
			}
		}
	}

private:

	static IDebugHistoryManager* s_pDebugHistoryManager;
};

IDebugHistoryManager* CForceFeedBackSystemDebug::s_pDebugHistoryManager = NULL;
#else
class CForceFeedBackSystemDebug
{
public:
	ILINE static void Init()
	{

	}

	ILINE static void Shutdown()
	{

	}

	ILINE static void DebugFFOutput(bool debugEnabled, float motorA, float motorB, const SFFTriggerOutputData& triggers)
	{

	}
};
#endif

#ifdef DEBUG_FORCEFEEDBACK_SYSTEM
	#define FORCEFEEDBACK_LOG(...) GameWarning("[ForceFeedback System] " __VA_ARGS__)
#else
	#define FORCEFEEDBACK_LOG(...) (void)(0)
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
#if defined(_DEBUG)

void CForceFeedBackSystem::FFBInternalId::Set(const char* _name)
{
	assert(_name);
	name = _name;
	id = CryStringUtils::CalculateHash(_name);
}

const char* CForceFeedBackSystem::FFBInternalId::GetDebugName() const
{
	return name.c_str();
}

#else

void CForceFeedBackSystem::FFBInternalId::Set(const char* _name)
{
	assert(_name);
	id = CryStringUtils::CalculateHash(_name);
}

const char* CForceFeedBackSystem::FFBInternalId::GetDebugName() const
{
	return "";
}

#endif

CForceFeedBackSystem::FFBInternalId& CForceFeedBackSystem::FFBInternalId::GetIdForName(const char* name)
{
	static FFBInternalId cachedId;

	cachedId.id = CryStringUtils::CalculateHash(name);

	return cachedId;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CForceFeedBackSystem::CForceFeedBackSystem()
	: m_effectLock(0)
{
	CForceFeedBackSystemDebug::Init();

	m_defaultPattern.ResetToDefault();
	m_defaultEnvelope.ResetToDefault();

	m_activeEffects.reserve(MAX_FORCE_FEEDBACK_EFFECTS);
}

CForceFeedBackSystem::~CForceFeedBackSystem()
{
	CForceFeedBackSystemDebug::Shutdown();
}

void CForceFeedBackSystem::PlayForceFeedbackEffect(ForceFeedbackFxId id, const SForceFeedbackRuntimeParams& runtimeParams)
{
	if (m_effectLock > 0)
	{
		return;
	}

	//Note: As requested, if the effect is running prefer to re-start, and reset its runtimeParams
	if (TryToRestartEffectIfAlreadyRunning(id, runtimeParams))
	{
		return;
	}

	const int activeEffectSize = m_activeEffects.size();
	bool freeSlotsAvailable = (activeEffectSize < (int)MAX_FORCE_FEEDBACK_EFFECTS);

	if (freeSlotsAvailable)
	{
		const int testId = (int)id;
		bool validId = ((testId >= 0) && (testId < (int)(m_effects.size())));

		if (validId)
		{
			const SEffect& effect = m_effects[id];

			const SPattern& effectPatternA = FindPattern(effect.patternA);
			const SPattern& effectPatternB = (effect.patternA == effect.patternB) ? effectPatternA : FindPattern(effect.patternB);
			const SPattern& effectPatternLT = FindPattern(effect.patternLT);
			const SPattern& effectPatternRT = (effect.patternLT == effect.patternRT) ? effectPatternLT : FindPattern(effect.patternRT);

			const SEnvelope& effectEnvelopeA = FindEnvelope(effect.envelopeA);
			const SEnvelope& effectEnvelopeB = (effect.envelopeA == effect.envelopeB) ? effectEnvelopeA : FindEnvelope(effect.envelopeB);
			const SEnvelope& effectEnvelopeLT = FindEnvelope(effect.envelopeLT);
			const SEnvelope& effectEnvelopeRT = (effect.envelopeLT == effect.envelopeRT) ? effectEnvelopeLT : FindEnvelope(effect.envelopeRT);

			SActiveEffect newActiveEffect;
			m_activeEffects.push_back(newActiveEffect);

			SActiveEffect& justAddedEffect = m_activeEffects[activeEffectSize];
			justAddedEffect.effectId = id;
			justAddedEffect.effectTime = effect.time;
			justAddedEffect.runningTime = 0.0f;
			justAddedEffect.frequencyA = effect.frequencyA;
			justAddedEffect.frequencyB = effect.frequencyB;
			justAddedEffect.frequencyLT = effect.frequencyLT;
			justAddedEffect.frequencyRT = effect.frequencyRT;
			justAddedEffect.runtimeParams = runtimeParams;

			//Patters are copied, for faster access when loop-processing, instead of being a pointer
			//Since we have a very small amount of fixed FX it should not be a big deal.
			justAddedEffect.m_patternA = effectPatternA;
			justAddedEffect.m_envelopeA = effectEnvelopeA;
			justAddedEffect.m_patternB = effectPatternB;
			justAddedEffect.m_envelopeB = effectEnvelopeB;
			justAddedEffect.m_patternLT = effectPatternLT;
			justAddedEffect.m_envelopeLT = effectEnvelopeLT;
			justAddedEffect.m_patternRT = effectPatternRT;
			justAddedEffect.m_envelopeRT = effectEnvelopeRT;
		}
		else
		{
			FORCEFEEDBACK_LOG("Play effect could not find effect. Invalid id '%d' provided", id);
		}

	}
	else
	{
		FORCEFEEDBACK_LOG("Too many effects already running, could not execute");
	}
}

bool CForceFeedBackSystem::TryToRestartEffectIfAlreadyRunning(ForceFeedbackFxId id, const SForceFeedbackRuntimeParams& runtimeParams)
{
	TActiveEffectsArray::iterator activeEffectsEndIt = m_activeEffects.end();

	for (TActiveEffectsArray::iterator activeEffectIt = m_activeEffects.begin(); activeEffectIt != activeEffectsEndIt; ++activeEffectIt)
	{
		if (activeEffectIt->effectId != id)
			continue;

		activeEffectIt->runningTime = 0.0f;
		activeEffectIt->runtimeParams = runtimeParams;
		return true;
	}

	return false;
}

void CForceFeedBackSystem::StopForceFeedbackEffect(ForceFeedbackFxId id)
{
	TActiveEffectsArray::iterator activeEffectIt = m_activeEffects.begin();

	while (activeEffectIt != m_activeEffects.end())
	{
		if (activeEffectIt->effectId != id)
		{
			++activeEffectIt;
		}
		else
		{
			TActiveEffectsArray::iterator next = m_activeEffects.erase(activeEffectIt);
			activeEffectIt = next;
		}
	}
}

ForceFeedbackFxId CForceFeedBackSystem::GetEffectIdByName(const char* effectName) const
{
	TEffectToIndexMap::const_iterator cit = m_effectToIndexMap.find(FFBInternalId::GetIdForName(effectName));

	if (cit != m_effectToIndexMap.end())
	{
		return cit->second;
	}
	FORCEFEEDBACK_LOG("Could not get effect id for \"%s\".", effectName);

	return InvalidForceFeedbackFxId;
}

void CForceFeedBackSystem::StopAllEffects()
{
	m_activeEffects.clear();

	UpdateInputSystem(0.0f, 0.0f, SFFTriggerOutputData(SFFTriggerOutputData::Initial::ZeroIt));
}

void CForceFeedBackSystem::AddFrameCustomForceFeedback(const float amplifierA, const float amplifierB, const float amplifierLT /*= 0.0f*/, const float amplifierRT /*= 0.0f*/)
{
	m_frameCustomForceFeedback.forceFeedbackA += amplifierA;
	m_frameCustomForceFeedback.forceFeedbackB += amplifierB;
	m_frameCustomForceFeedback.forceFeedbackLT += amplifierLT;
	m_frameCustomForceFeedback.forceFeedbackRT += amplifierRT;
}

void CForceFeedBackSystem::AddCustomTriggerForceFeedback(const SFFTriggerOutputData& triggersData)
{
	m_triggerCustomForceFeedBack += triggersData;
}

void CForceFeedBackSystem::Update(float frameTime)
{
	SFFOutput forceFeedback;
	SFFTriggerOutputData triggerForceFeedback;

	// If the game is paused then we must not apply force feedback, otherwise
	// it might be left on whilst the game is paused
	if (gEnv->pSystem->IsPaused() == false && frameTime > 0.001f)
	{
		TActiveEffectsArray::iterator activeEffectIt = m_activeEffects.begin();

		while (activeEffectIt != m_activeEffects.end())
		{
			SActiveEffect& currentEffect = *activeEffectIt;

			SFFOutput current = currentEffect.Update(frameTime);
			forceFeedback += current;
			triggerForceFeedback.leftStrength += current.forceFeedbackLT;
			triggerForceFeedback.rightStrength += current.forceFeedbackRT;

			if (!currentEffect.HasFinished())
			{
				++activeEffectIt;
			}
			else
			{
				TActiveEffectsArray::iterator next = m_activeEffects.erase(activeEffectIt);
				activeEffectIt = next;
			}
		}

		forceFeedback += m_frameCustomForceFeedback;
		// DARIO_NOTE: so far designers do not want patters loaded from XML so all the data come direclty from
		// FlowGraph nodes
		triggerForceFeedback += m_triggerCustomForceFeedBack;
		m_frameCustomForceFeedback.ZeroIt();
		m_triggerCustomForceFeedBack.Init(SFFTriggerOutputData::Initial::ZeroIt);
	}

	UpdateInputSystem(forceFeedback.GetClampedFFA(), forceFeedback.GetClampedFFB(), triggerForceFeedback);

	CForceFeedBackSystemDebug::DebugFFOutput((m_cvars.ffs_debug != 0), forceFeedback.GetClampedFFA(), forceFeedback.GetClampedFFB(), triggerForceFeedback);
}

void CForceFeedBackSystem::UpdateInputSystem(const float amplifierA, const float amplifierB, const SFFTriggerOutputData& triggers)
{
	if (gEnv->pInput)
	{
		SFFOutputEvent ffEvent;
		ffEvent.deviceType = eIDT_Gamepad;
		ffEvent.eventId = eFF_Rumble_Frame;
		ffEvent.amplifierA = amplifierA;
		ffEvent.amplifierS = amplifierB;
		ffEvent.triggerData = triggers;
		gEnv->pInput->ForceFeedbackEvent(ffEvent);
	}
}

void CForceFeedBackSystem::Initialize()
{
	LoadXmlData();
}

void CForceFeedBackSystem::Reload()
{
	StopAllEffects();

	m_patters.clear();
	m_envelopes.clear();
	m_effects.clear();
	m_effectToIndexMap.clear();

	LoadXmlData();
}

void CForceFeedBackSystem::LoadXmlData()
{
	const char* xmlDataFile = "Libs/GameForceFeedback/ForceFeedbackEffects.xml";
	XmlNodeRef rootNode = gEnv->pSystem->LoadXmlFromFile(xmlDataFile);

	if (!rootNode || strcmpi(rootNode->getTag(), "ForceFeedback"))
	{
		FORCEFEEDBACK_LOG("Could not load force feedback system data. Invalid XML file '%s'! ", xmlDataFile);
		return;
	}

	const int childCount = rootNode->getChildCount();
	for (int i = 0; i < childCount; ++i)
	{
		XmlNodeRef childNode = rootNode->getChild(i);

		const char* childTag = childNode->getTag();
		if (strcmp(childTag, "Patterns") == 0)
		{
			LoadPatters(childNode);
		}
		else if (strcmp(childTag, "Envelopes") == 0)
		{
			LoadEnvelopes(childNode);
		}
		else if (strcmp(childTag, "Effects") == 0)
		{
			LoadEffects(childNode);
		}
	}
}

void CForceFeedBackSystem::LoadPatters(XmlNodeRef& patternsNode)
{
	const int patterCount = patternsNode->getChildCount();

	m_patters.reserve(patterCount);

	TSamplesBuffer samplesBuffer;
	const int maxSampleCount = FFSYSTEM_MAX_PATTERN_SAMPLES / 2;
	float readValues[maxSampleCount];

	for (int i = 0; i < patterCount; ++i)
	{
		XmlNodeRef childPatternNode = patternsNode->getChild(i);

		const char* customPatternName = childPatternNode->getAttr("name");
		if (!customPatternName || (customPatternName[0] == '\0'))
		{
			FORCEFEEDBACK_LOG("Could not load pattern without name (at line %d)", childPatternNode->getLine());
			continue;
		}

		samplesBuffer = childPatternNode->haveAttr("name") ? childPatternNode->getAttr("samples") : "";

		int samplesFound = ParseSampleBuffer(samplesBuffer, &readValues[0], maxSampleCount);

		if (samplesFound != 0)
		{
			SPattern customPattern;
			customPattern.ResetToDefault();

			DistributeSamples(&readValues[0], samplesFound, &customPattern.m_patternSamples[0], FFSYSTEM_MAX_PATTERN_SAMPLES);

			customPattern.m_patternId.Set(customPatternName);
			m_patters.push_back(customPattern);
		}
		else
		{
			FORCEFEEDBACK_LOG("Pattern '%s' (at line %d) has not samples, skipping", customPatternName, childPatternNode->getLine());
		}
	}

	std::sort(m_patters.begin(), m_patters.end());
}

void CForceFeedBackSystem::LoadEnvelopes(XmlNodeRef& envelopesNode)
{
	const int envelopesCount = envelopesNode->getChildCount();

	m_envelopes.reserve(envelopesCount);

	TSamplesBuffer samplesBuffer;
	const int maxSampleCount = FFSYSTEM_MAX_ENVELOPE_SAMPLES / 2;
	float readValues[maxSampleCount];

	for (int i = 0; i < envelopesCount; ++i)
	{
		XmlNodeRef envelopeChildNode = envelopesNode->getChild(i);

		const char* customEnvelopeName = envelopeChildNode->getAttr("name");
		if (!customEnvelopeName || (customEnvelopeName[0] == '\0'))
		{
			FORCEFEEDBACK_LOG("Could not load envelope without name (at line %d)", envelopeChildNode->getLine());
			continue;
		}

		samplesBuffer = envelopeChildNode->haveAttr("name") ? envelopeChildNode->getAttr("samples") : "";

		int samplesFound = ParseSampleBuffer(samplesBuffer, &readValues[0], maxSampleCount);

		if (samplesFound != 0)
		{
			SEnvelope customEnvelope;
			customEnvelope.ResetToDefault();

			DistributeSamples(&readValues[0], samplesFound, &customEnvelope.m_envelopeSamples[0], FFSYSTEM_MAX_ENVELOPE_SAMPLES);

			customEnvelope.m_envelopeId.Set(customEnvelopeName);
			m_envelopes.push_back(customEnvelope);
		}
		else
		{
			FORCEFEEDBACK_LOG("Envelope '%s' (at line %d) has not samples, skipping", customEnvelopeName, envelopeChildNode->getLine());
		}
	}

	std::sort(m_envelopes.begin(), m_envelopes.end());
}

void CForceFeedBackSystem::LoadEffects(XmlNodeRef& effectsNode)
{
	CGameXmlParamReader paramReader(effectsNode);
	const int effectsCount = paramReader.GetUnfilteredChildCount();

	m_effectToIndexMap.reserve(effectsCount);
	m_effects.reserve(effectsCount);
	m_effectNames.reserve(effectsCount);

	for (int i = 0; i < effectsCount; ++i)
	{
		XmlNodeRef childEffectNode = paramReader.GetFilteredChildAt(i);

		if (childEffectNode)
		{
			SEffect newEffect;
			const int effectDataCount = childEffectNode->getChildCount();

			const char* effectName = childEffectNode->getAttr("name");

			//Check for invalid name
			if ((effectName == NULL) || (effectName[0] == '\0'))
			{
				FORCEFEEDBACK_LOG("Could not load effect without name (at line %d)", childEffectNode->getLine());
				continue;
			}

			//Check for duplicates
			if (m_effectToIndexMap.find(FFBInternalId::GetIdForName(effectName)) != m_effectToIndexMap.end())
			{
				FORCEFEEDBACK_LOG("Effect '%s' does already exists, skipping", effectName);
				continue;
			}

			childEffectNode->getAttr("time", newEffect.time);

			for (int j = 0; j < effectDataCount; ++j)
			{
				XmlNodeRef motorNode = childEffectNode->getChild(j);

				const char* motorTag = motorNode->getTag();

				if (strcmp(motorTag, "MotorAB") == 0)
				{
					newEffect.patternA.Set(motorNode->haveAttr("pattern") ? motorNode->getAttr("pattern") : "");
					newEffect.patternB = newEffect.patternA;
					newEffect.envelopeA.Set(motorNode->haveAttr("envelope") ? motorNode->getAttr("envelope") : "");
					newEffect.envelopeB = newEffect.envelopeA;
					motorNode->getAttr("frequency", newEffect.frequencyA);
					newEffect.frequencyB = newEffect.frequencyA;
				}
				else if (strcmp(motorTag, "MotorA") == 0)
				{
					newEffect.patternA.Set(motorNode->haveAttr("pattern") ? motorNode->getAttr("pattern") : "");
					newEffect.envelopeA.Set(motorNode->haveAttr("envelope") ? motorNode->getAttr("envelope") : "");
					motorNode->getAttr("frequency", newEffect.frequencyA);
				}
				else if (strcmp(motorTag, "MotorB") == 0)
				{
					newEffect.patternB.Set(motorNode->haveAttr("pattern") ? motorNode->getAttr("pattern") : "");
					newEffect.envelopeB.Set(motorNode->haveAttr("envelope") ? motorNode->getAttr("envelope") : "");
					motorNode->getAttr("frequency", newEffect.frequencyB);
				}
				else if (strcmp(motorTag, "MotorLTRT") == 0)
				{
					newEffect.patternLT.Set(motorNode->haveAttr("pattern") ? motorNode->getAttr("pattern") : "");
					newEffect.patternRT = newEffect.patternLT;
					newEffect.envelopeLT.Set(motorNode->haveAttr("envelope") ? motorNode->getAttr("envelope") : "");
					newEffect.envelopeRT = newEffect.envelopeLT;
					motorNode->getAttr("frequency", newEffect.frequencyLT);
					newEffect.frequencyRT = newEffect.frequencyLT;
				}
				else if (strcmp(motorTag, "MotorLT") == 0)
				{
					newEffect.patternLT.Set(motorNode->haveAttr("pattern") ? motorNode->getAttr("pattern") : "");
					newEffect.envelopeLT.Set(motorNode->haveAttr("envelope") ? motorNode->getAttr("envelope") : "");
					motorNode->getAttr("frequency", newEffect.frequencyLT);
				}
				else if (strcmp(motorTag, "MotorRT") == 0)
				{
					newEffect.patternRT.Set(motorNode->haveAttr("pattern") ? motorNode->getAttr("pattern") : "");
					newEffect.envelopeRT.Set(motorNode->haveAttr("envelope") ? motorNode->getAttr("envelope") : "");
					motorNode->getAttr("frequency", newEffect.frequencyRT);
				}
			}

			newEffect.frequencyA = (float)__fsel(-newEffect.frequencyA, 1.0f, newEffect.frequencyA);
			newEffect.frequencyB = (float)__fsel(-newEffect.frequencyB, 1.0f, newEffect.frequencyB);
			newEffect.frequencyLT = (float)__fsel(-newEffect.frequencyLT, 1.0f, newEffect.frequencyLT);
			newEffect.frequencyRT = (float)__fsel(-newEffect.frequencyRT, 1.0f, newEffect.frequencyRT);

			m_effects.push_back(newEffect);
			m_effectNames.push_back(effectName);

			FFBInternalId internalId;
			internalId.Set(effectName);
			m_effectToIndexMap.insert(TEffectToIndexMap::value_type(internalId, ((int)m_effects.size() - 1)));
		}
	}
}

int CForceFeedBackSystem::ParseSampleBuffer(const TSamplesBuffer& buffer, float* outputValues, const int maxOutputValues)
{
	int tokenStart = 0;
	int tokenEnd = 0;
	int samplesFound = 0;

	for (int i = 0; i < maxOutputValues; ++i)
	{
		tokenEnd = buffer.find(",", tokenStart);
		if (tokenEnd != buffer.npos)
		{
			const int charCount = tokenEnd - tokenStart;
			samplesFound++;
			outputValues[i] = (float)atof(buffer.substr(tokenStart, charCount).c_str());
			tokenStart = tokenEnd + 1;
		}
		else
		{
			//Last token
			const int charCount = buffer.length() - tokenStart;
			samplesFound++;
			outputValues[i] = (float)atof(buffer.substr(tokenStart, charCount).c_str());
			break;
		}
	}

	return samplesFound;
}

void CForceFeedBackSystem::DistributeSamples(const float* sampleInput, const int sampleInputCount, uint16* sampleOutput, const int sampleOutputCount)
{
	const int sampleDistributionStep = (sampleOutputCount / sampleInputCount);
	const int sampleIterations = ((sampleInputCount % 2) == 0) ? (sampleInputCount / 2) : (sampleInputCount / 2) + 1;

	int lastStartOffsetIdx = 0;
	int lastEndOffsetIdx = (sampleOutputCount - 1);

	for (int i = 0; i < sampleIterations; ++i)
	{
		const int startOffsetIdx = sampleDistributionStep * i;
		const int endOffsetIdx = sampleOutputCount - 1 - startOffsetIdx;

		CRY_ASSERT((startOffsetIdx >= 0) && (startOffsetIdx < sampleOutputCount));
		CRY_ASSERT((endOffsetIdx >= 0) && (endOffsetIdx < sampleOutputCount));

		sampleOutput[startOffsetIdx] = (uint16)(clamp_tpl(sampleInput[i], 0.0f, 1.0f) * 65535.0f);
		sampleOutput[endOffsetIdx] = (uint16)(clamp_tpl(sampleInput[sampleInputCount - 1 - i], 0.0f, 1.0f) * 65535.0f);

		// Fill values in between, left side
		if (lastStartOffsetIdx < startOffsetIdx)
		{
			const int startValue = sampleOutput[lastStartOffsetIdx];
			const int step = (sampleOutput[startOffsetIdx] - sampleOutput[lastStartOffsetIdx]) / (startOffsetIdx - lastStartOffsetIdx);
			for (int j = lastStartOffsetIdx; j < startOffsetIdx; ++j)
			{
				sampleOutput[j] = startValue + ((j - lastStartOffsetIdx) * step);
			}
		}

		// ...and right side
		if (endOffsetIdx < lastEndOffsetIdx)
		{
			const int startValue = sampleOutput[endOffsetIdx];
			const int step = (sampleOutput[lastEndOffsetIdx] - sampleOutput[endOffsetIdx]) / (lastEndOffsetIdx - endOffsetIdx);
			for (int j = endOffsetIdx; j < lastEndOffsetIdx; ++j)
			{
				sampleOutput[j] = startValue + ((j - endOffsetIdx) * step);
			}
		}

		//Last iteration, requires to fill the remaining ones in the middle
		if (i == (sampleIterations - 1))
		{
			if (startOffsetIdx < endOffsetIdx)
			{
				const int startValue = sampleOutput[startOffsetIdx];
				const int step = (sampleOutput[endOffsetIdx] - sampleOutput[startOffsetIdx]) / (endOffsetIdx - startOffsetIdx);
				for (int j = startOffsetIdx; j < endOffsetIdx; ++j)
				{
					sampleOutput[j] = startValue + ((j - startOffsetIdx) * step);
				}
			}
		}

		lastStartOffsetIdx = startOffsetIdx;
		lastEndOffsetIdx = endOffsetIdx;

	}
}

const CForceFeedBackSystem::SPattern& CForceFeedBackSystem::FindPattern(const FFBInternalId& id) const
{
	TPatternsVector::const_iterator citEnd = m_patters.end();

	for (TPatternsVector::const_iterator cit = m_patters.begin(); ((cit != citEnd) && (id >= cit->m_patternId)); ++cit)
	{
		if (cit->m_patternId != id)
			continue;

		return (*cit);
	}

	return m_defaultPattern;
}

const CForceFeedBackSystem::SEnvelope& CForceFeedBackSystem::FindEnvelope(const FFBInternalId& id) const
{
	TEnvelopesVector::const_iterator citEnd = m_envelopes.end();

	for (TEnvelopesVector::const_iterator cit = m_envelopes.begin(); ((cit != citEnd) && (id >= cit->m_envelopeId)); ++cit)
	{
		if (cit->m_envelopeId != id)
			continue;

		return (*cit);
	}

	return m_defaultEnvelope;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

namespace
{
void FFSReload(IConsoleCmdArgs* pArgs)
{
	CForceFeedBackSystem* pFFS = static_cast<CForceFeedBackSystem*>(CCryAction::GetCryAction()->GetIForceFeedbackSystem());

	if (pFFS)
	{
		pFFS->Reload();
	}
}

void FFSPlayEffect(IConsoleCmdArgs* pArgs)
{
	CForceFeedBackSystem* pFFS = static_cast<CForceFeedBackSystem*>(CCryAction::GetCryAction()->GetIForceFeedbackSystem());

	if (pFFS)
	{
		if (pArgs->GetArgCount() >= 2)
		{
			ForceFeedbackFxId fxId = pFFS->GetEffectIdByName(pArgs->GetArg(1));
			const float intensity = (pArgs->GetArgCount() >= 3) ? (float)atof(pArgs->GetArg(2)) : 1.0f;
			const float delay = (pArgs->GetArgCount() >= 4) ? (float)atof(pArgs->GetArg(3)) : 0.0f;
			pFFS->PlayForceFeedbackEffect(fxId, SForceFeedbackRuntimeParams(intensity, delay));
		}
	}
}

void FFSStopAllEffects(IConsoleCmdArgs* pArgs)
{
	CForceFeedBackSystem* pFFS = static_cast<CForceFeedBackSystem*>(CCryAction::GetCryAction()->GetIForceFeedbackSystem());

	if (pFFS)
	{
		pFFS->StopAllEffects();
	}
}

};

SForceFeedbackSystemCVars::SForceFeedbackSystemCVars()
{
	REGISTER_CVAR(ffs_debug, 0, 0, "Turns on/off force feedback system debug.");

	REGISTER_COMMAND("ffs_PlayEffect", FFSPlayEffect, 0, "Play force feedback effect, passed by name as first parameter");
	REGISTER_COMMAND("ffs_StopAllEffects", FFSStopAllEffects, 0, "Stop force feedback effect, passed by name as first parameter");
	REGISTER_COMMAND("ffs_Reload", FFSReload, 0, "Reload force feedback system data");
}

SForceFeedbackSystemCVars::~SForceFeedbackSystemCVars()
{
	IConsole* pConsole = gEnv->pConsole;

	if (pConsole)
	{
		pConsole->UnregisterVariable("ffs_debug", true);

		pConsole->RemoveCommand("ffs_PlayEffect");
		pConsole->RemoveCommand("ffs_StopAllEffects");
		pConsole->RemoveCommand("ffs_Reload");
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

CForceFeedBackSystem::SFFOutput CForceFeedBackSystem::SActiveEffect::Update(float frameTime)
{
	SFFOutput effectFF;

	bool canPlay = (runtimeParams.delay <= 0.0f);

	if (canPlay)
	{
		bool isLoopingEffect = (effectTime <= 0.0f);

		const float effectTimeInv = !isLoopingEffect ? (float)__fres(effectTime) : 1.0f;
		const float sampleTime = runningTime * effectTimeInv;

		const float sampleTimeAAtFreq = sampleTime * frequencyA;
		const float sampleTimeAAtFreqNorm = clamp_tpl(sampleTimeAAtFreq - floor_tpl(sampleTimeAAtFreq), 0.0f, 1.0f);

		const float sampleTimeBAtFreq = sampleTime * frequencyB;
		const float sampleTimeBAtFreqNorm = clamp_tpl(sampleTimeBAtFreq - floor_tpl(sampleTimeBAtFreq), 0.0f, 1.0f);

		const float sampleTimeLTAtFreq = sampleTime * frequencyLT;
		const float sampleTimeLTAtFreqNorm = clamp_tpl(sampleTimeLTAtFreq - floor_tpl(sampleTimeLTAtFreq), 0.0f, 1.0f);

		const float sampleTimeRTAtFreq = sampleTime * frequencyRT;
		const float sampleTimeRTAtFreqNorm = clamp_tpl(sampleTimeRTAtFreq - floor_tpl(sampleTimeRTAtFreq), 0.0f, 1.0f);

		effectFF.forceFeedbackA = m_patternA.SamplePattern(sampleTimeAAtFreqNorm) * m_envelopeA.SampleEnvelope(sampleTime) * runtimeParams.intensity;
		effectFF.forceFeedbackB = m_patternB.SamplePattern(sampleTimeBAtFreqNorm) * m_envelopeB.SampleEnvelope(sampleTime) * runtimeParams.intensity;
		effectFF.forceFeedbackLT = m_patternLT.SamplePattern(sampleTimeLTAtFreqNorm) * m_envelopeLT.SampleEnvelope(sampleTime) * runtimeParams.intensity;
		effectFF.forceFeedbackRT = m_patternRT.SamplePattern(sampleTimeRTAtFreqNorm) * m_envelopeRT.SampleEnvelope(sampleTime) * runtimeParams.intensity;
		runningTime += frameTime;

		//Re-start the loop
		if (isLoopingEffect)
		{
			runningTime = (float)__fsel(1.0f - runningTime, runningTime, 0.0f);
		}
	}
	else
	{
		runtimeParams.delay = clamp_tpl(runtimeParams.delay - frameTime, 0.0f, runtimeParams.delay);
	}

	return effectFF;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// calls callback once for each effect

void CForceFeedBackSystem::EnumerateEffects(IFFSPopulateCallBack* pCallBack)
{
	TEffectNamesArray::const_iterator iter = m_effectNames.begin();

	while (iter != m_effectNames.end())
	{
		const char* const pName = iter->c_str();
		pCallBack->AddFFSEffectName(pName);
		++iter;
	}
}

int CForceFeedBackSystem::GetEffectNamesCount() const
{
	return m_effectNames.size();
}

void CForceFeedBackSystem::SuppressEffects(bool bSuppressEffects)
{
	if (bSuppressEffects)
	{
		if (m_effectLock == 0)
			StopAllEffects();

		m_effectLock++;
	}
	else
		m_effectLock--;

	CryLog("[FFB] ForceFeedback EffectLock now: %d was %d", m_effectLock, bSuppressEffects ? m_effectLock - 1 : m_effectLock + 1);
}
