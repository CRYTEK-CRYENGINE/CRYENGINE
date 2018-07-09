// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

////////////////////////////////////////////////////////////////////////////
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 1999-2014.
// -------------------------------------------------------------------------
//  File name:   Messages.h
//  Version:     v1.00
//  Created:     03/03/2014 by Matthijs vd Meide
//  Compilers:   Visual Studio 2010
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#pragma once
#include <CryCore/Platform/platform.h>
#include <CryString/CryString.h>
#include <vector>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/STL.h>
#include <CrySerialization/Enum.h>

//this is the address that the engine-side will be listening on
#define CONSOLE_ENGINE_CHANNEL "Console/Engine"

//this is the address to which history events are sent (unless in response to SHistoryRequest)
#define CONSOLE_HISTORY_CHANNEL "Console/History"

//this is the address to which cvar updates are sent (unless in response to SCVarRequest)
#define CONSOLE_CVAR_CHANNEL "Console/CVar"

namespace Messages
{
//types of CVars supported
enum EVarType
{
	eVarType_None,
	eVarType_Int,
	eVarType_Float,
	eVarType_String,
};

//serialization information for enum
YASLI_ENUM_BEGIN(EVarType, "var_type")
YASLI_ENUM_VALUE(eVarType_None, "none")
YASLI_ENUM_VALUE(eVarType_Int, "int")
YASLI_ENUM_VALUE(eVarType_Float, "float")
YASLI_ENUM_VALUE(eVarType_String, "string")
YASLI_ENUM_END()

//used to request CVar information
//message is delivered on CONSOLE_ENGINE_CHANNEL
struct SCVarRequest
{
	Serialization::string address;      //where the generated SCVarUpdateXXX messages should be delivered

	static const char*    GetName()
	{
		return "cvar_request";
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(address, "address");
	}

	void Set(const Serialization::string& address)
	{
		this->address = address;
	}
};

//shared base for typed SCVarUpdateXXX
struct SCVarUpdate
{
	EVarType              type;
	Serialization::string name;

	void                  Serialize(Serialization::IArchive& ar)
	{
		//	ar(type, "type");
		//	ar(name, "name");
	}
};

//used to inform about updated int vars
//message is delivered on CONSOLE_CVAR_CHANNEL (unless in response to SCVarRequest)
//also used to request an update to int var
//message is delivered on CONSOLE_ENGINE_CHANNEL
struct SCVarUpdateInt : SCVarUpdate
{
	int value;

	static const char* GetName()
	{
		return "cvar_update_int";
	}

	void Serialize(Serialization::IArchive& ar)
	{
		SCVarUpdate::Serialize(ar);
		ar(value, "value_int");
	}

	void Set(const Serialization::string& name, int value)
	{
		this->type = eVarType_Int;
		this->name = name;
		this->value = value;
	}
};

//used to inform about updated float vars
//message is delivered on CONSOLE_CVAR_CHANNEL (unless in response to SCVarRequest)
//also used to request an update to float var
//message is delivered on CONSOLE_ENGINE_CHANNEL
struct SCVarUpdateFloat : SCVarUpdate
{
	float value;

	static const char* GetName()
	{
		return "cvar_update_float";
	}

	void Serialize(Serialization::IArchive& ar)
	{
		SCVarUpdate::Serialize(ar);
		ar(value, "value_float");
	}

	void Set(const Serialization::string& name, float value)
	{
		this->type = eVarType_Float;
		this->name = name;
		this->value = value;
	}
};

//used to inform about updated string vars
//message is delivered on CONSOLE_CVAR_CHANNEL (unless in response to SCVarRequest)
//also used to request an update to int var
//message is delivered on CONSOLE_ENGINE_CHANNEL
struct SCVarUpdateString : SCVarUpdate
{
	Serialization::string value;

	static const char* GetName()
	{
		return "cvar_update_string";
	}

	void Serialize(Serialization::IArchive& ar)
	{
		SCVarUpdate::Serialize(ar);
		ar(value, "value_string");
	}

	void Set(const Serialization::string& name, const Serialization::string& value)
	{
		this->type = eVarType_String;
		this->name = name;
		this->value = value;
	}
};

//used to inform about deleted CVars
//message is delivered on CONSOLE_CVAR_CHANNEL
struct SCVarDestroyed
{
	Serialization::string name;

	static const char*    GetName()
	{
		return "cvar_destroyed";
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(name, "name");
	}
};

//used to request history
//message is delivered on CONSOLE_ENGINE_CHANNEL
struct SHistoryRequest
{
	Serialization::string address;      //where the generated SHistoryLine messages should be delivered

	static const char*    GetName()
	{
		return "history_request";
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(address, "address");
	}

	void Set(const Serialization::string& address)
	{
		this->address = address;
	}
};

//used to inform consoles of history lines
//message is delivered on CONSOLE_MAIN_CHANNEL (unless in response to SHistoryRequest)
struct SHistoryLine
{
	size_t                index; //the index of the line, 1 based
	Serialization::string line;  //the contents of the line

	static const char*    GetName()
	{
		return "history_line";
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(index, "index");
		ar(line, "line");
	}

	void Set(size_t index, const Serialization::string& line)
	{
		this->index = index;
		this->line = line;
	}
};

//used to request auto-complete information
//message is delivered on CONSOLE_ENGINE_CHANNEL
struct SAutoCompleteRequest
{
	Serialization::string address;    //the matching SAutoCompleteReply is sent to this address
	Serialization::string query;      //the query

	static const char*    GetName()
	{
		return "autocomplete_request";
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(address, "address");
		ar(query, "query");
	}

	void Set(const Serialization::string& address, const Serialization::string& query)
	{
		this->address = address;
		this->query = query;
	}
};

//used to reply to auto-complete requests
//message is delivered in response to SAutoCompleteRequest
struct SAutoCompleteReply
{
	Serialization::string query;      //the query that prompted this response
	struct SItem
	{
		Serialization::string name;  //the name of the variable or command
		EVarType              type;  //the type of variable, or VarTypeNone in case of a command
		Serialization::string value; //as Serialization::string, only for display purposes, empty for commands

		void                  Serialize(Serialization::IArchive& ar)
		{
			ar(name, "name");
			//ar(type, "type");
			ar(value, "value");
		}

		void Set(const Serialization::string& name, EVarType type, const Serialization::string& value)
		{
			this->name = name;
			this->type = type;
			this->value = value;
		}
	};
	std::vector<SItem> matches;

	static const char* GetName()
	{
		return "autocomplete_reply";
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(query, "query");
		ar(matches, "matches");
	}

	void Set(const Serialization::string& query, std::vector<SItem>&& matches)
	{
		this->query = query;
		this->matches = std::move(matches);
	}
};

//used to request execution of a console command
//message is delivered on CONSOLE_ENGINE_CHANNEL
struct SExecuteRequest
{
	Serialization::string command;      //the command to be executed

	static const char*    GetName()
	{
		return "execute_request";
	}

	void Serialize(Serialization::IArchive& ar)
	{
		ar(command, "command");
	}

	void Set(const Serialization::string& command)
	{
		this->command = command;
	}
};
}

