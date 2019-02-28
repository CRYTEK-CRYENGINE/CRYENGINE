// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: CCryLobbyUI implementation for Durango

   -------------------------------------------------------------------------
   History:
   - 26:06:2013 : Created by Yeonwoon JUNG

*************************************************************************/

#include "StdAfx.h"

#include "CryDurangoLiveOnlineStorage.h"

#if USE_DURANGOLIVE && USE_CRY_ONLINE_STORAGE

	#ifndef RELEASE
		#define DEBUG_ONLINE_STORAGE_ALLOW_TRACE
	#endif

	#include "CryDurangoLiveLobby.h"

	#include <robuffer.h>
	#include <CryCore/Platform/IPlatformOS.h>
	#include <CryString/UnicodeFunctions.h>

	#pragma warning(push)
	#pragma warning(disable:6102)

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Storage::Streams;
using namespace ABI::Windows::Xbox::System;
using namespace ABI::Microsoft::Xbox::Services;

namespace // anonymous
{
byte* GetBytePointer(ABI::Windows::Storage::Streams::IBuffer* pBuffer) noexcept
{
	byte* bytes = nullptr;

	IUnknown* unknown = reinterpret_cast<IUnknown*>(pBuffer);

	Microsoft::WRL::ComPtr<::Windows::Storage::Streams::IBufferByteAccess> bufferByteAccess;
	HRESULT hr = unknown->QueryInterface(_uuidof(::Windows::Storage::Streams::IBufferByteAccess), &bufferByteAccess);
	if (SUCCEEDED(hr))
	{
		bufferByteAccess->Buffer(&bytes);
	}

	return bytes;
}
}

//////////////////////////////////////////////////////////////////////////
class CAsyncInfoPimpl
{
public:
	template<typename T>
	void Set(ComPtr<IAsyncOperation<T>> op)
	{
		op.As(&pAction);
	}

	ABI::Windows::Foundation::AsyncStatus Status() const noexcept
	{
		ABI::Windows::Foundation::AsyncStatus status = ABI::Windows::Foundation::AsyncStatus::Error;
		if (pAction.Get())
		{
			HRESULT hr = pAction->get_Status(&status);
			assert(SUCCEEDED(hr));
		}
		return status;
	}

	HRESULT ErrorCode() const noexcept
	{
		HRESULT errorCode = S_FALSE;
		if (pAction.Get())
		{
			HRESULT hr = pAction->get_ErrorCode(&errorCode);
			assert(SUCCEEDED(hr));
		}
		return errorCode;
	}

	template<typename T>
	HRESULT GetResults(ComPtr<T>& results) const noexcept
	{
		ComPtr<IAsyncOperation<TitleStorage::TitleStorageBlobResult*>> asyncOp;
		HRESULT hr = pAction.As(&asyncOp);
		assert(SUCCEEDED(hr));
		if (FAILED(hr))
		{
			return hr;
		}

		hr = asyncOp->GetResults(&results);
		assert(SUCCEEDED(hr));
		return hr;
	}

private:
	ComPtr<IAsyncInfo> pAction;
};

//////////////////////////////////////////////////////////////////////////
CCryDurangoLiveOnlineStorage::CCryDurangoLiveOnlineStorage(CCryLobby* pLobby, CCryLobbyService* pService)
	: m_pendingCount(0)
	, m_status(eCOSS_Idle)
	, m_pAsyncOperation(new CAsyncInfoPimpl())
	, m_pLobby(pLobby)
	, m_pService(pService)
{
	memset(&m_pendingQueries, 0, sizeof(CryOnlineStorageQueryData) * MAXIMUM_PENDING_QUERIES);
}

namespace
{
void GetCurrentUser(ComPtr<IUser>& user, boolean& isSignedIn, boolean& isGuest)
{
	ISystem* pSystem = GetISystem();
	if (pSystem)
	{
		int userIndex = pSystem->GetPlatformOS()->GetFirstSignedInUser();
		if (userIndex != IPlatformOS::Unknown_User)
		{
			int userId = pSystem->GetPlatformOS()->UserGetId(userIndex);
			HRESULT hr = Interface::Statics<IUser>()->GetUserById(userId, &user);
		}
	}
	if (user.Get())
	{
		user->get_IsSignedIn(&isSignedIn);
		user->get_IsGuest(&isGuest);
	}
	else
	{
		isSignedIn = false;
		isGuest = true;
	}
}

void DebugProfileLogCurrentUser(const char* reason, int queryId)
{
	#ifdef DEBUG_ONLINE_STORAGE_ALLOW_TRACE
	ComPtr<IUser> user;
	boolean isSignedIn = false;
	boolean isGuest = true;
	HString XboxUserId;
	GetCurrentUser(user, isSignedIn, isGuest);
	if (user.Get())
	{
		user->get_XboxUserId(XboxUserId.GetAddressOf());
	}
	CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_WARNING, "[>PROFILE_DEBUG<][ONLINE] = = = = = = %s = = = = = (?)", reason);
	CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_WARNING, "[>PROFILE_DEBUG<][ONLINE] signed in = %s", isSignedIn ? "true" : "false");
	CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_WARNING, "[>PROFILE_DEBUG<][ONLINE] guest = %s", isGuest ? "true" : "false");
	CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_WARNING, "[>PROFILE_DEBUG<][ONLINE] userId = %s", user.Get() ? (const char*)XboxUserId.Get() : "#####NoId#####");
	CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_WARNING, "[>PROFILE_DEBUG<][ONLINE] (optional query id = %d)", queryId);
	CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_WARNING, "[>PROFILE_DEBUG<][ONLINE] ~ ~ ~ ~ ~ ~ %s ~ ~ ~ ~ ~", reason);
	#endif // DEBUG_ONLINE_STORAGE_ALLOW_TRACE
}
}

void CCryDurangoLiveOnlineStorage::Tick(CTimeValue tv)
{
	DWORD liveStatus = ERROR_IO_PENDING;

	switch (m_status)
	{
	case eCOSS_Idle:
		if (m_pendingCount)
		{
			CryOnlineStorageQueryData* pQueryData = &m_pendingQueries[0];

			ComPtr<IUser> user;
			boolean isSignedIn = false;
			boolean isGuest = true;
			GetCurrentUser(user, isSignedIn, isGuest);

			if (!user.Get() || !isSignedIn || isGuest)
			{
				CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_WARNING, "CCryDurangoLiveOnlineStorage::Tick - Trying to query title managed storage while offline");
				liveStatus = !ERROR_SUCCESS;
				break;
			}

			pQueryData->eResult = eCLE_Success;

			CCryDurangoLiveLobbyService* pDurangoLiveLobbyService = (CCryDurangoLiveLobbyService*)m_pService;

			wstring blobPathW;
			Unicode::Convert(blobPathW, pQueryData->szItemURL);

			ComPtr<TitleStorage::ITitleStorageBlobMetadata> blobMetadata;
			switch (pQueryData->dataType)
			{
			case eCOSDT_Json:
				{
					blobPathW += L".json";

					HString scid;
					pDurangoLiveLobbyService->GetServiceConfigID(scid.GetAddressOf());

					HString blobPath;
					blobPath.Set(blobPathW);

					HString XboxUserId;
					if (pQueryData->targetXuid && *pQueryData->targetXuid)
					{
						XboxUserId.Set(pQueryData->targetXuid);
					}
					else
					{
						user->get_XboxUserId(XboxUserId.GetAddressOf());
					}

					Activation::Factory<TitleStorage::TitleStorageBlobMetadata>()->CreateInstance3(scid.Get()
					                                                                               , TitleStorage::TitleStorageType_JsonStorage
					                                                                               , blobPath.Get()
					                                                                               , TitleStorage::TitleStorageBlobType_Json
					                                                                               , XboxUserId.Get()
					                                                                               , &blobMetadata
					                                                                               );
				}
				break;

			default:
				assert(false);
				break;
			}

			ComPtr<IXboxLiveContext> liveContext;
			Activation::Factory<XboxLiveContext>()->CreateInstance1(user.Get(), &liveContext);

			switch (pQueryData->operationType)
			{
			case eCOSO_Upload:
				{
					DebugProfileLogCurrentUser("Perform Upload", (int)pQueryData->lTaskID);
					const char* data = pQueryData->pBuffer;
					size_t dataLen = pQueryData->bufferSize;

					ComPtr<ABI::Windows::Storage::Streams::IBuffer> buffer;
					HRESULT hr = Activation::Factory<ABI::Windows::Storage::Streams::Buffer>()->Create(dataLen, &buffer);
					if (SUCCEEDED(hr))
					{
						buffer->put_Length(dataLen);
						byte* bytes = GetBytePointer(buffer.Get());
						assert(bytes);
						if (bytes)
						{
							memcpy(bytes, data, dataLen);
						}

						const int preferredUploadBlockSize = 16 * 1024;
						assert(dataLen <= preferredUploadBlockSize);

						CryLogAlways("CCryDurangoLiveOnlineStorage - Upload: started");

						ComPtr<TitleStorage::ITitleStorageService> titleStorageService;
						hr = liveContext->get_TitleStorageService(&titleStorageService);
						if (SUCCEEDED(hr) && hr != S_FALSE)
						{
							ComPtr<IAsyncOperation<TitleStorage::TitleStorageBlobMetadata*>> op;
							hr = titleStorageService->UploadBlobAsync(blobMetadata.Get(), buffer.Get(), TitleStorage::TitleStorageETagMatchCondition_NotUsed, preferredUploadBlockSize, &op);
							if (S_FALSE == hr)
							{
								CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "CCryDurangoLiveOnlineStorage: UploadBlobAsync returned S_FALSE");
								liveStatus = !ERROR_SUCCESS;
							}
							else if (SUCCEEDED(hr))
							{
								m_pAsyncOperation->Set(op);
							}
							else
							{
								CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "CCryDurangoLiveOnlineStorage: UploadBlobAsync failed: %08X", hr);
								liveStatus = !ERROR_SUCCESS;
							}
						}
						else
						{
							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "CCryDurangoLiveOnlineStorage: get_TitleStorageService failed: %08X", hr);
							liveStatus = !ERROR_SUCCESS;
						}
					}
					else
					{
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "CCryDurangoLiveOnlineStorage: failed to create buffer for upload: %08X", hr);
						liveStatus = !ERROR_SUCCESS;
					}
				}
				break;

			case eCOSO_Download:
				{
					DebugProfileLogCurrentUser("Perform Download", (int)pQueryData->lTaskID);
					ComPtr<ABI::Windows::Storage::Streams::IBuffer> buffer;
					HRESULT hr = Activation::Factory<ABI::Windows::Storage::Streams::Buffer>()->Create(pQueryData->bufferSize, &buffer);
					if (SUCCEEDED(hr))
					{
						CryLogAlways("CCryDurangoLiveOnlineStorage - Download: started");

						ComPtr<TitleStorage::ITitleStorageService> titleStorageService;
						hr = liveContext->get_TitleStorageService(&titleStorageService);
						if (SUCCEEDED(hr))
						{
							ComPtr<IAsyncOperation<TitleStorage::TitleStorageBlobResult*>> op;
							hr = titleStorageService->DownloadBlobAsync2(blobMetadata.Get(), buffer.Get(), TitleStorage::TitleStorageETagMatchCondition_NotUsed, nullptr, &op);
							if (SUCCEEDED(hr))
							{
								m_pAsyncOperation->Set(op);
							}
							else
							{
								CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "CCryDurangoLiveOnlineStorage: DownloadBlobAsync2 failed: %08X", hr);
								liveStatus = !ERROR_SUCCESS;
							}
						}
						else
						{
							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "CCryDurangoLiveOnlineStorage: get_TitleStorageService failed: %08X", hr);
							liveStatus = !ERROR_SUCCESS;
						}
					}
					else
					{
						CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "CCryDurangoLiveOnlineStorage: failed to create buffer for download: %08X", hr);
						liveStatus = !ERROR_SUCCESS;
					}
				}
				break;
			}

			m_status = eCOSS_Working;
		}
		break;

	case eCOSS_Working:
		switch (m_pAsyncOperation->Status())
		{
		case ABI::Windows::Foundation::AsyncStatus::Started:
			liveStatus = ERROR_IO_PENDING;
			break;

		case ABI::Windows::Foundation::AsyncStatus::Completed:
			if (m_pendingCount)
			{
				CryOnlineStorageQueryData* pQueryData = &m_pendingQueries[0];
				pQueryData->eResult = eCLE_Success;
				switch (pQueryData->operationType)
				{
				case eCOSO_Upload:
					CryLogAlways("CCryDurangoLiveOnlineStorage: successfully saved.");
					liveStatus = ERROR_SUCCESS;
					break;
				case eCOSO_Download:
					{
						ComPtr<TitleStorage::ITitleStorageBlobResult> result;
						HRESULT hr = m_pAsyncOperation->GetResults(result);
						if (SUCCEEDED(hr))
						{
							ComPtr<ABI::Windows::Storage::Streams::IBuffer> buffer;
							hr = result->get_BlobBuffer(&buffer);
							if (SUCCEEDED(hr))
							{
								byte* bytes = GetBytePointer(buffer.Get());
								assert(bytes);
								liveStatus = ERROR_SUCCESS;
								if (bytes)
								{
									uint readBytes = 0;
									hr = buffer->get_Length(&readBytes);

									if (SUCCEEDED(hr))
									{
										if (pQueryData->bufferSize >= readBytes)
										{
											memcpy(pQueryData->pBuffer, bytes, readBytes);
											pQueryData->bufferSize = readBytes;
											CryLogAlways("CCryDurangoLiveOnlineStorage: successfully downloaded.");
										}
										else
										{
											CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "CCryDurangoLiveOnlineStorage: bad buffer size: %u < %u", (uint)pQueryData->bufferSize, (uint)readBytes);
											liveStatus = !ERROR_SUCCESS;
										}
									}
									else
									{
										CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "CCryDurangoLiveOnlineStorage: failed to get length of buffer: %08X", hr);
										liveStatus = !ERROR_SUCCESS;
									}
								}
							}
							else
							{
								CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "CCryDurangoLiveOnlineStorage: failed to get buffer from result: %08X", hr);
								liveStatus = !ERROR_SUCCESS;
							}
						}
						else
						{
							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "CCryDurangoLiveOnlineStorage: failed to fetch result: %08X", hr);
							liveStatus = !ERROR_SUCCESS;
						}
					}
					break;
				}
			}
			break;

		default:
			if (m_pendingCount)
			{
				CryOnlineStorageQueryData* pQueryData = &m_pendingQueries[0];
				pQueryData->eResult = eCLE_InternalError;

				auto status = m_pAsyncOperation->Status();
				HRESULT hr = m_pAsyncOperation->ErrorCode();

				if (HRESULT_FACILITY(hr) == FACILITY_HTTP)
				{
					pQueryData->httpStatusCode = HRESULT_CODE(hr);
				}

				switch (pQueryData->operationType)
				{
				case eCOSO_Upload:
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "CCryDurangoLiveOnlineStorage: UploadBlobAsync failed for status=%d,hr=%08X", status, hr);
					break;
				case eCOSO_Download:
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "CCryDurangoLiveOnlineStorage: DownloadBlobAsync failed for status=%d,hr=%08X", status, hr);
					break;
				}
				liveStatus = ERROR_SUCCESS; // it means that request has been processed. no matter what result is.
			}
			break;
		}
		break;
	}

	switch (liveStatus)
	{
	case ERROR_IO_INCOMPLETE: // We're waiting on something else...
	case ERROR_IO_PENDING:    // We're waiting for this call to finish...
		{
			return;
		}

	default: // Error case, intentionally falls through to success case to pop the queue
		{
			m_pendingQueries[0].eResult = eCLE_InternalError;
		}
	case ERROR_SUCCESS:
		{
			//Up/Download complete. Remove the pending request and notify the callback
			//-- release lobby task handle
			m_pLobby->ReleaseTask(m_pendingQueries[0].lTaskID);

			//-- put callback for all the batched awards onto game thread.
			TO_GAME_FROM_LOBBY(&CCryDurangoLiveOnlineStorage::TriggerCallbackOnGameThread, this, m_pendingQueries[0]);

			if (m_pendingCount)
			{
				--m_pendingCount;
			}
			for (uint8 i = 0; i < m_pendingCount; i++)
			{
				memcpy(&m_pendingQueries[i], &m_pendingQueries[i + 1], sizeof(CryOnlineStorageQueryData));
			}

			m_status = eCOSS_Idle;

			break;
		}
	}
}

ECryLobbyError CCryDurangoLiveOnlineStorage::OnlineStorageDataQuery(CryOnlineStorageQueryData* pQueryData)
{
	LOBBY_AUTO_LOCK;

	if (m_pendingCount == (MAXIMUM_PENDING_QUERIES - 1))
	{
		CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_ERROR, "CCryDurangoLiveOnlineStorage::OnlineStorageDataQuery ran out of storage, too many queries at once");
		return eCLE_InternalError;
	}

	pQueryData->lTaskID = m_pLobby->CreateTask();

	DebugProfileLogCurrentUser("TMS add query", (int)pQueryData->lTaskID);

	if (pQueryData->lTaskID != CryLobbyInvalidTaskID)
	{
		memcpy(&m_pendingQueries[m_pendingCount], pQueryData, sizeof(CryOnlineStorageQueryData));
		m_pendingCount++;
	}
	else
	{
		return eCLE_TooManyTasks;
	}

	return eCLE_Success;
}

void CCryDurangoLiveOnlineStorage::TriggerCallbackOnGameThread(const CryOnlineStorageQueryData& QueryData)
{
	if (QueryData.pListener)
	{
		DebugProfileLogCurrentUser("TMS MainThread listener", (int)QueryData.lTaskID);
		QueryData.pListener->OnOnlineStorageOperationComplete(QueryData);
	}
}

	#pragma warning(pop)

#endif//USE_DURANGOLIVE && USE_CRY_ONLINE_STORAGE
