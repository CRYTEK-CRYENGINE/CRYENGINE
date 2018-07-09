#include "StdAfx.h"

#include <steam/steam_api.h>

#include "SteamPlatform.h"
#include "SteamRemoteFile.h"

namespace Cry
{
	namespace GamePlatform
	{
		namespace Steam
		{
			CRemoteFile::CRemoteFile(const char* name)
				: m_name(name)
				, m_sharedHandle(0)
			{
			}

			int CRemoteFile::GetFileSize() const
			{
				ISteamRemoteStorage* pSteamRemoteStorage = SteamRemoteStorage();
				if (!pSteamRemoteStorage || m_name.empty())
					return 0;

				return pSteamRemoteStorage->GetFileSize(m_name.c_str());
			}

			bool CRemoteFile::Share()
			{
				ISteamRemoteStorage* pSteamRemoteStorage = SteamRemoteStorage();
				if (pSteamRemoteStorage && m_sharedHandle == 0)
				{
					SteamAPICall_t hSteamAPICall = pSteamRemoteStorage->FileShare(m_name.c_str());
					m_callResultFileShared.Set(hSteamAPICall, this, &CRemoteFile::OnFileShared);

					CPlugin::GetInstance()->SetAwaitingCallback(1);

					return true;
				}

				return false;
			}

			bool CRemoteFile::Exists() const
			{
				if (m_name.empty())
					return false;

				if (ISteamRemoteStorage* pSteamRemoteStorage = SteamRemoteStorage())
					return pSteamRemoteStorage->FileExists(m_name);

				return false;
			}

			bool CRemoteFile::Write(const char* pData, int length)
			{
				if (m_name.length() == 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Steam] Attempted to write remote file with empty name");
					return false;
				}

				if (ISteamRemoteStorage* pSteamRemoteStorage = SteamRemoteStorage())
				{
					bool bResult = pSteamRemoteStorage->FileWrite(m_name, pData, length);
					if (!bResult)
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Steam] Failed to write %i bytes to remote file %s", length, m_name.c_str());

					return bResult;
				}

				return false;
			}

			bool CRemoteFile::Read(std::vector<char>& bufferOut)
			{
				if (m_name.length() == 0)
				{
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "[Steam] Attempted to read remote file with empty name");
					return false;
				}

				if (ISteamRemoteStorage* pSteamRemoteStorage = SteamRemoteStorage())
				{
					bufferOut.resize(pSteamRemoteStorage->GetFileSize(m_name));
					if (bufferOut.size() == 0)
						return false;

					int read = pSteamRemoteStorage->FileRead(m_name, bufferOut.data(), bufferOut.size());

					return read >= bufferOut.size();
				}

				return false;
			}

			bool CRemoteFile::Delete()
			{
				ISteamRemoteStorage* pSteamRemoteStorage = SteamRemoteStorage();
				if (!pSteamRemoteStorage || m_name.length() == 0)
					return false;

				return pSteamRemoteStorage->FileDelete(m_name.c_str());
			}

			// Steam callbacks
			void CRemoteFile::OnFileShared(RemoteStorageFileShareResult_t* pResult, bool bIOFailure)
			{
				CPlugin::GetInstance()->SetAwaitingCallback(-1);

				if (pResult->m_eResult == k_EResultOK)
				{
					m_sharedHandle = pResult->m_hFile;

					for (auto it = m_listeners.begin(); it != m_listeners.end();)
					{
						IListener* pListener =* it;
						if (pListener->OnFileShared(this) == false)
						{
							it = m_listeners.erase(it);
						}
						else
							++it;
					}
				}
			}
		}
	}
}