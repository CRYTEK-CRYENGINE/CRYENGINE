// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
*************************************************************************/

#include "StdAfx.h"
#include "XMLCPB_Common.h"
#include <CryCore/TypeInfo_impl.h>

using namespace XMLCPB;

// !!!this array needs to be in the same order and with same elements than eAttrDataType!!!
STypeInfo s_TypeInfos[DT_NUMTYPES] =
{
	//...basic types.......
	{ DT_STR,       2,                                 DT_STR,     "DT_STR"       },
	{ DT_INT32,     sizeof(uint32),                    DT_INT32,   "DT_INT32"     },
	{ DT_F1,        sizeof(float),                     DT_F1,      "DT_F1"        },
	{ DT_F3,        sizeof(float) * 3,                 DT_F3,      "DT_F3"        },
	{ DT_F4,        sizeof(float) * 4,                 DT_F4,      "DT_F4"        },
	{ DT_INT64,     sizeof(uint64),                    DT_INT64,   "DT_INT64"     },
	{ DT_RAWDATA,   VARIABLE_DATA_SIZE,                DT_RAWDATA, "DT_RAWDATA"   },

	// compressed types....
	{ DT_POS16,     sizeof(uint16),                    DT_INT32,   "DT_POS16"     },
	{ DT_NEG16,     sizeof(uint16),                    DT_INT32,   "DT_NEG16"     },
	{ DT_POS8,      sizeof(uint8),                     DT_INT32,   "DT_POS8"      },
	{ DT_NEG8,      sizeof(uint8),                     DT_INT32,   "DT_NEG8"      },
	{ DT_F1_1DEC,   sizeof(int8),                      DT_F1,      "DT_F1_1DEC"   },
	{ DT_F3_1CONST, sizeof(float) * 2 + sizeof(uint8), DT_F3,      "DT_F3_1CONST" },
	{ DT_F3_2CONST, sizeof(float) + sizeof(uint8),     DT_F3,      "DT_F3_2CONST" },
	{ DT_F3_3CONST, sizeof(uint8),                     DT_F3,      "DT_F3_3CONST" },
	{ DT_F4_1CONST, sizeof(float) * 3 + sizeof(uint8), DT_F4,      "DT_F4_1CONST" },
	{ DT_F4_2CONST, sizeof(float) * 2 + sizeof(uint8), DT_F4,      "DT_F4_2CONST" },
	{ DT_F4_3CONST, sizeof(float) + sizeof(uint8),     DT_F4,      "DT_F4_3CONST" },
	{ DT_F4_4CONST, sizeof(uint8),                     DT_F4,      "DT_F4_4CONST" },

	//........numeric constants......
	{ DT_0,         0,                                 DT_INT32,   "DT_0"         },
	{ DT_1,         0,                                 DT_INT32,   "DT_1"         },
	{ DT_2,         0,                                 DT_INT32,   "DT_2"         },
	{ DT_3,         0,                                 DT_INT32,   "DT_3"         },
	{ DT_4,         0,                                 DT_INT32,   "DT_4"         },
	{ DT_5,         0,                                 DT_INT32,   "DT_5"         },
	{ DT_6,         0,                                 DT_INT32,   "DT_6"         },
	{ DT_7,         0,                                 DT_INT32,   "DT_7"         },
	{ DT_8,         0,                                 DT_INT32,   "DT_8"         },
	{ DT_9,         0,                                 DT_INT32,   "DT_9"         },
	{ DT_10,        0,                                 DT_INT32,   "DT_10"        },
	{ DT_255,       0,                                 DT_INT32,   "DT_255"       },

	{ DT_F3_100,    0,                                 DT_F3,      "DT_F3_100"    },
	{ DT_F3_010,    0,                                 DT_F3,      "DT_F3_010"    },
	{ DT_F3_001,    0,                                 DT_F3,      "DT_F3_001"    },
};

// string constants
const char* s_constStrings[] =
{
	"n",
	"table",
	"RigidBodyEx",
	"bRigidBodyActive",
	"false",
	"true",
	"Idle",
	"bActive",
	"Light",
	"h",
	"s",
	"vec",
	"ParticleEffect",
	"SmartObject",
	"b",
	"TagPoint",
	"GrabableLedge",
	"health",
	"nParticleSlot",
	"zero",
	"enabled",
	"state",
	"Alive",
	"currentSlot",
	"LastHit",
	"pos",
	"impulse",
	"shooterId",
	"bIgnoreObstruction",
	"bIgnoreCulling",
};

//////////////////////////////////////////////////////////////////////////
// some of the types are a bit procedural and need this

void XMLCPB::InitializeDataTypeInfo()
{
	static_assert(CRY_ARRAY_COUNT(s_constStrings) == DT_NUM_CONST_STR, "Unexpected array size!");

	// str constant types initialization
	for (int i = DT_FIRST_CONST_STR; i <= DT_LAST_CONST_STR; i++)
	{
		s_TypeInfos[i].type = eAttrDataType(i);
		s_TypeInfos[i].pName = "strConstant";
		s_TypeInfos[i].size = 0;
		s_TypeInfos[i].basicType = DT_STR;
	}

	// some checking
	for (uint i = 0; i < DT_NUMTYPES; ++i)
	{
		const STypeInfo& info = s_TypeInfos[i];
		assert(info.type == i);
		assert(info.basicType < DT_NUM_BASIC_TYPES);
	}
}

//////////////////////////////////////////////////////////////////////////

const char* XMLCPB::GetConstantString(uint32 ind)
{
	assert(ind < DT_NUM_CONST_STR);
	return s_constStrings[ind];
}

//////////////////////////////////////////////////////////////////////////

eAttrDataType XMLCPB::GetBasicDataType(eAttrDataType type)
{
	return s_TypeInfos[type].basicType;
}

//////////////////////////////////////////////////////////////////////////

uint32 XMLCPB::GetDataTypeSize(eAttrDataType type)
{
	return s_TypeInfos[type].size;
}

//////////////////////////////////////////////////////////////////////////

const char* XMLCPB::GetDataTypeName(eAttrDataType type)
{
	return s_TypeInfos[type].pName;
}
