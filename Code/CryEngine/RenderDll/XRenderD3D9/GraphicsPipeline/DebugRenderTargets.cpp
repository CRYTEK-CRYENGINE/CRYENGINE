// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DebugRenderTargets.h"
#include "DriverD3D.h"
#include <Common/RenderDisplayContext.h>

struct SDebugRenderTargetConstants
{
	Vec4 colorMultiplier;
	Vec4 showRTFlags; // onlyAlpha, RGBKEncoded, aliased, 0
	Vec4 transform;
};

void CDebugRenderTargetsStage::ResetRenderTargetList()
{
	m_bShowList = false;
	m_columnCount = 2;
	m_renderTargetList.clear();
}

const char* CDebugRenderTargetsStage::showRenderTargetHelp =
	"Displays render targets - for debug purpose\n"
	"[Usage]\n"
	"r_ShowRenderTarget -l : list all available render targets\n"
	"r_ShowRenderTarget -l hdr : list all available render targets whose name contain 'hdr'\n"
	"r_ShowRenderTarget -nf zpass : show any render targets whose name contain 'zpass' with no filtering in 2x2(default) table\n"
	"r_ShowRenderTarget -c:3 pass : show any render targets whose name contain 'pass' in 3x3 table\n"
	"r_ShowRenderTarget z hdr : show any render targets whose name contain either 'z' or 'hdr'\n"
	"r_ShowRenderTarget scene:rg scene:b : show any render targets whose name contain 'scene' first with red-green channels only and then with a blue channel only\n"
	"r_ShowRenderTarget scenetarget:rgba:2 : show any render targets whose name contain 'scenetarget' with all channels multiplied by 2\n"
	"r_ShowRenderTarget scene:b hdr:a : show any render targets whose name contain 'scene' with a blue channel only and ones whose name contain 'hdr' with an alpha channel only\n"
	"r_ShowRenderTarget -e $ztarget : show a render target whose name exactly matches '$ztarget'\n"
	"r_ShowRenderTarget -s scene : separately shows each channel of any 'scene' render targets\n"
	"r_ShowRenderTarget -k scene : shows any 'scene' render targets with RGBK decoding\n"
	"r_ShowRenderTarget -a scene : shows any 'scene' render targets with 101110/8888 aliasing"
	"r_ShowRenderTarget -m:2 scene : shows mip map level 2 of 'scene' render targets"
	"r_ShowRenderTarget -s:3 scene : shows array slice 3 of 'scene' render targets";

void CDebugRenderTargetsStage::OnShowRenderTargetsCmd(IConsoleCmdArgs* pArgs)
{
	int argCount = pArgs->GetArgCount();

	ResetRenderTargetList();

	if (argCount <= 1)
	{
		string help = showRenderTargetHelp;
		int curPos = 0;
		string line = help.Tokenize("\n", curPos);
		while (false == line.empty())
		{
			gEnv->pLog->Log("%s", line.c_str());
			line = help.Tokenize("\n", curPos);
		}
		return;
	}

	// Check for '-l'.
	for (int i = 1; i < argCount; ++i)
	{
		if (strcmp(pArgs->GetArg(i), "-l") == 0)
		{
			m_bShowList = true;
			break;
		}
	}

	// Check for '-c:*'.
	for (int i = 1; i < argCount; ++i)
	{
		if (strlen(pArgs->GetArg(i)) > 3 && strncmp(pArgs->GetArg(i), "-c:", 3) == 0)
		{
			int col = atoi(pArgs->GetArg(i) + 3);
			m_columnCount = col > 0 ? col : 2;
		}
	}

	// Now gather all render targets.
	std::vector<CTexture*> allRTs;
	SResourceContainer* pRL = CBaseResource::GetResourcesForClass(CTexture::mfGetClassName());
	ResourcesMapItor it;
	for (it = pRL->m_RMap.begin(); it != pRL->m_RMap.end(); ++it)
	{
		CTexture* tp = (CTexture*)it->second;
		if (tp && !tp->IsNoTexture())
		{
			if ((tp->GetFlags() & (FT_USAGE_RENDERTARGET | FT_USAGE_DEPTHSTENCIL | FT_USAGE_UNORDERED_ACCESS)) && tp->GetDevTexture())
				allRTs.push_back(tp);
		}
	}

	// Process actual arguments with possible '-nf', '-f', '-e' options.
	bool bNoRegularArgs = true;
	bool bFiltered = true;
	bool bExactMatch = false;
	bool bRGBKEncoded = false;
	bool bAliased = false;
	bool bWeightedChannels = false;
	bool bSplitChannels = false;
	int  slice = -1;
	int  mip = -1;

	for (int i = 1; i < argCount; ++i)
	{
		const char* pCurArg = pArgs->GetArg(i);

		bool bColOption = strlen(pCurArg) > 3 && strncmp(pCurArg, "-c:", 3) == 0;
		if (strcmp(pCurArg, "-l") == 0 || bColOption)
			continue;

		if (strcmp(pCurArg, "-nf") == 0)
		{
			bFiltered = false;
		}
		else if (strcmp(pCurArg, "-f") == 0)
		{
			bFiltered = true;
		}
		else if (strcmp(pCurArg, "-e") == 0)
		{
			bExactMatch = true;
		}
		else if (strcmp(pCurArg, "-k") == 0)
		{
			bRGBKEncoded = true;
		}
		else if (strcmp(pCurArg, "-a") == 0)
		{
			bAliased = true;
		}
		else if (strcmp(pCurArg, "-s") == 0)
		{
			bSplitChannels = true;
		}
		else if (strncmp(pCurArg, "-s:", 3) == 0)
		{
			slice = atoi(pCurArg + 3);
		}
		else if (strncmp(pCurArg, "-m:", 3) == 0)
		{
			mip = atoi(pCurArg + 3);
		}
		else
		{
			bNoRegularArgs = false;
			string argTxt = pCurArg, nameTxt, channelTxt, mulTxt;
			argTxt.MakeLower();
			float multiplier = 1.0f;
			size_t pos = argTxt.find(':');
			if (pos == string::npos)
			{
				nameTxt = argTxt;
				channelTxt = "rgba";
			}
			else
			{
				nameTxt = argTxt.substr(0, pos);
				channelTxt = argTxt.substr(pos + 1, string::npos);
				pos = channelTxt.find(':');
				if (pos != string::npos)
				{
					mulTxt = channelTxt.substr(pos + 1, string::npos);
					multiplier = static_cast<float>(atof(mulTxt.c_str()));
					if (multiplier <= 0)
						multiplier = 1.0f;
				}
				bWeightedChannels = true;
			}

			Vec4 channelWeight(0, 0, 0, 0);
			if (channelTxt.find('r') != string::npos)
				channelWeight.x = 1.0f;
			if (channelTxt.find('g') != string::npos)
				channelWeight.y = 1.0f;
			if (channelTxt.find('b') != string::npos)
				channelWeight.z = 1.0f;
			if (channelTxt.find('a') != string::npos)
				channelWeight.w = 1.0f;

			channelWeight *= multiplier;

			for (size_t k = 0; k < allRTs.size(); ++k)
			{
				string texName = allRTs[k]->GetName();
				texName.MakeLower();
				bool bMatch = false;
				if (bExactMatch)
					bMatch = texName == nameTxt;
				else
					bMatch = texName.find(nameTxt.c_str()) != string::npos;
				if (bMatch)
				{
					SRenderTargetInfo rtInfo;
					rtInfo.bFiltered = bFiltered;
					rtInfo.bRGBKEncoded = bRGBKEncoded;
					rtInfo.bAliased = bAliased;
					rtInfo.pTexture = allRTs[k];
					rtInfo.channelWeight = channelWeight;
					rtInfo.mip = mip;
					rtInfo.slice = slice;

					if (bSplitChannels)
					{
						const Vec4 channels[4] = { Vec4(1, 0, 0, 0), Vec4(0, 1, 0, 0), Vec4(0, 0, 1, 0), Vec4(0, 0, 0, 1) };

						for (int j = 0; j < 4; ++j)
						{
							rtInfo.channelWeight = bWeightedChannels ? channelWeight : Vec4(1, 1, 1, 1);
							rtInfo.channelWeight.x *= channels[j].x;
							rtInfo.channelWeight.y *= channels[j].y;
							rtInfo.channelWeight.z *= channels[j].z;
							rtInfo.channelWeight.w *= channels[j].w;

							if (rtInfo.channelWeight[j] > 0.0f)
								m_renderTargetList.push_back(rtInfo);
						}
					}
					else
					{
						m_renderTargetList.push_back(rtInfo);
					}
				}
			}
		}
	}

	if (bNoRegularArgs && m_bShowList) // This means showing all items.
	{
		for (size_t k = 0; k < allRTs.size(); ++k)
		{
			SRenderTargetInfo rtInfo;
			rtInfo.pTexture = allRTs[k];
			m_renderTargetList.push_back(rtInfo);
		}
	}
}

void CDebugRenderTargetsStage::Execute()
{
	if (CRendererCVars::CV_r_DeferredShadingDebugGBuffer >= 1 && CRendererCVars::CV_r_DeferredShadingDebugGBuffer <= 9)
	{
		ResetRenderTargetList();

		SRenderTargetInfo rtInfo;
		rtInfo.pTexture = CRendererResources::s_ptexSceneDiffuseTmp;
		m_renderTargetList.push_back(rtInfo);
		m_columnCount = 1;

		ExecuteShowTargets();

		// *INDENT-OFF*
		struct { const char* name; const char* desc; } helpText[9] =
		{
			{ "Normals",                        "" },
			{ "Smoothness",                     "" },
			{ "Reflectance",                    "" },
			{ "Albedo",                         "" },
			{ "Lighting model",                 "gray: standard -- yellow: transmittance -- blue: pom self-shadowing" },
			{ "Translucency",                   "" },
			{ "Sun self-shadowing",             "" },
			{ "Subsurface Scattering",          "" },
			{ "Specular Validation"             "blue: too low -- orange: too high and not yet metal -- pink: just valid for oxidized metal/rust" }
		};
		
		auto& curText = helpText[clamp_tpl(CRendererCVars::CV_r_DeferredShadingDebugGBuffer - 1, 0, 8)];
		IRenderAuxText::WriteXY(10, 10, 1.0f,  1.0f,  0, 1, 0, 1, "%s", curText.name);
		IRenderAuxText::WriteXY(10, 30, 0.85f, 0.85f, 0, 1, 0, 1, "%s", curText.desc);
		// *INDENT-ON*

		ResetRenderTargetList();
	}
	else if (m_bShowList)
	{
		iLog->Log("RenderTargets:\n");
		for (size_t i = 0; i < m_renderTargetList.size(); ++i)
		{
			CTexture* pTex = m_renderTargetList[i].pTexture;
			if (pTex)
				iLog->Log("\t%" PRISIZE_T "  %s\t--------------%s  %d x %d\n", i, pTex->GetName(), pTex->GetFormatName(), pTex->GetWidth(), pTex->GetHeight());
			else
				iLog->Log("\t%" PRISIZE_T "  %s\t--------------(NOT INITIALIZED)\n", i, pTex->GetName());
		}

		ResetRenderTargetList();
	}
	else if (!m_renderTargetList.empty())
	{
		ExecuteShowTargets();
	}
}

void CDebugRenderTargetsStage::ExecuteShowTargets()
{
	if (!m_renderTargetList.empty())
	{
		PROFILE_LABEL_SCOPE("DEBUG RENDERTARGETS");

		float tileW = 1.f / static_cast<float>(m_columnCount);
		float tileH = 1.f / static_cast<float>(m_columnCount);

		const float tileGapW = tileW * 0.05f;
		const float tileGapH = tileH * 0.05f;

		if (m_columnCount != 1) // If not a fullsceen(= 1x1 table),
		{
			tileW -= tileGapW;
			tileH -= tileGapH;
		}

		const int tileCount = min(int(m_renderTargetList.size()), m_columnCount * m_columnCount);

		for (int i = m_debugPrimitives.size(); i < tileCount; ++i)
		{
			CRenderPrimitive prim;
			prim.AllocateTypedConstantBuffer(eConstantBufferShaderSlot_PerPrimitive, sizeof(SDebugRenderTargetConstants), EShaderStage_Vertex | EShaderStage_Pixel);

			m_debugPrimitives.push_back(std::move(prim));
		}

		CRenderDisplayContext* pContext = gcpRendD3D->GetActiveDisplayContext();

		m_debugPass.SetRenderTarget(0, pContext->GetCurrentColorOutput());
		m_debugPass.SetViewport(pContext->GetViewport());
		m_debugPass.BeginAddingPrimitives();
		m_primitiveCount = 0;

		for (size_t i = 0; i < tileCount; ++i)
		{
			SRenderTargetInfo& rtInfo = m_renderTargetList[i];
			if (!CTexture::IsTextureExist(rtInfo.pTexture))
				continue;

			float curX = (i % m_columnCount) * (tileW + tileGapW);
			float curY = (i / m_columnCount) * (tileH + tileGapH);

			ResourceViewHandle textureView = EDefaultResourceViews::Default;
			
			if (rtInfo.mip != -1 || rtInfo.slice != -1)
			{
				STextureLayout texLayout = rtInfo.pTexture->GetDevTexture()->GetLayout();

				int firstSlice = clamp_tpl(rtInfo.slice, 0, texLayout.m_nArraySize - 1);
				int sliceCount = clamp_tpl(rtInfo.slice < 0 ? texLayout.m_nArraySize : 1, 1, int(texLayout.m_nArraySize));

				int firstMip = clamp_tpl(rtInfo.mip, 0, texLayout.m_nMips - 1);
				int mipCount = clamp_tpl(rtInfo.mip < 0 ? texLayout.m_nMips : 1, 1, int(texLayout.m_nMips));

				SResourceView srv = SResourceView::ShaderResourceView(DeviceFormats::ConvertFromTexFormat(texLayout.m_eDstFormat), firstSlice, sliceCount, firstMip, mipCount);
				textureView = rtInfo.pTexture->GetDevTexture()->GetOrCreateResourceViewHandle(srv);
			}

			CRenderPrimitive& prim = m_debugPrimitives[m_primitiveCount++];
			prim.SetTechnique(CShaderMan::s_ShaderDebug, "Debug_RenderTarget", 0);
			prim.SetPrimitiveType(CRenderPrimitive::ePrim_FullscreenQuad);
			prim.SetCullMode(eCULL_None);
			prim.SetTexture(0, rtInfo.pTexture, textureView, EShaderStage_Pixel);
			prim.SetSampler(0, rtInfo.bFiltered ? EDefaultSamplerStates::LinearClamp : EDefaultSamplerStates::PointClamp);

			if (prim.Compile(m_debugPass) == CRenderPrimitive::eDirty_None)
			{
				Vec4 showRTFlags(ZERO); // onlyAlpha, RGBKEncoded, aliased, 0
				showRTFlags.x = (rtInfo.channelWeight.x == 0 && rtInfo.channelWeight.y == 0 && rtInfo.channelWeight.z == 0 && rtInfo.channelWeight.w > 0.5f) ? 1.0f : 0.0f;
				showRTFlags.y = rtInfo.bRGBKEncoded ? 1.0f : 0.0f;
				showRTFlags.z = rtInfo.bAliased ? 1.0f : 0.0f;

				auto constants = prim.GetConstantManager().BeginTypedConstantUpdate<SDebugRenderTargetConstants>(eConstantBufferShaderSlot_PerPrimitive, EShaderStage_Vertex | EShaderStage_Pixel);

				constants->colorMultiplier = rtInfo.channelWeight;
				constants->showRTFlags = showRTFlags;
				constants->transform.x = +2.0f * tileW;
				constants->transform.y = -2.0f * tileH;
				constants->transform.z = +2.0f * curX - 1.0f;
				constants->transform.w = -2.0f * curY + 1.0f;

				prim.GetConstantManager().EndTypedConstantUpdate(constants);

				m_debugPass.AddPrimitive(&prim);

				IRenderAuxText::WriteXY((int)(curX * 800 + 2), (int)((curY + tileH) * 600 - 15), 1, 1, 1, 1, 1, 1, "Fmt: %s, Type: %s", rtInfo.pTexture->GetFormatName(), CTexture::NameForTextureType(rtInfo.pTexture->GetTextureType()));
				IRenderAuxText::WriteXY((int)(curX * 800 + 2), (int)((curY + tileH) * 600 + 1),  1, 1, 1, 1, 1, 1, "%s   %d x %d",      rtInfo.pTexture->GetName(),       rtInfo.pTexture->GetWidth(), rtInfo.pTexture->GetHeight());
			}
		}

		m_debugPass.Execute();
	}
}
