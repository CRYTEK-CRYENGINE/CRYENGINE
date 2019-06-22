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

struct SkyImpl : public ITimeOfDay::Sky
{
	SkyImpl();
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

struct ColorGradingImpl : public ITimeOfDay::ColorGrading
{
	ColorGradingImpl();
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

struct STimeOfDayConstants : public ITimeOfDay::IConstants
{
	STimeOfDayConstants();

	virtual ITimeOfDay::Sun& GetSunParams() override;
	virtual ITimeOfDay::Moon& GetMoonParams() override;
	virtual ITimeOfDay::Sky& GetSkyParams() override;
	virtual ITimeOfDay::Wind& GetWindParams() override;
	virtual ITimeOfDay::CloudShadows& GetCloudShadowsParams() override;
	virtual ITimeOfDay::ColorGrading& GetColorGradingParams() override;
	virtual ITimeOfDay::TotalIllum& GetTotalIlluminationParams() override;
	virtual ITimeOfDay::TotalIllumAdv& GetTotalIlluminationAdvParams() override;

	virtual void Serialize(Serialization::IArchive& ar) override;
	virtual void Reset() override;

	SunImpl           sun;
	MoonImpl          moon;
	SkyImpl           sky;
	WindImpl          wind;
	CloudShadowsImpl  cloudShadows;
	ColorGradingImpl  colorGrading;
	TotalIllumImpl    totalIllumination;
	TotalIllumAdvImpl totalIlluminationAdvanced;
};
