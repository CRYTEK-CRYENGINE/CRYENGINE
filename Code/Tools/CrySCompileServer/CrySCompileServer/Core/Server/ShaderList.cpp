// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "ShaderList.hpp"
#include "CrySimpleServer.hpp"

#include <assert.h>
#ifdef _MSC_VER
#include <process.h>
#include <direct.h>
#endif
#ifdef UNIX
#include <pthread.h>
#endif

static bool g_bSaveThread = false;

CShaderList& CShaderList::Instance()
{
	static CShaderList g_Cache;
	return g_Cache;
}

//////////////////////////////////////////////////////////////////////////
CShaderList::CShaderList()
{
	m_lastTime = 0;
}

//////////////////////////////////////////////////////////////////////////
void CShaderList::Tick()
{
	DWORD t = GetTickCount();
	if (t < m_lastTime || (t - m_lastTime) > 1000)	//check every second
	{
		m_lastTime = t;

		Save();
		//CShaderList::Instance().m_PC.MergeNewLinesAndSave();
		//CShaderList::Instance().m_X360.MergeNewLinesAndSave();
		//CShaderList::Instance().m_PS3.MergeNewLinesAndSave();
	}
}

//////////////////////////////////////////////////////////////////////////
void CShaderList::Add(const std::string& rShaderListName,const char* pLine)
{
	tdShaderLists::iterator it;
	{
		CCrySimpleMutexAutoLock Lock(m_Mutex);
		it =	m_ShaderLists.find(rShaderListName);
		//not existing yet?
		if(it==m_ShaderLists.end())
		{
			CCrySimpleMutexAutoLock Lock(m_Mutex2);	//load/save mutex
			m_ShaderLists[rShaderListName]=new CShaderListFile(rShaderListName);
			it	=	m_ShaderLists.find(rShaderListName);
			it->second->Load((SEnviropment::Instance().m_Cache+rShaderListName).c_str());
		}
	}
	it->second->InsertLine(pLine);
}


//////////////////////////////////////////////////////////////////////////
void CShaderList::Save()
{
	CCrySimpleMutexAutoLock Lock(m_Mutex2);	//load/save mutex
	for(tdShaderLists::iterator it=m_ShaderLists.begin();it!=m_ShaderLists.end();++it)
		it->second->MergeNewLinesAndSave();
}

//////////////////////////////////////////////////////////////////////////
CShaderListFile::CShaderListFile(std::string ListName)
{
	m_bModified = false;

  m_listname = ListName;

	// some test cases
	SMetaData MD;
	assert(CheckSyntax("<1>watervolume@WaterVolumeOutofPS()()(0)(0)(0)(ps_2_0)",MD)==true);
	assert(CheckSyntax("<1>Blurcloak@BlurCloakPS(%BUMP_MAP)(%_RT_FOG|%_RT_HDR_MODE|%_RT_BUMP)(0)(0)(1)(ps_2_0)",MD)==true);
	assert(CheckSyntax("<1>Burninglayer@BurnPS()(%_RT_ADDBLEND|%_RT_)HDR_MODE|%_RT_BUMP|%_RT_3DC)(0)(0)(0)(ps_2_0)",MD)==false);
	assert(CheckSyntax("<1>Illum@IlluminationVS(%DIFFUSE|%SPECULAR|%BUMP_MAP|%VERTCOLORS|%STAT_BRANCHING)(%_RT_RAE_GEOMTERM)(101)(0)(0)(vs_2_0)",MD)==true);

	assert(CheckSyntax("<660><2>Cloth@Common_SG_VS()(%_RT_QUALITY|%_RT_SHAPEDEFORM|%_RT_SKELETON_SSD|%_RT_HW_PCF_COMPARE)(0)(0)(0)(VS)",MD)==true);
	assert(CheckSyntax("<6452><2>ShadowMaskGen@FrustumClipVolumeVS()()(0)(0)(0)(VS)",MD)==true);
	assert(CheckSyntax("<5604><2>ParticlesNoMat@ParticlePS()(%_RT_FOG|%_RT_AMBIENT|%_RT_ALPHABLEND|%_RT_QUALITY1)(0)(0)(0)(PS)",MD)==true);
	assert(CheckSyntax("<5433><2>SkyHDR@SkyHDRPS(%BILINEAR_FP16)(%_RT_HDR_MODE)(0)(0)(0)(PS)",MD)==true);
	assert(CheckSyntax("<6451><2>PostEffects@BaseVS(%BILINEAR_FP16)()(0)(0)(0)(VS)",MD)==true);
}

//////////////////////////////////////////////////////////////////////////
bool CShaderListFile::Reload()
{
	return Load( m_filename.c_str() );
}

void CShaderListFile::CreatePath(const std::string& rPath)
{
	std::string Path	=	rPath;
	CSTLHelper::Replace(Path,rPath,"\\","/");
	tdEntryVec rToks;
	CSTLHelper::Tokenize(rToks,Path,"/");

	Path="";
	for(size_t a=0;a+1<rToks.size();a++)
	{
		Path+=rToks[a]+"/";
		_mkdir(Path.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
bool CShaderListFile::Load( const char *filename )
{
	CreatePath(filename);
	printf( "Loading ShaderList file: %s\n",filename );
	m_filename = filename;
	m_filenametmp = filename;
	m_filenametmp += ".tmp";
	FILE *f = fopen( filename,"rt" );
	if (!f)
		return false;

	int nNumLines = 0;
	m_entries.clear();
	char str[65535];
	while (fgets(str,sizeof(str),f) != NULL)
	{
		if(*str && InsertLineInternal(str))
			++nNumLines;
	}
	fclose(f);
	if (nNumLines == m_entries.size())
		m_bModified = false;
	else
		m_bModified = true;

	printf( "Loaded %d combination for %s\n",nNumLines,filename );

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CShaderListFile::Save()
{
	//not needed regarding timur, m_entries is just accessed by one thread
	//CCrySimpleMutexAutoLock Lock(m_Mutex);
  
	CreatePath(m_filename);
	if (m_filename.empty())
		return false;

	// write to tmp file
	FILE *f = fopen( m_filenametmp.c_str(),"wt" );
	if(!f)
		return false;
	for (Entries::iterator it = m_entries.begin(); it != m_entries.end(); ++it)
	{
		const char *str = it->first.c_str();
		if(it->second.m_Count==-1)
			fprintf( f,"<%d>%s\n",it->second.m_Version,str);
		else
			fprintf( f,"<%d><%d>%s\n",it->second.m_Count,it->second.m_Version,str);
	}
	fclose(f);

	// first check if original file excists
	f = fopen(m_filename.c_str(), "rt");
	if (f)
	{
		fclose(f);

		// remove original file (keep on trying until success - shadercompiler could currently be copying it for example)
		int sleeptime = 0;
		while (remove(m_filename.c_str()))
		{
			Sleep(100);
			sleeptime += 100;
			if (sleeptime > 5000)
				break;
		}
	}

	{
		int sleeptime = 0;
		while(rename(m_filenametmp.c_str(), m_filename.c_str()))
		{
			Sleep(100);
			sleeptime += 100;
			if (sleeptime > 5000)
				break;
		}
	}

	m_bModified = false;
	return true;
}

//////////////////////////////////////////////////////////////////////////
inline bool IsHexNumberCharacter( const char c )
{
	return (c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F');
}

//////////////////////////////////////////////////////////////////////////
inline bool IsDecNumberCharacter( const char c )
{
	return (c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F');
}

//////////////////////////////////////////////////////////////////////////
inline bool IsNameCharacter( const char c )
{
	return (c>='a' && c<='z') || (c>='0' && c<='9') || (c>='A' && c<='Z') || c=='@' || c=='/' || c=='%' || c=='_';
}

int shGetHex(const char *buf)
{
	if (!buf)
		return 0;
	int i = 0;

	sscanf(buf, "%x", &i);

	return i;
}

//////////////////////////////////////////////////////////////////////////
bool CShaderListFile::CheckSyntax( const char *szLine,SMetaData& rMD,const char **sOutStr)
{
	assert(szLine);
	if(!szLine)
		return false;

	if (sOutStr)
		*sOutStr = 0;

	// e.g. Blurcloak@BlurCloakPS(%BUMP_MAP|%SPECULAR)(%_RT_FOG|%_RT_HDR_MODE|%_RT_BUMP)(0)(0)(0)(ps_2_0)

	const char *p=szLine;

	if (strlen(szLine) < 4)
		return false;

	int Value0	=	0;
	int Value1	=	0;

	if(*p!='<')
		return false;

	char Last=0;
	while(IsDecNumberCharacter(Last=*++p))
	{
		Value0	=	Value0*10+(Last-'0');
	}

	if(*p++!='>')
		return false;

	if(*p=='<')
	{
		while(IsDecNumberCharacter(Last=*++p))
		{
			Value1	=	Value1*10+(Last-'0');
		}

		if(*p++!='>')
			return false;

		rMD.m_Version	=	Value1;
		rMD.m_Count		=	Value0;
	}
	else
	{
		rMD.m_Version	=	Value0;
		rMD.m_Count		=	-1;
	}

	const char* pStart=p;

	// e.g. "Blurcloak@BlurCloakPS"
	while(IsNameCharacter(*p++));
	p--;

	// e.g. "(%BUMP_MAP|%SPECULAR)(%_RT_FOG|%_RT_HDR_MODE|%_RT_BUMP)"
	for(int i=0;i<2;++i)
	{
		if(*p++!='(')
			return false;
		while(true)
		{
			while(IsNameCharacter(*p++));
			p--;
			if(*p!='|')
				break;
			p++;
		}
		if(*p++!=')')
			return false;
	}

	// e.g. "(0)(0)(0)"
	for(int i=0;i<3;++i)
	{
		if(*p++!='(')	
			return false;
		while(IsHexNumberCharacter(*p++));
		p--;
		if(*p++!=')')
			return false;
	}

	// e.g. "(ps_2_0)"
	if(*p++!='(')	
		return false;
	while(IsNameCharacter(*p++));
	p--;
	if(*p++!=')')
		return false;

	// Copy rest of the line.
	if (sOutStr)
		*sOutStr = pStart;

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CShaderListFile::InsertLine( const char *szLine )
{
	if (*szLine != 0)
	{
		CCrySimpleMutexAutoLock Lock(m_Mutex);
		m_newLines.push_back( szLine );
		m_bModified = true;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CShaderListFile::InsertLineInternal( const char *szLine )
{
	const char *szCorrectedLine = 0;
	SMetaData MD;
	if(CheckSyntax(szLine,MD,&szCorrectedLine))
	{
		// Trim \n\r
		char *s = const_cast<char*>(szCorrectedLine);
		for (size_t p = strlen(s)-1; p > 0; p--)
		{
			if (s[p] == '\n' || s[p] == '\r')
				s[p] = '\0';
			else
				break;
		}

		if (szCorrectedLine)
		{
			Entries::iterator it	=	m_entries.find(szCorrectedLine);
			if(it==m_entries.end())
			{
				m_entries[szCorrectedLine]	=	MD;
				m_bModified = true;
			}
			else
			{
				if(it->second.m_Version<MD.m_Version)
				{
					it->second	=	MD;
					m_bModified = true;
				}
				else
				if(it->second.m_Count<MD.m_Count)
				{
					it->second.m_Count=MD.m_Count;
					m_bModified = true;
				}
			}
		}
		return true;
	}


	return false;
}

//////////////////////////////////////////////////////////////////////////
void CShaderListFile::MergeNewLines()
{
	std::vector<std::string> newLines;

	{
		CCrySimpleMutexAutoLock Lock(m_Mutex);
		newLines.swap( m_newLines );
	}
  
	m_bModified = false;

	if (newLines.empty())
		return;

	for (std::vector<std::string>::iterator it = newLines.begin(); it != newLines.end(); ++it)
	{
		InsertLineInternal((*it).c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
void CShaderListFile::MergeNewLinesAndSave()
{
	if (m_bModified)
		MergeNewLines();
	if (m_bModified)
  {
    if(SEnviropment::Instance().m_PrintListUpdates)
      logmessage( "Updating: %s\n",m_listname.c_str() );
		Save();
  }
}
