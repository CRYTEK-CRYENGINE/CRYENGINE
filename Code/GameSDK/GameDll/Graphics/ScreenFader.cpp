// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 3:06:2009   Created by Benito G.R.
*************************************************************************/

#include "StdAfx.h"
#include "ScreenFader.h"

#include "EngineFacade/3DEngine.h"
#include "EngineFacade/EngineFacade.h"

namespace Graphics
{
	CScreenFader::CScreenFader(EngineFacade::IEngineFacade& engineFacade)
		:m_engineFacade(engineFacade)
	{
		Reset();
	}

	void CScreenFader::Reset()
	{
		m_currentColor.Set(0.0f, 0.0f, 0.0f, 0.0f);
		m_targetColor.Set(0.0f, 0.0f, 0.0f, 0.0f);
		m_fadingIn = false;
		m_fadingOut = false;
		m_currentTime = m_fadeTime = 0.0f;
		m_texture.reset(new EngineFacade::CDummyEngineTexture());
	}

	ColorF CScreenFader::GetDrawColor() const
	{
		float blend = (m_fadeTime > 0.0f) ? CLAMP(m_currentTime/ m_fadeTime, 0.0f, 1.0f) : 1.0f;

		ColorF col;
		col.lerpFloat(m_currentColor, m_targetColor, blend);
		return col;
	}

	void CScreenFader::FadeIn(const string& texturePath, const ColorF& targetColor, float fadeTime, bool useCurrentColor)
	{
		if (useCurrentColor)
			m_currentColor = GetDrawColor();
		else
			m_currentColor.Set(targetColor.r, targetColor.g, targetColor.b, 1.0f);

		m_targetColor.Set(targetColor.r, targetColor.g, targetColor.b, 0.0f);

		m_fadeTime = fadeTime > 0.0f ? fadeTime : 0.0f;
		m_currentTime = 0.0f;
		m_fadingIn = true;
		m_fadingOut = false;

		SetTexture(texturePath);
	}

	void CScreenFader::FadeOut(const string& texturePath, const ColorF& targetColor, float fadeTime, bool useCurrentColor)
	{
		if (useCurrentColor)
			m_currentColor = GetDrawColor();
		else
			m_currentColor.Set(targetColor.r, targetColor.g, targetColor.b, 0.0f);

		m_targetColor.Set(targetColor.r, targetColor.g, targetColor.b, 1.0f);

		m_fadeTime = fadeTime > 0.0f ? fadeTime : 0.0f;
		m_currentTime = 0.0f;
		m_fadingIn = false;
		m_fadingOut = true;

		SetTexture(texturePath);
	}

	bool CScreenFader::IsFadingIn() const
	{
		return m_fadingIn;
	}

	bool CScreenFader::IsFadingOut() const
	{
		return m_fadingOut;
	}

	bool CScreenFader::ShouldUpdate() const
	{
		return (m_fadingOut) || (m_currentTime < m_fadeTime);
	}

	void CScreenFader::Update(float frameTime)
	{
		if (ShouldUpdate() == false)
			return;

		m_currentTime += frameTime;

		Render();
	}

	void CScreenFader::Render()
	{
		m_engineFacade.GetEngineRenderer().DrawFullScreenImage(m_texture->GetID(), GetDrawColor());
	}

	void CScreenFader::SetTexture(const string& textureName)
	{
		if (strcmp(textureName, m_texture->GetName()) == 0)
			return;

		m_texture.reset(m_engineFacade.GetEngineRenderer().LoadTexture(textureName, FT_DONT_ANISO, eTT_2D));
		m_texture->Clamp();
	}
}
