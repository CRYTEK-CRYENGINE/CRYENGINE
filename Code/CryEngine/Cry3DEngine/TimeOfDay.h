// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EnvironmentPreset.h"
#include <Cry3DEngine/ITimeOfDay.h>

class CTimeOfDay : public ITimeOfDay
{
public:
	CTimeOfDay();

	// ITimeOfDay
	virtual int         GetPresetCount() const override { return m_presets.size(); }
	virtual bool        GetPresetsInfos(SPresetInfo* resultArray, unsigned int arraySize) const override;
	virtual bool        SetCurrentPreset(const char* szPresetName) override;
	virtual const char* GetCurrentPresetName() const override;
	virtual IPreset&    GetCurrentPreset() override;
	virtual bool        SetDefaultPreset(const char* szPresetName) override;
	virtual const char* GetDefaultPresetName() const override;
	virtual bool        AddNewPreset(const char* szPresetName) override;
	virtual bool        RemovePreset(const char* szPresetName) override;
	virtual bool        SavePreset(const char* szPresetName) const override;
	virtual bool        LoadPreset(const char* szFilePath) override;
	virtual bool        ResetPreset(const char* szPresetName) override;
	virtual void        DiscardPresetChanges(const char* szPresetName) override;

	virtual bool        ImportPreset(const char* szPresetName, const char* szFilePath) override;
	virtual bool        ExportPreset(const char* szPresetName, const char* szFilePath) const override;

	virtual bool        PreviewPreset(const char* szPresetName) override;

	virtual float       GetAnimTimeSecondsIn24h() const override;

	// Time of day is specified in hours.
	virtual void  SetTime(float fHour, bool bForceUpdate = false) override;
	virtual float GetTime() const override        { return m_fTime; }

	virtual void  SetPaused(bool paused) override { m_bPaused = paused; }

	virtual void  SetAdvancedInfo(const SAdvancedInfo& advInfo) override;
	virtual void  GetAdvancedInfo(SAdvancedInfo& advInfo) const override;

	virtual void  Update(bool bInterpolate = true, bool bForceUpdate = false) override;
	virtual void  ConstantsChanged() override;

	virtual void  Serialize(XmlNodeRef& node, bool bLoading) override;
	virtual void  Serialize(TSerialize ser) override;

	virtual void  SetTimer(ITimer* pTimer) override;

	virtual void  NetSerialize(TSerialize ser, float lag, uint32 flags) override;

	virtual void  Tick() override;

	virtual void  SaveInternalState(struct IDataWriteStream& writer) override;
	virtual void  LoadInternalState(struct IDataReadStream& reader) override;

	//////////////////////////////////////////////////////////////////////////
	float GetHDRMultiplier() const { return m_fHDRMultiplier; }

	void  BeginEditMode() override { m_bEditMode = true; }
	void  EndEditMode() override   { m_bEditMode = false; }

	void  Reset();

protected:
	virtual bool RegisterListenerImpl(IListener* const pListener, const char* const szDbgName, const bool staticName) override;
	virtual void UnRegisterListenerImpl(IListener* const pListener) override;

private:
	CTimeOfDay(const CTimeOfDay&);
	CTimeOfDay(const CTimeOfDay&&);
	CTimeOfDay&         operator=(const CTimeOfDay&);
	CTimeOfDay&         operator=(const CTimeOfDay&&);

	CEnvironmentPreset& GetPreset() const;
	const Vec3          GetValue(ETimeOfDayParamID id) const;
	void                SetValue(ETimeOfDayParamID id, const Vec3& newValue);
	void                UpdateEnvLighting(bool forceUpdate);
	void                NotifyOnChange(const IListener::EChangeType changeType, const char* const szPresetName);

	// The bool indicates whether the creation took place. True if a new preset has been created, false if it already exists.
	std::pair<CEnvironmentPreset*, bool> GetOrCreatePreset(const string& presetName);

private:
	typedef std::map<string, std::unique_ptr<CEnvironmentPreset>, stl::less_stricmp<string>> TPresetsSet;
	typedef CListenerSet<IListener*>                                                         TListenerSet;

private:
	TPresetsSet         m_presets;
	TPresetsSet         m_previewPresets;
	CEnvironmentPreset* m_pCurrentPreset;
	string              m_currentPresetName;
	string              m_defaultPresetName;

	float               m_fTime;

	bool                m_bEditMode;
	bool                m_bPaused;

	SAdvancedInfo       m_advancedInfo;
	ITimer*             m_pTimer;
	float               m_fHDRMultiplier;
	ICVar*              m_pTimeOfDaySpeedCVar;
	CryAudio::ControlId m_timeOfDayRtpcId;
	TListenerSet        m_listeners;
};
