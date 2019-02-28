// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   PerfHUD.h
//  Created:     26/08/2009 by Timur.
//  Description: Button implementation in the MiniGUI
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __PerfHUD_h__
#define __PerfHUD_h__

#include <CrySystem/Profilers/IPerfHud.h>

#ifdef USE_PERFHUD

	#include <CryInput/IInput.h>
	#include "MiniGUI/MiniInfoBox.h"
	#include "MiniGUI/MiniTable.h"

//Macros for console based widget control

	#define SET_WIDGET_DECL(WIDGET_NAME) \
	  static int s_cvar_ ## WIDGET_NAME; \
	  static void Set_ ## WIDGET_NAME ## _Widget(ICVar * pCvar);

	#define SET_WIDGET_DEF(WIDGET_NAME, WIDGET_ID)                     \
	  int CPerfHUD::s_cvar_ ## WIDGET_NAME = 0;                        \
	  void CPerfHUD::Set_ ## WIDGET_NAME ## _Widget(ICVar * pCvar)     \
	  {                                                                \
	    ICryPerfHUD* pPerfHud = gEnv->pSystem->GetPerfHUD();           \
	    if (pPerfHud)                                                  \
	    {                                                              \
	      int val = pCvar->GetIVal();                                  \
	      if (val)                                                     \
	      {                                                            \
	        pPerfHud->SetState(eHudOutOfFocus);                        \
	        pPerfHud->EnableWidget(ICryPerfHUDWidget::WIDGET_ID, val); \
	      }                                                            \
	      else                                                         \
	      {                                                            \
	        pPerfHud->DisableWidget(ICryPerfHUDWidget::WIDGET_ID);     \
	      }                                                            \
	    }                                                              \
	  }

	#define SET_WIDGET_COMMAND(COMMAND_NAME, WIDGET_NAME) \
	  REGISTER_CVAR2_CB(COMMAND_NAME, &s_cvar_ ## WIDGET_NAME, 0, VF_ALWAYSONCHANGE, "", Set_ ## WIDGET_NAME ## _Widget);

//////////////////////////////////////////////////////////////////////////
// Root window all other controls derive from
class CPerfHUD : public ICryPerfHUD, public minigui::IMiniGUIEventListener, public IInputEventListener
{
public:
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(ICryPerfHUD)
	CRYINTERFACE_END()
	CRYGENERATE_SINGLETONCLASS_GUID(CPerfHUD, "PerfHUD", "006945f9-985e-4ce2-8721-20bfdec09ca5"_cry_guid)

	CPerfHUD();
	virtual ~CPerfHUD() {}

public:
	//////////////////////////////////////////////////////////////////////////
	// ICryPerfHUD implementation
	//////////////////////////////////////////////////////////////////////////
	virtual void Init() override;
	virtual void Done() override;
	virtual void Draw() override;
	virtual void LoadBudgets() override;
	virtual void SaveStats(const char* filename) override;
	virtual void ResetWidgets() override;
	virtual void Reset() override;
	virtual void Destroy() override;

	//Set state through code (rather than using joypad input)
	virtual void SetState(EHudState state) override;

	virtual void Show(bool bRestoreState) override;

	virtual void AddWidget(ICryPerfHUDWidget* pWidget) override;
	virtual void RemoveWidget(ICryPerfHUDWidget* pWidget) override;

	//////////////////////////////////////////////////////////////////////////
	// Gui Creation helper funcs
	//////////////////////////////////////////////////////////////////////////
	virtual minigui::IMiniCtrl*    CreateMenu(const char* name, minigui::IMiniCtrl* pParent = NULL) override;
	virtual bool                   CreateCVarMenuItem(minigui::IMiniCtrl* pMenu, const char* name, const char* controlVar, float controlVarOn, float controlVarOff) override;
	virtual bool                   CreateCallbackMenuItem(minigui::IMiniCtrl* pMenu, const char* name, minigui::ClickCallback callback, void* pCallbackData) override;
	virtual minigui::IMiniInfoBox* CreateInfoMenuItem(minigui::IMiniCtrl* pMenu, const char* name, minigui::RenderCallback renderCallback, const minigui::Rect& rect, bool onAtStart = false) override;
	virtual minigui::IMiniTable*   CreateTableMenuItem(minigui::IMiniCtrl* pMenu, const char* name) override;

	virtual minigui::IMiniCtrl*    GetMenu(const char* name) override;

	virtual void                   EnableWidget(ICryPerfHUDWidget::EWidgetID id, int mode) override;
	virtual void                   DisableWidget(ICryPerfHUDWidget::EWidgetID id) override;

	//////////////////////////////////////////////////////////////////////////
	// WARNINGS - Widget Specific Interface
	//////////////////////////////////////////////////////////////////////////
	virtual void AddWarning(float duration, const char* fmt, va_list argList) override;
	virtual bool WarningsWindowEnabled() const override;

	//////////////////////////////////////////////////////////////////////////
	// FPS - Widget Specific Interface
	//////////////////////////////////////////////////////////////////////////
	virtual const std::vector<PerfBucket>* GetFpsBuckets(float& totalTime) const override;

	//////////////////////////////////////////////////////////////////////////
	// IMiniGUIEventListener implementation
	//////////////////////////////////////////////////////////////////////////
	virtual void OnCommand(minigui::SCommand& cmd) override;
	//////////////////////////////////////////////////////////////////////////

	// IInputEventListener
	virtual bool OnInputEvent(const SInputEvent& rInputEvent) override;

	//////////////////////////////////////////////////////////////////////////
	// CLICK CALLBACKS
	//////////////////////////////////////////////////////////////////////////

	static void ResetCallback(void* data, bool status);
	static void ReloadBudgetsCallback(void* data, bool status);
	static void SaveStatsCallback(void* data, bool status);

	//////////////////////////////////////////////////////////////////////////
	// RENDER CALLBACKS
	//////////////////////////////////////////////////////////////////////////

	static void DisplayRenderInfoCallback(const minigui::Rect& rect);

	//////////////////////////////////////////////////////////////////////////
	// CVAR CALLBACK
	//////////////////////////////////////////////////////////////////////////

	static void CVarChangeCallback(ICVar* pCvar);

	SET_WIDGET_DECL(Warnings);
	SET_WIDGET_DECL(RenderSummary);
	SET_WIDGET_DECL(RenderBatchStats);
	SET_WIDGET_DECL(FpsBuckets);
	SET_WIDGET_DECL(Particles);
	SET_WIDGET_DECL(PakFile);

	//////////////////////////////////////////////////////////////////////////
	// Static Data
	//////////////////////////////////////////////////////////////////////////

	static const float  OVERSCAN_X;
	static const float  OVERSCAN_Y;

	static const ColorB COL_NORM;
	static const ColorB COL_WARN;
	static const ColorB COL_ERROR;

	static const float  TEXT_SIZE_NORM;
	static const float  TEXT_SIZE_WARN;
	static const float  TEXT_SIZE_ERROR;

	static const float  ACTIVATE_TIME_FROM_GAME;
	static const float  ACTIVATE_TIME_FROM_HUD;

protected:
	void InitUI(minigui::IMiniGUI* pGUI);
	void SetNextState();

protected:

	static int m_sys_perfhud;
	static int m_sys_perfhud_pause;

	int        m_sys_perfhud_prev;

	//record last menu position
	float     m_menuStartX;
	float     m_menuStartY;

	bool      m_hudCreated;

	bool      m_L1Pressed;
	bool      m_L2Pressed;
	bool      m_R1Pressed;
	bool      m_R2Pressed;
	bool      m_changingState;
	bool      m_hwMouseEnabled;

	float     m_triggersDownStartTime;

	EHudState m_hudState;
	EHudState m_hudLastState;

	typedef std::vector<_smart_ptr<ICryPerfHUDWidget>, stl::STLGlobalAllocator<_smart_ptr<ICryPerfHUDWidget>>>::iterator TWidgetIterator;

	std::vector<_smart_ptr<ICryPerfHUDWidget>, stl::STLGlobalAllocator<_smart_ptr<ICryPerfHUDWidget>>> m_widgets;
	std::vector<minigui::IMiniCtrl*, stl::STLGlobalAllocator<minigui::IMiniCtrl*>>                     m_rootMenus;
};

class CFpsWidget : public ICryPerfHUDWidget
{
public:

	CFpsWidget(minigui::IMiniCtrl* pParentMenu, ICryPerfHUD* pPerfHud);

	virtual void                                Reset();
	virtual void                                Update();
	virtual bool                                ShouldUpdate();
	virtual void                                LoadBudgets(XmlNodeRef perfXML);
	virtual void                                SaveStats(XmlNodeRef statsXML);
	virtual void                                Enable(int mode) { m_pInfoBox->Hide(false); }
	virtual void                                Disable()        { m_pInfoBox->Hide(true); }

	void                                        Init();

	const std::vector<ICryPerfHUD::PerfBucket>* GetFpsBuckets(float& totalTime) const;

	static void                                 ResetCallback(void* data, bool status);

protected:

	static const uint32 NUM_FPS_BUCKETS_DEFAULT = 6;

	struct PerfBucketsStat
	{
		std::vector<ICryPerfHUD::PerfBucket> buckets;
		float                                totalTime;
	};

	template<bool LESS_THAN>
	void UpdateBuckets(PerfBucketsStat& buckets, float frameTime, const char* name, float stat);

	// Data
	static int m_cvarPerfHudFpsExclusive;

	enum EPerfBucketType
	{
		BUCKET_FPS = 0,
		BUCKET_GPU,
		BUCKET_DP,
		BUCKET_TYPE_NUM
	};

	PerfBucketsStat        m_perfBuckets[BUCKET_TYPE_NUM];

	float                  m_fpsBucketSize;
	float                  m_fpsBudget;
	float                  m_dpBudget;
	float                  m_dpBucketSize;

	minigui::IMiniInfoBox* m_pInfoBox;
};

class CRenderStatsWidget : public ICryPerfHUDWidget
{
public:

	CRenderStatsWidget(minigui::IMiniCtrl* pParentMenu, ICryPerfHUD* pPerfHud);

	virtual void Reset() {}
	virtual void Update();
	virtual bool ShouldUpdate();
	virtual void LoadBudgets(XmlNodeRef perfXML);
	virtual void SaveStats(XmlNodeRef statsXML);
	virtual void Enable(int mode) { m_pInfoBox->Hide(false); }
	virtual void Disable()        { m_pInfoBox->Hide(true); }

protected:

	//budgets
	float  m_fpsBudget;
	uint32 m_dpBudget;
	uint32 m_polyBudget;
	uint32 m_postEffectBudget;
	uint32 m_shadowCastBudget;
	uint32 m_particlesBudget;

	//runtime data
	struct SRuntimeData
	{
		Vec3   cameraPos;
		Ang3   cameraRot;
		float  fps;
		uint32 nDrawPrims;
		uint32 nPolys;
		uint32 nPostEffects;
		uint32 nFwdLights;
		uint32 nFwdShadowLights;
		uint32 nDefLights;
		uint32 nDefShadowLights;
		uint32 nDefCubeMaps;
		int    nParticles;
		bool   hdrEnabled;
		bool   renderThreadEnabled;
	};

	SRuntimeData           m_runtimeData;
	minigui::IMiniInfoBox* m_pInfoBox;
	ICryPerfHUD*           m_pPerfHUD;
	uint32                 m_buildNum;
};

class CStreamingStatsWidget : public ICryPerfHUDWidget
{
public:

	CStreamingStatsWidget(minigui::IMiniCtrl* pParentMenu, ICryPerfHUD* pPerfHud);

	virtual void Reset() {}
	virtual void Update();
	virtual bool ShouldUpdate();
	virtual void LoadBudgets(XmlNodeRef perfXML);
	virtual void SaveStats(XmlNodeRef statsXML) {};
	virtual void Enable(int mode)               { m_pInfoBox->Hide(false); }
	virtual void Disable()                      { m_pInfoBox->Hide(true); }

protected:
	//float m_maxMeshSizeArroundMB;
	//float m_maxTextureSizeArroundMB;
	minigui::IMiniInfoBox* m_pInfoBox;
	ICryPerfHUD*           m_pPerfHUD;
};

class CWarningsWidget : public ICryPerfHUDWidget
{
public:

	CWarningsWidget(minigui::IMiniCtrl* pParentMenu, ICryPerfHUD* pPerfHud);

	virtual void Reset();
	virtual void Update();
	virtual bool ShouldUpdate();
	virtual void LoadBudgets(XmlNodeRef perfXML) {};
	virtual void SaveStats(XmlNodeRef statsXML);
	virtual void Enable(int mode)                { m_pInfoBox->Hide(false); }
	virtual void Disable()                       { m_pInfoBox->Hide(true); }

	void         AddWarningV(float duration, const char* fmt, va_list argList);
	void         AddWarning(float duration, const char* warning);

protected:

	static const uint32 WARNING_LENGTH = 64;

	struct SWarning
	{
		char  text[WARNING_LENGTH];
		float remainingDuration;
	};

	minigui::IMiniInfoBox* m_pInfoBox;

	typedef std::vector<SWarning> TSWarnings;

	TSWarnings             m_warnings;

	CryMT::queue<SWarning> m_threadWarnings;

	threadID               m_nMainThreadId;
};

class CRenderBatchWidget : public ICryPerfHUDWidget
{
public:

	CRenderBatchWidget(minigui::IMiniCtrl* pParentMenu, ICryPerfHUD* pPerfHud);

	virtual void Reset();
	virtual void Update();
	virtual bool ShouldUpdate();
	virtual void LoadBudgets(XmlNodeRef perfXML) {};
	virtual void SaveStats(XmlNodeRef statsXML);
	virtual void Enable(int mode);
	virtual void Disable();

	void         Update_ModeGpuTimes();
	void         Update_ModeBatchStats();

protected:

	struct BatchInfoGpuTimes
	{
		const char* name;
		uint32      nBatches;
		float       gpuTime;
		int         numVerts;
		int         numIndices;
	};

	struct BatchInfoSortGpuTimes
	{
		inline bool operator()(const BatchInfoGpuTimes& lhs, const BatchInfoGpuTimes& rhs) const
		{
			return lhs.gpuTime > rhs.gpuTime;
		}
	};

	struct BatchInfoPerPass
	{
		const char* name;
		uint16      nBatches;
		uint16      nInstances;
		uint16      nZpass;
		uint16      nShadows;
		uint16      nGeneral;
		uint16      nTransparent;
		uint16      nMisc;
		ColorB      col;

		BatchInfoPerPass()
		{
			Reset();
		}

		void Reset()
		{
			name = NULL;
			nBatches = 0;
			nInstances = 0;
			nZpass = 0;
			nShadows = 0;
			nGeneral = 0;
			nTransparent = 0;
			nMisc = 0;
			col.set(255, 255, 255, 255);
		}

		void operator+=(const BatchInfoPerPass& rhs)
		{
			nBatches += rhs.nBatches;
			nInstances += rhs.nInstances;
			nZpass += rhs.nZpass;
			nShadows += rhs.nShadows;
			nGeneral += rhs.nGeneral;
			nTransparent += rhs.nTransparent;
			nMisc += rhs.nMisc;
		}
	};

	struct BatchInfoSortPerPass
	{
		inline bool operator()(const BatchInfoPerPass* lhs, const BatchInfoPerPass* rhs) const
		{
			return lhs->nBatches > rhs->nBatches;
		}
	};

	enum EDisplayMode
	{
		DISPLAY_MODE_NONE = 0,
		DISPLAY_MODE_BATCH_STATS,
		DISPLAY_MODE_GPU_TIMES,
		DISPLAY_MODE_NUM,
	};

	minigui::IMiniTable* m_pTable;
	ICVar*               m_pRStatsCVar;
	EDisplayMode         m_displayMode;
};

#endif //USE_PERFHUD

#endif // __PerfHUD_h__
