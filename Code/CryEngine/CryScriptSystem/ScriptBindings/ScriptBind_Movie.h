// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   ScriptBind_Movie.h
//  Version:     v1.00
//  Created:     8/7/2004 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __ScriptBind_Movie_h__
#define __ScriptBind_Movie_h__
#pragma once

#include <CryScriptSystem/IScriptSystem.h>

//! <description>Interface to movie system.</description>
class CScriptBind_Movie : public CScriptableBase
{
public:
	CScriptBind_Movie(IScriptSystem* pScriptSystem, ISystem* pSystem);
	virtual ~CScriptBind_Movie();
	virtual void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(this, sizeof(*this));
	}

	//! <code>Movie.PlaySequence(sSequenceName)</code>
	//!		<param name="sSequenceName">Sequence name.</param>
	//! <description>Plays the specified sequence.</description>
	int PlaySequence(IFunctionHandler* pH, const char* sSequenceName);

	//! <code>Movie.StopSequence(sSequenceName)</code>
	//!		<param name="sSequenceName">Sequence name.</param>
	//! <description>Stops the specified sequence.</description>
	int StopSequence(IFunctionHandler* pH, const char* sSequenceName);

	//! <code>Movie.AbortSequence(sSequenceName)</code>
	//!		<param name="sSequenceName">Sequence name.</param>
	//! <description>Aborts the specified sequence.</description>
	int AbortSequence(IFunctionHandler* pH, const char* sSequenceName);

	//! <code>Movie.StopAllSequences()</code>
	//! <description>Stops all the video sequences.</description>
	int StopAllSequences(IFunctionHandler* pH);

	//! <code>Movie.StopAllCutScenes()</code>
	//! <description>Stops all the cut scenes.</description>
	int StopAllCutScenes(IFunctionHandler* pH);

	//! <code>Movie.PauseSequences()</code>
	//! <description>Pauses all the sequences.</description>
	int PauseSequences(IFunctionHandler* pH);

	//! <code>Movie.ResumeSequences()</code>
	//! <description>Resume all the sequences.</description>
	int ResumeSequences(IFunctionHandler* pH);
private:
	ISystem*      m_pSystem;
	IMovieSystem* m_pMovieSystem;
};

#endif // __ScriptBind_Movie_h__
