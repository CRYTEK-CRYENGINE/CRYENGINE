// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "GenerationParam.h"
#include "Noise.h"

/////////////////////////////////////////////////////////////////////////////
// CGenerationParam dialog

CGenerationParam::CGenerationParam(CWnd* pParent /*=NULL*/)
	: CDialog(CGenerationParam::IDD, pParent)
{
	//{{AFX_DATA_INIT(CGenerationParam)
	//}}AFX_DATA_INIT
}

void CGenerationParam::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CGenerationParam)
	DDX_Slider(pDX, IDC_PASSES, m_sldPasses);
	DDX_Slider(pDX, IDC_FREQUENCY, m_sldFrequency);
	DDX_Slider(pDX, IDC_FREQSTEP, m_sldFrequencyStep);
	DDX_Slider(pDX, IDC_FADE, m_sldFade);
	DDX_Slider(pDX, IDC_COVER, m_sldCover);
	DDX_Slider(pDX, IDC_RAND, m_sldRandomBase);
	DDX_Slider(pDX, IDC_SHARPNESS, m_sldSharpness);
	DDX_Slider(pDX, IDC_BLUR, m_sldBlur);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CGenerationParam, CDialog)
//{{AFX_MSG_MAP(CGenerationParam)
ON_WM_HSCROLL()
//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGenerationParam message handlers

BOOL CGenerationParam::OnInitDialog()
{
	////////////////////////////////////////////////////////////////////////
	// Set the ranges for the slider controls
	////////////////////////////////////////////////////////////////////////

	CSliderCtrl ctrlSlider;

	CLogFile::WriteLine("Opening generation parameter dialog...");

	CDialog::OnInitDialog();

	VERIFY(ctrlSlider.Attach(GetDlgItem(IDC_PASSES)->m_hWnd));
	ctrlSlider.SetRange(1, 10, TRUE);
	ctrlSlider.Detach();

	VERIFY(ctrlSlider.Attach(GetDlgItem(IDC_FREQUENCY)->m_hWnd));
	ctrlSlider.SetRange(11, 100, TRUE); // Has to be divided by 10
	ctrlSlider.Detach();

	VERIFY(ctrlSlider.Attach(GetDlgItem(IDC_FREQSTEP)->m_hWnd));
	ctrlSlider.SetRange(10, 25, TRUE); // Has to be divided by 10
	ctrlSlider.Detach();

	VERIFY(ctrlSlider.Attach(GetDlgItem(IDC_FADE)->m_hWnd));
	ctrlSlider.SetRange(1, 20, TRUE); // Has to be divided by 10
	ctrlSlider.Detach();

	VERIFY(ctrlSlider.Attach(GetDlgItem(IDC_COVER)->m_hWnd));
	ctrlSlider.SetRange(0, 255, TRUE);
	ctrlSlider.Detach();

	VERIFY(ctrlSlider.Attach(GetDlgItem(IDC_RAND)->m_hWnd));
	ctrlSlider.SetRange(0, 32, TRUE);
	ctrlSlider.Detach();

	VERIFY(ctrlSlider.Attach(GetDlgItem(IDC_SHARPNESS)->m_hWnd));
	ctrlSlider.SetRange(990, 999, TRUE); // Has to be divided by 1000
	ctrlSlider.Detach();

	VERIFY(ctrlSlider.Attach(GetDlgItem(IDC_BLUR)->m_hWnd));
	ctrlSlider.SetRange(0, 3, TRUE);
	ctrlSlider.Detach();

	UpdateStaticNum();

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CGenerationParam::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	// TODO: Add your message handler code here and/or call default

	UpdateStaticNum();

	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CGenerationParam::UpdateStaticNum()
{
	////////////////////////////////////////////////////////////////////////
	// Update the static number controls with the values from the sliders
	////////////////////////////////////////////////////////////////////////

	char szFloatNum[32];

	VERIFY(UpdateData(TRUE));

	// Passes
	SetDlgItemInt(IDC_PASSES_NUM, m_sldPasses, FALSE);

	// Frequency
	cry_sprintf(szFloatNum, "%f", m_sldFrequency / 10.0f);
	SetDlgItemText(IDC_FREQUENCY_NUM, szFloatNum);

	// Frequency step
	cry_sprintf(szFloatNum, "%f", m_sldFrequencyStep / 10.0f);
	SetDlgItemText(IDC_FREQSTEP_NUM, szFloatNum);

	// Fade
	cry_sprintf(szFloatNum, "%f", m_sldFade / 10.0f);
	SetDlgItemText(IDC_FADE_NUM, szFloatNum);

	// Cover
	SetDlgItemInt(IDC_COVER_NUM, m_sldCover, FALSE);

	// Random base
	SetDlgItemInt(IDC_RAND_NUM, m_sldRandomBase, FALSE);

	// Sharpness
	cry_sprintf(szFloatNum, "%f", m_sldSharpness / 1000.0f);
	SetDlgItemText(IDC_SHARPNESS_NUM, szFloatNum);

	// Blurring
	SetDlgItemInt(IDC_BLUR_NUM, m_sldBlur, FALSE);
}

void CGenerationParam::FillParam(SNoiseParams* pParam)
{
	////////////////////////////////////////////////////////////////////////
	// Fill a SNoiseParams structure with the data from the dialog
	////////////////////////////////////////////////////////////////////////

	pParam->bBlueSky = false;
	pParam->fFade = m_sldFade / 10.0f;
	pParam->fFrequency = m_sldFrequency / 10.0f;
	pParam->fFrequencyStep = m_sldFrequencyStep / 10.0f;
	pParam->iCover = m_sldCover;
	pParam->iHeight = 512;
	pParam->iPasses = m_sldPasses;
	pParam->iRandom = m_sldRandomBase;
	pParam->iSharpness = m_sldSharpness / 1000.0f;
	pParam->iSmoothness = m_sldBlur;
	pParam->iWidth = 512;

	CryLog("Retrieving parameters (fFade: %f, fFreq: %f, fFreqStep: %f) from dialog...",
	                     pParam->fFade, pParam->fFrequency, pParam->fFrequencyStep);
}

void CGenerationParam::LoadParam(SNoiseParams* pParam)
{
	////////////////////////////////////////////////////////////////////////
	// Fill the dialog with the data from the SNoiseParams structure
	////////////////////////////////////////////////////////////////////////

	m_sldFade = (int) (pParam->fFade * 10.0f);
	m_sldFrequency = (int) (pParam->fFrequency * 10.0f);
	m_sldFrequencyStep = (int) (pParam->fFrequencyStep * 10.0f);
	m_sldCover = pParam->iCover;
	m_sldPasses = pParam->iPasses;
	m_sldRandomBase = pParam->iRandom;
	m_sldSharpness = (int) (pParam->iSharpness * 1000.0f);
	m_sldBlur = pParam->iSmoothness;

	CryLog("Loading parameters (fFade: %f, fFreq: %f, fFreqStep: %f) to dialog...",
	                     pParam->fFade, pParam->fFrequency, pParam->fFrequencyStep);
}

