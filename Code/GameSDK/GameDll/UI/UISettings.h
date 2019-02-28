// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   UISettings.h
//  Version:     v1.00
//  Created:     10/8/2011 by Paul Reindell.
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __UISettings_H__
#define __UISettings_H__

#include "IUIGameEventSystem.h"
#include <CrySystem/Scaleform/IFlashUI.h>
#include <CryGame/IGameFramework.h>
#include <CryAudio/IAudioSystem.h>
#include <CryCore/SFunctor.h>

struct SNullCVar : public ICVar
{
	virtual ~SNullCVar() {}
	virtual void Release() override { return; }
	virtual int GetIVal() const override { return 0; }
	virtual int64 GetI64Val() const override { return 0; }
	virtual float GetFVal() const override { return 0; }
	virtual const char *GetString() const override { return "NULL"; }
#if defined( DEDICATED_SERVER )
	virtual void SetDataProbeString(const char* pDataProbeString) override { return; }
#endif
	virtual const char *GetDataProbeString() const override { return "NULL"; }
	virtual void Set(const char* s) override { return; }
	virtual void ForceSet(const char* s) override { return; }
	virtual void Set(const float f) override { return; }
	virtual void Set(const int i) override { return; }
	virtual void ClearFlags (int flags) override { return; }
	virtual int GetFlags() const override { return 0; }
	virtual int SetFlags(int flags) override { return 0; }
	virtual int GetType() override { return 0; }
	virtual const char* GetName() const override { return "NULL"; }
	virtual const char* GetHelp() override { return "NULL"; }
	virtual bool IsConstCVar() const override { return 0; }

	virtual void SetOnChangeCallback(ConsoleVarFunc pChangeFunc) override { return; }
	virtual uint64 AddOnChangeFunctor(const SFunctor& pChangeFunctor) override { return 0; }
	virtual bool RemoveOnChangeFunctor(const uint64 nElement) override { return true; }
	virtual uint64 GetNumberOfOnChangeFunctors() const override { return 0; }
	virtual const SFunctor& GetOnChangeFunctor(uint64 nFunctorIndex) const override { static SFunctor oDummy; return oDummy; }
	virtual ConsoleVarFunc GetOnChangeCallback() const override { return nullptr; }

	virtual void GetMemoryUsage( class ICrySizer* pSizer ) const override { return; }
	virtual int GetRealIVal() const override { return 0; }
	virtual void DebugLog(const int iExpectedValue, const EConsoleLogMode mode) const override {}

	static SNullCVar* Get()
	{
		static SNullCVar inst;
		return &inst;
	}

private:
	SNullCVar() {}
};

class CUISettings
	: public IUIGameEventSystem
	, public IUIModule
{
public:
	CUISettings();

	// IUIGameEventSystem
	UIEVENTSYSTEM( "UISettings" );
	virtual void InitEventSystem() override;
	virtual void UnloadEventSystem() override;

	//IUIModule
	virtual void Init() override;
	virtual void Update(float fDelta);

private:
	// ui functions
	void SendResolutions();
	void SendGraphicSettingsChange();
	void SendSoundSettingsChange();
	void SendGameSettingsChange();

	// ui events
	void OnSetGraphicSettings( int resIndex, int graphicsQuality, bool fullscreen );
	void OnSetResolution( int resX, int resY, bool fullscreen );
	void OnSetSoundSettings( float music, float sfx, float video );
	void OnSetGameSettings( float sensitivity, bool invertMouse, bool invertController );

	void OnGetResolutions();
	void OnGetCurrGraphicsSettings();
	void OnGetCurrSoundSettings();
	void OnGetCurrGameSettings();

	void OnGetLevels( string levelPathFilter );

	void OnLogoutUser();

private:
	enum EUIEvent
	{
		eUIE_GraphicSettingsChanged,
		eUIE_SoundSettingsChanged,
		eUIE_GameSettingsChanged,

		eUIE_OnGetResolutions,
		eUIE_OnGetResolutionItems,
		eUIE_OnGetLevelItems,
	};

	SUIEventReceiverDispatcher<CUISettings> m_eventDispatcher;
	SUIEventSenderDispatcher<EUIEvent> m_eventSender;
	IUIEventSystem* m_pUIEvents;
	IUIEventSystem* m_pUIFunctions;

	ICVar* m_pRXVar;
	ICVar* m_pRYVar;
 	ICVar* m_pFSVar;
	ICVar* m_pGQVar;


	CryAudio::ControlId m_musicVolumeId;
	CryAudio::ControlId m_sfxVolumeId;

	ICVar* m_pVideoVar;

	ICVar* m_pMouseSensitivity;
	ICVar* m_pInvertMouse;
	ICVar* m_pInvertController;

	int m_currResId;

	typedef std::vector< std::pair<int,int> > TResolutions;
	TResolutions m_Resolutions;
};


#endif // __UISettings_H__
