// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _H_LIVECREATECOMMANDS_CAMERA_H_
#define _H_LIVECREATECOMMANDS_CAMERA_H_

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if !defined(NO_LIVECREATE)

	#include "LiveCreateCommands.h"

namespace LiveCreate
{

//-----------------------------------------------------------------------------

class CLiveCreateCmd_SetCameraPosition : public ILiveCreateCommand
{
	DECLARE_REMOTE_COMMAND(CLiveCreateCmd_SetCameraPosition);

public:
	Vec3 m_position;
	Quat m_rotation;

public:
	template<class T>
	void Serialize(T& stream)
	{
		stream << m_position.x;
		stream << m_position.y;
		stream << m_position.z;
		stream << m_rotation.v.x;
		stream << m_rotation.v.y;
		stream << m_rotation.v.z;
		stream << m_rotation.w;
	}

	virtual void Execute() LC_COMMAND;
};

//-----------------------------------------------------------------------------

class CLiveCreateCmd_SetCameraFOV : public ILiveCreateCommand
{
	DECLARE_REMOTE_COMMAND(CLiveCreateCmd_SetCameraFOV);

public:
	float m_fov;

public:
	template<class T>
	void Serialize(T& stream)
	{
		stream << m_fov;
	}

	virtual void Execute() LC_COMMAND;
};

//-----------------------------------------------------------------------------

class CLiveCreateCmd_EnableLiveCreate : public ILiveCreateCommand
{
	DECLARE_LIVECREATE_COMMAND(CLiveCreateCmd_EnableLiveCreate);

public:
	std::vector<string> m_visibleLayers;
	std::vector<string> m_hiddenLayers;

public:
	template<class T>
	void Serialize(T& stream)
	{
		stream << m_visibleLayers;
		stream << m_hiddenLayers;
	}

	virtual void Execute() LC_COMMAND;
};

//-----------------------------------------------------------------------------

class CLiveCreateCmd_DisableLiveCreate : public ILiveCreateCommand
{
	DECLARE_LIVECREATE_COMMAND(CLiveCreateCmd_DisableLiveCreate);

public:
	bool m_bRestorePhysicsState;
	bool m_bWakePhysicalObjects;

public:
	template<class T>
	void Serialize(T& stream)
	{
		stream << m_bRestorePhysicsState;
		stream << m_bWakePhysicalObjects;
	}

	virtual void Execute() LC_COMMAND;
};

//-----------------------------------------------------------------------------

class CLiveCreateCmd_EnableCameraSync : public ILiveCreateCommand
{
	DECLARE_LIVECREATE_COMMAND(CLiveCreateCmd_EnableCameraSync);

public:
	Vec3 m_position;
	Quat m_rotation;

public:
	template<class T>
	void Serialize(T& stream)
	{
		stream << m_position.x;
		stream << m_position.y;
		stream << m_position.z;
		stream << m_rotation.v.x;
		stream << m_rotation.v.y;
		stream << m_rotation.v.z;
		stream << m_rotation.w;
	}

	virtual void Execute() LC_COMMAND;
};

//-----------------------------------------------------------------------------

class CLiveCreateCmd_DisableCameraSync : public ILiveCreateCommand
{
	DECLARE_LIVECREATE_COMMAND(CLiveCreateCmd_DisableCameraSync);

public:
	template<class T>
	void Serialize(T& stream)
	{
	}

	virtual void Execute() LC_COMMAND;
};

//-----------------------------------------------------------------------------

class CLiveCreateCmd_SetCVar : public ILiveCreateCommand
{
	DECLARE_LIVECREATE_COMMAND(CLiveCreateCmd_SetCVar);

public:
	string m_name;
	string m_value;

public:
	template<class T>
	void Serialize(T& stream)
	{
		stream << m_name;
		stream << m_value;
	}

	virtual void Execute() LC_COMMAND;
};

//-----------------------------------------------------------------------------

class CLiveCreateCmd_SetTimeOfDay : public ILiveCreateCommand
{
	DECLARE_LIVECREATE_COMMAND(CLiveCreateCmd_SetTimeOfDay);

public:
	float m_time;

public:
	template<class T>
	void Serialize(T& stream)
	{
		stream << m_time;
	}

	virtual void Execute() LC_COMMAND;
};

//-----------------------------------------------------------------------------

class CLiveCreateCmd_SetTimeOfDayValue : public ILiveCreateCommand
{
	DECLARE_LIVECREATE_COMMAND(CLiveCreateCmd_SetTimeOfDayValue);

public:
	int    m_index;
	string m_name;
	string m_data;

public:
	template<class T>
	void Serialize(T& stream)
	{
		stream << m_index;
		stream << m_name;
		stream << m_data;
	}

	virtual void Execute() LC_COMMAND;
};

//-----------------------------------------------------------------------------

class CLiveCreateCmd_SetTimeOfDayFull : public ILiveCreateCommand
{
	DECLARE_LIVECREATE_COMMAND(CLiveCreateCmd_SetTimeOfDayFull);

public:
	std::vector<int8> m_data;

public:
	template<class T>
	void Serialize(T& stream)
	{
		stream << m_data;
	}

	virtual void Execute() LC_COMMAND;
};

//-----------------------------------------------------------------------------

class CLiveCreateCmd_SetEnvironmentFull : public ILiveCreateCommand
{
	DECLARE_LIVECREATE_COMMAND(CLiveCreateCmd_SetEnvironmentFull);

public:
	std::vector<int8> m_data;

public:
	template<class T>
	void Serialize(T& stream)
	{
		stream << m_data;
	}

	virtual void Execute() LC_COMMAND;
};

//-----------------------------------------------------------------------------

class CLiveCreateCmd_SetArchetypesFull : public ILiveCreateCommand
{
	DECLARE_LIVECREATE_COMMAND(CLiveCreateCmd_SetArchetypesFull);

public:
	std::vector<int8> m_data;

public:
	template<class T>
	void Serialize(T& stream)
	{
		stream << m_data;
	}

	virtual void Execute() LC_COMMAND;
};

//-----------------------------------------------------------------------------

class CLiveCreateCmd_SetParticlesFull : public ILiveCreateCommand
{
	DECLARE_LIVECREATE_COMMAND(CLiveCreateCmd_SetParticlesFull);

public:
	std::vector<int8> m_data;

public:
	template<class T>
	void Serialize(T& stream)
	{
		stream << m_data;
	}

	virtual void Execute() LC_COMMAND;
};

//-----------------------------------------------------------------------------

class CLiveCreateCmd_FileSynced : public ILiveCreateCommand
{
	DECLARE_LIVECREATE_COMMAND(CLiveCreateCmd_FileSynced);

public:
	string m_fileName;

public:
	template<class T>
	void Serialize(T& stream)
	{
		stream << m_fileName;
	}

	virtual void Execute() LC_COMMAND;
};

//-----------------------------------------------------------------------------

class CLiveCreateCmd_SyncSelection : public ILiveCreateCommand
{
	DECLARE_LIVECREATE_COMMAND(CLiveCreateCmd_SyncSelection);

public:
	std::vector<SSelectedObject> m_selection;

public:
	template<class T>
	void Serialize(T& stream)
	{
		stream << m_selection;
	}

	virtual void Execute() LC_COMMAND;
};

//-----------------------------------------------------------------------------

class CLiveCreateCmd_SyncLayerVisibility : public ILiveCreateCommand
{
	DECLARE_LIVECREATE_COMMAND(CLiveCreateCmd_SyncLayerVisibility);

public:
	string m_layerName;
	bool   m_bIsVisible;

public:
	template<class T>
	void Serialize(T& stream)
	{
		stream << m_layerName;
		stream << m_bIsVisible;
	}

	virtual void Execute() LC_COMMAND;
};

//-----------------------------------------------------------------------------

}

#endif
#endif
