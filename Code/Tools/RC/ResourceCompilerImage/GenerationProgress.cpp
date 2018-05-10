// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

//#include "GenerationProgress.h"
#include "ImageCompiler.h"                  // CImageCompiler
#include "ImageUserDialog.h"                // CImageUserDialog

// ------------------------------------------------------------------

//constructor
CGenerationProgress::CGenerationProgress(CImageCompiler& rImageCompiler) : m_rImageCompiler(rImageCompiler), m_fProgress(0)
{
}

float CGenerationProgress::Get()
{
	return m_fProgress;
}

void CGenerationProgress::Start()
{
	Set(0.0f);
}

void CGenerationProgress::Finish()
{
	Set(1.0f);
}

void CGenerationProgress::Set(float fProgress)
{
	if (m_fProgress!=fProgress && m_rImageCompiler.m_bInternalPreview)
		m_rImageCompiler.m_pImageUserDialog->UpdateProgressBar(fProgress);

	m_fProgress = fProgress;
}

void CGenerationProgress::Increment(float fProgress)
{
	m_fProgress += fProgress;

	if (fProgress>0 && m_rImageCompiler.m_bInternalPreview)
		m_rImageCompiler.m_pImageUserDialog->UpdateProgressBar(m_fProgress);
}

// ------------------------------------------------------------------

//the tuned "magic" numbers for progress increments in the generation phases

//phase 1: 10%
void CGenerationProgress::Phase1()
{
	Increment(0.1f);
}

//phase 2: 25%
void CGenerationProgress::Phase2(const uint32 dwY, const uint32 dwHeight)
{
	if (dwY%8 == 0)
		Increment(0.249f / (dwHeight/8.0f));
}

//phase 3: 45%
void CGenerationProgress::Phase3(const uint32 dwY, const uint32 dwHeight, const int iTemp)
{
	if (dwY%8 == 0)
	{
		assert(iTemp>=0);
		if (iTemp < 3)
		{
			const float increment = (3-iTemp)*(3-iTemp)*(3-iTemp) / 36.0f;
			Increment((0.449f * increment) / (dwHeight/8.0f));
		}
	}
}

//phase 4: 20%
void CGenerationProgress::Phase4(const uint32 dwMip, const int iBlockLines)
{
	if (dwMip < 2)
	{
		const float increment = (3-dwMip)*(3-dwMip)*(3-dwMip) / 36.0f;
		Increment((0.199f * increment)/(float)iBlockLines);
	}
}

