// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GroupManager.h"
#include "Group.h"

static Group s_emptyGroup;

CGroupManager::CGroupManager()
	: m_notifications(0)
{
}

CGroupManager::~CGroupManager()
{
	Group::ClearStaticData();
}

void CGroupManager::Reset(EObjectResetType type)
{
	Group::ClearStaticData();

	Groups::iterator it = m_groups.begin();
	Groups::iterator end = m_groups.end();

	for (; it != end; ++it)
	{
		Group& group = it->second;

		group.Reset(type);
	}
}

void CGroupManager::Update(float updateTime)
{
	Groups::iterator it = m_groups.begin();
	Groups::iterator end = m_groups.end();

	for (; it != end; ++it)
	{
		Group& group = it->second;

		group.Update(updateTime);
	}
}

void CGroupManager::AddGroupMember(const GroupID& groupID, tAIObjectID objectID)
{
	std::pair<Groups::iterator, bool> iresult = m_groups.insert(Groups::value_type(groupID, Group()));
	Group& group = iresult.first->second;

	if (iresult.second)
	{
		Group(groupID).Swap(group);
	}

	group.AddMember(objectID);
}

void CGroupManager::RemoveGroupMember(const GroupID& groupID, tAIObjectID objectID)
{
	Groups::iterator git = m_groups.find(groupID);

	if (git != m_groups.end())
	{
		Group& group = git->second;

		group.RemoveMember(objectID);
	}
}

Group& CGroupManager::GetGroup(const GroupID& groupID)
{
	Groups::iterator git = m_groups.find(groupID);
	if (git != m_groups.end())
	{
		Group& group = git->second;

		return group;
	}

	return s_emptyGroup;
}

const Group& CGroupManager::GetGroup(const GroupID& groupID) const
{
	Groups::const_iterator git = m_groups.find(groupID);
	if (git != m_groups.end())
	{
		const Group& group = git->second;

		return group;
	}

	return s_emptyGroup;
}

uint32 CGroupManager::GetGroupMemberCount(const GroupID& groupID) const
{
	Groups::const_iterator git = m_groups.find(groupID);

	if (git != m_groups.end())
	{
		const Group& group = git->second;

		return group.GetMemberCount();
	}

	return 0;
}

// Kept for backwards compatibility
Group::NotificationID CGroupManager::NotifyGroup(const GroupID& groupID, EntityId senderID, const char* name)
{
	const AISignals::SignalSharedPtr pSignal = GetAISystem()->GetSignalManager()->CreateSignal_DEPRECATED(AISIGNAL_INCLUDE_DISABLED, name, senderID);
	return NotifyGroup(groupID, pSignal);
}

Group::NotificationID CGroupManager::NotifyGroup(const GroupID& groupID, AISignals::SignalSharedPtr pSignal)
{
	if (groupID > 0)
	{
		Group& group = GetGroup(groupID);

		group.Notify(++m_notifications, pSignal);
	}

	return m_notifications;
}

void CGroupManager::Serialize(TSerialize ser)
{
	ser.BeginGroup("CGroupManager");
	{
		ser.Value("Groups", m_groups);
		ser.Value("m_notifications", m_notifications);
	}
	ser.EndGroup();
}
