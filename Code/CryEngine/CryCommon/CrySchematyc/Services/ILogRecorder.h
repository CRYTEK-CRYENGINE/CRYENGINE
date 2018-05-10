// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Services/ILog.h"
#include "CrySchematyc/Utils/Delegate.h"

namespace Schematyc
{
typedef std::function<EVisitStatus (const SLogMessageData&)> LogMessageVisitor;

struct ILogRecorder
{
	virtual ~ILogRecorder() {}

	virtual void Begin() = 0;
	virtual void End() = 0;
	virtual void VisitMessages(const LogMessageVisitor& visitor) = 0;
	virtual void Clear() = 0;
};
} // Schematyc
