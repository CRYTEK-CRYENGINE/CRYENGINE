// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MANNEQUIN_GOAL_OP__H__
#define __MANNEQUIN_GOAL_OP__H__

#include "EnterLeaveUpdateGoalOp.h"

//////////////////////////////////////////////////////////////////////////
class CMannequinTagGoalOp
	: public EnterLeaveUpdateGoalOp
{
protected:
	CMannequinTagGoalOp();
	CMannequinTagGoalOp(const char* tagName);
	CMannequinTagGoalOp(const uint32 tagCrc);
	CMannequinTagGoalOp(const XmlNodeRef& node);

	uint32 GetTagCrc() const { return m_tagCrc; }

private:
	uint32 m_tagCrc;
};

//////////////////////////////////////////////////////////////////////////
class CSetAnimationTagGoalOp
	: public CMannequinTagGoalOp
{
public:
	CSetAnimationTagGoalOp();
	CSetAnimationTagGoalOp(const char* tagName);
	CSetAnimationTagGoalOp(const uint32 tagCrc);
	CSetAnimationTagGoalOp(const XmlNodeRef& node);

	virtual void Update(CPipeUser& pipeUser);
};

//////////////////////////////////////////////////////////////////////////
class CClearAnimationTagGoalOp
	: public CMannequinTagGoalOp
{
public:
	CClearAnimationTagGoalOp();
	CClearAnimationTagGoalOp(const char* tagName);
	CClearAnimationTagGoalOp(const uint32 tagCrc);
	CClearAnimationTagGoalOp(const XmlNodeRef& node);

	virtual void Update(CPipeUser& pipeUser);
};

#endif
