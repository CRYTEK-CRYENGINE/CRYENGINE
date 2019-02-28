// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Basic encapsulation of a 2D Hud Progress bar
	-------------------------------------------------------------------------
	History:
	- 21:07:2011  : Created by Jonathan Bunner

*************************************************************************/


#ifndef _PROGRESSBAR_H_
#define _PROGRESSBAR_H_

#if _MSC_VER > 1000
# pragma once
#endif

// Helper struct
struct ProgressBarParams
{
	// Constructor
	ProgressBarParams()
	{
		// Set some arbitrary defaults
		m_normalisedScreenPosition= Vec2(0.f,0.f);  
		m_filledBarColor					= ColorB(255,255,255,255);
		m_emptyBarColor						= ColorB(127,127,127,255);
		m_backerColor							= ColorB(0,0,0,255);
		m_width										= 1.0f;
		m_height									= 1.0f;
		m_zeroToOneProgressValue  = 0.5f;
	}

	// Data
	CryFixedStringT<64> m_text;
	Vec2			m_normalisedScreenPosition; 
	ColorB		m_filledBarColor;
	ColorB		m_backerColor;
	ColorB		m_emptyBarColor;
	ColorF		m_textColor;
	float			m_width;
	float			m_height; 
	float			m_zeroToOneProgressValue; 
};

// Main class decl
class CProgressBar
{

public:

	CProgressBar(); 
	virtual ~CProgressBar(); 

	// Accessors

	// Set all params in one go
	void Init(const ProgressBarParams& params); 

	// Set the value of the progress bar
	void SetProgressValue(float zeroToOneProgress); 

	// Set the value of the progress colour
	void SetFilledBarColour(const ColorB& col); 

	// Set the value of the progress colour
	void SetEmptyBarColour(const ColorB& col); 

	// Set the backer colour
	void SetBackerColour(ColorB& col); 

	// Set the progress bar position
	void SetPosition(const Vec2& pos); 

	// Sets text to be displayed over the top of the bar
	void SetText(const char *inText, const ColorF &col);

	// render the progress bar at its current position (Top left)
	void Render();

protected:
	// Parameters
	ProgressBarParams	m_params;

private:


};

#endif // #ifndef _PROGRESSBAR_H_