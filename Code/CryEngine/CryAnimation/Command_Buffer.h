// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Skeleton.h"

class CDefaultSkeleton;
class CCharInstance;

namespace Command
{

// TODO: Needs rename
enum
{
	TmpBuffer    = 0,
	TargetBuffer = 4,
};

class CState
{
public:
	CState()
		: m_pDefaultSkeleton(nullptr)
		, m_location(IDENTITY)
		, m_pPoseData(nullptr)
		, m_jointCount(0)
		, m_originalTimeDelta(0.0f)
		, m_pJointMask(nullptr)
		, m_pInstance(nullptr)
	{
		Reset();
	}

public:
	bool       Initialize(CCharInstance* pInstance, const QuatTS& location);

	ILINE void Reset()
	{
		m_lod = 0;

		m_pJointMask = NULL;
		m_jointMaskCount = 0;
	}

	ILINE bool IsJointActive(uint32 nameCrc32) const
	{
		if (!m_pJointMask)
			return true;

		const uint32* pValue = std::lower_bound(&m_pJointMask[0], &m_pJointMask[m_jointMaskCount], nameCrc32);
		if (pValue == &m_pJointMask[m_jointMaskCount] || *pValue != nameCrc32)
			return false;

		return true;
	}

public:
	const CDefaultSkeleton*    m_pDefaultSkeleton;

	QuatTS                     m_location;

	const Skeleton::CPoseData* m_pFallbackPoseData;
	Skeleton::CPoseData*       m_pPoseData;

	uint32                     m_jointCount;

	uint32                     m_lod;

	f32                        m_originalTimeDelta;

	const uint32*              m_pJointMask;
	uint32                     m_jointMaskCount;

	// NOTE: Do not access this, here only for PoseModifier back-compat.
	CCharInstance* m_pInstance;
};

class CEvaluationContext
{
public:
	void* m_buffers[TargetBuffer * 2]; // TODO: Provide strong typing for these buffers.
	bool  m_isScalingPresent;
};

class CBuffer
{
public:
	enum { BufferMemorySize = 8192 };

	ILINE uint32 GetLengthTotal() const  { return sizeof(m_pBuffer); }
	ILINE uint32 GetLengthUsed() const   { return uint32(m_pCommands - m_pBuffer); }
	ILINE uint32 GetLengthFree() const   { return GetLengthTotal() - GetLengthUsed(); }
	ILINE uint32 GetCommandCount() const { return m_commandCount; }

	bool         Initialize(CCharInstance* pInstance, const QuatTS& location);

	template<class Type>
	Type* CreateCommand()
	{
		assert((sizeof(Type) & 3) == 0);

		uint32 lengthFree = GetLengthFree();
		if (lengthFree < sizeof(Type))
		{
			CryFatalError("CryAnimation: CommandBuffer overflow!");
			return NULL;
		}

		Type* pCommand = (Type*)m_pCommands;
		*(uint8*)pCommand = Type::ID;
		m_pCommands += sizeof(Type);
		m_commandCount++;
		return pCommand;
	}

	void Execute();

private:
	void DebugDraw();

private:
	uint8  m_pBuffer[BufferMemorySize];

	uint8* m_pCommands;
	uint8  m_commandCount;

	CState m_state;

	// NOTE: Do not access this, here only for PoseModifier back-compat.
	CCharInstance* m_pInstance;
};

} // namespace Command
