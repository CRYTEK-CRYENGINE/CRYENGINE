// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Reflection/TypeDesc.h"
#include "CrySchematyc/Services/ILog.h"

namespace Schematyc
{
struct SLogStreamName
{
	inline SLogStreamName()
		: value(gEnv->pSchematyc->GetLog().GetStreamName(LogStreamId::Default))
	{}

	inline SLogStreamName(const char* _szValue)
		: value(_szValue)
	{}

	inline bool operator==(const SLogStreamName& rhs) const
	{
		return value == rhs.value;
	}

	static void ReflectType(CTypeDesc<SLogStreamName>& desc)
	{
		desc.SetGUID("e43f1341-ac7c-4dd7-892f-097810eee478"_cry_guid);
	}

	string value;
};

inline bool Serialize(Serialization::IArchive& archive, SLogStreamName& value, const char* szName, const char* szLabel)
{
	if (archive.isEdit())
	{
		Serialization::StringList logStreamNames;

		auto visitLogStream = [&logStreamNames](const char* szStreamName, LogStreamId streamId) -> EVisitStatus
		{
			logStreamNames.push_back(szStreamName);
			return EVisitStatus::Continue;
		};
		gEnv->pSchematyc->GetLog().VisitStreams(visitLogStream);

		if (archive.isInput())
		{
			Serialization::StringListValue temp(logStreamNames, 0);
			archive(temp, szName, szLabel);
			value.value = temp.c_str();
		}
		else if (archive.isOutput())
		{
			const int pos = logStreamNames.find(value.value.c_str());
			archive(Serialization::StringListValue(logStreamNames, pos), szName, szLabel);
		}
		return true;
	}
	else
	{
		return archive(value.value, szName, szLabel);
	}
}
} // Schematyc