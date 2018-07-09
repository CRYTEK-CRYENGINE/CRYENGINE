// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

struct IProtocolBuilder;

struct IManualFrameStepController
{
	//! Add message sinks to receive network events
	virtual void DefineProtocols(IProtocolBuilder* pBuilder) = 0;
};