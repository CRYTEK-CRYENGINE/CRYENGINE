// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include <CryFlowGraph/IFlowSystem.h> // <> required for Interfuscator

enum EGameTokenFlags
{
	EGAME_TOKEN_MODIFIED      = BIT(0),    //!< If the token has been modified at least once for this game run.
	EGAME_TOKEN_GRAPHVARIABLE = BIT(1),    //!< Token is constrained to the flowgraph it appears in.
	EGAME_TOKEN_LOCALONLY     = BIT(2),    //!< This token will not be synchronized to clients.
};

#ifndef _RELEASE
#define _GAMETOKENSDEBUGINFO  // just to be able to fast activate it in _release easily if needed.
#endif

//! Game Token can be used as any Plot event in the game.
struct IGameToken
{
	// <interfuscator:shuffle>
	virtual ~IGameToken(){}

	//! Retrieve name of the game token.
	virtual const char* GetName() const = 0;

	//! Sets game token flags.
	virtual void SetFlags(uint32 flags) = 0;

	//! Retrieve game token flags.
	virtual uint32 GetFlags() const = 0;

	//! Retrieve current data type of the game token.
	virtual EFlowDataTypes GetType() const = 0;

	//! Assign a new value to the game token.
	virtual void SetValue(const TFlowInputData& val) = 0;

	//! Retrieve a value of the game token.
	//! Returns false if a value cannot be retrieved.
	virtual bool GetValue(TFlowInputData& val) const = 0;

	//! Set token value from a string.
	virtual void SetValueFromString(const char* sValue) = 0;

	//! Get the token value as a string
	virtual const char* GetValueAsString() const = 0;

	//! Propagate token changed event. use bIsGameStart to true when it is the first time the token is set for the game
	virtual void TriggerAsChanged(bool bIsGameStart) = 0;
	// </interfuscator:shuffle>

	//! A small template helper (yes, in the i/f) to get the value.
	//! Returns true if successful, false otherwise.
	template<typename T>
	bool GetValueAs(T& value)
	{
		TFlowInputData data;
		if (GetValue(data))
		{
			return data.GetValueWithConversion(value);
		}
		return false;
	}
};

//! Events that game token event listener can receive.
enum EGameTokenEvent
{
	EGAMETOKEN_EVENT_CHANGE,
	EGAMETOKEN_EVENT_DELETE
};

//! Game Token iterator, used to access all game tokens currently loaded.
struct IGameTokenIt
{
	// <interfuscator:shuffle>
	virtual ~IGameTokenIt(){}

	virtual void AddRef() = 0;

	//! Deletes this iterator and frees any memory it might have allocated.
	virtual void Release() = 0;

	//! Check whether current iterator position is the end position.
	//! \return True if iterator at end position.
	virtual bool IsEnd() = 0;

	//! Retrieves next entity.
	//! \return The entity that the iterator points to before it goes to the next.
	virtual IGameToken* Next() = 0;

	//! Retrieves current game token.
	//! \return The game token that the iterator points to.
	virtual IGameToken* This() = 0;

	//! Positions the iterator at the begining of the game token list.
	virtual void MoveFirst() = 0;
	// </interfuscator:shuffle>
};

typedef _smart_ptr<IGameTokenIt> IGameTokenItPtr;

//! Derive your class from this interface to listen for game the token events.
//! Listener must also be registered in game token manager.
struct IGameTokenEventListener
{
	// <interfuscator:shuffle>
	virtual ~IGameTokenEventListener(){}
	virtual void OnGameTokenEvent(EGameTokenEvent event, IGameToken* pGameToken) = 0;
	virtual void GetMemoryUsage(class ICrySizer* pSizer) const = 0;
	// </interfuscator:shuffle>
};

//! Manages collection of game tokens.
//! Responsible for saving/loading and accessing game tokens.
struct IGameTokenSystem
{
	// <interfuscator:shuffle>
	virtual ~IGameTokenSystem(){}
	virtual void GetMemoryStatistics(ICrySizer*) = 0;

	//! Create a new token.
	virtual IGameToken* SetOrCreateToken(const char* sTokenName, const TFlowInputData& defaultValue) = 0;

	//! Deletes existing game token.
	virtual void DeleteToken(IGameToken* pToken) = 0;

	//! Find a game token by name.
	virtual IGameToken*   FindToken(const char* sTokenName) = 0;

	virtual IGameTokenIt* GetGameTokenIterator() = 0;

	//! Rename existing game token.
	virtual void RenameToken(IGameToken* pToken, const char* sNewTokenName) = 0;

	virtual void RegisterListener(const char* sGameToken, IGameTokenEventListener* pListener, bool bForceCreate = false, bool bImmediateCallback = false) = 0;
	virtual void UnregisterListener(const char* sGameToken, IGameTokenEventListener* pListener) = 0;

	virtual void RegisterListener(IGameTokenEventListener* pListener) = 0;
	virtual void UnregisterListener(IGameTokenEventListener* pListener) = 0;

	//! Load all libraries found with given file spec.
	virtual void LoadLibs(const char* sFileSpec) = 0;

	//! Remove all tokens beginning with prefix (e.g. Library).
	virtual void RemoveLibrary(const char* sPrefix) = 0;

	//! Level to level serialization.
	virtual void SerializeSaveLevelToLevel(const char** ppGameTokensList, uint32 numTokensToSave) = 0;
	virtual void SerializeReadLevelToLevel() = 0;

	//! Reset system (deletes all libraries).
	virtual void Reset() = 0;
	virtual void Unload() = 0;

	//! Propagate token changed event for all tokens. This is signaled and should only be used at game start (editor and pure game)
	virtual void TriggerTokensAsChanged() = 0;

	//! Set the internal state of the GTS, eg. so that it does not send events to other systems when initializing the game
	virtual void SetGoingIntoGame(bool bGoingIntoGame) = 0;

	virtual void Serialize(TSerialize ser) = 0;

	virtual void DebugDraw() = 0;
	// </interfuscator:shuffle>

	//! A small template helper (yes, in the i/f) to get the value of a token.
	//! \return true if successful (value found and conversion OK), false otherwise (not found, or conversion failed).
	template<typename T>
	bool GetTokenValueAs(const char* sGameToken, T& value)
	{
		IGameToken* pToken = FindToken(sGameToken);
		if (pToken == 0) return false;
		return pToken->GetValueAs(value);
	}

#ifdef _GAMETOKENSDEBUGINFO
	virtual void AddTokenToDebugList(const char* pToken) = 0;
	virtual void RemoveTokenFromDebugList(const char* pToken) = 0;
#endif
};

//! \endcond