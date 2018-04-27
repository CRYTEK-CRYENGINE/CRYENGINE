// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   CryGame Source File.
   -------------------------------------------------------------------------
   File name:   AIFaceManager.h
   Version:     v1.00
   Description: take care of playing proper face expresion sequence

   -------------------------------------------------------------------------
   History:
   - 05:05:2007   12:04 : Created by Kirill Bulatsev

 *********************************************************************/

#ifndef __AIFaceManager_H__
#define __AIFaceManager_H__

#pragma once

//
//---------------------------------------------------------------------------------------------------

class CAIFaceManager
{
public:
	enum e_ExpressionEvent
	{
		EE_NONE,
		EE_IDLE,
		EE_ALERT,
		EE_COMBAT,
		EE_Count
	};

	CAIFaceManager(IEntity* pEntity);
	~CAIFaceManager(void);

	static bool              LoadStatic();
	static bool              Load(const char* szFileName);
	static e_ExpressionEvent StringToEnum(const char* str);

	void                     SetExpression(e_ExpressionEvent expression, bool forceChange = false);
	void                     Update();
	void                     Reset();
	void                     PrecacheSequences();

	void                     OnReused(IEntity* pEntity);

protected:

	typedef std::vector<string> TExprState;
	static TExprState s_Expressions[EE_Count + 1];

	void MakeFace(const char* pFaceName);
	int  SelectExpressionTime() const;

	e_ExpressionEvent m_CurrentState;
	int               m_CurrentExprIdx;
	CTimeValue        m_ExprStartTime;
	int               m_ChangeExpressionTimeMs;

	IEntity*          m_pEntity;
};

#endif // __AIFaceManager_H__
