// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.


//--------------------------------------------------------------------------------------------------
//structure used to store current filtering settings
//--------------------------------------------------------------------------------------------------
struct SCubemapFilterParams
{
	float  BaseFilterAngle;
	float  InitialMipAngle;
	float  MipAnglePerLevelScale;
	float  BRDFGlossScale;
	float  BRDFGlossBias;
	int32  FilterType;
	int32  FixupType; 
	int32  FixupWidth;
	int32  SampleCountGGX;
	bool8  bUseSolidAngle;

	SCubemapFilterParams()
	{
		BaseFilterAngle = 20.0;
		InitialMipAngle = 1.0;
		MipAnglePerLevelScale = 1.0;
		BRDFGlossScale = 1.0f;
		BRDFGlossBias = 0.0f;
		FilterType = CP_FILTER_TYPE_ANGULAR_GAUSSIAN;
		FixupType = CP_FIXUP_PULL_LINEAR; 
		FixupWidth = 3;
		bUseSolidAngle = TRUE;
	}
};


