// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __HELPERDATA_H__
#define __HELPERDATA_H__

struct SHelperData
{
public:
	enum EHelperType
	{
		eHelperType_UNKNOWN,
		eHelperType_Point,
		eHelperType_Dummy
	};

public:
	SHelperData()
		: m_eHelperType(eHelperType_UNKNOWN)
	{
		for (int i = 0; i < 3; ++i)
		{
			m_boundBoxMin[i] = 0;
			m_boundBoxMax[i] = 0;
		}
	}

public:
	EHelperType m_eHelperType;
	float m_boundBoxMin[3];	    // used for eHelperType_Dummy only
	float m_boundBoxMax[3];     // used for eHelperType_Dummy only
};

#endif
