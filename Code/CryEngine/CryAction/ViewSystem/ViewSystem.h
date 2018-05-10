// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "View.h"
#include <CryMovie/IMovieSystem.h>
#include <ILevelSystem.h>

class CViewSystem : public IViewSystem, public IMovieUser, public ILevelSystemListener
{

private:

	using TViewMap = std::map<unsigned int, CView*>;
	using TViewIdVector = std::vector<unsigned int>;

public:

	//IViewSystem
	virtual IView* CreateView() override;
	virtual void   RemoveView(IView* pView) override;
	virtual void   RemoveView(unsigned int viewId) override;

	virtual void   SetActiveView(IView* pView) override;
	virtual void   SetActiveView(unsigned int viewId) override;

	//utility functions
	virtual IView*       GetView(unsigned int viewId) const override;
	virtual IView*       GetActiveView() const override;
	virtual bool         IsClientActorViewActive() const override;

	virtual unsigned int GetViewId(IView* pView) const override;
	virtual unsigned int GetActiveViewId() const override;

	virtual void         Serialize(TSerialize ser) override;
	virtual void         PostSerialize() override;

	virtual IView*       GetViewByEntityId(EntityId id, bool forceCreate) override;

	virtual float        GetDefaultZNear() const override { return m_fDefaultCameraNearZ; };
	virtual void         SetBlendParams(float fBlendPosSpeed, float fBlendRotSpeed, bool performBlendOut) override
	{
		m_fBlendInPosSpeed = fBlendPosSpeed;
		m_fBlendInRotSpeed = fBlendRotSpeed;
		m_bPerformBlendOut = performBlendOut;
	};
	virtual void SetOverrideCameraRotation(bool bOverride, Quat rotation) override;
	virtual bool IsPlayingCutScene() const override                         { return m_cutsceneCount > 0; }

	virtual void SetDeferredViewSystemUpdate(bool const bDeferred) override { m_useDeferredViewSystemUpdate = bDeferred; }
	virtual bool UseDeferredViewSystemUpdate() const override               { return m_useDeferredViewSystemUpdate; }
	virtual void SetControlAudioListeners(bool const bActive) override;
	virtual void UpdateAudioListeners() override;
	//~IViewSystem

	//IMovieUser
	virtual void SetActiveCamera(const SCameraParams& Params) override;
	virtual void BeginCutScene(IAnimSequence* pSeq, unsigned long dwFlags, bool bResetFX) override;
	virtual void EndCutScene(IAnimSequence* pSeq, unsigned long dwFlags) override;
	virtual void SendGlobalEvent(const char* pszEvent) override;
	//~IMovieUser

	// ILevelSystemListener
	virtual void OnLevelNotFound(const char* levelName) override                    {}
	virtual void OnLoadingStart(ILevelInfo* pLevel) override;
	virtual void OnLoadingLevelEntitiesStart(ILevelInfo* pLevel) override           {}
	virtual void OnLoadingComplete(ILevelInfo* pLevel) override                     {}
	virtual void OnLoadingError(ILevelInfo* pLevel, const char* error) override     {}
	virtual void OnLoadingProgress(ILevelInfo* pLevel, int progressAmount) override {}
	virtual void OnUnloadComplete(ILevelInfo* pLevel) override;
	//~ILevelSystemListener

	explicit CViewSystem(ISystem* const pSystem);
	virtual ~CViewSystem() override;

	void Release() { delete this; };
	void Update(float frameTime);

	//void RegisterViewClass(const char *name, IView *(*func)());

	bool AddListener(IViewSystemListener* pListener) override
	{
		return stl::push_back_unique(m_listeners, pListener);
	}

	bool RemoveListener(IViewSystemListener* pListener) override
	{
		return stl::find_and_erase(m_listeners, pListener);
	}

	void GetMemoryUsage(ICrySizer* s) const;

	void ClearAllViews();

	bool ShouldApplyHmdOffset() const { return m_bApplyHmdOffset != 0; }

private:

	void RemoveViewById(unsigned int viewId);
	void ClearCutsceneViews();
	void DebugDraw();

	ISystem* const m_pSystem;

	//TViewClassMap	m_viewClasses;
	TViewMap      m_views;
	TViewIdVector m_cutsceneViewIdVector;

	// Listeners
	std::vector<IViewSystemListener*> m_listeners;

	unsigned int                      m_activeViewId;
	unsigned int                      m_nextViewIdToAssign; // next id which will be assigned
	unsigned int                      m_preSequenceViewId;  // viewId before a movie cam dropped in

	unsigned int                      m_cutsceneViewId;
	unsigned int                      m_cutsceneCount;

	bool                              m_bActiveViewFromSequence;

	bool                              m_bOverridenCameraRotation;
	Quat                              m_overridenCameraRotation;
	float                             m_fCameraNoise;
	float                             m_fCameraNoiseFrequency;

	float                             m_fDefaultCameraNearZ;
	float                             m_fBlendInPosSpeed;
	float                             m_fBlendInRotSpeed;
	bool                              m_bPerformBlendOut;
	int                               m_nViewSystemDebug;

	int                               m_bApplyHmdOffset;

	bool                              m_useDeferredViewSystemUpdate;
};
