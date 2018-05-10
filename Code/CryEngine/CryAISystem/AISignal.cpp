// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AISignal.h"
#include <CryCore/CryCrc32.h>

//====================================================================
// AISIGNAL_EXTRA_DATA
//====================================================================
AISignalExtraData::SignalExtraDataAlloc AISignalExtraData::m_signalExtraDataAlloc;

void AISignalExtraData::CleanupPool()
{
	m_signalExtraDataAlloc.FreeMemoryIfEmpty();
}

AISignalExtraData::AISignalExtraData()
{
	point.zero();
	point2.zero();
	fValue = 0.0f;
	nID = 0;
	iValue = 0;
	iValue2 = 0;
	sObjectName = NULL;
}

AISignalExtraData::~AISignalExtraData()
{
	if (sObjectName)
		delete[] sObjectName;
}

void AISignalExtraData::SetObjectName(const char* objectName)
{
	if (sObjectName)
	{
		delete[] sObjectName;
		sObjectName = NULL;
	}
	if (objectName && *objectName)
	{
		sObjectName = new char[strlen(objectName) + 1];
		strcpy(sObjectName, objectName);
	}
}

void AISignalExtraData::Serialize(TSerialize ser)
{
	ser.Value("point", point);
	ser.Value("point2", point2);
	//	ScriptHandle nID;
	ser.Value("fValue", fValue);
	ser.Value("iValue", iValue);
	ser.Value("iValue2", iValue2);
	string objectNameString(sObjectName);
	ser.Value("sObjectName", objectNameString);

	// (MATT) This change in #149616 will probably break save compatibility for Crysis - however, we may find a way to do without these strings anyway.
	// I believe it's the only part of the change that will actually impact the saves - the rest could be left as-is. {2008/01/14:18:14:18}
	string sStringData1(string1);
	ser.Value("string1", sStringData1);

	string sStringData2(string2);
	ser.Value("string2", sStringData2);

	if (ser.IsReading())
	{
		SetObjectName(objectNameString.c_str());
		string1 = sStringData1.c_str();
		string1 = sStringData2.c_str();
	}
}

AISignalExtraData& AISignalExtraData::operator=(const AISignalExtraData& other)
{
	point = other.point;
	point2 = other.point2;
	nID = other.nID;
	fValue = other.fValue;
	iValue = other.iValue;
	iValue2 = other.iValue2;
	string1 = other.string1;
	string2 = other.string2;
	SetObjectName(other.sObjectName);
	return *this;
};

void AISignalExtraData::ToScriptTable(SmartScriptTable& table) const
{
	CScriptSetGetChain chain(table);
	{
		chain.SetValue("id", nID);
		chain.SetValue("fValue", fValue);
		chain.SetValue("iValue", iValue);
		chain.SetValue("iValue2", iValue2);
		chain.SetValue("string1", string1);
		chain.SetValue("string2", string2);

		if (sObjectName && sObjectName[0])
			chain.SetValue("ObjectName", sObjectName);
		else
			chain.SetToNull("ObjectName");

		Script::SetCachedVector(point, chain, "point");
		Script::SetCachedVector(point2, chain, "point2");
	}
}

void AISignalExtraData::FromScriptTable(const SmartScriptTable& table)
{
	point.zero();
	point2.zero();

	CScriptSetGetChain chain(table);
	{
		chain.GetValue("id", nID);
		chain.GetValue("fValue", fValue);
		chain.GetValue("iValue", iValue);
		chain.GetValue("iValue2", iValue2);
		chain.GetValue("string1", string1);
		chain.GetValue("string2", string2);
		chain.GetValue("point", point);
		chain.GetValue("point2", point2);

		const char* sTableObjectName;
		if (chain.GetValue("ObjectName", sTableObjectName) && sTableObjectName[0])
			SetObjectName(sTableObjectName);
	}
}

//====================================================================
// AISIGNAL Serialize
//====================================================================
void AISIGNAL::Serialize(TSerialize ser)
{
	ser.Value("nSignal", nSignal);

	string textString(strText);
	ser.Value("strText", textString);
	ser.Value("senderID", senderID);

	if (ser.IsReading())
	{
		cry_strcpy(strText, textString.c_str());
		m_nCrcText = CCrc32::Compute(textString);
	}

	if (ser.IsReading())
	{
		if (pEData)
			delete (AISignalExtraData*) pEData;
		pEData = new AISignalExtraData;
	}

	if (pEData)
		pEData->Serialize(ser);
	else
	{
		AISignalExtraData dummy;
		dummy.Serialize(ser);
	}
}
