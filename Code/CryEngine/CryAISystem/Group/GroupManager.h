// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GroupManager_h__
#define __GroupManager_h__

#include "Group.h"

class CGroupManager
{
public:
	CGroupManager();
	~CGroupManager();

	void                  Reset(EObjectResetType type);
	void                  Update(float updateTime);

	void                  AddGroupMember(const GroupID& groupID, tAIObjectID objectID);
	void                  RemoveGroupMember(const GroupID& groupID, tAIObjectID objectID);

	Group&                GetGroup(const GroupID& groupID);
	const Group&          GetGroup(const GroupID& groupID) const;

	uint32                GetGroupMemberCount(const GroupID& groupID) const;

	Group::NotificationID NotifyGroup(const GroupID& groupID, tAIObjectID senderID, const char* name);

	void                  Serialize(TSerialize ser);

private:
	typedef std::unordered_map<GroupID, Group, stl::hash_uint32> Groups;
	Groups                m_groups;

	Group::NotificationID m_notifications;
};

#endif //__GroupManager_h__
