// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/StlUtils.h>

class CDialogScript
{
public:
	typedef uint8 TActorID;
	static const TActorID MAX_ACTORS = sizeof(TActorID) * 8; // number of bits in TActorID (8 for uint8)
	static const TActorID NO_ACTOR_ID = ~TActorID(0);
	static const TActorID STICKY_LOOKAT_RESET_ID = NO_ACTOR_ID - 1;

	enum VersionFlags
	{
		VF_EXCEL_BASED = 0x0001,
	};

	// helper struct (basically a bitset replicate)
	struct SActorSet
	{
		SActorSet() : m_actorBits(0) {}
		SActorSet(TActorID requiredActors) : m_actorBits(0) {}
		void SetActor(TActorID id);
		void ResetActor(TActorID id);
		bool HasActor(TActorID id);
		int  NumActors() const;
		bool Matches(const SActorSet& other) const;   // exact match
		bool Satisfies(const SActorSet& other) const; // fulfills or super-fulfills other
		TActorID m_actorBits;
	};

	struct SScriptLine
	{
		SScriptLine()
			: m_delay(0.0f)
			, m_facialWeight(0.0f)
			, m_facialFadeTime(0.0f)
		{
		}

		SScriptLine(TActorID actor, TActorID lookat, CryAudio::ControlId audioID, const char* anim, const char* facial, float delay, float facialWeight, float facialFadeTime, bool bLookAtSticky, bool bResetFacial, bool bResetLookAt, bool bSoundStopsAnim, bool bUseAGSignal, bool bUseAGEP)
			: m_actor(actor),
			m_lookatActor(lookat),
			m_audioID(audioID),
			m_anim(anim),
			m_facial(facial),
			m_delay(delay),
			m_facialWeight(facialWeight),
			m_facialFadeTime(facialFadeTime),
			m_flagLookAtSticky(bLookAtSticky),
			m_flagResetFacial(bResetFacial),
			m_flagResetLookAt(bResetLookAt),
			m_flagSoundStopsAnim(bSoundStopsAnim),
			m_flagAGSignal(bUseAGSignal),
			m_flagAGEP(bUseAGEP),
			m_flagUnused(0)
		{
		}

		TActorID            m_actor;            // [0..MAX_ACTORS)
		TActorID            m_lookatActor;      // [0..MAX_ACTORS)
		uint16              m_flagLookAtSticky   : 1;
		uint16              m_flagResetFacial    : 1;
		uint16              m_flagResetLookAt    : 1;
		uint16              m_flagSoundStopsAnim : 1;
		uint16              m_flagAGSignal       : 1;      // it's an AG Signal / AG Action
		uint16              m_flagAGEP           : 1;      // use exact positioning
		uint16              m_flagUnused         : 10;

		CryAudio::ControlId m_audioID;
		string              m_anim;                // Animation to Play
		string              m_facial;              // Facial Animation to Play
		float               m_delay;               // Delay
		float               m_facialWeight;        // Weight of facial expression
		float               m_facialFadeTime;      // Time of facial fade-in

		void                GetMemoryUsage(ICrySizer* pSizer) const
		{
			//pSizer->AddObject(m_audioID);
			pSizer->AddObject(m_anim);
			pSizer->AddObject(m_facial);
		}
	};
	typedef std::vector<SScriptLine> TScriptLineVec;

public:
	CDialogScript(const string& dialogScriptID);
	virtual ~CDialogScript();

	// Get unique ID of this DialogScript
	const string& GetID() const
	{
		return m_id;
	}

	// Get description
	const string& GetDescription() const
	{
		return m_desc;
	}

	// Set a description
	void SetDescription(const string& desc)
	{
		m_desc = desc;
	}

	// Add one line after another
	// Ugly interface exists purely for speed reasons
	bool AddLine(TActorID actorID, CryAudio::ControlId audioID, const char* anim, const char* facial, TActorID lookAtTargetID, float delay, float facialWeight, float facialFadeTime, bool bLookAtSticky, bool bResetFacial, bool bResetLookAt, bool bSoundStopsAnim, bool bUseAGSignal, bool bUseAGEP);

	// Add one line after another
	bool AddLine(const SScriptLine& line);

	// Call this after all lines have been added
	bool Complete();

	// Is the dialogscript completed
	bool IsCompleted() const;

	// Retrieves an empty actor set
	SActorSet GetRequiredActorSet() const
	{
		return m_reqActorSet;
	}

	// Get number of required actors (these may not be in sequence!)
	int GetNumRequiredActors() const;

	// Get number of dialog lines
	int GetNumLines() const;

	// Get a certain line
	// const SScriptLine& GetLine(int index) const;

	// Get a certain line
	const SScriptLine* GetLine(int index) const;

	// Get special version flags
	uint32 GetVersionFlags() const { return m_versionFlags; }

	// Set special version flag
	void SetVersionFlags(uint32 which, bool bSet = true);

	void GetMemoryUsage(ICrySizer* pSizer) const;

protected:
	string         m_id;
	string         m_desc;
	TScriptLineVec m_lines;
	SActorSet      m_reqActorSet;
	int            m_versionFlags;
	bool           m_bComplete;
};

typedef std::map<string, CDialogScript*, stl::less_stricmp<string>> TDialogScriptMap;
