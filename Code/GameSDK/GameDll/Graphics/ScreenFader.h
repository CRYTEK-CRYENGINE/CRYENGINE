// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 3:06:2009   Created by Benito G.R.
*************************************************************************/

#pragma once

#ifndef SCREEN_FADER_H
#define SCREEN_FADER_H

#include "EngineFacade/Renderer.h"

namespace EngineFacade
{
	struct IEngineFacade;
}

namespace Graphics
{
	class CScreenFader
	{
	public:
		CScreenFader(EngineFacade::IEngineFacade& engineFacade);

		void FadeIn(const string& texturePath, const ColorF& targetColor, float fadeTime, bool useCurrentColor);
		void FadeOut(const string& texturePath, const ColorF& targetColor, float fadeTime, bool useCurrentColor);

		bool IsFadingIn() const;
		bool IsFadingOut() const;

		void Update(float frameTime);
		void Reset();

	protected:

		void Render();
		void SetTexture(const string& textureName);
		ColorF GetDrawColor() const;
		bool ShouldUpdate() const;

	private:

		ColorF         m_currentColor;
		ColorF         m_targetColor;
		
		float          m_fadeTime;
		float          m_currentTime;
		
		bool	m_fadingIn;
		bool	m_fadingOut;					

		EngineFacade::IEngineFacade& m_engineFacade;
		EngineFacade::IEngineTexture::Ptr m_texture;
	};
}

#endif