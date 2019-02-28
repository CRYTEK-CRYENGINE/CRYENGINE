// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   ShaderCache.cpp : implementation of the Shaders cache management.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#include "StdAfx.h"
#include <Cry3DEngine/I3DEngine.h>
#include "RemoteCompiler.h"
#include <CryCore/Base64.h>

bool SShaderCombIdent::operator==(const SShaderCombIdent& other) const
{
	if ((m_MDVMask & ~SF_PLATFORM) == other.m_MDVMask && m_RTMask == other.m_RTMask
	    && m_GLMask == other.m_GLMask && m_FastCompare1 == other.m_FastCompare1
	    && m_pipelineState.opaque == other.m_pipelineState.opaque)
		return true;
	return false;
}

uint32 SShaderCombIdent::PostCreate()
{
	FUNCTION_PROFILER_RENDER_FLAT
	// using actual CRC is to expensive,  so replace with cheaper version with
	// has more changes of hits
	//uint32 hash = CCrc32::Compute(&m_RTMask, sizeof(SShaderCombIdent)-sizeof(uint32));
	const uint32* acBuffer = alias_cast<uint32*>(&m_RTMask);
	int len = (sizeof(SShaderCombIdent) - sizeof(uint32)) / sizeof(uint32);
	uint32 hash = 5381;
	while (len--)
	{
		int c = *acBuffer++;
		// hash = hash*33 + c
		hash = ((hash << 5) + hash) + c;
	}

	m_nHash = hash;
	m_MDVMask &= ~SF_PLATFORM;
	return hash;
}

SShaderGenComb* CShaderMan::mfGetShaderGenInfo(const char* nmFX)
{
	SShaderGenComb* c = NULL;
	uint32 i;
	for (i = 0; i < m_SGC.size(); i++)
	{
		c = &m_SGC[i];
		if (!stricmp(c->Name.c_str(), nmFX))
			break;
	}
	SShaderGenComb cmb;
	if (i == m_SGC.size())
	{
		cmb.pGen = mfCreateShaderGenInfo(nmFX, false);
		cmb.Name = CCryNameR(nmFX);
		m_SGC.push_back(cmb);
		c = &m_SGC[i];
	}
	return c;
}

static uint64 sGetGL(char** s, CCryNameR& name, uint32& nHWFlags)
{
	uint32 i;

	nHWFlags = 0;
	SShaderGenComb* c = NULL;
	const char* m = strchr(name.c_str(), '@');
	if (!m)
		m = strchr(name.c_str(), '/');
	assert(m);
	if (!m)
		return -1;
	char nmFX[128];
	char nameExt[128];
	nameExt[0] = 0;
	cry_strcpy(nmFX, name.c_str(), (size_t)(m - name.c_str()));
	c = gRenDev->m_cEF.mfGetShaderGenInfo(nmFX);
	if (!c || !c->pGen || !c->pGen->m_BitMask.Num())
		return 0;
	uint64 nGL = 0;
	SShaderGen* pG = c->pGen;
	while (true)
	{
		char theChar;
		int n = 0;
		while ((theChar = **s) != 0)
		{
			if (theChar == ')' || theChar == '|')
			{
				nameExt[n] = 0;
				break;
			}
			nameExt[n++] = theChar;
			++*s;
		}
		if (!nameExt[0])
			break;
		for (i = 0; i < pG->m_BitMask.Num(); i++)
		{
			SShaderGenBit* pBit = pG->m_BitMask[i];
			if (!stricmp(pBit->m_ParamName.c_str(), nameExt))
			{
				nGL |= pBit->m_Mask;
				break;
			}
		}
		if (i == pG->m_BitMask.Num())
		{
			if (!strncmp(nameExt, "0x", 2))
			{
				//nGL |= shGetHex(&nameExt[2]);
			}
			else
			{
				//assert(0);
				if (CRenderer::CV_r_shadersdebug)
					iLog->Log("WARNING: Couldn't find global flag '%s' in shader '%s' (skipped)", nameExt, c->Name.c_str());
			}
		}
		if (**s == '|')
			++*s;
	}
	return nGL;
}

static uint64 sGetRT(char** s)
{
	uint32 i;

	SShaderGen* pG = gRenDev->m_cEF.m_pGlobalExt;
	if (!pG)
		return 0;
	uint64 nRT = 0;
	char nm[128] = { 0 };
	while (true)
	{
		char theChar;
		int n = 0;
		while ((theChar = **s) != 0)
		{
			if (theChar == ')' || theChar == '|')
			{
				nm[n] = 0;
				break;
			}
			nm[n++] = theChar;
			++*s;
		}
		if (!nm[0])
			break;
		for (i = 0; i < pG->m_BitMask.Num(); i++)
		{
			SShaderGenBit* pBit = pG->m_BitMask[i];
			if (!stricmp(pBit->m_ParamName.c_str(), nm))
			{
				nRT |= pBit->m_Mask;
				break;
			}
		}
		if (i == pG->m_BitMask.Num())
		{
			//assert(0);
			//      iLog->Log("WARNING: Couldn't find runtime flag '%s' (skipped)", nm);
		}
		if (**s == '|')
			++*s;
	}
	return nRT;
}

static int sEOF(bool bFromFile, char* pPtr, FILE* fp)
{
	int nStatus;
	if (bFromFile)
		nStatus = gEnv->pCryPak->FEof(fp);
	else
	{
		SkipCharacters(&pPtr, kWhiteSpace);
		if (!*pPtr)
			nStatus = 1;
		else
			nStatus = 0;
	}
	return nStatus;
}

void CShaderMan::mfCloseShadersCache(int nID)
{
	if (m_FPCacheCombinations[nID])
	{
		gEnv->pCryPak->FClose(m_FPCacheCombinations[nID]);
		m_FPCacheCombinations[nID] = NULL;
	}
}

void sSkipLine(char*& s)
{
	if (!s) return;

	char* sEnd = strchr(s, '\n');
	if (sEnd)
	{
		sEnd++;
		s = sEnd;
	}
}

static void sIterateHW_r(FXShaderCacheCombinations* Combinations, SCacheCombination& cmb, int i, uint64 nHW, const char* szName)
{
	string str;
	gRenDev->m_cEF.mfInsertNewCombination(cmb.Ident, cmb.eCL, szName, 0, &str, false);
	CCryNameR nm = CCryNameR(str.c_str());
	FXShaderCacheCombinationsItor it = Combinations->find(nm);
	if (it == Combinations->end())
	{
		cmb.CacheName = str.c_str();
		Combinations->insert(FXShaderCacheCombinationsItor::value_type(nm, cmb));
	}
	for (int j = i; j < 64; j++)
	{
		if (((uint64)1 << j) & nHW)
		{
			cmb.Ident.m_GLMask &= ~((uint64)1 << j);
			sIterateHW_r(Combinations, cmb, j + 1, nHW, szName);
			cmb.Ident.m_GLMask |= ((uint64)1 << j);
			sIterateHW_r(Combinations, cmb, j + 1, nHW, szName);
		}
	}

}

void CShaderMan::mfGetShaderListPath(stack_string& nameOut, int nType)
{
	if (nType == 0)
		nameOut = stack_string(m_szUserPath.c_str()) + stack_string("shaders/shaderlist.txt");
	else
		nameOut = stack_string(m_szUserPath.c_str()) + stack_string("shaders/cache/shaderlistactivate.txt");
}

void CShaderMan::mfMergeShadersCombinations(FXShaderCacheCombinations* Combinations, int nType)
{
	FXShaderCacheCombinationsItor itor;
	for (itor = m_ShaderCacheCombinations[nType].begin(); itor != m_ShaderCacheCombinations[nType].end(); itor++)
	{
		SCacheCombination* cmb = &itor->second;
		FXShaderCacheCombinationsItor it = Combinations->find(cmb->CacheName);
		if (it == Combinations->end())
		{
			Combinations->insert(FXShaderCacheCombinationsItor::value_type(cmb->CacheName, *cmb));
		}
	}
}

//==========================================================================================================================================

struct CompareCombItem
{
	bool operator()(const SCacheCombination& p1, const SCacheCombination& p2) const
	{
		int n = stricmp(p1.Name.c_str(), p2.Name.c_str());
		if (n)
			return n < 0;
		n = p1.nCount - p2.nCount;
		if (n)
			return n > 0;
		return (stricmp(p1.CacheName.c_str(), p2.CacheName.c_str()) < 0);
	}
};

#define g_szTestResults "%USER%/TestResults"

void CShaderMan::mfInitShadersCacheMissLog()
{
	m_ShaderCacheMissCallback = 0;
	m_ShaderCacheMissPath = "";

	// don't access the HD if we don't have any logging to file enabled
	if (!CRenderer::CV_r_shaderslogcachemisses)
	{
		return;
	}

	// create valid path (also for xbox dvd emu)
	gEnv->pCryPak->MakeDir(g_szTestResults);

	char path[ICryPak::g_nMaxPath];
	path[sizeof(path) - 1] = 0;
	gEnv->pCryPak->AdjustFileName("%USER%\\Shaders\\ShaderCacheMisses.txt",
	                              path, ICryPak::FLAGS_PATH_REAL | ICryPak::FLAGS_FOR_WRITING);

	m_ShaderCacheMissPath = string(path);

	// load data which is already stored
	FILE* fp = fopen(path, "r");
	if (fp)
	{
		char str[2048];
		int nLine = 0;

		while (!feof(fp))
		{
			nLine++;
			str[0] = 0;
			fgets(str, 2047, fp);
			if (!str[0])
				continue;

			// remove new line at end
			int len = strlen(str);
			if (len > 0)
				str[strlen(str) - 1] = 0;

			m_ShaderCacheMisses.push_back(str);
		}

		std::sort(m_ShaderCacheMisses.begin(), m_ShaderCacheMisses.end());

		fclose(fp);
		fp = NULL;
	}
}

static inline bool IsHexDigit(int ch)
{
	const int nDigit = (ch - int('0'));
	const int bHex = (ch - int('a'));
	return ((nDigit >= 0) && (nDigit <= 9)) || ((bHex >= 0) && (bHex < 6));
}

void CShaderMan::mfInitShadersCache(byte bForLevel, FXShaderCacheCombinations* Combinations, const char* pCombinations, int nType)
{
	static_assert(SHADER_LIST_VER != SHADER_SERIALISE_VER, "Version mismatch!");

	char str[2048];
	bool bFromFile = (Combinations == NULL);
	stack_string nameComb;
	m_ShaderCacheExportCombinations.clear();
	FILE* fp = NULL;
	if (bFromFile)
	{
		if (!gRenDev->IsEditorMode() && !CRenderer::CV_r_shadersdebug && !gRenDev->IsShaderCacheGenMode())
			return;
		mfGetShaderListPath(nameComb, nType);
		fp = gEnv->pCryPak->FOpen(nameComb.c_str(), "r+");
		if (!fp)
			fp = gEnv->pCryPak->FOpen(nameComb.c_str(), "w+");
		if (!fp)
		{
			gEnv->pCryPak->AdjustFileName(nameComb.c_str(), str, 0);
			FILE* statusdst = fopen(str, "rb");
			if (statusdst)
			{
				fclose(statusdst);
				CrySetFileAttributes(str, FILE_ATTRIBUTE_ARCHIVE);
				fp = gEnv->pCryPak->FOpen(nameComb.c_str(), "r+");
			}
		}
		m_FPCacheCombinations[nType] = fp;
		Combinations = &m_ShaderCacheCombinations[nType];
	}

	int nLine = 0;
	char* pPtr = (char*)pCombinations;
	char* ss;
	if (fp || !bFromFile)
	{
		while (!sEOF(bFromFile, pPtr, fp))
		{
			nLine++;

			str[0] = 0;
			if (bFromFile)
				gEnv->pCryPak->FGets(str, 2047, fp);
			else
				fxFillCR(&pPtr, str);
			if (!str[0])
				continue;
			bool bNewFormat = false;

			if (str[0] == '/' && str[1] == '/')     // commented line e.g. // BadLine: Metal@Common_ShadowPS(%BIllum@IlluminationPS(%DIFFUSE|%ENVCMAMB|%ALPHAGLOW|%STAT_BRANCHING)(%_RT_AMBIENT|%_RT_BUMP|%_RT_GLOW)(101)(0)(0)(ps_2_0)
				continue;

			bool bExportEntry = false;
			int size = strlen(str);
			if (str[size - 1] == 0xa)
				str[size - 1] = 0;
			SCacheCombination cmb;
			char* s = str;
			SkipCharacters(&s, kWhiteSpace);
			if (s[0] != '<')
				continue;
			char* st;
			if (!bForLevel)
			{
				int nVer = atoi(&s[1]);
				if (nVer != SHADER_LIST_VER)
				{
					if (nVer == SHADER_SERIALISE_VER && bFromFile)
					{
						bExportEntry = true;
					}
					else
					{
						continue;
					}
				}
				if (s[2] != '>')
					continue;
				s += 3;
			}
			else
			{
				st = s;
				s = strchr(&st[1], '>');
				if (!s)
					continue;
				cmb.nCount = atoi(st);
				s++;
			}
			if (bForLevel == 2)
			{
				st = s;
				if (s[0] != '<')
					continue;
				s = strchr(st, '>');
				if (!s)
					continue;
				int nVer = atoi(&st[1]);

				if (nVer != SHADER_LIST_VER)
				{
					if (nVer == SHADER_SERIALISE_VER)
					{
						bExportEntry = true;
					}
					else
					{
						continue;
					}
				}
				s++;
			}
			st = s;
			s = strchr(s, '(');
			char name[128];
			if (s)
			{
				memcpy(name, st, s - st);
				name[s - st] = 0;
				cmb.Name = name;
				s++;
			}
			else
			{
				continue;
			}
			uint32 nHW = 0;
			cmb.Ident.m_GLMask = sGetGL(&s, cmb.Name, nHW);
			if (cmb.Ident.m_GLMask == -1)
			{
				const char* szFileName = nameComb.c_str();
				if (szFileName)
				{
					iLog->Log("Error: Error in '%s' file (Line: %d)", szFileName, nLine);
				}
				else
				{
					assert(!bFromFile);
					iLog->Log("Error: Error in non-file shader (Line: %d)", nLine);
				}
				sSkipLine(s);
				goto end;
			}

			ss = strchr(s, '(');
			if (!ss)
			{
				sSkipLine(s);
				goto end;
			}
			s = ss + 1;
			cmb.Ident.m_RTMask = sGetRT(&s);

			ss = strchr(s, '(');
			if (!ss)
			{
				sSkipLine(s);
				goto end;
			}
			s = ss + 1;
			cmb.Ident.m_LightMask = shGetHex(s);

			ss = strchr(s, '(');
			if (!ss)
			{
				sSkipLine(s);
				goto end;
			}
			s = ss + 1;
			cmb.Ident.m_MDMask = shGetHex(s);

			ss = strchr(s, '(');
			if (!ss)
			{
				sSkipLine(s);
				goto end;
			}
			s = ss + 1;
			cmb.Ident.m_MDVMask = shGetHex(s);

			ss = strchr(s, '(');
			if (!ss)
			{
				sSkipLine(s);
				goto end;
			}
			if (IsHexDigit(ss[1]) && (ss[2] != 'S'))
			{
				s = ss + 1;
				cmb.Ident.m_pipelineState.opaque = shGetHex64(s);
				bNewFormat = true;
				s = strchr(s, '(');
			}
			else
			{
				cmb.Ident.m_pipelineState.opaque = 0;
				s = ss;
			}
			if (s)
			{
				s++;
				cmb.eCL = CHWShader::mfStringClass(s);
				assert(cmb.eCL < eHWSC_Num);
			}
			else
			{
				cmb.eCL = eHWSC_Num;
				s++;
			}
#if CRY_RENDERER_VULKAN
			ss = strchr(s, '(');
			if (!ss)
			{
				sSkipLine(s);
				goto end;
			}
			const char* descriptorSetBegin = ss + 1;
			const char* descriptorSetEnd = strchr(descriptorSetBegin, ')');
			const unsigned int descriptorSetLength = static_cast<int>(descriptorSetEnd - descriptorSetBegin);
			if (descriptorSetLength > 0)
			{
				unsigned int decodedBufferSize  = Base64::decodedsize_base64(descriptorSetLength);
				std::vector<uint8> encodedLayout(decodedBufferSize);
				Base64::decode_base64((char*)&encodedLayout[0], descriptorSetBegin, descriptorSetLength, false);
				encodedLayout.resize(GetDeviceObjectFactory().GetEncodedResourceLayoutSize(encodedLayout));

				if (!GetDeviceObjectFactory().LookupResourceLayoutEncoding(cmb.Ident.m_pipelineState.VULKAN.resourceLayoutHash))
				{
					GetDeviceObjectFactory().RegisterEncodedResourceLayout(cmb.Ident.m_pipelineState.VULKAN.resourceLayoutHash, std::move(encodedLayout));
				}
			}
#endif			
			if (bNewFormat)
			{
				CCryNameR nm = CCryNameR(st);
				if (bExportEntry)
				{
					FXShaderCacheCombinationsItor it = m_ShaderCacheExportCombinations.find(nm);
					if (it == m_ShaderCacheExportCombinations.end())
					{
						cmb.CacheName = nm;
						m_ShaderCacheExportCombinations.insert(FXShaderCacheCombinationsItor::value_type(nm, cmb));
					}
				}
				else
				{
					FXShaderCacheCombinationsItor it = Combinations->find(nm);
					if (it != Combinations->end())
					{
						//assert(false);
					}
					else
					{
						cmb.CacheName = nm;
						Combinations->insert(FXShaderCacheCombinationsItor::value_type(nm, cmb));
					}
					if (nHW)
					{
						for (int j = 0; j < 64; j++)
						{
							if (((uint64)1 << j) & nHW)
							{
								cmb.Ident.m_GLMask &= ~((uint64)1 << j);
								sIterateHW_r(Combinations, cmb, j + 1, nHW, name);
								cmb.Ident.m_GLMask |= ((uint64)1 << j);
								sIterateHW_r(Combinations, cmb, j + 1, nHW, name);
								cmb.Ident.m_GLMask &= ~((uint64)1 << j);
							}
						}
					}
				}
			}
			continue;
end:
			iLog->Log("Error: Error in '%s' file (Line: %d)", nameComb.c_str(), nLine);
		}
	}
}

#if CRY_PLATFORM_DESKTOP
static void sResetDepend_r(SShaderGen* pGen, SShaderGenBit* pBit, SCacheCombination& cm)
{
	if (!pBit->m_DependResets.size())
		return;
	uint32 i, j;

	for (i = 0; i < pBit->m_DependResets.size(); i++)
	{
		const char* c = pBit->m_DependResets[i].c_str();
		for (j = 0; j < pGen->m_BitMask.Num(); j++)
		{
			SShaderGenBit* pBit1 = pGen->m_BitMask[j];
			if (!stricmp(pBit1->m_ParamName.c_str(), c))
			{
				cm.Ident.m_RTMask &= ~pBit1->m_Mask;
				sResetDepend_r(pGen, pBit1, cm);
				break;
			}
		}
	}
}

static void sSetDepend_r(SShaderGen* pGen, SShaderGenBit* pBit, SCacheCombination& cm)
{
	if (!pBit->m_DependSets.size())
		return;
	uint32 i, j;

	for (i = 0; i < pBit->m_DependSets.size(); i++)
	{
		const char* c = pBit->m_DependSets[i].c_str();
		for (j = 0; j < pGen->m_BitMask.Num(); j++)
		{
			SShaderGenBit* pBit1 = pGen->m_BitMask[j];
			if (!stricmp(pBit1->m_ParamName.c_str(), c))
			{
				cm.Ident.m_RTMask |= pBit1->m_Mask;
				sSetDepend_r(pGen, pBit1, cm);
				break;
			}
		}
	}
}

// Support for single light only
static bool sIterateDL(DWORD& dwDL)
{
	int nLights = dwDL & 0xf;
	int nType[4];
	int i;

	if (!nLights)
	{
		dwDL = 1;
		return true;
	}
	for (i = 0; i < nLights; i++)
	{
		nType[i] = (dwDL >> (SLMF_LTYPE_SHIFT + (i * SLMF_LTYPE_BITS))) & ((1 << SLMF_LTYPE_BITS) - 1);
	}
	switch (nLights)
	{
	case 1:
		if ((nType[0] & 3) < 2)
		{
			nType[0]++;
		}
		else
		{
			return false;
			nLights = 2;
			nType[0] = SLMF_DIRECT;
			nType[1] = SLMF_POINT;
		}
		break;
	case 2:
		if ((nType[0] & 3) == SLMF_DIRECT)
		{
			nType[0] = SLMF_POINT;
			nType[1] = SLMF_POINT;
		}
		else
		{
			nLights = 3;
			nType[0] = SLMF_DIRECT;
			nType[1] = SLMF_POINT;
			nType[2] = SLMF_POINT;
		}
		break;
	case 3:
		if ((nType[0] & 3) == SLMF_DIRECT)
		{
			nType[0] = SLMF_POINT;
			nType[1] = SLMF_POINT;
			nType[2] = SLMF_POINT;
		}
		else
		{
			nLights = 4;
			nType[0] = SLMF_DIRECT;
			nType[1] = SLMF_POINT;
			nType[2] = SLMF_POINT;
			nType[3] = SLMF_POINT;
		}
		break;
	case 4:
		if ((nType[0] & 3) == SLMF_DIRECT)
		{
			nType[0] = SLMF_POINT;
			nType[1] = SLMF_POINT;
			nType[2] = SLMF_POINT;
			nType[3] = SLMF_POINT;
		}
		else
			return false;
	}
	dwDL = nLights;
	for (i = 0; i < nLights; i++)
	{
		dwDL |= nType[i] << (SLMF_LTYPE_SHIFT + i * SLMF_LTYPE_BITS);
	}
	return true;
}

/*static bool sIterateDL(DWORD& dwDL)
   {
   int nLights = dwDL & 0xf;
   int nType[4];
   int i;

   if (!nLights)
   {
    dwDL = 1;
    return true;
   }
   for (i=0; i<nLights; i++)
   {
    nType[i] = (dwDL >> (SLMF_LTYPE_SHIFT + (i*SLMF_LTYPE_BITS))) & ((1<<SLMF_LTYPE_BITS)-1);
   }
   switch (nLights)
   {
   case 1:
    if (!(nType[0] & SLMF_RAE_ENABLED))
      nType[0] |= SLMF_RAE_ENABLED;
    else
      if (nType[0]<2)
        nType[0]++;
      else
      {
        nLights = 2;
        nType[0] = SLMF_DIRECT;
        nType[1] = SLMF_POINT;
      }
      break;
   case 2:
    if (!(nType[0] & SLMF_RAE_ENABLED))
      nType[0] |= SLMF_RAE_ENABLED;
    else
      if (!(nType[1] & SLMF_RAE_ENABLED))
        nType[1] |= SLMF_RAE_ENABLED;
      else
        if (nType[0] == SLMF_DIRECT)
          nType[0] = SLMF_POINT;
        else
        {
          nLights = 3;
          nType[0] = SLMF_DIRECT;
          nType[1] = SLMF_POINT;
          nType[2] = SLMF_POINT;
        }
        break;
   case 3:
    if (!(nType[0] & SLMF_RAE_ENABLED))
      nType[0] |= SLMF_RAE_ENABLED;
    else
      if (!(nType[1] & SLMF_RAE_ENABLED))
        nType[1] |= SLMF_RAE_ENABLED;
      else
        if (!(nType[2] & SLMF_RAE_ENABLED))
          nType[2] |= SLMF_RAE_ENABLED;
        else
          if (nType[0] == SLMF_DIRECT)
            nType[0] = SLMF_POINT;
          else
          {
            nLights = 4;
            nType[0] = SLMF_DIRECT;
            nType[1] = SLMF_POINT;
            nType[2] = SLMF_POINT;
            nType[3] = SLMF_POINT;
          }
          break;
   case 4:
    if (!(nType[0] & SLMF_RAE_ENABLED))
      nType[0] |= SLMF_RAE_ENABLED;
    else
      if (!(nType[1] & SLMF_RAE_ENABLED))
        nType[1] |= SLMF_RAE_ENABLED;
      else
        if (!(nType[2] & SLMF_RAE_ENABLED))
          nType[2] |= SLMF_RAE_ENABLED;
        else
          if (!(nType[3] & SLMF_RAE_ENABLED))
            nType[3] |= SLMF_RAE_ENABLED;
          else
            if (nType[0] == SLMF_DIRECT)
              nType[0] = SLMF_POINT;
            else
              return false;
   }
   dwDL = nLights;
   for (i=0; i<nLights; i++)
   {
    dwDL |= nType[i] << (SLMF_LTYPE_SHIFT + i*SLMF_LTYPE_BITS);
   }
   return true;
   }*/

void CShaderMan::mfAddLTCombination(SCacheCombination* cmb, FXShaderCacheCombinations& CmbsMapDst, DWORD dwL)
{
	char str[1024];
	char sLT[64];

	SCacheCombination cm;
	cm = *cmb;
	cm.Ident.m_LightMask = dwL;

	const char* c = strchr(cmb->CacheName.c_str(), ')');
	c = strchr(&c[1], ')');
	int len = (int)(c - cmb->CacheName.c_str() + 1);
	assert(len < sizeof(str));
	cry_strcpy(str, cmb->CacheName.c_str(), len);
	cry_strcat(str, "(");
	cry_sprintf(sLT, "%x", (uint32)dwL);
	cry_strcat(str, sLT);
	c = strchr(&c[2], ')');
	cry_strcat(str, c);
	CCryNameR nm = CCryNameR(str);
	cm.CacheName = nm;
	FXShaderCacheCombinationsItor it = CmbsMapDst.find(nm);
	if (it == CmbsMapDst.end())
	{
		CmbsMapDst.insert(FXShaderCacheCombinationsItor::value_type(nm, cm));
	}
}

void CShaderMan::mfAddLTCombinations(SCacheCombination* cmb, FXShaderCacheCombinations& CmbsMapDst)
{
	if (!CRenderer::CV_r_shadersprecachealllights)
		return;

	DWORD dwL = 0;  // 0 lights
	bool bRes = false;
	do
	{
		// !HACK: Do not iterate multiple lights for low spec
		if ((cmb->Ident.m_RTMask & (g_HWSR_MaskBit[HWSR_QUALITY] | g_HWSR_MaskBit[HWSR_QUALITY1])) || (dwL & 0xf) <= 1)
			mfAddLTCombination(cmb, CmbsMapDst, dwL);
		bRes = sIterateDL(dwL);
	}
	while (bRes);
}

void CShaderMan::mfAddRTCombination_r(int nComb, FXShaderCacheCombinations& CmbsMapDst, SCacheCombination* cmb, CHWShader* pSH, bool bAutoPrecache)
{
	uint32 i, j, n;
	uint32 dwType = pSH->m_dwShaderType;
	if (!dwType)
		return;
	for (i = nComb; i < m_pGlobalExt->m_BitMask.Num(); i++)
	{
		SShaderGenBit* pBit = m_pGlobalExt->m_BitMask[i];
		if (bAutoPrecache && !(pBit->m_Flags & (SHGF_AUTO_PRECACHE | SHGF_LOWSPEC_AUTO_PRECACHE)))
			continue;

		// Precache this flag on low-spec only
		if (pBit->m_Flags & SHGF_LOWSPEC_AUTO_PRECACHE)
		{
			if (cmb->Ident.m_RTMask & (g_HWSR_MaskBit[HWSR_QUALITY] | g_HWSR_MaskBit[HWSR_QUALITY1]))
				continue;
		}
		// Only in runtime used
		if (pBit->m_Flags & SHGF_RUNTIME)
			continue;
		for (n = 0; n < pBit->m_PrecacheNames.size(); n++)
		{
			if (pBit->m_PrecacheNames[n] == dwType)
				break;
		}
		if (n < pBit->m_PrecacheNames.size())
		{
			SCacheCombination cm;
			cm = *cmb;
			cm.Ident.m_RTMask &= ~pBit->m_Mask;
			cm.Ident.m_RTMask |= (pBit->m_Mask ^ cmb->Ident.m_RTMask) & pBit->m_Mask;
			if (!bAutoPrecache)
			{
				uint64 nBitSet = pBit->m_Mask & cmb->Ident.m_RTMask;
				if (nBitSet)
					sSetDepend_r(m_pGlobalExt, pBit, cm);
				else
					sResetDepend_r(m_pGlobalExt, pBit, cm);
			}

			char str[1024];
			const char* c = strchr(cmb->CacheName.c_str(), '(');
			const size_t len = (size_t)(c - cmb->CacheName.c_str());
			cry_strcpy(str, cmb->CacheName.c_str(), len);
			const char* c1 = strchr(&c[1], '(');
			cry_strcat(str, c, (size_t)(c1 - c));
			cry_strcat(str, "(");
			SShaderGen* pG = m_pGlobalExt;
			stack_string sRT;
			for (j = 0; j < pG->m_BitMask.Num(); j++)
			{
				SShaderGenBit* pBit = pG->m_BitMask[j];
				if (pBit->m_Mask & cm.Ident.m_RTMask)
				{
					if (!sRT.empty())
						sRT += "|";
					sRT += pBit->m_ParamName.c_str();
				}
			}
			cry_strcat(str, sRT.c_str());
			c1 = strchr(&c1[1], ')');
			cry_strcat(str, c1);
			CCryNameR nm = CCryNameR(str);
			cm.CacheName = nm;
			// HACK: don't allow unsupported quality mode
			uint64 nQMask = g_HWSR_MaskBit[HWSR_QUALITY] | g_HWSR_MaskBit[HWSR_QUALITY1];
			if ((cm.Ident.m_RTMask & nQMask) != nQMask)
			{
				FXShaderCacheCombinationsItor it = CmbsMapDst.find(nm);
				if (it == CmbsMapDst.end())
				{
					CmbsMapDst.insert(FXShaderCacheCombinationsItor::value_type(nm, cm));
				}
			}
			if (pSH->m_Flags & (HWSG_SUPPORTS_MULTILIGHTS | HWSG_SUPPORTS_LIGHTING))
				mfAddLTCombinations(&cm, CmbsMapDst);
			mfAddRTCombination_r(i + 1, CmbsMapDst, &cm, pSH, bAutoPrecache);
		}
	}
}

void CShaderMan::mfAddRTCombinations(FXShaderCacheCombinations& CmbsMapSrc, FXShaderCacheCombinations& CmbsMapDst, CHWShader* pSH, bool bListOnly)
{
	if (pSH->m_nFrameLoad == gRenDev->GetMainFrameID())
		return;
	pSH->m_nFrameLoad = gRenDev->GetMainFrameID();
	uint32 dwType = pSH->m_dwShaderType;
	if (!dwType)
		return;
	const char* str2 = pSH->mfGetEntryName();
	FXShaderCacheCombinationsItor itor;
	for (itor = CmbsMapSrc.begin(); itor != CmbsMapSrc.end(); itor++)
	{
		SCacheCombination* cmb = &itor->second;
		const char* c = strchr(cmb->Name.c_str(), '@');
		if (!c)
			c = strchr(cmb->Name.c_str(), '/');
		assert(c);
		if (!c)
			continue;
		if (stricmp(&c[1], str2))
			continue;
		/*if (!stricmp(str2, "MetalReflVS"))
		   {
		   if (cmb->nGL == 0x1093)
		   {
		    int nnn = 0;
		   }
		   }*/
		if (bListOnly)
		{
			if (pSH->m_Flags & (HWSG_SUPPORTS_MULTILIGHTS | HWSG_SUPPORTS_LIGHTING))
				mfAddLTCombinations(cmb, CmbsMapDst);
			mfAddRTCombination_r(0, CmbsMapDst, cmb, pSH, true);
		}
		else
			mfAddRTCombination_r(0, CmbsMapDst, cmb, pSH, false);
	}
}
#endif // CRY_PLATFORM_DESKTOP

void CShaderMan::mfInsertNewCombination(SShaderCombIdent& Ident, EHWShaderClass eCL, const char* name, int nID, string* Str, byte bStore)
{
	char str[2048];
	if (!m_FPCacheCombinations[nID] && bStore)
		return;

	stack_string sGL;
	stack_string sRT;
	uint32 i, j;
	SShaderGenComb* c = NULL;
	if (Ident.m_GLMask)
	{
		const char* m = strchr(name, '@');
		if (!m)
			m = strchr(name, '/');
		assert(m);
		char nmFX[128];
		if (m)
		{
			cry_strcpy(nmFX, name, (size_t)(m - name));
			c = mfGetShaderGenInfo(nmFX);
			if (c && c->pGen && c->pGen->m_BitMask.Num())
			{
				SShaderGen* pG = c->pGen;
				for (i = 0; i < 64; i++)
				{
					if (Ident.m_GLMask & ((uint64)1 << i))
					{
						for (j = 0; j < pG->m_BitMask.Num(); j++)
						{
							SShaderGenBit* pBit = pG->m_BitMask[j];
							if (pBit->m_Mask & (Ident.m_GLMask & ((uint64)1 << i)))
							{
								if (!sGL.empty())
									sGL += "|";
								sGL += pBit->m_ParamName.c_str();
								break;
							}
						}
					}
				}
			}
		}
	}
	if (Ident.m_RTMask)
	{
		SShaderGen* pG = m_pGlobalExt;
		if (pG)
		{
			for (i = 0; i < pG->m_BitMask.Num(); i++)
			{
				SShaderGenBit* pBit = pG->m_BitMask[i];
				if (pBit->m_Mask & Ident.m_RTMask)
				{
					if (!sRT.empty())
						sRT += "|";
					sRT += pBit->m_ParamName.c_str();
				}
			}
		}
	}
	uint32 nLT = Ident.m_LightMask;
	if (bStore == 1 && Ident.m_LightMask)
		nLT = 1;
	
#ifdef CRY_RENDERER_VULKAN
	std::string base64EncodedLayout;
	const std::vector<uint8>* pEncodedLayout = GetDeviceObjectFactory().LookupResourceLayoutEncoding(Ident.m_pipelineState.VULKAN.resourceLayoutHash);
	if (pEncodedLayout)
	{
		size_t base64EncodedLayoutSize = (Base64::encodedsize_base64(pEncodedLayout->size()));
		base64EncodedLayout.resize(base64EncodedLayoutSize);
		Base64::encode_base64((char*)&base64EncodedLayout[0], (const char*)pEncodedLayout->data(), pEncodedLayout->size(), false);
	}
	sprintf(str, "<%d>%s(%s)(%s)(%x)(%x)(%x)(%llx)(%s)(%s)", SHADER_LIST_VER, name, sGL.c_str(), sRT.c_str(), nLT, Ident.m_MDMask, Ident.m_MDVMask, Ident.m_pipelineState.opaque, CHWShader::mfClassString(eCL), base64EncodedLayout.c_str());
#else
	sprintf(str, "<%d>%s(%s)(%s)(%x)(%x)(%x)(%llx)(%s)", SHADER_LIST_VER, name, sGL.c_str(), sRT.c_str(), nLT, Ident.m_MDMask, Ident.m_MDVMask, Ident.m_pipelineState.opaque, CHWShader::mfClassString(eCL));
#endif
	if (!bStore)
	{
		if (Str)
			*Str = str;
		return;
	}
	CCryNameR nm;
	if (str[0] == '<' && str[2] == '>')
		nm = CCryNameR(&str[3]);
	else
		nm = CCryNameR(str);
	FXShaderCacheCombinationsItor it = m_ShaderCacheCombinations[nID].find(nm);
	if (it != m_ShaderCacheCombinations[nID].end())
		return;
	SCacheCombination cmb;
	cmb.Name = name;
	cmb.CacheName = nm;
	cmb.Ident = Ident;
	cmb.eCL = eCL;
	{
		stack_string nameOut;
		mfGetShaderListPath(nameOut, nID);

		static CryCriticalSection s_cResLock;
		AUTO_LOCK(s_cResLock); // Not thread safe without this

		if (m_FPCacheCombinations[nID])
		{
			m_ShaderCacheCombinations[nID].insert(FXShaderCacheCombinationsItor::value_type(nm, cmb));
			gEnv->pCryPak->FPrintf(m_FPCacheCombinations[nID], "%s\n", str);
			gEnv->pCryPak->FFlush(m_FPCacheCombinations[nID]);
		}
	}
	if (Str)
		*Str = str;
}

string CShaderMan::mfGetShaderCompileFlags(EHWShaderClass eClass, UPipelineState pipelineState) const
{
	string result;

#define STRICT_MODE		" /Ges"
#define COMPATIBLE_MODE	" /Gec"
	// NOTE: when updating remote compiler folders, please ensure folders path is matching
	const char* pCompilerOrbis   = "ORBIS/V033/DXOrbisShaderCompiler.exe %s %s %s %s";
	const char* pCompilerDurango = "Durango/March2017/fxc.exe /nologo /E %s /T %s /Zpr" COMPATIBLE_MODE "" STRICT_MODE " /Fo %s %s";
	const char* pCompilerD3D11   = "PCD3D11/v007/fxc.exe /nologo /E %s /T %s /Zpr" COMPATIBLE_MODE "" STRICT_MODE " /Fo %s %s";
	
	string compilerSPIRV = "";
	if (CRendererCVars::CV_r_VkShaderCompiler && strcmp(CRendererCVars::CV_r_VkShaderCompiler->GetString(), STR_VK_SHADER_COMPILER_HLSLCC) == 0)
	{
		compilerSPIRV = "SPIRV/V002/HLSL2SPIRV.exe %s %s %s %s";
	}
	else if (CRendererCVars::CV_r_VkShaderCompiler && strcmp(CRendererCVars::CV_r_VkShaderCompiler->GetString(), STR_VK_SHADER_COMPILER_DXC) == 0)
	{
		// " -spirv -Od -Zpr \"%s.%s\" -Fo \"%s.%s\" -Fc \"%s.%s\" -T %s -E \"%s\" %s %s"
		compilerSPIRV.Format("SPIRV/V003/dxc/dxc.exe %s %s ", eClass == eHWSC_Vertex ? "-fvk-invert-y" : "", CRenderer::CV_r_shadersdebug == 3 ? "-Od" : "-O3");
		compilerSPIRV += "-spirv -no-warnings -E %s -T %s -Zpr -Fo %s %s";
	}

#define ESSL_VERSION   "es310"
#if DXGL_REQUIRED_VERSION >= DXGL_VERSION_45
	#define GLSL_VERSION "450"
#elif DXGL_REQUIRED_VERSION >= DXGL_VERSION_44
	#define GLSL_VERSION "440"
#elif DXGL_REQUIRED_VERSION >= DXGL_VERSION_43
	#define GLSL_VERSION "430"
#elif DXGL_REQUIRED_VERSION >= DXGL_VERSION_42
	#define GLSL_VERSION "420"
#elif DXGL_REQUIRED_VERSION >= DXGL_VERSION_41
	#define GLSL_VERSION "410"
#elif DXGLES_REQUIRED_VERSION >= DXGLES_VERSION_31
	#define GLSL_VERSION "310"
#elif DXGLES_REQUIRED_VERSION >= DXGLES_VERSION_30
	#define GLSL_VERSION "300"
#else
	#error "Shading language revision not defined for this GL version"
#endif

	const char* pCompilerGL4 = "PCGL/V012/HLSLcc.exe -lang=" GLSL_VERSION " -flags=36609 -fxc=\"..\\..\\PCD3D11\\v007\\fxc.exe /nologo /E %s /T %s /Zpr" COMPATIBLE_MODE "" STRICT_MODE " /Fo\" -out=%s -in=%s";
	const char* pCompilerGLES3 = "PCGL/V012/HLSLcc.exe -lang=" ESSL_VERSION " -flags=36609 -fxc=\"..\\..\\PCD3D11\\v007\\fxc.exe /nologo /E %s /T %s /Zpr" COMPATIBLE_MODE "" STRICT_MODE " /Fo\" -out=%s -in=%s";

	if (CRenderer::CV_r_shadersdebug == 3)
	{
		// Set debug information
		pCompilerD3D11   = "PCD3D11/v007/fxc.exe /nologo /E %s /T %s /Zpr" COMPATIBLE_MODE "" STRICT_MODE " /Zi /Od /Fo %s %s";
		pCompilerDurango = "Durango/March2017/fxc.exe /nologo /E %s /T %s /Zpr" COMPATIBLE_MODE "" STRICT_MODE " /Zi /Od /Fo %s %s";
	}
	else if (CRenderer::CV_r_shadersdebug == 4)
	{
		// Set debug information, optimized shaders
		pCompilerD3D11   = "PCD3D11/v007/fxc.exe /nologo /E %s /T %s /Zpr" COMPATIBLE_MODE "" STRICT_MODE " /Zi /O3 /Fo %s %s";
		pCompilerDurango = "Durango/March2017/fxc.exe /nologo /E %s /T %s /Zpr" COMPATIBLE_MODE "" STRICT_MODE " /Zi /O3 /Fo %s %s";
	}

	if (pipelineState.opaque != 0)
	{
		if (CParserBin::m_nPlatform == SF_ORBIS)
		{
			const char* const pCompilerGnm = "ORBIS/V033/GnmShaderCompiler.exe %s %s %s %s --RowMajorMatrixStorage --UpgradeLegacySamplers --DebugHelperFiles";

			static const char* kVsStages[] =
			{
				"V2P",
				"V2H",
				"V2G",
				"V2L",
				"C2V2P",
				"C2I2P",
				nullptr,
				nullptr
			};
			static const int kPsDepthBits[] =
			{
				0,
				16,
				32,
				0,
			};
			static const char kISA[] =
			{
				'B',
				'C',
				'N',
				'R',
			};

			switch (eClass)
			{
			case eHWSC_Vertex:
				result.Format("%s -HwStage=%s -HwISA=%c", pCompilerGnm, kVsStages[pipelineState.GNM.VS.targetStage & 7], kISA[(pipelineState.GNM.VS.targetStage >> 5) & 3]);
				break;
			case eHWSC_Hull:
				result.Format("%s -HwStage=%s -HwISA=%c", pCompilerGnm, (pipelineState.GNM.HS.targetStage & 1) ? "H2D" : "H2M", kISA[(pipelineState.GNM.HS.targetStage >> 5) & 3]);
				break;
			case eHWSC_Domain:
				result.Format("%s -HwStage=%s -HwISA=%c", pCompilerGnm, (pipelineState.GNM.DS.targetStage & 1) ? "D2P" : "M2P", kISA[(pipelineState.GNM.DS.targetStage >> 5) & 3]);
				break;
			case eHWSC_Geometry:
				result.Format("%s -HwStage=%s -HwISA=%c", pCompilerGnm, (pipelineState.GNM.GS.targetStage & 1) ? "L2C2P" : "G2C2P", kISA[(pipelineState.GNM.GS.targetStage >> 5) & 3]);
				break;
			case eHWSC_Pixel:
				result.Format("%s -HwStage=%s -HwISA=%c -PsColor=0x%x -PsDepth=%d -PsStencil=%d", pCompilerGnm, "P", kISA[(pipelineState.GNM.PS.depthStencilInfo >> 29) & 3], pipelineState.GNM.PS.targetFormats, kPsDepthBits[(pipelineState.GNM.PS.depthStencilInfo >> 1) & 3], pipelineState.GNM.PS.depthStencilInfo & 1 ? 8 : 0);
				break;
			case eHWSC_Compute:
				result.Format("%s -HwStage=%s -HwISA=%c", pCompilerGnm, "C", kISA[(pipelineState.GNM.CS.targetStage >> 5) & 3]);
				break;
			default:
				CRY_ASSERT(false && "Unknown stage");
				break;
			}
			
			return result;
		}
	}

#if CRY_PLATFORM_ORBIS
	result = pCompilerOrbis;
#elif CRY_PLATFORM_DURANGO
	result = pCompilerDurango;
#elif CRY_RENDERER_OPENGLES && DXGL_INPUT_GLSL
	result = pCompilerGLES3;
#elif CRY_RENDERER_OPENGL && DXGL_INPUT_GLSL
	result = pCompilerGL4;
#else
	if (CParserBin::m_nPlatform == SF_D3D11)
		result = pCompilerD3D11;
	else if (CParserBin::m_nPlatform == SF_ORBIS)
		result = pCompilerOrbis;
	else if (CParserBin::m_nPlatform == SF_DURANGO)
		result = pCompilerDurango;
	else if (CParserBin::m_nPlatform == SF_GL4)
		result = pCompilerGL4;
	else if (CParserBin::m_nPlatform == SF_GLES3)
		result = pCompilerGLES3;
	else if (CParserBin::m_nPlatform == SF_VULKAN)
		result = compilerSPIRV;
	else
	{
		CryFatalError("Compiling shaders for unsupported platform");
		result = "";
	}
#endif

	if (!CRenderer::CV_r_shadersCompileStrict)
	{
		result = result.replace("" STRICT_MODE "", "");
	}

	if (!CRenderer::CV_r_shadersCompileCompatible)
	{
		result = result.replace("" COMPATIBLE_MODE "", "");
	}

	return result;
}

inline bool sCompareComb(const SCacheCombination& a, const SCacheCombination& b)
{
	int32 dif;

	char shader1[128], shader2[128];
	char* tech1 = NULL, * tech2 = NULL;
	cry_strcpy(shader1, a.Name.c_str());
	cry_strcpy(shader2, b.Name.c_str());
	char* c = strchr(shader1, '@');
	if (!c)
		c = strchr(shader1, '/');
	//assert(c);
	if (c)
	{
		*c = 0;
		tech1 = c + 1;
	}
	c = strchr(shader2, '@');
	if (!c)
		c = strchr(shader2, '/');
	//assert(c);
	if (c)
	{
		*c = 0;
		tech2 = c + 1;
	}

	dif = stricmp(shader1, shader2);
	if (dif != 0)
		return dif < 0;

	if (tech1 == NULL && tech2 != NULL)
		return true;
	if (tech1 != NULL && tech2 == NULL)
		return false;

	if (tech1 != NULL && tech2 != NULL)
	{
		dif = stricmp(tech1, tech2);
		if (dif != 0)
			return dif < 0;
	}

	if (a.Ident.m_GLMask != b.Ident.m_GLMask)
		return a.Ident.m_GLMask < b.Ident.m_GLMask;

	if (a.Ident.m_RTMask != b.Ident.m_RTMask)
		return a.Ident.m_RTMask < b.Ident.m_RTMask;

	if (a.Ident.m_pipelineState.opaque != b.Ident.m_pipelineState.opaque)
		return a.Ident.m_pipelineState.opaque < b.Ident.m_pipelineState.opaque;

	if (a.Ident.m_FastCompare1 != b.Ident.m_FastCompare1)
		return a.Ident.m_FastCompare1 < b.Ident.m_FastCompare1;

	if (a.Ident.m_FastCompare2 != b.Ident.m_FastCompare2)
		return a.Ident.m_FastCompare2 < b.Ident.m_FastCompare2;

	return false;
}

#if CRY_PLATFORM_DESKTOP

void CShaderMan::AddGLCombinations(CShader* pSH, std::vector<SCacheCombination>& CmbsGL)
{
	int i, j;
	uint64 nMask = 0;
	if (pSH->m_pGenShader)
	{
		SShaderGen* pG = pSH->m_pGenShader->m_ShaderGenParams;
		for (i = 0; i < pG->m_BitMask.size(); i++)
		{
			SShaderGenBit* pB = pG->m_BitMask[i];
			SCacheCombination cc;
			cc.Name = pB->m_ParamName;
			for (j = 0; j < m_pGlobalExt->m_BitMask.size(); j++)
			{
				SShaderGenBit* pB1 = m_pGlobalExt->m_BitMask[j];
				if (pB1->m_ParamName == pB->m_ParamName)
				{
					nMask |= pB1->m_Mask;
					break;
				}
			}
			assert(j != m_pGlobalExt->m_BitMask.size());
			for (i = 0; i < 64; i++)
			{
				//if (nMask & (1<<i))
				//  SetBit_r(i, CmbsGL);
			}
		}
	}
	else
	{
		SCacheCombination cc;
		cc.Ident.m_GLMask = 0;
		CmbsGL.push_back(cc);
	}
}

void CShaderMan::AddGLCombination(FXShaderCacheCombinations& CmbsMap, SCacheCombination& cmb)
{
	char str[1024];
	const char* st = cmb.CacheName.c_str();
	if (st[0] == '<')
		st += 3;
	const char* s = strchr(st, '@');
	char name[128];
	if (!s)
		s = strchr(st, '/');
	if (s)
	{
		memcpy(name, st, s - st);
		name[s - st] = 0;
	}
	else
		cry_strcpy(name, st);
	#ifdef __GNUC__
	cry_sprintf(str, "%s(%llx)(%x)(%x)", name, cmb.Ident.m_GLMask, cmb.Ident.m_MDMask, cmb.Ident.m_MDVMask);
	#else
	cry_sprintf(str, "%s(%I64x)(%x)(%x)", name, cmb.Ident.m_GLMask, cmb.Ident.m_MDMask, cmb.Ident.m_MDVMask);
	#endif
	CCryNameR nm = CCryNameR(str);
	FXShaderCacheCombinationsItor it = CmbsMap.find(nm);
	if (it == CmbsMap.end())
	{
		cmb.CacheName = nm;
		cmb.Name = nm;
		CmbsMap.insert(FXShaderCacheCombinationsItor::value_type(nm, cmb));
	}
}

/*string str;
   gRenDev->m_cEF.mfInsertNewCombination(cmb.nGL, cmb.nRT, cmb.nLT, cmb.nMD, cmb.nMDV, cmb.ePR, szName, 0, &str, false);
   CCryNameTSCRC nm = CCryNameTSCRC(str.c_str());
   FXShaderCacheCombinationsItor it = Combinations->find(nm);
   if (it == Combinations->end())
   {
   cmb.CacheName = nm;
   Combinations->insert(FXShaderCacheCombinationsItor::value_type(nm, cmb));
   }
   for (int j=i; j<64; j++)
   {
   if (((uint64)1<<j) & nHW)
   {
   cmb.nGL &= ~((uint64)1<<j);
   sIterateHW_r(Combinations, cmb, j+1, nHW, szName);
   cmb.nGL |= ((uint64)1<<j);
   sIterateHW_r(Combinations, cmb, j+1, nHW, szName);
   }
   }*/

void CShaderMan::AddCombination(SCacheCombination& cmb, FXShaderCacheCombinations& CmbsMap, CHWShader* pHWS)
{
	char str[2048];
	#ifdef __GNUC__
	sprintf(str, "%s(%llx)(%llx)(%d)(%d)(%d)(%llx)", cmb.Name.c_str(), cmb.Ident.m_GLMask, cmb.Ident.m_RTMask, cmb.Ident.m_LightMask, cmb.Ident.m_MDMask, cmb.Ident.m_MDVMask, cmb.Ident.m_pipelineState.opaque);
	#else
	sprintf(str, "%s(%I64x)(%I64x)(%d)(%d)(%d)(%llx)", cmb.Name.c_str(), cmb.Ident.m_GLMask, cmb.Ident.m_RTMask, cmb.Ident.m_LightMask, cmb.Ident.m_MDMask, cmb.Ident.m_MDVMask, cmb.Ident.m_pipelineState.opaque);
	#endif
	CCryNameR nm = CCryNameR(str);
	FXShaderCacheCombinationsItor it = CmbsMap.find(nm);
	if (it == CmbsMap.end())
	{
		cmb.CacheName = nm;
		CmbsMap.insert(FXShaderCacheCombinationsItor::value_type(nm, cmb));
	}
}

void CShaderMan::AddLTCombinations(SCacheCombination& cmb, FXShaderCacheCombinations& CmbsMap, CHWShader* pHWS)
{
	assert(pHWS->m_Flags & HWSG_SUPPORTS_LIGHTING);

	// Just single light support

	// Directional light
	cmb.Ident.m_LightMask = 1;
	AddCombination(cmb, CmbsMap, pHWS);

	// Point light
	cmb.Ident.m_LightMask = 0x101;
	AddCombination(cmb, CmbsMap, pHWS);

	// Projected light
	cmb.Ident.m_LightMask = 0x201;
	AddCombination(cmb, CmbsMap, pHWS);
}

void CShaderMan::AddRTCombinations(FXShaderCacheCombinations& CmbsMap, CHWShader* pHWS, CShader* pSH, FXShaderCacheCombinations* Combinations)
{
	SCacheCombination cmb;

	uint32 nType = pHWS->m_dwShaderType;

	uint32 i, j;
	SShaderGen* pGen = m_pGlobalExt;
	int nBits = 0;

	uint32 nBitsPlatform = 0;
	if (CParserBin::m_nPlatform == SF_ORBIS)
		nBitsPlatform |= SHGD_HW_ORBIS;
	else if (CParserBin::m_nPlatform == SF_DURANGO)
		nBitsPlatform |= SHGD_HW_DURANGO;
	else if (CParserBin::m_nPlatform == SF_D3D11)
		nBitsPlatform |= SHGD_HW_DX11;
	else if (CParserBin::m_nPlatform == SF_GL4)
		nBitsPlatform |= SHGD_HW_GL4;
	else if (CParserBin::m_nPlatform == SF_GLES3)
		nBitsPlatform |= SHGD_HW_GLES3;
	else if (CParserBin::m_nPlatform == SF_VULKAN)
		nBitsPlatform |= SHGD_HW_VULKAN;

	uint64 BitMask[64];
	memset(BitMask, 0, sizeof(BitMask));

	// Make a mask of flags affected by this type of shader
	uint64 nRTMask = 0;
	uint64 nSetMask = 0;

	if (nType)
	{
		for (i = 0; i < pGen->m_BitMask.size(); i++)
		{
			SShaderGenBit* pBit = pGen->m_BitMask[i];
			if (!pBit->m_Mask)
				continue;
			if (pBit->m_Flags & SHGF_RUNTIME)
				continue;
			if (nBitsPlatform & pBit->m_nDependencyReset)
				continue;
			for (j = 0; j < pBit->m_PrecacheNames.size(); j++)
			{
				if (pBit->m_PrecacheNames[j] == nType)
				{
					if (nBitsPlatform & pBit->m_nDependencySet)
					{
						nSetMask |= pBit->m_Mask;
						continue;
					}
					BitMask[nBits++] = pBit->m_Mask;
					nRTMask |= pBit->m_Mask;
					break;
				}
			}
		}
	}
	if (nBits > 10)
		CryLog("WARNING: Number of runtime bits for shader '%s' - %d: exceed 10 (too many combinations will be produced)...", pHWS->GetName(), nBits);
	if (nBits > 30)
	{
		CryLog("Error: Ignore...");
		return;
	}

	cmb.eCL = pHWS->m_eSHClass;
	string szName = string(pSH->GetName());
	szName += string("@") + string(pHWS->m_EntryFunc.c_str());
	cmb.Name = szName;
	cmb.Ident.m_GLMask = pHWS->m_nMaskGenShader;

	// For unknown shader type just add combinations from the list
	if (!nType)
	{
		FXShaderCacheCombinationsItor itor;
		for (itor = Combinations->begin(); itor != Combinations->end(); itor++)
		{
			SCacheCombination* c = &itor->second;
			if (c->Name == cmb.Name && c->Ident.m_GLMask == pHWS->m_nMaskGenFX)
			{
				cmb = *c;
				AddCombination(cmb, CmbsMap, pHWS);
			}
		}
		return;
	}

	cmb.Ident.m_LightMask = 0;
	cmb.Ident.m_MDMask = 0;
	cmb.Ident.m_MDVMask = 0;
	cmb.Ident.m_RTMask = 0;

	int nIterations = 1 << nBits;
	for (i = 0; i < nIterations; i++)
	{
		cmb.Ident.m_RTMask = nSetMask;
		cmb.Ident.m_LightMask = 0;
		for (j = 0; j < nBits; j++)
		{
			if ((1 << j) & i)
				cmb.Ident.m_RTMask |= BitMask[j];
		}
		/*if (cmb.nRT == 0x40002)
		   {
		   int nnn = 0;
		   }*/
		AddCombination(cmb, CmbsMap, pHWS);
		if (pHWS->m_Flags & HWSG_SUPPORTS_LIGHTING)
			AddLTCombinations(cmb, CmbsMap, pHWS);
	}
}

void CShaderMan::_PrecacheShaderList(bool bStatsOnly)
{
	float t0 = gEnv->pTimer->GetAsyncCurTime();

	if (!m_pGlobalExt)
		return;

	m_eCacheMode = eSC_BuildGlobalList;

	uint32 nSaveFeatures = gRenDev->m_Features;
	int nAsync = CRenderer::CV_r_shadersasynccompiling;
	if (nAsync != 3)
		CRenderer::CV_r_shadersasynccompiling = 0;

	// Command line shaders precaching
	gRenDev->m_Features |= RFT_HW_SM20 | RFT_HW_SM2X | RFT_HW_SM30;
	FXShaderCacheCombinations* Combinations = &m_ShaderCacheCombinations[0];

	std::vector<SCacheCombination> Cmbs;
	std::vector<SCacheCombination> CmbsRT;
	FXShaderCacheCombinations CmbsMap;
	char str1[128], str2[128];

	// Extract global combinations only (including MD and MDV)
	for (FXShaderCacheCombinationsItor itor = Combinations->begin(); itor != Combinations->end(); itor++)
	{
		SCacheCombination* cmb = &itor->second;
		FXShaderCacheCombinationsItor it = CmbsMap.find(cmb->CacheName);
		if (it == CmbsMap.end())
		{
			CmbsMap.insert(FXShaderCacheCombinationsItor::value_type(cmb->CacheName, *cmb));
		}
	}
	for (FXShaderCacheCombinationsItor itor = CmbsMap.begin(); itor != CmbsMap.end(); itor++)
	{
		SCacheCombination* cmb = &itor->second;
		Cmbs.push_back(*cmb);
	}

	int nEmpty = 0;
	int nProcessed = 0;
	int nCompiled = 0;
	int nMaterialCombinations = 0;

	std::vector<CShader*> allocatedShaders;

	if (Cmbs.size() >= 1)
	{
		std::stable_sort(Cmbs.begin(), Cmbs.end(), sCompareComb);

		nMaterialCombinations = Cmbs.size();

		m_nCombinationsProcess = Cmbs.size();
		m_bReload = true;
		m_nCombinationsCompiled = 0;
		m_nCombinationsEmpty = 0;
		uint64 nGLLast = -1;
		for (int i = 0; i < Cmbs.size(); i++)
		{
			SCacheCombination* cmb = &Cmbs[i];
			cry_strcpy(str1, cmb->Name.c_str());
			char* c = strchr(str1, '@');
			if (!c)
				c = strchr(str1, '/');
			//assert(c);
			if (c)
			{
				*c = 0;
				m_szShaderPrecache = &c[1];
			}
			else
			{
				c = strchr(str1, '(');
				if (c)
				{
					*c = 0;
					m_szShaderPrecache = "";
				}
			}

			CShader* pSH = CShaderMan::mfForName(str1, 0, NULL, cmb->Ident.m_GLMask);
			if (!pSH)
				continue;

			std::vector<SCacheCombination>* pCmbs = &Cmbs;
			FXShaderCacheCombinations CmbsMapRTSrc;
			FXShaderCacheCombinations CmbsMapRTDst;

			for (int m = 0; m < pSH->m_HWTechniques.Num(); m++)
			{
				SShaderTechnique* pTech = pSH->m_HWTechniques[m];
				for (int n = 0; n < pTech->m_Passes.Num(); n++)
				{
					SShaderPass* pPass = &pTech->m_Passes[n];
					if (pPass->m_PShader)
						pPass->m_PShader->m_nFrameLoad = -10;
					if (pPass->m_VShader)
						pPass->m_VShader->m_nFrameLoad = -10;
				}
			}

			for (; i < Cmbs.size(); i++)
			{
				SCacheCombination* cmba = &Cmbs[i];
				cry_strcpy(str2, cmba->Name.c_str());
				c = strchr(str2, '@');
				if (!c)
					c = strchr(str2, '/');
				assert(c);
				if (c)
					*c = 0;
				if (stricmp(str1, str2) || cmb->Ident.m_GLMask != cmba->Ident.m_GLMask)
					break;
				CmbsMapRTSrc.insert(FXShaderCacheCombinationsItor::value_type(cmba->CacheName, *cmba));
			}
			// surrounding for(;;i++) will increment this again
			i--;
			m_nCombinationsProcess -= CmbsMapRTSrc.size();

			for (FXShaderCacheCombinationsItor itor = CmbsMapRTSrc.begin(); itor != CmbsMapRTSrc.end(); itor++)
			{
				SCacheCombination* cmb = &itor->second;
				cry_strcpy(str2, cmb->Name.c_str());
				FXShaderCacheCombinationsItor it = CmbsMapRTDst.find(cmb->CacheName);
				if (it == CmbsMapRTDst.end())
				{
					CmbsMapRTDst.insert(FXShaderCacheCombinationsItor::value_type(cmb->CacheName, *cmb));
				}
			}

			for (int m = 0; m < pSH->m_HWTechniques.Num(); m++)
			{
				SShaderTechnique* pTech = pSH->m_HWTechniques[m];
				for (int n = 0; n < pTech->m_Passes.Num(); n++)
				{
					SShaderPass* pPass = &pTech->m_Passes[n];
					if (pPass->m_PShader)
						mfAddRTCombinations(CmbsMapRTSrc, CmbsMapRTDst, pPass->m_PShader, true);
					if (pPass->m_VShader)
						mfAddRTCombinations(CmbsMapRTSrc, CmbsMapRTDst, pPass->m_VShader, true);
				}
			}

			CmbsRT.clear();
			for (FXShaderCacheCombinationsItor itor = CmbsMapRTDst.begin(); itor != CmbsMapRTDst.end(); itor++)
			{
				SCacheCombination* cmb = &itor->second;
				CmbsRT.push_back(*cmb);
			}
			pCmbs = &CmbsRT;
			m_nCombinationsProcessOverall = CmbsRT.size();
			m_nCombinationsProcess = 0;

			CmbsMapRTDst.clear();
			CmbsMapRTSrc.clear();
			uint32 nFlags = HWSF_PRECACHE;
			if (CParserBin::m_nPlatform & (SF_D3D11 | SF_DURANGO | SF_ORBIS | SF_GL4 | SF_GLES3 | SF_VULKAN))
				nFlags |= HWSF_STOREDATA;
			if (bStatsOnly)
				nFlags |= HWSF_FAKE;
			for (int jj = 0; jj < pCmbs->size(); jj++)
			{
				m_nCombinationsProcess++;
				SCacheCombination* cmba = &(*pCmbs)[jj];
				c = (char*)strchr(cmba->Name.c_str(), '@');
				if (!c)
					c = (char*)strchr(cmba->Name.c_str(), '/');
				assert(c);
				if (!c)
					continue;
				m_szShaderPrecache = &c[1];

				for (int m = 0; m < pSH->m_HWTechniques.Num(); m++)
				{
					SShaderTechnique* pTech = pSH->m_HWTechniques[m];
					for (int n = 0; n < pTech->m_Passes.Num(); n++)
					{
						SShaderPass* pPass = &pTech->m_Passes[n];
						SCacheCombination cmbSaved = *cmba;

						// Adjust some flags for low spec
						CHWShader* shaders[] = { pPass->m_PShader, pPass->m_VShader, pPass->m_GShader, pPass->m_HShader, pPass->m_CShader, pPass->m_DShader };
						for (auto* shader : shaders)
						{
							if (shader && (!m_szShaderPrecache || !stricmp(m_szShaderPrecache, shader->m_EntryFunc.c_str()) != 0))
							{
								uint64 nFlagsOrigShader_RT = cmbSaved.Ident.m_RTMask & shader->m_nMaskAnd_RT | shader->m_nMaskOr_RT;
								uint64 nFlagsOrigShader_GL = shader->m_nMaskGenShader;
								uint32 nFlagsOrigShader_LT = cmbSaved.Ident.m_LightMask;

								shader->PrecacheShader(pSH, cmba->Ident, nFlags);

								if (nFlagsOrigShader_RT != cmbSaved.Ident.m_RTMask || nFlagsOrigShader_GL != shader->m_nMaskGenShader || nFlagsOrigShader_LT != cmbSaved.Ident.m_LightMask)
								{
									m_nCombinationsEmpty++;
									if (!bStatsOnly)
										shader->mfAddEmptyCombination(nFlagsOrigShader_RT, nFlagsOrigShader_GL, nFlagsOrigShader_LT, cmbSaved);
								}

								shader->mfWriteoutTokensToCache();
							}
							
						}

						if (bStatsOnly)
						{
							static int nLastCombs = 0;
							if (m_nCombinationsCompiled != nLastCombs && !(m_nCombinationsCompiled & 0x7f))
							{
								nLastCombs = m_nCombinationsCompiled;
								CryLog("-- Processed: %d, Compiled: %d, Referenced (Empty): %d...", m_nCombinationsProcess, m_nCombinationsCompiled, m_nCombinationsEmpty);
							}
						}
	#if CRY_PLATFORM_WINDOWS
						gEnv->pSystem->PumpWindowMessage(true);
	#endif
					}
				}
				pSH->mfFlushPendedShaders();
				iLog->Update();
			}

			pSH->mfFlushCache();

			// HACK HACK HACK:
			// should be bigger than 0, but could cause issues right now when checking for RT
			// combinations when no shadertype is defined and the previous shader line
			// was still async compiling -- needs fix in HWShader for m_nMaskGenFX
			CHWShader::mfFlushPendedShadersWait(0);

			allocatedShaders.push_back(pSH);

			nProcessed += m_nCombinationsProcess;
			nCompiled += m_nCombinationsCompiled;
			nEmpty += m_nCombinationsEmpty;

			m_nCombinationsProcess = 0;
			m_nCombinationsCompiled = 0;
			m_nCombinationsEmpty = 0;
		}
	}
	CHWShader::mfFlushPendedShadersWait(-1);

	// Optimise shader resources
	SOptimiseStats Stats;
	EHWShaderClass classes[] = { eHWSC_Vertex, eHWSC_Pixel, eHWSC_Geometry, eHWSC_Domain, eHWSC_Hull, eHWSC_Compute };
	for (const auto &c : classes)
	{
		CryAutoReadLock<CryRWLock> lock(CBaseResource::s_cResLock);
		const SResourceContainer* shaderCaches = CBaseResource::GetResourcesForClass(CHWShader::mfGetCacheClassName(c));
		if (!shaderCaches)
			continue;

		for (auto *resource : shaderCaches->m_RList)
		{
			auto *hwCache = static_cast<SHWShaderCache*>(resource);
			auto* userCache = hwCache ? hwCache->AcquireDiskCache(cacheSource::user) : nullptr;

			// Optimize user disk caches
			if (userCache)
			{
				SOptimiseStats _Stats;
				userCache->mfOptimiseCacheFile(&_Stats);
				Stats.nEntries += _Stats.nEntries;
				Stats.nUniqueEntries += _Stats.nUniqueEntries;
				Stats.nSizeCompressed += _Stats.nSizeCompressed;
				Stats.nSizeUncompressed += _Stats.nSizeUncompressed;
				Stats.nTokenDataSize += _Stats.nTokenDataSize;
			}
		}
	}

	for (auto &pSH : allocatedShaders)
		SAFE_RELEASE(pSH);

	string sShaderPath(gRenDev->m_cEF.m_szUserPath);
	sShaderPath += gRenDev->m_cEF.m_ShadersCache;

	m_eCacheMode = eSC_Normal;
	m_bReload = false;
	m_szShaderPrecache = NULL;
	CRenderer::CV_r_shadersasynccompiling = nAsync;

	gRenDev->m_Features = nSaveFeatures;

	float t1 = gEnv->pTimer->GetAsyncCurTime();
	CryLogAlways("All shaders combinations compiled in %.2f seconds", (t1 - t0));
	CryLogAlways("Combinations: (Material: %d, Processed: %d; Compiled: %d; Removed: %d)", nMaterialCombinations, nProcessed, nCompiled, nEmpty);
	CryLogAlways("-- Shader cache overall stats: Entries: %d, Unique Entries: %d, Size: %d, Compressed Size: %d, Token data size: %d", Stats.nEntries, Stats.nUniqueEntries, Stats.nSizeUncompressed, Stats.nSizeCompressed, Stats.nTokenDataSize);

	m_nCombinationsProcess = -1;
	m_nCombinationsCompiled = -1;
	m_nCombinationsEmpty = -1;
}

#endif

void CHWShader::mfGenName(uint64 GLMask, uint64 RTMask, uint32 LightMask, uint32 MDMask, uint32 MDVMask, uint64 PSS, EHWShaderClass eClass, char* dstname, int nSize, byte bType)
{
	assert(dstname);
	dstname[0] = 0;

	char str[32];
	if (bType != 0 && GLMask)
	{
#if defined(__GNUC__)
		cry_sprintf(str, "(GL%llx)", GLMask);
#else
		cry_sprintf(str, "(GL%I64x)", GLMask);
#endif
		cry_strcat(dstname, nSize, str);
	}
	if (bType != 0)
	{
#if defined(__GNUC__)
		cry_sprintf(str, "(RT%llx)", RTMask);
#else
		cry_sprintf(str, "(RT%I64x)", RTMask);
#endif
		cry_strcat(dstname, nSize, str);
	}
	if (bType != 0 && LightMask)
	{
		cry_sprintf(str, "(LT%x)", LightMask);
		cry_strcat(dstname, nSize, str);
	}
	if (bType != 0 && MDMask)
	{
		cry_sprintf(str, "(MD%x)", MDMask);
		cry_strcat(dstname, nSize, str);
	}
	if (bType != 0 && MDVMask)
	{
		cry_sprintf(str, "(MDV%x)", MDVMask);
		cry_strcat(dstname, nSize, str);
	}
	if (bType != 0 && PSS)
	{
		sprintf(str, "(PSS%llx)", PSS);
		cry_strcat(dstname, nSize, str);
	}
	if (bType != 0)
	{
		cry_sprintf(str, "(%s)", mfClassString(eClass));
		cry_strcat(dstname, nSize, str);
	}
}

#if CRY_PLATFORM_DESKTOP

void CShaderMan::mfPrecacheShaders(bool bStatsOnly)
{
	CHWShader::mfFlushPendedShadersWait(-1);

	if (CRenderer::ShaderTargetFlag & SF_ORBIS)
	{
	#ifdef WATER_TESSELLATION_RENDERER
		CRenderer::CV_r_WaterTessellationHW = 0;
	#endif
	}

	gRenDev->m_bDeviceSupportsTessellation = CRenderer::ShaderTargetFlag & (SF_D3D11 | SF_GL4 | SF_VULKAN);
	gRenDev->m_bDeviceSupportsGeometryShaders =	CRenderer::ShaderTargetFlag & (SF_D3D11 | SF_GL4 | SF_VULKAN | SF_DURANGO | SF_ORBIS);

	CParserBin::m_bShaderCacheGen = true;
	gRenDev->m_Features |= RFT_HW_SM50;
	CParserBin::SetupForPlatform(CRenderer::ShaderTargetFlag);
	CryLogAlways("\nStarting shader compilation for %s...", CRenderer::CV_r_ShaderTarget->GetString());
	mfInitShadersList(NULL);
	mfPreloadShaderExts();
	_PrecacheShaderList(bStatsOnly);

	if (CRenderer::ShaderTargetFlag & SF_ORBIS)
		CRenderer::ShaderTargetFlag &= ~SF_ORBIS;

	CParserBin::SetupForPlatform(SF_D3D11);

	gRenDev->m_cEF.m_Bin.InvalidateCache();
}

static SResFileLookupData* sStoreLookupData(CResFileLookupDataMan& LevelLookup, CResFile* pRF, uint32 CRC, float fVersion)
{
	CCryNameTSCRC name = pRF->mfGetFileName();
	SResFileLookupData* pData = LevelLookup.GetData(name);
	uint32 nMinor = (int)(((float)fVersion - (float)(int)fVersion) * 10.1f);
	uint32 nMajor = (int)fVersion;
	if (!pData || (CRC && pData->m_CRC32 != CRC) || pData->m_CacheMinorVer != nMinor || pData->m_CacheMajorVer != nMajor)
	{
		LevelLookup.AddData(pRF, CRC);
		pData = LevelLookup.GetData(name);
		LevelLookup.MarkDirty(true);
	}
	assert(pData);
	return pData;
}

struct SMgData
{
	CCryNameTSCRC Name;
	int           nSize;
	uint32        CRC;
	uint32        flags;
	byte*         pData;
	int           nID;
	byte          bProcessed;
};

static int snCurListID;
typedef std::map<CCryNameTSCRC, SMgData> ShaderData;
typedef ShaderData::iterator             ShaderDataItor;

struct SNameData
{
	CCryNameR Name;
	bool      bProcessed;
};

//////////////////////////////////////////////////////////////////////////
bool CShaderMan::CheckAllFilesAreWritable(const char* szDir) const
{
	#if CRY_PLATFORM_WINDOWS
	assert(szDir);

	ICryPak* pack = gEnv->pCryPak;
	assert(pack);

	string sPathWithFilter = string(szDir) + "/*.*";

	// Search files that match filter specification.
	_finddata_t fd;
	int res;
	intptr_t handle;
	if ((handle = pack->FindFirst(sPathWithFilter.c_str(), &fd)) != -1)
	{
		do
		{
			if ((fd.attrib & _A_SUBDIR) == 0)
			{
				string fullpath = string(szDir) + "/" + fd.name;

				FILE* out = pack->FOpen(fullpath.c_str(), "rb");
				if (!out)
				{
					res = pack->FindNext(handle, &fd);
					continue;
				}
				if (pack->IsInPak(out))
				{
					pack->FClose(out);
					res = pack->FindNext(handle, &fd);
					continue;
				}
				pack->FClose(out);

				out = pack->FOpen(fullpath.c_str(), "ab");

				if (out)
					pack->FClose(out);
				else
				{
					gEnv->pLog->LogError("ERROR: Shader cache is not writable (file: '%s')", fullpath.c_str());
					return false;
				}
			}

			res = pack->FindNext(handle, &fd);
		}
		while (res >= 0);

		pack->FindClose(handle);

		gEnv->pLog->LogToFile("Shader cache directory '%s' was successfully tested for being writable", szDir);
	}
	else
		CryLog("Shader cache directory '%s' does not exist", szDir);

	#endif

	return true;
}

#endif // CRY_PLATFORM_DESKTOP

bool CShaderMan::mfPreloadBinaryShaders()
{
	LOADING_TIME_PROFILE_SECTION;
	// don't preload binary shaders if we are in editing mode
	if (CRenderer::CV_r_shadersediting)
		return false;

	// don't load all binary shaders twice
	if (m_Bin.m_bBinaryShadersLoaded)
		return true;

	//bool bFound = iSystem->GetIPak()->LoadPakToMemory("Engine/ShadersBin.pak", ICryPak::eInMemoryPakLocale_CPU);
	//if (!bFound)
	//	return false;

#ifndef _RELEASE
	// also load shaders pak file to memory because shaders are also read, when data not found in bin, and to check the CRC
	// of the source shaders against the binary shaders in non release mode
	iSystem->GetIPak()->LoadPakToMemory("%ENGINE%/Shaders.pak", ICryPak::eInMemoryPakLocale_CPU);
#endif

	stack_string szPath = stack_string("%ENGINE%/") + m_ShadersCache;

	struct _finddata_t fileinfo;
	intptr_t handle;

	handle = gEnv->pCryPak->FindFirst(szPath + "/*.*", &fileinfo);
	if (handle == -1)
		return false;
	std::vector<string> FilesCFX;
	std::vector<string> FilesCFI;

	do
	{
		if (gEnv->pSystem && gEnv->pSystem->IsQuitting())
			return false;
		if (fileinfo.name[0] == '.')
			continue;
		if (fileinfo.attrib & _A_SUBDIR)
			continue;
		const char* szExt = PathUtil::GetExt(fileinfo.name);
		if (!stricmp(szExt, "cfib"))
			FilesCFI.push_back(fileinfo.name);
		else if (!stricmp(szExt, "cfxb"))
			FilesCFX.push_back(fileinfo.name);
	}
	while (gEnv->pCryPak->FindNext(handle, &fileinfo) != -1);

	if (FilesCFX.size() + FilesCFI.size() > MAX_FXBIN_CACHE)
		SShaderBin::s_nMaxFXBinCache = FilesCFX.size() + FilesCFI.size();
	uint32 i;
	char sName[256];

	{
		LOADING_TIME_PROFILE_SECTION_NAMED("CShaderMan::mfPreloadBinaryShaders(): FilesCFI");
		for (i = 0; i < FilesCFI.size(); i++)
		{
			if (gEnv->pSystem && gEnv->pSystem->IsQuitting())
				return false;

			const string& file = FilesCFI[i];
			cry_strcpy(sName, file.c_str());
			PathUtil::RemoveExtension(sName);
			SShaderBin* pBin = m_Bin.GetBinShader(sName, true, 0);
			assert(pBin);
		}
	}

	{
		LOADING_TIME_PROFILE_SECTION_NAMED("CShaderMan::mfPreloadBinaryShaders(): FilesCFX");
		for (i = 0; i < FilesCFX.size(); i++)
		{
			if (gEnv->pSystem && gEnv->pSystem->IsQuitting())
				return false;

			const string& file = FilesCFX[i];
			cry_strcpy(sName, file.c_str());
			PathUtil::RemoveExtension(sName);
			SShaderBin* pBin = m_Bin.GetBinShader(sName, false, 0);
			assert(pBin);
		}
	}

	gEnv->pCryPak->FindClose(handle);

	// Unload pak from memory.
	//iSystem->GetIPak()->LoadPakToMemory("Engine/ShadersBin.pak", ICryPak::eInMemoryPakLocale_Unload);

#ifndef _RELEASE
	iSystem->GetIPak()->LoadPakToMemory("%ENGINE%/Shaders.pak", ICryPak::eInMemoryPakLocale_Unload);
#endif

	m_Bin.m_bBinaryShadersLoaded = true;

	return SShaderBin::s_nMaxFXBinCache > 0;
}