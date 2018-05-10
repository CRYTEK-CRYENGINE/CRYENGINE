// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

struct DisplayContext;

struct IRenderListener
{
	virtual void Render(DisplayContext& rDisplayContext) = 0;
};


