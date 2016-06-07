// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <AK/SoundEngine/Common/AkStreamMgrModule.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
class CFileIOHandler : public AK::StreamMgr::IAkFileLocationResolver, public AK::StreamMgr::IAkIOHookBlocking
{
public:

	CFileIOHandler();
	virtual ~CFileIOHandler();

	AKRESULT Init(AkDeviceSettings const& rDeviceSettings, bool const bAsyncOpen = false);
	void     ShutDown();

	// IAkFileLocationResolver overrides.
	virtual AKRESULT Open(AkOSChar const* sFileName, AkOpenMode eOpenMode, AkFileSystemFlags* pFlags, bool& rSyncOpen, AkFileDesc& rFileDesc);
	virtual AKRESULT Open(AkFileID nFileID, AkOpenMode eOpenMode, AkFileSystemFlags* pFlags, bool& rSyncOpen, AkFileDesc& rFileDesc);
	// ~IAkFileLocationResolver overrides.

	// IAkIOHookBlocking overrides.
	virtual AKRESULT Read(AkFileDesc& rFileDesc, AkIoHeuristics const& rHeuristics, void* pBuffer, AkIOTransferInfo& rTransferInfo);
	virtual AKRESULT Write(AkFileDesc& in_fileDesc, AkIoHeuristics const& in_heuristics, void* in_pData, AkIOTransferInfo& io_transferInfo);
	// ~IAkIOHookBlocking overrides.

	// IAkLowLevelIOHook overrides.
	virtual AKRESULT Close(AkFileDesc& rFileDesc);
	virtual AkUInt32 GetBlockSize(AkFileDesc& in_fileDesc);
	virtual void     GetDeviceDesc(AkDeviceDesc& out_deviceDesc);
	virtual AkUInt32 GetDeviceData();
	// ~IAkLowLevelIOHook overrides.

	void SetBankPath(AkOSChar const* const sBankPath);
	void SetLanguageFolder(AkOSChar const* const sLanguageFolder);

private:

	AkOSChar   m_sBankPath[AK_MAX_PATH];
	AkOSChar   m_sLanguageFolder[AK_MAX_PATH];
	bool       m_bAsyncOpen;
	AkDeviceID m_nDeviceID;

	PREVENT_OBJECT_COPY(CFileIOHandler);
};
}
}
}
