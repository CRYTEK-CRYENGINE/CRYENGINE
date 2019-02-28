// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/TimeValue.h>

#include "CrySchematyc/FundamentalTypes.h"
#include "CrySchematyc/Utils/Delegate.h"
#include "CrySchematyc/Utils/GUID.h"

namespace Schematyc
{
struct IScriptElement;

typedef std::function<EVisitStatus (IScriptElement&)> ScriptElementVisitor;

struct IScript
{
	virtual ~IScript() {}

	virtual CryGUID             GetGUID() const = 0;

	virtual const char*       SetFilePath(const char* szFilePath) = 0;
	virtual const char*       GetFilePath() const = 0;

	virtual const CTimeValue& GetTimeStamp() const = 0;

	virtual void              SetRoot(IScriptElement* pRoot) = 0; // #SchematycTODO : Does this really need to be part of the public interface?
	virtual IScriptElement*   GetRoot() = 0;

	virtual EVisitStatus      VisitElements(const ScriptElementVisitor& visitor) = 0;

};
} // Schematyc
