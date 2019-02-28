// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __DIALOGMANAGER_H__
#define __DIALOGMANAGER_H__

#pragma once

#include <CryCore/StlUtils.h>

class CDialogManager;
class CEditorDialogScriptSerializer;

class CEditorDialogScript
{
public:
	typedef uint8 TActorID;
	static const TActorID MAX_ACTORS = sizeof(TActorID) * 8; // number of bits in TActorID (8 for uint8)
	static const TActorID NO_ACTOR_ID = ~TActorID(0);
	static const TActorID STICKY_LOOKAT_RESET_ID = NO_ACTOR_ID - 1;

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
		SScriptLine() { Reset(); };

		void Reset()
		{
			m_actor = 0;
			m_lookatActor = NO_ACTOR_ID;
			m_flagLookAtSticky = false;
			// m_flagResetFacial = false;
			// m_flagResetLookAt = false;
			m_flagSoundStopsAnim = false;
			m_flagAGSignal = false;
			m_flagAGEP = false;
			m_audioTriggerName = "";
			m_anim = "";
			m_facial = "";
			m_delay = 0.0f;
			m_facialWeight = 0.5f;
			m_facialFadeTime = 0.5f;
			m_desc = "";
		}

		TActorID m_actor;           // [0..MAX_ACTORS)
		TActorID m_lookatActor;     // [0..MAX_ACTORS)
		bool     m_flagLookAtSticky;
		// bool   m_flagResetFacial;
		// bool   m_flagResetLookAt;
		bool   m_flagSoundStopsAnim;
		bool   m_flagAGSignal;      // it's an AG Signal / AG Action
		bool   m_flagAGEP;          // use exact positioning

		string m_audioTriggerName;
		string m_anim;              // Animation to Play
		string m_facial;            // Facial Animation to Play
		float  m_delay;             // Delay
		float  m_facialWeight;      // Weight of facial expression
		float  m_facialFadeTime;    // Time of facial fade-in
		string m_desc;              // Description
	};
	typedef std::vector<SScriptLine> TScriptLineVec;

public:
	void          RemoveAll() { m_lines.clear(); }

	const string& GetID() const
	{
		return m_id;
	}

	const CString& GetDecription() const
	{
		return m_description;
	}

	void SetDescription(const CString& desc)
	{
		m_description = desc;
	}

	// Add one line after another
	bool AddLine(const SScriptLine& line);

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

protected:
	friend class CDialogManager;
	friend class CEditorDialogScriptSerializer;

	CEditorDialogScript(const string& dialogScriptID);
	virtual ~CEditorDialogScript();

	void SetID(const string& newName) { m_id = newName; }

protected:
	string         m_id;
	CString        m_description;
	TScriptLineVec m_lines;
	SActorSet      m_reqActorSet;
	uint32         m_scFileAttributes; // source control
};

typedef std::map<string, CEditorDialogScript*, stl::less_stricmp<string>> TEditorDialogScriptMap;

class CEditorDialogScriptSerializer
{
public:
	CEditorDialogScriptSerializer(CEditorDialogScript* pScript);
	bool Save(XmlNodeRef node);
	bool Load(XmlNodeRef node);
protected:
	bool ReadLine(const XmlNodeRef& lineNode, CEditorDialogScript::SScriptLine& line);
	bool WriteLine(XmlNodeRef lineNode, const CEditorDialogScript::SScriptLine& line);

protected:
	CEditorDialogScript* m_pScript;
};

class CDialogManager
{
public:
	CDialogManager();
	~CDialogManager();

public:
	// Reloads all scripts
	bool ReloadScripts();

	// Load script by name. if bReplace is true, old script can be deleted (so don't pass a reference to it's ID. that's why 'name' is not const)
	CEditorDialogScript* LoadScript(string& name, bool bReplace = true);

	// Save script
	bool SaveScript(CEditorDialogScript* pScript, bool bSync = true, bool bBackup = false);

	// Save all scripts
	bool SaveAllScripts();

	// Get a script or create if, not existent
	CEditorDialogScript* GetScript(const string& name, bool bForceCreate);

	// Get a script or create if, not existent
	CEditorDialogScript* GetScript(const CString& name, bool bForceCreate);

	// Delete a script.
	// if bDeleteFromDisk is true, script file will also be deleted from disk and CryAction is synced.
	// if not deleted, manual delete + CryAction sync has to be done
	bool DeleteScript(CEditorDialogScript* pScript, bool bDeleteFromDisk);

	// Rename a script
	// if bDeleteFromDisk is true, old script file will also be deleted from disk and CryAction is synced.
	// if not deleted, manual delete + CryAction sync has to be done
	bool RenameScript(CEditorDialogScript* pScript, string& newName, bool bDeleteFromDisk);

	// Get access to all scripts
	const TEditorDialogScriptMap& GetAllScripts() const { return m_scripts; }

	// Small kludge to make CryAction aware of any changes [new script, delete script, rename script]
	void    SyncCryActionScripts();

	uint32  GetFileAttrs(CEditorDialogScript* pScript, bool bUpdateFromFile = true);
	CString FilenameToScript(const CString& filename); // game/full-path to script-id
	CString ScriptToFilename(const CString& scriptID); // game-path relative
	bool    CanModify(CEditorDialogScript* pScript);

protected:
	void DeleteAllScripts();

protected:
	TEditorDialogScriptMap m_scripts;
};

#endif

