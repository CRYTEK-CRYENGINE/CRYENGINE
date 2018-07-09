// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "..\ImageProperties.h"
#include "FIR-Windows.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const char* const s_filterFunctionNames[][3] =
{
	{ "none", "combined", "" },
	{ "point", "nearest", "" },
	{ "box", "average", "1" },	   // "1" obsolete setting, to keep compatibility with old files
	{ "triangle", "linear", "bartlett" },
	{ "quadric", "bilinear", "welch" },
	{ "cubic", "", "" },
	{ "hermite", "", "" },
	{ "catrom", "", "" },
	{ "", "sine", "" },
	{ "", "sinc", "" },
	{ "", "bessel", "" },
	{ "lanczos", "", "" },
	{ "gaussian", "", "" },
	{ "normal", "", "" },
	{ "mitchell", "", "" },
	{ "hann", "", "" },
	{ "bartlett-hann", "", "" },
	{ "hamming", "", "" },
	{ "blackman", "", "" },
	{ "blackman-harris", "", "" },
	{ "blackman-nuttall", "", "" },
	{ "flattop", "", "" },
	{ "kaiser", "", "" },
	{ "sigma-six", "gauss", "0" },	   // "0" obsolete setting, to keep compatibility with old files
	{ "kaiser-sinc", "", "" }
};

int GetFilterFunctionDefault()
{
	// eWindowFunction_BLACKMANHARRIS should be better, but sigmasix was the default
	return eWindowFunction_SigmaSix;
}

int GetFilterFunctionCount()
{
	return sizeof(s_filterFunctionNames) / sizeof(s_filterFunctionNames[0]);
}

const char* GetFilterFunctionName(int index)
{
	return s_filterFunctionNames[index][0];
}

const int GetFilterFunctionIndex(const char* name, int defaultIndex, const char* disabledValue)
{
	if (!stricmp(name, disabledValue))
	{
		return 0;
	}

	const int n = GetFilterFunctionCount();

	for (int i = 0; i < n; ++i)
	{	
		for (int a = 0; a < (sizeof(s_filterFunctionNames[0]) / sizeof(s_filterFunctionNames[0][0])); ++a)
		{
			if (s_filterFunctionNames[i][a][0] && !stricmp(name, s_filterFunctionNames[i][a]))
			{
				return i;
			}
		}
	}

	return defaultIndex;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const char* const s_filterEvaluationNames[][1] =
{
	{ "sum" },
	{ "max" },
	{ "min" },
};

int GetFilterEvaluationDefault()
{
	return eWindowEvaluation_Sum;
}

int GetFilterEvaluationCount()
{
	return sizeof(s_filterEvaluationNames) / sizeof(s_filterEvaluationNames[0]);
}

const char* GetFilterEvaluationName(int index)
{
	return s_filterEvaluationNames[index][0];
}

const int GetFilterEvaluationIndex(const char* name, int defaultIndex)
{
	const int n = GetFilterEvaluationCount();

	for (int i = 0; i < n; ++i)
	{	
		for (int a = 0; a < (sizeof(s_filterEvaluationNames[0]) / sizeof(s_filterEvaluationNames[0][0])); ++a)
		{
			if (s_filterEvaluationNames[i][a][0] && !stricmp(name, s_filterEvaluationNames[i][a]))
			{
				return i;
			}
		}
	}

	return defaultIndex;
}
