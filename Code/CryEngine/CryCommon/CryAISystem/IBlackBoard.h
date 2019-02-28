// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct IBlackBoard
{
	// <interfuscator:shuffle>
	virtual SmartScriptTable& GetForScript() = 0;
	virtual void              SetFromScript(SmartScriptTable&) = 0;
	virtual void              Clear() = 0;
	virtual ~IBlackBoard(){}
	// </interfuscator:shuffle>
};
