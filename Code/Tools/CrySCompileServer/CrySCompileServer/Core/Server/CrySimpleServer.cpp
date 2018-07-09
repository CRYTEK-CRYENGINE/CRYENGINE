// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "../StdTypes.hpp"
#ifdef _MSC_VER
#include <process.h>
#include <direct.h>
#endif
#ifdef UNIX
#include <pthread.h>
#endif
#include "../Error.hpp"
#include "../STLHelper.hpp"
#include "../tinyxml/tinyxml.h"
#include "CrySimpleServer.hpp"
#include "CrySimpleSock.hpp"
#include "CrySimpleJob.hpp"
#include "CrySimpleJobCompile1.hpp"
#include "CrySimpleJobCompile2.hpp"
#include "CrySimpleJobRequest.hpp"
#include "CrySimpleCache.hpp"
#include "CrySimpleErrorLog.hpp"
#include "../Mailer.h"
#include "ShaderList.hpp"
#include <assert.h>
#include <io.h>
#include <algorithm>
#include <memory>


#ifdef WIN32
	#define EXTENSION ".exe"
#else
	#define EXTENSION ""
#endif

volatile long CCrySimpleServer::ms_ExceptionCount	= 0;

const static std::string SHADER_STRIPPER					=	"sce-cgcstrip" EXTENSION;
const static std::string SHADER_COMPILER					=	"sce-cgc" EXTENSION;
const static std::string SHADER_DISASSEMBLER			=	"sce-cgcdisasm" EXTENSION;
const static std::string SHADER_PROFILER					=	"NVShaderPerf" EXTENSION;

const static std::string SHADER_PATH_SOURCE				=	"Source";
const static std::string SHADER_PATH_BINARY				=	"Binary";
const static std::string SHADER_PATH_HALFSTRIPPED	=	"HalfStripped";
const static std::string SHADER_PATH_DISASSEMBLED	=	"DisAsm";
const static std::string SHADER_PATH_STRIPPPED		=	"Stripped";
const static std::string SHADER_PATH_CACHE				=	"Cache";

static SEnviropment gEnv;

SEnviropment&	SEnviropment::Instance()
{
	return gEnv;
}

class CThreadData
{
	uint32_t		m_Counter;
	CCrySimpleSock*	m_pSock;
public:
							CThreadData(uint32_t		Counter,CCrySimpleSock*	pSock):
							m_Counter(Counter),
							m_pSock(pSock){}

							~CThreadData(){delete m_pSock;}

	CCrySimpleSock*	Socket(){return m_pSock;}
	uint32_t		ID()const{return m_Counter;}

};

struct SFileInfo
{
	std::string file;
	__time64_t  time_create;
};

//////////////////////////////////////////////////////////////////////////
inline bool CompareFileInfo( const SFileInfo &f1,const SFileInfo &f2 )
{
	return f1.time_create < f2.time_create;
}

//////////////////////////////////////////////////////////////////////////
void ScanDirectory( const char *path,std::vector<SFileInfo> &files )
{
	__finddata64_t c_file;
	intptr_t hFile;
	char full_path[MAX_PATH];
	char file_spec[MAX_PATH];
	char temp_path[MAX_PATH];

	strcpy_s(full_path,path);
	// Trim ending slash
	if (full_path[strlen(path)-1] == '/' || full_path[strlen(path)-1] == '\\')
		full_path[strlen(path)-1] = 0;

	sprintf_s(file_spec,"%s/*.*",full_path);

	if( (hFile = _findfirst64( file_spec, &c_file )) != -1L ) {
		do {
			if (c_file.attrib & _A_SUBDIR)
			{
				if (c_file.name[0] != '.')
				{
					sprintf_s(temp_path, "%s/%s", full_path, c_file.name);
					ScanDirectory(temp_path,files);
				}
			}
			else
			{
				sprintf_s(temp_path, "%s/%s", full_path, c_file.name);
				SFileInfo f;
				f.file = temp_path;
				f.time_create = c_file.time_create;
				files.push_back( f );
			}
		} while( _findnext64( hFile, &c_file ) == 0 );

		_findclose( hFile );
	}
}

void MakeErrorVec(const std::string &errorText, tdDataVector &Vec)
{
  Vec.resize(errorText.size()+1);
  for (size_t i = 0; i < errorText.size();i++)
  {
    Vec[i] = errorText[i];
  }
  Vec[errorText.size()] = 0;

  // Compress output data
  tdDataVector rDataRaw;
  rDataRaw.swap(Vec);
  if (!CSTLHelper::Compress( rDataRaw,Vec ))
    Vec.resize(0);
}

//////////////////////////////////////////////////////////////////////////
uint32_t __stdcall Compile(void* pData)
{
	std::auto_ptr<CThreadData> pThreadData	=	std::auto_ptr<CThreadData>(reinterpret_cast<CThreadData*>(pData));
	std::vector<uint8_t> Vec;
	std::auto_ptr<CCrySimpleJob> Job; 
	EProtocolVersion Version	=	EPV_V001;
	ECrySimpleJobState State	=	ECSJS_JOBNOTFOUND;
	try
	{
		if(pThreadData->Socket()->Recv(Vec))
		{
			std::string Request(reinterpret_cast<const char*>(&Vec[0]),Vec.size());
			TiXmlDocument ReqParsed( "Request.xml" );
			ReqParsed.Parse( Request.c_str() );

			if(ReqParsed.Error())
			{
				CrySimple_ERROR("failed to parse request XML");
				return 0;
			}
			const TiXmlElement* pElement = ReqParsed.FirstChildElement();
			if(!pElement)
			{
				CrySimple_ERROR("failed to extract First Element of the request");
				return false;
			}
			const char* pVersion			=	pElement->Attribute("Version");

			//new request type?
			if(pVersion)
			{
				if (std::string(pVersion)=="2.1")	{
					Version	=	EPV_V0021;
				} else if (std::string(pVersion)=="2.0") {
					Version	=	EPV_V002;
				}
			}

			if (Version >= EPV_V002)
			{
				if (Version >= EPV_V0021)
				{
					pThreadData->Socket()->WaitForShutDownEvent(true);
				}

				const char* pJobType	=	pElement->Attribute("JobType");
				if(pJobType)
				{
					const std::string JobType(pJobType);
					if(JobType=="RequestLine")
					{
						Job	=	std::auto_ptr<CCrySimpleJob>(new CCrySimpleJobRequest(pThreadData->Socket()->PeerIP()));
						Job->Execute(pElement);
						State	=	Job->State();
						Vec.resize(0);
					}
					else
						if(JobType=="Compile")
						{
							Job	=	std::auto_ptr<CCrySimpleJob>(new CCrySimpleJobCompile2(pThreadData->Socket()->PeerIP(), &Vec) );
							Job->Execute(pElement);
							State	=	Job->State();
						}
						else
							printf("\nRequested unkown job %s\n",pJobType);
				}
				else
					printf("\nVersion 2.0 or higher but has no JobType tag\n");
			}			
			else
			{
				//legacy request
				Version	=	EPV_V001;
				Job	=	std::auto_ptr<CCrySimpleJob>(new CCrySimpleJobCompile1(pThreadData->Socket()->PeerIP(), &Vec) );
				Job->Execute(pElement);
			}
			pThreadData->Socket()->Send(Vec,State,Version);

			if (Version >= EPV_V0021)
			{
				/*
				// wait until message has been succesfully delived before shutting down the connection
				if(!pThreadData->Socket()->RecvResult())
				{
					printf("\nInvalid result from client\n");
				}
				*/
			}

		}
	}
	catch(const ICryError *err)
	{
		CCrySimpleServer::IncrementExceptionCount();

		CRYSIMPLE_LOG( "<Error> "+err->GetErrorName() );
		
    std::string returnStr = err->GetErrorDetails(ICryError::OUTPUT_TTY);

		// Send error back
    MakeErrorVec(returnStr, Vec);

		if (Job.get())
    {
			State	=	Job->State();
    
      if(State == ECSJS_ERROR_COMPILE && SEnviropment::Instance().m_PrintErrors)
      {
        printf("\nXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
        printf("%s\n", err->GetErrorName().c_str());
        printf("%s\n", returnStr.c_str());
        printf("\nXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n\n");
      }
    }

    bool added = CCrySimpleErrorLog::Instance().Add((ICryError*)err);

    // error log hasn't taken ownership, delete this error.
    if(!added)
    {
      delete err;
    }

		pThreadData->Socket()->Send(Vec,State,Version);
	}
	//delete pThreadData;
	return 0;
}

//////////////////////////////////////////////////////////////////////////
uint32_t __stdcall TickThread(void* pData)
{
	DWORD t0,t1;
	t0 = GetTickCount();
	while (true)
	{
    CrySimple_SECURE_START

		t1 = GetTickCount();
		if ((t1 < t0) || (t1 - t0 > 100))
		{
			t0 = t1;
			char str[512];
			sprintf_s(str,"Crytek Remote Shader Compiler (%d compile tasks | %d open sockets | %d exceptions)",
				CCrySimpleJobCompile::GlobalCompileTasks(), CCrySimpleSock::GetOpenSockets() + CSMTPMailer::GetOpenSockets(),
				CCrySimpleServer::GetExceptionCount());
			SetConsoleTitle(str);
		}

		const uint32_t T1	=	GetTickCount();
		CCrySimpleErrorLog::Instance().Tick();
		CShaderList::Instance().Tick();
		CCrySimpleCache::Instance().ThreadFunc_SavePendingCacheEntries();
		const uint32_t T2	=	GetTickCount();
		if(T2-T1<100)
			Sleep(100-T2+T1);

    CrySimple_SECURE_END
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
uint32_t __stdcall LoadCache(void* pUserData)
{

	if(CCrySimpleCache::Instance().LoadCacheFile( SEnviropment::Instance().m_Cache+"Cache.dat" ))
	{
#ifdef WIN32
		printf( "Creating cache backup...\n" );
		DeleteFile( (SEnviropment::Instance().m_Cache+"Cache.bak2").c_str() );
		printf( "Move %s to %s\n",(SEnviropment::Instance().m_Cache+"Cache.bak").c_str(),(SEnviropment::Instance().m_Cache+"Cache.bak2").c_str() );
		MoveFile( (SEnviropment::Instance().m_Cache+"Cache.bak").c_str(),(SEnviropment::Instance().m_Cache+"Cache.bak2").c_str() );
		printf( "Copy %s to %s\n",(SEnviropment::Instance().m_Cache+"Cache.dat").c_str(),(SEnviropment::Instance().m_Cache+"Cache.bak").c_str() );
		CopyFile( (SEnviropment::Instance().m_Cache+"Cache.dat").c_str(),(SEnviropment::Instance().m_Cache+"Cache.bak").c_str(),FALSE );
		printf( "Cache backup done.\n" );
#endif
	}
	else
	{
		// Restoring backup cache!
		printf( "Cache file corrupted!!!\n" );
		printf( "Restoring backup cache...\n" );
		DeleteFile( (SEnviropment::Instance().m_Cache+"Cache.dat").c_str() );
		printf( "Copy %s to %s\n",(SEnviropment::Instance().m_Cache+"Cache.bak").c_str(),(SEnviropment::Instance().m_Cache+"Cache.dat").c_str() );
		CopyFile( (SEnviropment::Instance().m_Cache+"Cache.bak").c_str(),(SEnviropment::Instance().m_Cache+"Cache.dat").c_str(),FALSE );
		if (!CCrySimpleCache::Instance().LoadCacheFile( SEnviropment::Instance().m_Cache+"Cache.dat" ))
		{
			// Backup file corrupted too!
			printf( "Backup file corrupted too!!!\n" );
			printf( "Deleting cache completely\n" );
			DeleteFile( (SEnviropment::Instance().m_Cache+"Cache.dat").c_str() );
		}
	}

	CCrySimpleCache::Instance().Finalize();
	printf( "Ready\n" );


	/*
	printf( "Scanning Directory: %s\n",SEnviropment::Instance().m_Cache.c_str() );

	CCrySimpleCache::Instance().LoadCacheFile( SEnviropment::Instance().m_Cache+"Cache.dat" );

	DWORD t0 = GetTickCount();

	std::vector<SFileInfo> FileList;
	FileList.reserve( 1000000 );
	//feeding cache with existing files
	//ScanDirectory( SEnviropment::Instance().m_Cache.c_str(),FileList );

	// Sort by creation time, to minimize seek time of file reads.
	std::sort( FileList.begin(),FileList.end(),CompareFileInfo );

	if (FileList.size() > 0)
	{
		char fname[_MAX_FNAME];
		std::vector<uint8_t> Data;
		Data.resize(100000);

		printf("Loading Cache:   0%%");
		uint32_t num=0;
		uint32_t Loaded=0;
		for(uint32_t a=0,Size=(uint32_t)FileList.size();a<Size;a++)
		{
			//if(a*100/Size!=Loaded)
			if (a%10 == 0)
			{
				DWORD t = GetTickCount();

				Loaded	=	static_cast<uint32_t>(a*100/Size);
				printf("\rLoading Cache: %3d%%   %6d  time=%d sec ",Loaded,static_cast<uint32_t>(a),(t-t0)/1000);
			}
			const std::string &Entry	=	FileList[a].file;
			if(Entry.size()<32)
				continue;
			const std::string &Name = &Entry.c_str()[Entry.size()-32];

			_splitpath( Entry.c_str(),NULL,NULL,fname,NULL );

			assert( strcmp( Name.c_str(),CSTLHelper::Hash2String(CSTLHelper::String2Hash(fname)).c_str()) == 0 );
			const tdHash	Hash	=	CSTLHelper::String2Hash(fname);

			if (CSTLHelper::FromFile(Entry,Data))
			{
				CCrySimpleCache::Instance().AddToHash(Hash,Data);
			}
			num=a;
		}
		DWORD t = GetTickCount();
		printf("\rLoading Cache: 100%% done %d shaders loaded in %d second  \n",num+1,(t-t0)/1000 );
	}
	*/
	return 0;
}


//////////////////////////////////////////////////////////////////////////
CCrySimpleServer::CCrySimpleServer(const char* pShaderModel,const char* pDst,const char* pSrc,const char* pEntryFunction):
m_pServerSocket(0)
{
	Init();
}

CCrySimpleServer::CCrySimpleServer():
m_pServerSocket(0)
{
	CrySimple_SECURE_START

		uint32_t Port = SEnviropment::Instance().m_port;

		m_pServerSocket	=	new CCrySimpleSock(Port);
		Init();
		m_pServerSocket->Listen();

		{
			uint32_t Ret;
			CloseHandle(reinterpret_cast<HANDLE>(_beginthreadex(0,0,TickThread,0,0,&Ret)));
		}

		uint32_t JobCounter=0;
		while(1)
		{
			CThreadData* pData	=	new CThreadData(JobCounter++,m_pServerSocket->Accept());
			uint32_t Ret;
			CloseHandle(reinterpret_cast<HANDLE>(_beginthreadex(0,0,Compile,(void*)pData,0,&Ret)));
		}
	CrySimple_SECURE_END
}

void CCrySimpleServer::Init()
{
	//creating cache file structure
	_mkdir("Error");
	_mkdir("Cache");

	char CurrentDir[1024];
	GetCurrentDirectory(sizeof(CurrentDir),CurrentDir);
	SEnviropment::Instance().m_Root			=	CurrentDir;
	SEnviropment::Instance().m_Root			+=	"\\";

	SEnviropment::Instance().m_Compiler	=	SEnviropment::Instance().m_Root+"Compiler\\";
	SEnviropment::Instance().m_Cache		=	SEnviropment::Instance().m_Root+"Cache\\";
	if (SEnviropment::Instance().m_Temp.empty())
		SEnviropment::Instance().m_Temp			=	SEnviropment::Instance().m_Root+"Temp\\";
	if (SEnviropment::Instance().m_Error.empty())
		SEnviropment::Instance().m_Error			=	SEnviropment::Instance().m_Root+"Error\\";

	_mkdir( SEnviropment::Instance().m_Error.c_str() );
	_mkdir( SEnviropment::Instance().m_Temp.c_str() );
	_mkdir( SEnviropment::Instance().m_Cache.c_str() );


//	CShaderList::Instance().m_PC.Load( (SEnviropment::Instance().m_Cache+"ShaderList_PC.txt").c_str() );
//	CShaderList::Instance().m_X360.Load( (SEnviropment::Instance().m_Cache+"ShaderList_X360.txt").c_str() );
//	CShaderList::Instance().m_PS3.Load( (SEnviropment::Instance().m_Cache+"ShaderList_PS3.txt").c_str() );


	if(SEnviropment::Instance().m_Caching)
	{
		uint32_t Ret;
		CloseHandle(reinterpret_cast<HANDLE>(_beginthreadex(0,0,LoadCache,NULL,0,&Ret)));
	}
	else
		printf("\nNO CACHING, disabled by config\n");
	


}

void CCrySimpleServer::IncrementExceptionCount()
{
	InterlockedIncrement(&ms_ExceptionCount);
}
