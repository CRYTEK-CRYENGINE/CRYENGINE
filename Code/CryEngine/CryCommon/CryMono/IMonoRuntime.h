// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/smartptr.h>

enum EMonoLogLevel
{
	eMLL_NULL = 0,
	eMLL_Error,
	eMLL_Critical,
	eMLL_Warning,
	eMLL_Message,
	eMLL_Info,
	eMLL_Debug
};

struct IMonoLibrary
{
	virtual ~IMonoLibrary() {}

	virtual const char* GetName() = 0;
	virtual const char* GetPath() = 0;
	virtual bool        IsLoaded() = 0;
	virtual bool        IsInMemory() = 0;
	virtual bool        RunMethod(const char* name) = 0;
};

struct IMonoLibraryIt
{
	virtual ~IMonoLibraryIt() {}

	virtual void          AddRef() = 0;
	virtual void          Release() = 0;
	virtual bool          IsEnd() = 0;
	virtual IMonoLibrary* This() = 0;
	virtual IMonoLibrary* Next() = 0;
	virtual void          MoveFirst() = 0;
};

typedef _smart_ptr<IMonoLibraryIt> IMonoLibraryItPtr;

struct IMonoListener
{
	virtual ~IMonoListener() {}

	//! Forwarded from CrySystem
	//! \param flags One or more flags from ESystemUpdateFlags structure.
	//! \param nPauseMode 0=normal(no pause), 1=menu/pause, 2=cutscene.
	virtual void OnUpdate(int updateFlags, int nPauseMode) = 0;
};

struct IMonoRuntime
{
	virtual ~IMonoRuntime() {}

	virtual void            Initialize(EMonoLogLevel logLevel) = 0;
	virtual void            LoadGame() = 0;
	virtual void            UnloadGame() = 0;
	virtual void            ReloadGame() { UnloadGame(); LoadGame(); }
	virtual void            RegisterListener(IMonoListener* pListener) = 0;
	virtual void            UnregisterListener(IMonoListener* pListener) = 0;
	virtual void            Update(int updateFlags = 0, int nPauseMode = 0) = 0;
	virtual const char*     GetProjectDllDir() = 0;
	virtual EMonoLogLevel   GetLogLevel() = 0;
	virtual IMonoLibraryIt* GetLibraryIterator() = 0;
};

struct IMonoEntityPropertyHandler : IEntityPropertyHandler
{
	virtual ~IMonoEntityPropertyHandler() {}

	virtual IEntityPropertyHandler::SPropertyInfo* GetMonoPropertyInfo(int index) const = 0;
	virtual bool GetPropertyInfo(int index, IEntityPropertyHandler::SPropertyInfo& info) const
	{
		const IEntityPropertyHandler::SPropertyInfo* pProperty = GetMonoPropertyInfo(index);
		if (!pProperty)
			return false;
		info = *pProperty;
		return true;
	}
};