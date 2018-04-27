// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   CImage.cpp : Common Image class implementation.

   Revision history:
* Created by Khonich Andrey

   =============================================================================*/

#include "StdAfx.h"
#include "CImage.h"
#include "DDSImage.h"

#include "../TextureCompiler.h"       // CTextureCompiler

CImageFile::CImageFile(const string& filename) : m_FileName(filename)
{
	m_nRefCount = 0;
	m_pByteImage[0] = NULL;
	m_pByteImage[1] = NULL;
	m_pByteImage[2] = NULL;
	m_pByteImage[3] = NULL;
	m_pByteImage[4] = NULL;
	m_pByteImage[5] = NULL;

	m_eError = eIFE_OK;
	m_eFormat = eTF_Unknown;
	m_eTileMode = eTM_None;
	m_NumMips = 0;
	m_NumPersistantMips = 0;
	m_Flags = 0;
	m_ImgSize = 0;
	m_Depth = 1;
	m_nStartSeek = 0;
	m_Sides = 1;

	m_pStreamState = NULL;
}

CImageFile::~CImageFile()
{
	mfAbortStreaming();

	for (int i = 0; i < 6; i++)
		mfFree_image(i);
}

void CImageFile::mfSet_dimensions(const int w, const int h)
{
	m_Width = w;
	m_Height = h;
}

void CImageFile::mfSet_error(const EImFileError error, const char* detail)
{
	m_eError = error;
	if (detail)
	{
		TextureWarning(m_FileName.c_str(), "%s", detail);
	}
}

namespace
{
struct DDSCallback : public IImageFileStreamCallback
{
	DDSCallback()
		: ok(false)
	{
	}
	virtual void OnImageFileStreamComplete(CImageFile* pImFile)
	{
		ok = pImFile != NULL;
		waitEvent.Set();
	};
	CryEvent      waitEvent;
	volatile bool ok;
};
}

_smart_ptr<CImageFile> CImageFile::mfLoad_file(const string& filename, const uint32 nFlags)
{
	string sFileToLoad;
	{
		char buffer[512];
		CResourceCompilerHelper::GetOutputFilename(filename, buffer, sizeof(buffer));   // change filename: e.g. instead of TIF, pass DDS
		sFileToLoad = buffer;
	}

	char ext[16];
	EResourceCompilerResult result = mfInvokeRC(sFileToLoad, filename, ext, 16, (nFlags & FIM_IMMEDIATE_RC) != 0);
	if (result == EResourceCompilerResult::Failed || result == EResourceCompilerResult::Queued)
		return NULL;

	_smart_ptr<CImageFile> pImageFile;

	// Try DDS first
	if (!strcmp(ext, "dds"))
	{
		if (!(nFlags & FIM_READ_VIA_STREAMS))
		{
			pImageFile = new CImageDDSFile(sFileToLoad, nFlags);
		}
		else
		{
			_smart_ptr<CImageDDSFile> pImageDDSFile;
			pImageDDSFile = new CImageDDSFile(sFileToLoad);

			std::unique_ptr<DDSCallback> cb(new DDSCallback);
			if (!pImageDDSFile->Stream(nFlags, &*cb))
				return _smart_ptr<CImageFile>();

			cb->waitEvent.Wait();

			if (!cb->ok)
				return _smart_ptr<CImageFile>();

			pImageFile = pImageDDSFile;
		}
	}
	else
		TextureWarning(sFileToLoad.c_str(), "Unsupported texture extension '%s': '%s'", ext, filename.c_str());

	if (pImageFile && pImageFile->mfGet_error() != eIFE_OK)
		return _smart_ptr<CImageFile>();

	return pImageFile;
}

_smart_ptr<CImageFile> CImageFile::mfStream_File(const string& filename, const uint32 nFlags, IImageFileStreamCallback* pCallback)
{
	string sFileToLoad;
	{
		char buffer[512];
		CResourceCompilerHelper::GetOutputFilename(filename, buffer, sizeof(buffer));   // change filename: e.g. instead of TIF, pass DDS
		sFileToLoad = buffer;
	}

	char ext[16];
	EResourceCompilerResult result = mfInvokeRC(sFileToLoad, filename, ext, 16, true);
	if (result == EResourceCompilerResult::Failed || result == EResourceCompilerResult::Queued)
		return NULL;

	_smart_ptr<CImageFile> pImageFile;

	// Try DDS first
	if (!strcmp(ext, "dds"))
	{
		CImageDDSFile* pDDS = new CImageDDSFile(sFileToLoad);
		pImageFile = pDDS;
		pDDS->Stream(nFlags, pCallback);
	}
	else
		TextureWarning(sFileToLoad.c_str(), "Unsupported texture extension '%s': '%s'", ext, filename.c_str());

	return pImageFile;
}

CImageFile::EResourceCompilerResult CImageFile::mfInvokeRC(const string& sFileToLoad, const string& filename, char* extOut, size_t extOutCapacity, bool immediate)
{
	cry_strcpy(extOut, extOutCapacity, PathUtil::GetExt(sFileToLoad));

	// is it needed to invoke the resource compiler?
#if defined(CRY_ENABLE_RC_HELPER)
	if (CRenderer::CV_r_rc_autoinvoke != 0 && CTextureCompiler::IsImageFormatSupported(extOut))
	{
		if (/*!gEnv->pRenderer->EF_Query(EFQ_Fullscreen) && */ gEnv->pSystem->IsDevMode())
		{
			CTextureCompiler& txCompiler = CTextureCompiler::GetInstance();
			char buffer[512];

			CTextureCompiler::EResult result = txCompiler.ProcessTextureIfNeeded(filename, buffer, sizeof(buffer), immediate);

			string sFileToLoad_enable_rc_helper = buffer;
			cry_strcpy(extOut, extOutCapacity, PathUtil::GetExt(sFileToLoad_enable_rc_helper)); // update extension

			switch(result)
			{
				case CTextureCompiler::EResult::AlreadyCompiled:
					return EResourceCompilerResult::AlreadyCompiled;
				case CTextureCompiler::EResult::Available:
					return EResourceCompilerResult::Available;
				case CTextureCompiler::EResult::Queued:
					return EResourceCompilerResult::Queued;
				case CTextureCompiler::EResult::Failed:
				default:
				{
					gEnv->pLog->LogError("ProcessTextureIfNeeded() failed (missing rc.exe?)");
					return EResourceCompilerResult::Failed;
				}
				break;
			}
		}
		else
		{
			gEnv->pLog->LogWarning("r_rc_autoinvoke of '%s' suppressed (full screen or non DevMode)", filename.c_str());
		}
	}
#endif //CRY_ENABLE_RC_HELPER

	return EResourceCompilerResult::Skipped;
}

void CImageFile::mfFree_image(const int nSide)
{
	SAFE_DELETE_ARRAY(m_pByteImage[nSide]);
}

byte* CImageFile::mfGet_image(const int nSide)
{
	if (!m_pByteImage[nSide] && m_ImgSize)
		m_pByteImage[nSide] = new byte[m_ImgSize];
	return m_pByteImage[nSide];
}

void CImageFile::mfAbortStreaming()
{
	if (m_pStreamState)
	{
		for (int i = 0; i < m_pStreamState->MaxStreams; ++i)
		{
			if (m_pStreamState->m_pStreams[i])
				m_pStreamState->m_pStreams[i]->Abort();
		}
		delete m_pStreamState;
		m_pStreamState = NULL;
	}
}
