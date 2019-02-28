// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Fades the screen to and from a specific color

-------------------------------------------------------------------------
History:
- 28/04/2015: Created by Niels Stoelinga

*************************************************************************/
#ifndef __FLOW_FADE_NODE_H__
#define __FLOW_FADE_NODE_H__

#pragma once

#define NUM_FADERS 8
#define MFX_FADER_OFFSET 0
#define MFX_FADER_END 3
#define GAME_FADER_OFFSET 4

class CHUDFader
{
public:
	CHUDFader();
	virtual ~CHUDFader();

	const char* GetDebugName() const;
	void Stop();
	void Reset();
	bool IsActive() const { return m_bActive; }
	bool IsPlaying(int ticket) const { return m_ticket > 0 && m_ticket==ticket; }
	ColorF GetCurrentColor();

	int FadeIn(const ColorF& targetColor, float fDuration, bool bUseCurrentColor=true, bool bUpdateAlways=false);
	int FadeOut(const ColorF& targetColor, float fDuration, const char* textureName="", bool bUseCurrentColor=true, bool bUpdateAlways=false, float fTargetAlpha = 1.f, float fSourceAlpha = -1.f);

	virtual void Update(float fDeltaTime);

	virtual void Draw();

	static ITexture* LoadTexture(const char* textureName);

protected:
	void SetTexture(const char* textureName);

protected:
	IRenderer     *m_pRenderer;
	ColorF         m_currentColor;
	ColorF         m_targetColor;
	ColorF         m_drawColor;
	ITexture      *m_pTexture;
	float          m_duration;
	float          m_curTime;
	int            m_direction;
	int            m_ticket;
	bool           m_bActive;
	bool           m_bUpdateAlways;
	float          m_lastTime;
};


class CMasterFader
	: public IGameFrameworkListener
{
public:
	CMasterFader();

	~CMasterFader();

	// IGameFrameworkListener
	virtual void OnPostUpdate(float fDeltaTime) override;

	virtual void OnSaveGame(ISaveGame* pSaveGame) override {}
	virtual void OnLoadGame(ILoadGame* pLoadGame) override {}
	virtual void OnLevelEnd(const char* nextLevel) override {}
	virtual void OnActionEvent(const SActionEvent& event) override;
	// ~IGameFrameworkListener

	CHUDFader* GetHUDFader(int group);

	void Update(float fDeltaTime);
	void OnHUDToBeDestroyed();
	void Serialize(TSerialize ser);

	void Register();
	void UnRegister();

	bool m_bRegistered;
	CHUDFader* m_pHUDFader[NUM_FADERS];
};

#endif //__FLOW_FADE_NODE_H__
