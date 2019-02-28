// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Custom nodes relating to general entity features

#pragma once

#include <ILevelSystem.h>

class CEntityAttachmentExNodeRegistry : ISystemEventListener
{
public:
	typedef std::vector<string> TAttachmentNames;
	typedef std::map<EntityId, TAttachmentNames> TEntityAttachments;

	CEntityAttachmentExNodeRegistry();
	~CEntityAttachmentExNodeRegistry();

	inline const TAttachmentNames* GetOwnedAttachmentNames(EntityId id) const
	{
		TEntityAttachments::const_iterator it = m_entityAttachments.find(id);
		return it != m_entityAttachments.end() ? &it->second : nullptr;
	}

	inline void RegisterAttachmentOwnership(EntityId id, const string& attachmentName)
	{
		TAttachmentNames& names = m_entityAttachments[id];
		stl::binary_insert_unique(names, attachmentName);
	}

	bool OwnsAttachment(EntityId id, const string& attachmentName) const;
	bool DeleteAttachment(EntityId id, const string& attachmentName);
	void DeleteAllAttachments(EntityId id);
	void CleanAll();

private:
	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener

private:
	void DestroyAllListedAttachments(EntityId id, const TAttachmentNames& names);

	TEntityAttachments                    m_entityAttachments;
};

