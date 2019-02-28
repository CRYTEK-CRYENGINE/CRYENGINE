// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class EDITOR_COMMON_API CScopedRenderLock
{
public:
	CScopedRenderLock();
	~CScopedRenderLock();
	explicit operator bool();

	static void Lock();
	static void Unlock();

private:
	bool m_bOwnsLock;

	static int s_renderLock;
};

