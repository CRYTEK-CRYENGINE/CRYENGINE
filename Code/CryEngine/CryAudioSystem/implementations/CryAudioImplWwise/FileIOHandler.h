// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <AK/SoundEngine/Common/AkStreamMgrModule.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
class CFileIOHandler final : public AK::StreamMgr::IAkFileLocationResolver, public AK::StreamMgr::IAkIOHookBlocking
{
public:

	CFileIOHandler();
	CFileIOHandler(CFileIOHandler const&) = delete;
	CFileIOHandler(CFileIOHandler&&) = delete;
	CFileIOHandler& operator=(CFileIOHandler const&) = delete;
	CFileIOHandler& operator=(CFileIOHandler&&) = delete;

	AKRESULT        Init(AkDeviceSettings const& rDeviceSettings, bool const bAsyncOpen = false);
	void            ShutDown();

	// IAkFileLocationResolver overrides.
	virtual AKRESULT Open(AkOSChar const* sFileName, AkOpenMode eOpenMode, AkFileSystemFlags* pFlags, bool& rSyncOpen, AkFileDesc& rFileDesc) override;
	virtual AKRESULT Open(AkFileID nFileID, AkOpenMode eOpenMode, AkFileSystemFlags* pFlags, bool& rSyncOpen, AkFileDesc& rFileDesc) override;
	// ~IAkFileLocationResolver overrides.

	// IAkIOHookBlocking overrides.
	virtual AKRESULT Read(AkFileDesc& rFileDesc, AkIoHeuristics const& rHeuristics, void* pBuffer, AkIOTransferInfo& rTransferInfo) override;
	virtual AKRESULT Write(AkFileDesc& in_fileDesc, AkIoHeuristics const& in_heuristics, void* in_pData, AkIOTransferInfo& io_transferInfo) override;
	// ~IAkIOHookBlocking overrides.

	// IAkLowLevelIOHook overrides.
	virtual AKRESULT Close(AkFileDesc& rFileDesc) override;
	virtual AkUInt32 GetBlockSize(AkFileDesc& in_fileDesc) override;
	virtual void     GetDeviceDesc(AkDeviceDesc& out_deviceDesc) override;
	virtual AkUInt32 GetDeviceData() override;
	// ~IAkLowLevelIOHook overrides.

	void SetBankPath(AkOSChar const* const sBankPath);
	void SetLanguageFolder(AkOSChar const* const sLanguageFolder);

private:

	AkOSChar   m_sBankPath[AK_MAX_PATH];
	AkOSChar   m_sLanguageFolder[AK_MAX_PATH];
	bool       m_bAsyncOpen;
	AkDeviceID m_nDeviceID;
};
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
