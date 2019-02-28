// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Action.h"
#include "CrySchematyc/Reflection/TypeDesc.h"
#include "CrySchematyc/Utils/EnumFlags.h"

namespace Schematyc
{

enum class EActionFlags
{
	None                    = 0,
	Singleton               = BIT(0), // Allow only of one instance of this action to run at a time per object.
	ServerOnly              = BIT(1), // Action should only be active on server.
	ClientOnly              = BIT(2), // Action should only be active on clients.
	NetworkReplicateServer  = BIT(3), // Action should be replicated on server.
	NetworkReplicateClients = BIT(4)  // Action should be replicated on clients.
};

typedef CEnumFlags<EActionFlags> ActionFlags;

class CActionDesc : public CCustomClassDesc
{
public:

	inline void SetIcon(const char* szIcon)
	{
		m_icon = szIcon;
	}

	inline const char* GetIcon() const
	{
		return m_icon.c_str();
	}

	inline void SetActionFlags(const ActionFlags& flags)
	{
		m_flags = flags;
	}

	inline ActionFlags GetActionFlags() const
	{
		return m_flags;
	}

private:

	string      m_icon;
	ActionFlags m_flags;
};

SCHEMATYC_DECLARE_CUSTOM_CLASS_DESC(CAction, CActionDesc, CClassDescInterface)

} // Schematyc
