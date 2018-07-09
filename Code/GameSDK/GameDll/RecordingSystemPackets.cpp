// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RecordingSystemPackets.h"
#include "RecordingSystem.h"
#include "Utility/BufferUtil.h"

void SRecording_FrameData::Serialise( CBufferUtil &buffer )
{
	size = sizeof(SRecording_FrameData);

	buffer.Serialise(frametime);
}

void SRecording_FPChar::Serialise( CBufferUtil &buffer )
{
	size = sizeof(SRecording_FPChar);

	buffer.Serialise(camlocation);
	buffer.Serialise(relativePosition);
	buffer.Serialise(fov);
	buffer.Serialise(frametime);
	buffer.Serialise(playerFlags);
}

void SRecording_Flashed::Serialise( CBufferUtil &buffer )
{
	size = sizeof(SRecording_Flashed);

	buffer.Serialise(frametime);
	buffer.Serialise(duration);
	buffer.Serialise(blindAmount);
}

void SRecording_RenderNearest::Serialise( CBufferUtil &buffer )
{
	size = sizeof(SRecording_RenderNearest);

	buffer.Serialise(frametime);
	buffer.Serialise(renderNearest);
}

void SRecording_VictimPosition::Serialise( CBufferUtil &buffer )
{
	size = sizeof(SRecording_VictimPosition);

	buffer.Serialise(frametime);
	buffer.Serialise(victimPosition);
}

void SRecording_KillHitPosition::Serialise( CBufferUtil &buffer )
{
	size = sizeof(SRecording_KillHitPosition);

	buffer.Serialise(hitRelativePos);
	buffer.Serialise(victimId);
	buffer.Serialise(fRemoteKillTime);
}

void SRecording_PlayerHealthEffect::Serialise( CBufferUtil &buffer )
{
	size = sizeof(SRecording_PlayerHealthEffect);

	buffer.Serialise(hitDirection);
	buffer.Serialise(frametime);
	buffer.Serialise(hitStrength);
	buffer.Serialise(hitSpeed);
}

void SRecording_PlaybackTimeOffset::Serialise( CBufferUtil &buffer )
{
	size = sizeof(SRecording_PlaybackTimeOffset);

	buffer.Serialise(timeOffset);
}

