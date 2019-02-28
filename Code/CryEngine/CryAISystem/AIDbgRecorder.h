// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   AIDbgRecorder.h
   Description: Simple text AI debugging event recorder

   -------------------------------------------------------------------------
   History:
   - 1:07:2005 : Created by Kirill Bulatsev
   -19:11:2008 : Separated out by Matthew

   Notes:        Really, this class is two separate debuggers - consider splitting
              Move the access point to gAIEnv
              Only creates the files on files on first logging - add some kind of init

 *********************************************************************/

#ifndef __AIDBGRECORDER_H__
#define __AIDBGRECORDER_H__

#pragma once

#ifdef CRYAISYSTEM_DEBUG

// Simple text debug recorder
// Completely independent from CAIRecorder, which is more sophisticated
class CAIDbgRecorder
{
public:

	CAIDbgRecorder() {};
	~CAIDbgRecorder() {};

	bool IsRecording(const IAIObject* pTarget, IAIRecordable::e_AIDbgEvent event) const;
	void Record(const IAIObject* pTarget, IAIRecordable::e_AIDbgEvent event, const char* pString) const;

protected:
	void InitFile() const;
	void InitFileSecondary() const;

	void LogString(const char* pString) const;
	void LogStringSecondary(const char* pString) const;

	// Empty indicates currently unused
	// Has to be mutable right now because it changes on first logging
	mutable string m_sFile, m_sFileSecondary;
};

#endif //CRYAISYSTEM_DEBUG

#endif //__AIDBGRECORDER_H__
