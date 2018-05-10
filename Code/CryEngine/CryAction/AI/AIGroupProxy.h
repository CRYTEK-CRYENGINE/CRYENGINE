// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __AIGroupProxy_h__
#define __AIGroupProxy_h__

#pragma once

#include <CryAISystem/IAIObject.h>
#include <CryAISystem/IAIGroupProxy.h>

class CAIGroupProxy :
	public IAIGroupProxy
{
	friend class CAIProxyManager;
public:
	virtual void          Reset(EObjectResetType type);
	virtual void          Serialize(TSerialize ser);

	virtual const char*   GetCurrentBehaviorName() const;
	virtual const char*   GetPreviousBehaviorName() const;

	virtual void          Notify(uint32 notificationID, tAIObjectID senderID, const char* notification);
	virtual void          SetBehaviour(const char* behaviour, bool callCDtor = true);

	virtual void          MemberAdded(tAIObjectID memberID);
	virtual void          MemberRemoved(tAIObjectID memberID);

	virtual IScriptTable* GetScriptTable();

protected:
	CAIGroupProxy(int groupID);
	virtual ~CAIGroupProxy();

	bool CallScript(IScriptTable* table, const char* funcName);
	bool CallNotification(IScriptTable* table, const char* notification, uint32 notificationID, IScriptTable* sender);
	void PopulateMembersTable();

	typedef std::vector<tAIObjectID> Members;
	Members          m_members;

	SmartScriptTable m_script;
	SmartScriptTable m_prevBehavior;
	SmartScriptTable m_behavior;
	SmartScriptTable m_behaviorsTable;
	SmartScriptTable m_membersTable;

	string           m_behaviorName;
};

#endif
