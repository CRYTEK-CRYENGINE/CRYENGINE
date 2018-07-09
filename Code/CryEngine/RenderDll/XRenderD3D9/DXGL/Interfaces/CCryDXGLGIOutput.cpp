// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CCryDXGLGIOutput.cpp
//  Version:     v1.00
//  Created:     20/02/2013 by Valerio Guagliumi.
//  Description: Definition of the DXGL wrapper for IDXGIOutput
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "CCryDXGLGIOutput.hpp"
#include "../Implementation/GLDevice.hpp"
#include <CryString/UnicodeFunctions.h>

CCryDXGLGIOutput::CCryDXGLGIOutput(NCryOpenGL::SOutput* pGLOutput)
	: m_spGLOutput(pGLOutput)
{
	DXGL_INITIALIZE_INTERFACE(DXGIOutput)
}

CCryDXGLGIOutput::~CCryDXGLGIOutput()
{
}

bool CCryDXGLGIOutput::Initialize()
{
	ZeroMemory(&m_kDesc, sizeof(m_kDesc));

	Unicode::Convert(m_kDesc.DeviceName, m_spGLOutput->m_strDeviceName);

	if (m_spGLOutput->m_kModes.empty())
	{
		DXGL_ERROR("GL Output has no display modes");
		return false;
	}

	m_kDisplayModes.resize(m_spGLOutput->m_kModes.size());

	std::vector<NCryOpenGL::SDisplayMode>::const_iterator kGLModeIter(m_spGLOutput->m_kModes.begin());
	const std::vector<NCryOpenGL::SDisplayMode>::const_iterator kGLModeEnd(m_spGLOutput->m_kModes.end());
	std::vector<DXGI_MODE_DESC>::iterator kModeIter;
	for (kModeIter = m_kDisplayModes.begin(); kGLModeIter != kGLModeEnd; ++kGLModeIter, ++kModeIter)
	{
		NCryOpenGL::GetDXGIModeDesc(&*kModeIter, *kGLModeIter);
	}

	return true;
}

NCryOpenGL::SOutput* CCryDXGLGIOutput::GetGLOutput()
{
	return m_spGLOutput;
}

////////////////////////////////////////////////////////////////////////////////
// IDXGIOutput implementation
////////////////////////////////////////////////////////////////////////////////

HRESULT CCryDXGLGIOutput::GetDesc(DXGI_OUTPUT_DESC* pDesc)
{
	*pDesc = m_kDesc;
	return S_OK;
}

HRESULT CCryDXGLGIOutput::GetDisplayModeList(DXGI_FORMAT EnumFormat, UINT Flags, UINT* pNumModes, DXGI_MODE_DESC* pDesc)
{
	DXGL_TODO("Take into account Flags as well (for filtering scaled/interlaced modes) if required")
	Flags;

	DXGI_MODE_DESC* pDescEnd(pDesc == NULL ? NULL : pDesc + *pNumModes);
	std::vector<DXGI_MODE_DESC>::const_iterator kModeIter(m_kDisplayModes.begin());
	const std::vector<DXGI_MODE_DESC>::const_iterator kModeEnd(m_kDisplayModes.end());
	for (; kModeIter != kModeEnd; ++kModeIter)
	{
		if (EnumFormat == DXGI_FORMAT_UNKNOWN || EnumFormat == kModeIter->Format)
		{
			if (pDesc == NULL)
				++(*pNumModes);
			else if (pDesc < pDescEnd)
				*(pDesc++) = *kModeIter;
			else
				return DXGI_ERROR_MORE_DATA;
		}
	}

	return S_OK;
}

HRESULT CCryDXGLGIOutput::FindClosestMatchingMode(const DXGI_MODE_DESC* pModeToMatch, DXGI_MODE_DESC* pClosestMatch, IUnknown* pConcernedDevice)
{
	struct SRank
	{
		uint32 m_uOrdering;
		uint32 m_uScaling;
		uint32 m_uFormat;
		uint32 m_uResolution;
		uint32 m_uRefreshRate;

		SRank(uint32 uDefaultValue)
			: m_uOrdering(uDefaultValue)
			, m_uScaling(uDefaultValue)
			, m_uFormat(uDefaultValue)
			, m_uResolution(uDefaultValue)
			, m_uRefreshRate(uDefaultValue)
		{
		}

		SRank(const DXGI_MODE_DESC& kDesc, const DXGI_MODE_DESC& kReference)
		{
			m_uOrdering = MatchOrdering(kDesc, kReference);
			m_uScaling = MatchScaling(kDesc, kReference);
			m_uFormat = kDesc.Format == kReference.Format;
			m_uResolution = abs((int32)kDesc.Width * (int32)kDesc.Height - (int32)kReference.Width * (int32)kReference.Height);
			m_uRefreshRate = (uint32)abs((float)kDesc.RefreshRate.Numerator / (float)kDesc.RefreshRate.Denominator - (float)kReference.RefreshRate.Numerator / (float)kReference.RefreshRate.Denominator);
		}

		static bool MatchScaling(const DXGI_MODE_DESC& kDesc, const DXGI_MODE_DESC& kReference)
		{
			return kReference.Scaling != DXGI_MODE_SCALING_UNSPECIFIED && kReference.Scaling != kDesc.Scaling;
		}

		static bool MatchOrdering(const DXGI_MODE_DESC& kDesc, const DXGI_MODE_DESC& kReference)
		{
			return kReference.ScanlineOrdering != DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED && kReference.ScanlineOrdering != kDesc.ScanlineOrdering;
		}

		static bool MatchFormat(const DXGI_MODE_DESC& kDesc, const DXGI_MODE_DESC& kReference)
		{
			return kReference.Format == kDesc.Format;
		}

		static bool MatchResolution(const DXGI_MODE_DESC& kDesc, const DXGI_MODE_DESC& kReference)
		{
			return kDesc.Height >= kReference.Height && kDesc.Width >= kReference.Width;
		}

		static bool MatchRefreshRate(const DXGI_MODE_DESC& kDesc, const DXGI_MODE_DESC& kReference)
		{
			return kDesc.RefreshRate.Numerator * kReference.RefreshRate.Denominator >= kReference.RefreshRate.Numerator * kDesc.RefreshRate.Denominator;
		}

		bool operator<(const SRank& kOther) const
		{
			if (m_uOrdering != kOther.m_uOrdering)
				return m_uOrdering < kOther.m_uOrdering;
			if (m_uScaling != kOther.m_uScaling)
				return m_uScaling < kOther.m_uScaling;
			if (m_uFormat != kOther.m_uFormat)
				return m_uFormat < kOther.m_uFormat;
			if (m_uResolution != kOther.m_uResolution)
				return m_uResolution < kOther.m_uResolution;
			return m_uRefreshRate < kOther.m_uRefreshRate;
		}
	};

	bool bScanlineOrdering(pModeToMatch->ScanlineOrdering != DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED);
	bool bScaling(pModeToMatch->Scaling != DXGI_MODE_SCALING_UNSPECIFIED);
	bool bFormat(pModeToMatch->Format != DXGI_FORMAT_UNKNOWN);
	bool bResolution(pModeToMatch->Width != 0 || pModeToMatch->Height != 0);
	bool bRefreshRate(pModeToMatch->RefreshRate.Numerator != 0 || pModeToMatch->RefreshRate.Denominator != 0);

	if ((!bFormat && pConcernedDevice == NULL) ||
	    bResolution != (pModeToMatch->Width != 0 && pModeToMatch->Height != 0) ||
	    bRefreshRate != (pModeToMatch->RefreshRate.Numerator != 0 && pModeToMatch->RefreshRate.Denominator != 0))
	{
		return E_FAIL;
	}

	ID3D11Device* pConcernedD3D11Device(NULL);
	DXGI_MODE_DESC kTarget(*pModeToMatch);
	if (!bScanlineOrdering)
		kTarget.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
	if (!bScaling)
		kTarget.Scaling = DXGI_MODE_SCALING_STRETCHED;
	if (!bFormat || !bResolution || !bRefreshRate)
	{
		DXGI_MODE_DESC kDXGIDesktopModeDesc;
		GetDXGIModeDesc(&kDXGIDesktopModeDesc, m_spGLOutput->m_kDesktopMode);

		if (!bFormat)
		{
			void* pvConcernedD3D11Device(NULL);
			if (pConcernedDevice != NULL && !FAILED(pConcernedDevice->QueryInterface(__uuidof(ID3D11Device), &pvConcernedD3D11Device)))
				pConcernedD3D11Device = static_cast<ID3D11Device*>(pvConcernedD3D11Device);
			else
				kTarget.Format = kDXGIDesktopModeDesc.Format;
		}

		if (!bResolution)
		{
			kTarget.Width = kDXGIDesktopModeDesc.Width;
			kTarget.Height = kDXGIDesktopModeDesc.Height;
		}

		if (!bRefreshRate)
		{
			kTarget.RefreshRate.Numerator = kDXGIDesktopModeDesc.RefreshRate.Numerator;
			kTarget.RefreshRate.Denominator = kDXGIDesktopModeDesc.RefreshRate.Denominator;
		}
	}

	uint32 uMinMode = m_kDisplayModes.size();
	SRank kMinRank(~0);
	for (uint32 uMode = 0; uMode < m_kDisplayModes.size(); ++uMode)
	{
		const DXGI_MODE_DESC& kMode(m_kDisplayModes.at(uMode));

		if ((bScanlineOrdering && !SRank::MatchOrdering(kMode, kTarget)) ||
		    (bScaling && !SRank::MatchScaling(kMode, kTarget)) ||
		    (bFormat && !SRank::MatchFormat(kMode, kTarget)) ||
		    (bResolution && !SRank::MatchResolution(kMode, kTarget)) ||
		    (bRefreshRate && !SRank::MatchRefreshRate(kMode, kTarget)))
			continue;

		if (pConcernedD3D11Device != NULL)
		{
			UINT uModeFormatSuppport;
			if (FAILED(pConcernedD3D11Device->CheckFormatSupport(kMode.Format, &uModeFormatSuppport)) ||
			    (uModeFormatSuppport & D3D11_FORMAT_SUPPORT_DISPLAY) == 0)
				continue;
		}

		SRank kModeRank(kMode, kTarget);
		if (kModeRank < kMinRank)
		{
			uMinMode = uMode;
			kMinRank = kModeRank;
		}
	}

	if (uMinMode < m_kDisplayModes.size())
	{
		*pClosestMatch = m_kDisplayModes.at(uMinMode);
		if (pClosestMatch->Scaling == DXGI_MODE_SCALING_UNSPECIFIED)
			pClosestMatch->Scaling = kTarget.Scaling;
		if (pClosestMatch->ScanlineOrdering == DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED)
			pClosestMatch->ScanlineOrdering = kTarget.ScanlineOrdering;
		return S_OK;
	}

	return E_FAIL;
}

HRESULT CCryDXGLGIOutput::WaitForVBlank(void)
{
	DXGL_NOT_IMPLEMENTED
	return E_FAIL;
}

HRESULT CCryDXGLGIOutput::TakeOwnership(IUnknown* pDevice, BOOL Exclusive)
{
	DXGL_NOT_IMPLEMENTED
	return E_FAIL;
}

void CCryDXGLGIOutput::ReleaseOwnership(void)
{
	DXGL_NOT_IMPLEMENTED
}

HRESULT CCryDXGLGIOutput::GetGammaControlCapabilities(DXGI_GAMMA_CONTROL_CAPABILITIES* pGammaCaps)
{
	DXGL_NOT_IMPLEMENTED
	return E_FAIL;
}

HRESULT CCryDXGLGIOutput::SetGammaControl(const DXGI_GAMMA_CONTROL* pArray)
{
	DXGL_NOT_IMPLEMENTED
	return E_FAIL;
}

HRESULT CCryDXGLGIOutput::GetGammaControl(DXGI_GAMMA_CONTROL* pArray)
{
	DXGL_NOT_IMPLEMENTED
	return E_FAIL;
}

HRESULT CCryDXGLGIOutput::SetDisplaySurface(IDXGISurface* pScanoutSurface)
{
	DXGL_NOT_IMPLEMENTED
	return E_FAIL;
}

HRESULT CCryDXGLGIOutput::GetDisplaySurfaceData(IDXGISurface* pDestination)
{
	DXGL_NOT_IMPLEMENTED
	return E_FAIL;
}

HRESULT CCryDXGLGIOutput::GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats)
{
	DXGL_NOT_IMPLEMENTED
	return E_FAIL;
}
