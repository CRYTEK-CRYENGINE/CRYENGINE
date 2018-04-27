// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Cry3DEngine/ITimeOfDay.h>

//Environment preset settings, that are not dependent of time change

struct SunImpl : public ITimeOfDay::Sun
{
	SunImpl();
	void ResetVariables();
	void Serialize(Serialization::IArchive& ar);
};

struct MoonImpl : public ITimeOfDay::Moon
{
	MoonImpl();
	void ResetVariables();
	void Serialize(Serialization::IArchive& ar);
};

struct WindImpl : public ITimeOfDay::Wind
{
	WindImpl();
	void ResetVariables();
	void Serialize(Serialization::IArchive& ar);
};

struct CloudShadowsImpl : public ITimeOfDay::CloudShadows
{
	CloudShadowsImpl();
	void ResetVariables();
	void Serialize(Serialization::IArchive& ar);
};

struct TotalIllumImpl : public ITimeOfDay::TotalIllum
{
	TotalIllumImpl();
	void ResetVariables();
	void Serialize(Serialization::IArchive& ar);
};

struct TotalIllumAdvImpl : public ITimeOfDay::TotalIllumAdv
{
	TotalIllumAdvImpl();
	void ResetVariables();
	void Serialize(Serialization::IArchive& ar);
};

struct CTimeOfDayConstants
{
	CTimeOfDayConstants();
	void ResetVariables();
	void Serialize(Serialization::IArchive& ar);

	SunImpl           sun;
	MoonImpl          moon;
	WindImpl          wind;
	CloudShadowsImpl  cloudShadows;
	TotalIllumImpl    totalIllumination;
	TotalIllumAdvImpl totalIlluminationAdvanced;
};
