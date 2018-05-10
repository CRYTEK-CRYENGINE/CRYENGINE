// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#if CRY_PLATFORM_DURANGO && USE_DURANGOLIVE && !defined(_LIB)

	#include "platform.h"

	#pragma warning(push)
	#pragma warning(disable:28204)

RTC_NAME_F(ABI::Windows::Foundation::Uri, RuntimeClass_Windows_Foundation_Uri);

RTC_NAME_S(ABI::Windows::ApplicationModel::Core::CoreApplication, RuntimeClass_Windows_ApplicationModel_Core_CoreApplication);

RTC_NAME_S(ABI::Windows::Data::Json::JsonObject, RuntimeClass_Windows_Data_Json_JsonObject);
RTC_NAME(ABI::Windows::Data::Json::JsonArray, RuntimeClass_Windows_Data_Json_JsonArray);
RTC_NAME_S(ABI::Windows::Data::Json::JsonValue, RuntimeClass_Windows_Data_Json_JsonValue);
RTC_NAME_F(ABI::Windows::Storage::Streams::Buffer, RuntimeClass_Windows_Storage_Streams_Buffer);
RTC_NAME_F(ABI::Windows::Storage::Streams::DataWriter, RuntimeClass_Windows_Storage_Streams_DataWriter);
RTC_NAME_F(ABI::Windows::Storage::Streams::DataReader, RuntimeClass_Windows_Storage_Streams_DataReader);

RTC_NAME_S(ABI::Windows::System::Launcher, RuntimeClass_Windows_System_Launcher);

RTC_NAME_S(ABI::Windows::Xbox::ApplicationModel::Store::Product, RuntimeClass_Windows_Xbox_ApplicationModel_Store_Product);

RTC_NAME_S(ABI::Windows::Xbox::Input::Controller, RuntimeClass_Windows_Xbox_Input_Controller);
RTC_NAME_S(ABI::Windows::Xbox::Input::Gamepad, RuntimeClass_Windows_Xbox_Input_Gamepad);
RTC_NAME_S(ABI::Windows::Xbox::Management::Deployment::PackageTransferManager, RuntimeClass_Windows_Xbox_Management_Deployment_PackageTransferManager);
RTC_NAME_S(ABI::Windows::Xbox::Multiplayer::Party, RuntimeClass_Windows_Xbox_Multiplayer_Party);
RTC_NAME_F(ABI::Windows::Xbox::Multiplayer::MultiplayerSessionReference, RuntimeClass_Windows_Xbox_Multiplayer_MultiplayerSessionReference);
RTC_NAME(ABI::Windows::Xbox::Storage::ConnectedStorageContainer, RuntimeClass_Windows_Xbox_Storage_ConnectedStorageContainer);
RTC_NAME_S(ABI::Windows::Xbox::Storage::ConnectedStorageSpace, RuntimeClass_Windows_Xbox_Storage_ConnectedStorageSpace);
RTC_NAME_S(ABI::Windows::Xbox::System::User, RuntimeClass_Windows_Xbox_System_User);

RTC_NAME_S(ABI::Windows::Xbox::Networking::SecureDeviceAssociationTemplate, RuntimeClass_Windows_Xbox_Networking_SecureDeviceAssociationTemplate);
RTC_NAME_S(ABI::Windows::Xbox::Networking::SecureDeviceAssociation, RuntimeClass_Windows_Xbox_Networking_SecureDeviceAssociation);
RTC_NAME_S(ABI::Windows::Xbox::Networking::SecureDeviceAddress, RuntimeClass_Windows_Xbox_Networking_SecureDeviceAddress);

RTC_NAME_F(ABI::Microsoft::Xbox::Services::XboxLiveContext, RuntimeClass_Microsoft_Xbox_Services_XboxLiveContext);
RTC_NAME_F(ABI::Microsoft::Xbox::Services::Multiplayer::MultiplayerSession, RuntimeClass_Microsoft_Xbox_Services_Multiplayer_MultiplayerSession);
RTC_NAME_F(ABI::Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference, RuntimeClass_Microsoft_Xbox_Services_Multiplayer_MultiplayerSessionReference);
RTC_NAME_F(ABI::Microsoft::Xbox::Services::Presence::PresenceData, RuntimeClass_Microsoft_Xbox_Services_Presence_PresenceData);
RTC_NAME_F(ABI::Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata, RuntimeClass_Microsoft_Xbox_Services_TitleStorage_TitleStorageBlobMetadata);

	#pragma warning(pop)

#endif
