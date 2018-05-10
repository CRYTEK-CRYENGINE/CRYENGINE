// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AnimationData.h"

AnimationData::AnimationData(int modelCount, float fps, float startTime)
: m_entries(modelCount),
	m_frameCount(0),
	m_startTime(startTime), m_fps(fps)
{
}

void AnimationData::SetFrameData(int modelIndex, int frameIndex, float translation[3], float rotation[3], float scale[3])
{
	State& state = m_entries[modelIndex].samples[frameIndex];
	state.translation[0] = translation[0]; state.translation[1] = translation[1]; state.translation[2] = translation[2];
	state.rotation[0] = rotation[0]; state.rotation[1] = rotation[1]; state.rotation[2] = rotation[2];
	state.scale[0] = scale[0]; state.scale[1] = scale[1]; state.scale[2] = scale[2];
}

void AnimationData::SetFrameCount(int frameCount)
{
	m_frameCount = frameCount;
	for (int modelIndex = 0, modelCount = int(m_entries.size()); modelIndex < modelCount; ++modelIndex)
	{
		m_entries[modelIndex].samples.resize(frameCount);
	}
}

void AnimationData::SetModelFlags(int modelIndex, unsigned modelFlags)
{
	m_entries[modelIndex].flags = modelFlags;
}

void AnimationData::GetFrameData(int modelIndex, int frameIndex, const float*& translation, const float*& rotation, const float*& scale) const
{
	translation = m_entries[modelIndex].samples[frameIndex].translation;
	rotation = m_entries[modelIndex].samples[frameIndex].rotation;
	scale = m_entries[modelIndex].samples[frameIndex].scale;
}

void AnimationData::GetFrameDataPos(int modelIndex, int frameIndex, const float*& translation) const
{
	translation = m_entries[modelIndex].samples[frameIndex].translation;
}

void AnimationData::GetFrameDataRot(int modelIndex, int frameIndex, const float*& rotation) const
{
	rotation = m_entries[modelIndex].samples[frameIndex].rotation;
}

void AnimationData::GetFrameDataScl(int modelIndex, int frameIndex, const float*& scale) const
{
	scale = m_entries[modelIndex].samples[frameIndex].scale;
}

int AnimationData::GetFrameCount() const
{
	return m_frameCount;
}

unsigned AnimationData::GetModelFlags(int modelIndex) const
{
	return m_entries[modelIndex].flags;
}

AnimationData::State::State()
{
	translation[0] = translation[1] = translation[2] = 0.0f;
	rotation[0] = rotation[1] = rotation[2] = 0.0f;
	scale[0] = scale[1] = scale[2] = 1.0f;
}

AnimationData::ModelEntry::ModelEntry()
: flags(0)
{
}

///////////////////////////////////////////////////////////////////////////
NonSkeletalAnimationData::NonSkeletalAnimationData(int modelCount)
: m_entries(modelCount)
{
}

void NonSkeletalAnimationData::SetModelFlags(int modelIndex, unsigned modelFlags)
{
	m_entries[modelIndex].flags = modelFlags;
}

unsigned NonSkeletalAnimationData::GetModelFlags(int modelIndex) const
{
	return m_entries[modelIndex].flags;
}

void NonSkeletalAnimationData::SetFrameTimePos(int modelIndex, int frameIndex, float time)
{
	State& state = m_entries[modelIndex].samplesPos[frameIndex];
	state.time = time;
}

void NonSkeletalAnimationData::SetFrameDataPos(int modelIndex, int frameIndex, float translation[3])
{
	State& state = m_entries[modelIndex].samplesPos[frameIndex];
	state.data[0] = translation[0]; state.data[1] = translation[1]; state.data[2] = translation[2];
}

void NonSkeletalAnimationData::SetFrameCountPos(int modelIndex, int frameCount)
{
	m_entries[modelIndex].samplesPos.resize(frameCount);
}

void NonSkeletalAnimationData::SetFrameTimeRot(int modelIndex, int frameIndex, float time)
{
	State& state = m_entries[modelIndex].samplesRot[frameIndex];
	state.time = time;
}

void NonSkeletalAnimationData::SetFrameDataRot(int modelIndex, int frameIndex, float rotation[3])
{
	State& state = m_entries[modelIndex].samplesRot[frameIndex];
	state.data[0] = rotation[0]; state.data[1] = rotation[1]; state.data[2] = rotation[2];
}

void NonSkeletalAnimationData::SetFrameCountRot(int modelIndex, int frameCount)
{
	m_entries[modelIndex].samplesRot.resize(frameCount);
}

void NonSkeletalAnimationData::SetFrameTimeScl(int modelIndex, int frameIndex, float time)
{
	State& state = m_entries[modelIndex].samplesScl[frameIndex];
	state.time = time;
}

void NonSkeletalAnimationData::SetFrameDataScl(int modelIndex, int frameIndex, float scale[3])
{
	State& state = m_entries[modelIndex].samplesScl[frameIndex];
	state.data[0] = scale[0]; state.data[1] = scale[1]; state.data[2] = scale[2];
}

void NonSkeletalAnimationData::SetFrameCountScl(int modelIndex, int frameCount)
{
	m_entries[modelIndex].samplesScl.resize(frameCount);
}

float NonSkeletalAnimationData::GetFrameTimePos(int modelIndex, int frameIndex) const
{
	return m_entries[modelIndex].samplesPos[frameIndex].time;
}

void NonSkeletalAnimationData::GetFrameDataPos(int modelIndex, int frameIndex, const float*& translation) const
{
	translation = m_entries[modelIndex].samplesPos[frameIndex].data;
}

int NonSkeletalAnimationData::GetFrameCountPos(int modelIndex) const
{
	return int(m_entries[modelIndex].samplesPos.size());
}

float NonSkeletalAnimationData::GetFrameTimeRot(int modelIndex, int frameIndex) const
{
	return m_entries[modelIndex].samplesRot[frameIndex].time;
}

void NonSkeletalAnimationData::GetFrameDataRot(int modelIndex, int frameIndex, const float*& rotation) const
{
	rotation = m_entries[modelIndex].samplesRot[frameIndex].data;
}

int NonSkeletalAnimationData::GetFrameCountRot(int modelIndex) const
{
	return int(m_entries[modelIndex].samplesRot.size());
}

float NonSkeletalAnimationData::GetFrameTimeScl(int modelIndex, int frameIndex) const
{
	return m_entries[modelIndex].samplesScl[frameIndex].time;
}

void NonSkeletalAnimationData::GetFrameDataScl(int modelIndex, int frameIndex, const float*& scale) const
{
	scale = m_entries[modelIndex].samplesScl[frameIndex].data;
}

int NonSkeletalAnimationData::GetFrameCountScl(int modelIndex) const
{
	return int(m_entries[modelIndex].samplesScl.size());
}

void NonSkeletalAnimationData::SetFrameTCBPos(int modelIndex, int frameIndex, IAnimationData::TCB tcb)
{
	State& state = m_entries[modelIndex].samplesPos[frameIndex];
	state.tcb = tcb;
}

void NonSkeletalAnimationData::SetFrameTCBRot(int modelIndex, int frameIndex, IAnimationData::TCB tcb)
{
	State& state = m_entries[modelIndex].samplesRot[frameIndex];
	state.tcb = tcb;
}

void NonSkeletalAnimationData::SetFrameTCBScl(int modelIndex, int frameIndex, IAnimationData::TCB tcb)
{
	State& state = m_entries[modelIndex].samplesScl[frameIndex];
	state.tcb = tcb;
}

void NonSkeletalAnimationData::SetFrameEaseInOutPos(int modelIndex, int frameIndex, IAnimationData::Ease ease)
{
	State& state = m_entries[modelIndex].samplesPos[frameIndex];
	state.ease = ease;
}

void NonSkeletalAnimationData::SetFrameEaseInOutRot(int modelIndex, int frameIndex, IAnimationData::Ease ease)
{
	State& state = m_entries[modelIndex].samplesRot[frameIndex];
	state.ease = ease;
}

void NonSkeletalAnimationData::SetFrameEaseInOutScl(int modelIndex, int frameIndex, IAnimationData::Ease ease)
{
	State& state = m_entries[modelIndex].samplesScl[frameIndex];
	state.ease = ease;
}

void NonSkeletalAnimationData::GetFrameTCBPos(int modelIndex, int frameIndex, IAnimationData::TCB& tcb) const
{
	const State& state = m_entries[modelIndex].samplesPos[frameIndex];
	tcb = state.tcb;
}

void NonSkeletalAnimationData::GetFrameTCBRot(int modelIndex, int frameIndex, IAnimationData::TCB& tcb) const
{
	const State& state = m_entries[modelIndex].samplesRot[frameIndex];
	tcb = state.tcb;
}

void NonSkeletalAnimationData::GetFrameTCBScl(int modelIndex, int frameIndex, IAnimationData::TCB& tcb) const
{
	const State& state = m_entries[modelIndex].samplesScl[frameIndex];
	tcb = state.tcb;
}

void NonSkeletalAnimationData::GetFrameEaseInOutPos(int modelIndex, int frameIndex, IAnimationData::Ease& ease) const
{
	const State& state = m_entries[modelIndex].samplesPos[frameIndex];
	ease = state.ease;
}

void NonSkeletalAnimationData::GetFrameEaseInOutRot(int modelIndex, int frameIndex, IAnimationData::Ease& ease) const
{
	const State& state = m_entries[modelIndex].samplesRot[frameIndex];
	ease = state.ease;
}

void NonSkeletalAnimationData::GetFrameEaseInOutScl(int modelIndex, int frameIndex, IAnimationData::Ease& ease) const
{
	const State& state = m_entries[modelIndex].samplesScl[frameIndex];
	ease = state.ease;
}


NonSkeletalAnimationData::State::State()
{
	time = 0.0f;
	data[0] = data[1] = data[2] = 0.0f;
}

NonSkeletalAnimationData::ModelEntry::ModelEntry()
: flags(0)
{
}