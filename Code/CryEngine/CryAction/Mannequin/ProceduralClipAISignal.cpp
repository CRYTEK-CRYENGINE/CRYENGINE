// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "ICryMannequin.h"
#include <CryAISystem/ISignal.h>
#include <Mannequin/Serialization.h>

struct SProceduralClipAISignalParams
	: public IProceduralParams
{
	TProcClipString dataString;

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		ar(dataString, "EnterAndExitSignalNames", "EnterAndExitSignalNames");
	}

	virtual void GetExtraDebugInfo(StringWrapper& extraInfoOut) const override
	{
		extraInfoOut = dataString.c_str();
	}
};

// This procedural clip will send a signal directly to the AI actor interface
// of the entity on which the clip is playing.
class CProceduralClipAISignal : public TProceduralClip<SProceduralClipAISignalParams>
{
public:

	CProceduralClipAISignal()
	{
	}

	virtual void OnEnter(float blendTime, float duration, const SProceduralClipAISignalParams& params)
	{
		// TODO: Temporary work-around: we need to be able to store 2 signal
		// names because the params.dataCRC string is not available in
		// release/console builds, and also used a CRC generator that is
		// incompatible with the one used by the AI signal systems.
		// This work-around uses a separator '|' character, so that we don't
		// need to add extra string support throughout the Mannequin editors and
		// such.
		CryFixedStringT<64> onEnterSignalName;
		ExtractSignalNames(params.dataString.c_str(), &onEnterSignalName, &m_onExitSignalName);

		const EntityId entityId = GetAIActorEntityId();
		IF_UNLIKELY(entityId == INVALID_ENTITYID)
		{
			return;
		}

		if (!onEnterSignalName.empty())
		{
			gEnv->pAISystem->SendSignal(AISignals::ESignalFilter::SIGNALFILTER_SENDER, gEnv->pAISystem->GetSignalManager()->CreateSignal_DEPRECATED(AISIGNAL_DEFAULT, onEnterSignalName, entityId));
		}
	}

	virtual void OnExit(float blendTime)
	{
		if (m_onExitSignalName.empty())
		{
			return;
		}

		const EntityId entityId = GetAIActorEntityId();
		IF_UNLIKELY (entityId == INVALID_ENTITYID)
		{
			return;
		}
		gEnv->pAISystem->SendSignal(AISignals::ESignalFilter::SIGNALFILTER_SENDER, gEnv->pAISystem->GetSignalManager()->CreateSignal_DEPRECATED(AISIGNAL_DEFAULT, m_onExitSignalName, entityId));
	}

	virtual void Update(float timePassed) {}

private:

	EntityId GetAIActorEntityId() const
	{
		IF_UNLIKELY (m_entity == NULL)
		{
			return INVALID_ENTITYID;
		}
		IF_UNLIKELY (!m_entity->HasAI())
		{
			return INVALID_ENTITYID;
		}
		return m_entity->GetId();
	}

	// TODO: Create proper separate signal names in the procedural clip.
	inline void ExtractSignalNames(const char* dataString, CryFixedStringT<64>* onEnterSignalString, CryFixedStringT<64>* onExitSignalString)
	{
		assert(onEnterSignalString != NULL);
		assert(onExitSignalString != NULL);

		// It is allowed to omit any of the signal names in the data string.
		onEnterSignalString->clear();
		onExitSignalString->clear();

		IF_UNLIKELY (dataString == NULL)
		{
			return;
		}

#if !defined(_RELEASE)
		const char* dataStringStart = dataString;
#endif
		char chr;
		while ((chr = *dataString) != '\0')
		{
			if (chr == '|')
			{
				// Switch to parsing the exit signal name.
				break;
			}
			*onEnterSignalString += chr;
			dataString++;
		}
#if !defined(_RELEASE)
		assert((dataString - dataStringStart) < onExitSignalString->MAX_SIZE);
#endif
		if (chr == '\0')
		{
			// No exit signal name specified apparently.
			return;
		}

		dataString++; // Skip the separator character.

#if !defined(_RELEASE)
		assert(strlen(dataString) <= onExitSignalString->MAX_SIZE);
#endif

		*onExitSignalString = dataString;
		return;
	}

private:

	CryFixedStringT<64> m_onExitSignalName;
};

REGISTER_PROCEDURAL_CLIP(CProceduralClipAISignal, "AISignal");
