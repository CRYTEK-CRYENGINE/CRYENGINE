// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __Group_h__
#define __Group_h__

#pragma once

typedef int GroupID;

class Group
{
public:
	typedef uint32 NotificationID;

	enum
	{
		MaxNotificationLength = 56,
	};

	Group();
	Group(const GroupID& groupID);
	~Group();

	static void ClearStaticData() { stl::free_container(s_targetInfos); }

	typedef VectorSet<tAIObjectID> Members;
	const Members&                GetMembers() const;

	IAIGroupProxy*                GetProxy() const { return m_proxy; }

	uint32                        GetMemberCount() const;
	void                          AddMember(tAIObjectID objectID);
	void                          RemoveMember(tAIObjectID objectID);

	void                          Reset(EObjectResetType type);
	void                          Update(float updateTime);
	bool                          Empty() const;

	void                          Swap(Group& other);

	const CCountedRef<CAIObject>& GetTarget() const;
	EAITargetThreat               GetTargetThreat() const;
	EAITargetType                 GetTargetType() const;
	const Vec3& GetTargetLocation() const;

	void        Notify(const NotificationID& notificationID, tAIObjectID recipientID, const char* name);

	void        Serialize(TSerialize ser);

private:
	struct TargetInfo
	{
		TargetInfo(const tAIObjectID& _targetID, uint32 _threat, uint32 _type, const Vec3& _position)
			: targetID(_targetID)
			, threat(_threat)
			, type(_type)
			, occurences(1)
			, position(_position)
		{
		}

		tAIObjectID targetID;
		uint32      threat;
		uint32      type;
		uint32      occurences;
		Vec3        position;

		bool operator<(const TargetInfo& other) const
		{
			if (threat == other.threat)
				return occurences < other.occurences;
			return threat < other.threat;
		}
	};

	typedef std::vector<TargetInfo> TargetInfos;
	static TargetInfos        s_targetInfos;

	CCountedRef<CAIObject>    m_target;
	EAITargetThreat           m_targetThreat;
	EAITargetType             m_targetType;
	EAITargetType             m_prevTargetType;

	Vec3                      m_targetLocation;

	_smart_ptr<IAIGroupProxy> m_proxy;

	Members                   m_members;

	struct QueuedNotification
	{
		QueuedNotification()
			: ID(0)
			, senderID(0)
		{
			notification[0] = 0;
		}

		QueuedNotification(const NotificationID& _notificationID, tAIObjectID _senderID, const char* _notification)
			: ID(_notificationID)
			, senderID(_senderID)
		{
			cry_strcpy(notification, _notification);
		}

		NotificationID ID;
		tAIObjectID    senderID;
		char           notification[MaxNotificationLength];

		void           Serialize(TSerialize ser)
		{
			ser.Value("ID", ID);
			ser.Value("senderID", senderID);

			string textString(notification);
			ser.Value("notification", textString);

			if (ser.IsReading())
				cry_strcpy(notification, textString.c_str());
		}
	};

	std::deque<QueuedNotification> m_notifications;
	GroupID                        m_groupID;
};

#endif // __Group_h__
