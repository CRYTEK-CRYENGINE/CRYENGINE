// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
struct CNaturalPointInputNull : public INaturalPointInput
{
public:
	CNaturalPointInputNull(){};
	virtual ~CNaturalPointInputNull(){};

	virtual bool Init()      { return true; }
	virtual void Update()    {};
	virtual bool IsEnabled() { return true; }

	virtual void Recenter()  {};

	// Summary;:
	//		Get raw skeleton data
	virtual bool GetNaturalPointData(NP_RawData& npRawData) const { return true; }

};
