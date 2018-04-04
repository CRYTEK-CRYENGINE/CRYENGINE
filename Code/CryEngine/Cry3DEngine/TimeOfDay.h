// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   TimeOfDay.h
//  Version:     v1.00
//  Created:     25/10/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __TimeOfDay_h__
#define __TimeOfDay_h__

#include <Cry3DEngine/ITimeOfDay.h>
#include <CryAudio/IAudioInterfacesCommonData.h>

class CEnvironmentPreset;
//////////////////////////////////////////////////////////////////////////
// ITimeOfDay interface implementation.
//////////////////////////////////////////////////////////////////////////
class CTimeOfDay : public ITimeOfDay
{
public:
	CTimeOfDay();
	~CTimeOfDay();

	//////////////////////////////////////////////////////////////////////////
	// ITimeOfDay
	//////////////////////////////////////////////////////////////////////////

	virtual int   GetPresetCount() const { return m_presets.size(); }
	virtual bool  GetPresetsInfos(SPresetInfo* resultArray, unsigned int arraySize) const;
	virtual bool  SetCurrentPreset(const char* szPresetName);
	virtual bool  AddNewPreset(const char* szPresetName);
	virtual bool  RemovePreset(const char* szPresetName);
	virtual bool  SavePreset(const char* szPresetName) const;
	virtual bool  LoadPreset(const char* szFilePath);
	virtual void  ResetPreset(const char* szPresetName);

	virtual bool  ImportPreset(const char* szPresetName, const char* szFilePath);
	virtual bool  ExportPreset(const char* szPresetName, const char* szFilePath) const;

	virtual int   GetVariableCount() { return ITimeOfDay::PARAM_TOTAL; };
	virtual bool  GetVariableInfo(int nIndex, SVariableInfo& varInfo);
	virtual void  SetVariableValue(int nIndex, float fValue[3]);

	virtual bool  InterpolateVarInRange(int nIndex, float fMin, float fMax, unsigned int nCount, Vec3* resultArray) const;
	virtual uint  GetSplineKeysCount(int nIndex, int nSpline) const;
	virtual bool  GetSplineKeysForVar(int nIndex, int nSpline, SBezierKey* keysArray, unsigned int keysArraySize) const;
	virtual bool  SetSplineKeysForVar(int nIndex, int nSpline, const SBezierKey* keysArray, unsigned int keysArraySize);
	virtual bool  UpdateSplineKeyForVar(int nIndex, int nSpline, float fTime, float newValue);
	virtual float GetAnimTimeSecondsIn24h();

	virtual void  ResetVariables();

	// Time of day is specified in hours.
	virtual void  SetTime(float fHour, bool bForceUpdate = false);
	virtual void  SetSunPos(float longitude, float latitude);
	virtual float GetSunLatitude()       { return m_sunRotationLatitude; }
	virtual float GetSunLongitude()      { return m_sunRotationLongitude; }
	virtual float GetTime()              { return m_fTime; };

	virtual void  SetPaused(bool paused) { m_bPaused = paused; }

	virtual void  SetAdvancedInfo(const SAdvancedInfo& advInfo);
	virtual void  GetAdvancedInfo(SAdvancedInfo& advInfo);

	float         GetHDRMultiplier() const { return m_fHDRMultiplier; }

	virtual void  Update(bool bInterpolate = true, bool bForceUpdate = false);

	virtual void  Serialize(XmlNodeRef& node, bool bLoading);
	virtual void  Serialize(TSerialize ser);

	virtual void  SetTimer(ITimer* pTimer);

	virtual void  NetSerialize(TSerialize ser, float lag, uint32 flags);

	virtual void  Tick();

	virtual void  SetEnvironmentSettings(const SEnvironmentInfo& envInfo);

	virtual void  SaveInternalState(struct IDataWriteStream& writer);
	virtual void  LoadInternalState(struct IDataReadStream& reader);

	//////////////////////////////////////////////////////////////////////////

	void BeginEditMode() { m_bEditMode = true; };
	void EndEditMode()   { m_bEditMode = false; };

private:
	CTimeOfDay(const CTimeOfDay&);
	CTimeOfDay(const CTimeOfDay&&);
	CTimeOfDay&    operator=(const CTimeOfDay&);
	CTimeOfDay&    operator=(const CTimeOfDay&&);

	SVariableInfo& GetVar(ETimeOfDayParamID id);
	void           UpdateEnvLighting(bool forceUpdate);

private:
	typedef std::map<string, CEnvironmentPreset> TPresetsSet;
	TPresetsSet         m_presets;
	CEnvironmentPreset* m_pCurrentPreset;

	SVariableInfo       m_vars[ITimeOfDay::PARAM_TOTAL];

	float               m_fTime;
	float               m_sunRotationLatitude;
	float               m_sunRotationLongitude;

	bool                m_bEditMode;
	bool                m_bPaused;
	bool                m_bSunLinkedToTOD;

	SAdvancedInfo       m_advancedInfo;
	ITimer*             m_pTimer;
	float               m_fHDRMultiplier;
	ICVar*              m_pTimeOfDaySpeedCVar;
	CryAudio::ControlId m_timeOfDayRtpcId;
};

#endif //__TimeOfDay_h__
