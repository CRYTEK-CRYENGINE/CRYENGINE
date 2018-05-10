// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace SchematycBaseEnv {

constexpr static uint32 AlphaBit(char c)
{
	return c >= 'a' && c <= 'z' ? 1 << (c - 'z' + 31) : 0;
}

enum EUpdateFlags
{
	eUpdateFlags_None = 0,
	eUpdateFlags_StagePrePhysics = AlphaBit('q'),
	eUpdateFlags_StageDefaultAndPost = AlphaBit('w'),
	eUpdateFlags_Timers = AlphaBit('e'),
	eUpdateFlags_StagePostDebug = AlphaBit('r'),
	eUpdateFlags_SpatialIndex = AlphaBit('t'),

	eUpdateFlags_GameDefaultUpdate = eUpdateFlags_StagePrePhysics | eUpdateFlags_StageDefaultAndPost | eUpdateFlags_Timers,
	eUpdateFlags_EditorGameDefaultUpdate = eUpdateFlags_GameDefaultUpdate | eUpdateFlags_StagePostDebug,

	eUpdateFlags_Default = 1,
};

struct IBaseEnv
{
	virtual void UpdateSpatialIndex() = 0;

	virtual int GetEditorGameDefaultUpdateMask() const = 0;
	virtual void SetEditorGameDefaultUpdateMask(const int mask) = 0;
	virtual int GetGameDefaultUpdateMask() const = 0;
	virtual void SetGameDefaultUpdateMask(const int mask) = 0;
};

}