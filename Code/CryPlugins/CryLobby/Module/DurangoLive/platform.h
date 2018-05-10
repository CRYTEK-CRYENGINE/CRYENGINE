// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#if CRY_PLATFORM_DURANGO && USE_DURANGOLIVE

// Definition of noexcept to cover future deprecation of throw()
	#define noexcept throw()

// {00000000-0000-0000-0000-000000000000}
static GUID const NULL_GUID =
{
	0x00000000, 0x0000, 0x0000, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
};

// WinRT core
	#include <winapifamily.h>

	#include <wrl.h>
	#include <wrl/ftm.h>
	#pragma warning(push)
	#pragma warning(disable : 28204)
	#include <wrl/async.h>
	#pragma warning(pop)
	#include <wrl/client.h>
	#include <wrl/event.h>
	#include <wrl/module.h>

	#include "robuffer.h"
	#include <Objbase.h>
	#include <tchar.h>

// Windows concurrency
	#include <ctxtcall.h>
	#pragma warning(push)
	#pragma warning(disable : 4355)
	#include "ppltasks_extra.h"
	#pragma warning(pop)

	#include "wmistr.h"
	#include "etwplus.h"
	#include "evntprov.h"
	#include "evntrace.h"

	#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_TV_TITLE)
		#include <winsock2.h>
		#include <ws2tcpip.h>
	#endif
	#include "conio.h"

	#include <stdio.h>

// StdLib
	#include <fstream>
	#include <iostream>
	#include <ios>
	#pragma warning(push)
	#pragma warning(disable : 4715)
	#include <filesystem>
	#pragma warning(pop)
	#include <tuple>
	#include <bitset>
	#include <memory>
	#include <sstream>
	#include <type_traits>
	#include <string>
	#include <sstream>

// STL Containers
	#include <deque>
	#include <array>
	#include <vector>
	#include <list>
	#include <string>
	#include <queue>
	#include <map>
	#include <unordered_map>
	#include <unordered_set>
	#include <set>

// STL Misc
	#include <regex>
	#include <functional>
	#include <random>
	#include <filesystem>

// STL Algorithms
	#include <algorithm>
	#include <numeric>

// STL concurrency
	#include <atomic>
	#include <functional>
	#include <thread>
	#include <mutex>
	#include <future>
	#include <condition_variable>
	#include <chrono>

// WinRT headers
	#include <Windows.Foundation.h>
	#include <Windows.ApplicationModel.Core.h>
	#include <Windows.Xbox.ApplicationModel.Store.h>
	#include <Windows.Data.Json.h>
	#include <Windows.Networking.h>
	#include <Windows.Storage.Streams.h>
	#include <Windows.System.h>
	#include <Windows.Ui.Core.h>
	#include <Windows.Xbox.Networking.h>
	#include <Windows.Xbox.Management.Deployment.h>

// #include <Windows.System.Threading.h>

	#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_TV_TITLE)
		#include <ixmlhttprequest2.h>
	#else
		#include <msxml6.h>
	#endif
	#include <Windows.Xbox.Input.h>
	#include <Windows.Xbox.Storage.h>
	#include <Windows.Xbox.System.h>
	#include <Windows.Xbox.Multiplayer.h>
	#include <Windows.Xbox.Networking.h>
	#pragma warning(push)
	#pragma warning(disable : 6244 28251)
	#include <Microsoft.Xbox.Services.h>
	#pragma warning(pop)
	#include <Microsoft.Xbox.Services.Marketplace.h>

	#include "AbiUtil.h"

namespace ABI {
namespace Windows {
namespace ApplicationModel {
namespace Core {
class CoreApplication
{
};
typedef ICoreApplication ICoreApplicationStatics;
}     // Core
}     // ApplicationModel
namespace System
{
class Launcher;
class ILauncher;
}
namespace Xbox {
namespace ApplicationModel {
namespace Store  {
class Product;
class IProduct;
}       // Store
}       // ApplicationModel
namespace System {
}       // } System
namespace Multiplayer {
class Party
{
};
class IParty
{
};
}   // } Multiplayer
}   // } Xbox
}   // } Windows

//ABI::Windows::Xbox::ApplicationMode
namespace Microsoft {
namespace Xbox {
namespace Services {
CX_TYPEDEF_F(IXboxLiveContext);
CX_TYPEDEF(IXboxLiveContextSettings);
namespace Multiplayer
{
CX_TYPEDEF_F(IMultiplayerSession);
CX_TYPEDEF_F(IMultiplayerSessionReference);
CX_TYPEDEF(IMultiplayerSessionMember);
CX_TYPEDEF(IMultiplayerSessionStates);
CX_TYPEDEF(IMultiplayerSessionConstants);
CX_TYPEDEF(IMultiplayerSessionProperties);
CX_TYPEDEF(IMultiplayerService);
}         // } Multiplayer
namespace Matchmaking
{
CX_TYPEDEF(IHopperStatisticsResponse);
CX_TYPEDEF(ICreateMatchTicketResponse);
CX_TYPEDEF(IMatchTicketDetailsResponse);
//          CX_TYPEDEF(IMatchesForUserResponse);
CX_TYPEDEF(IMatchmakingService);
}         // } Matchmaking
namespace Marketplace
{
CX_TYPEDEF(ICatalogService);
CX_TYPEDEF(ICatalogItem);
CX_TYPEDEF(IBrowseCatalogResult);
CX_TYPEDEF(IInventoryService);
CX_TYPEDEF(IInventoryItemsResult);
CX_TYPEDEF(IInventoryItem);
CX_TYPEDEF(IConsumeInventoryItemResult);
}         // } MarketPlace
namespace Presence
{
CX_TYPEDEF_F(IPresenceData);
CX_TYPEDEF(IPresenceService);
}         // } Presence
namespace TitleStorage
{
CX_TYPEDEF_F(ITitleStorageBlobMetadata);
CX_TYPEDEF(ITitleStorageBlobResult);
CX_TYPEDEF(ITitleStorageService);
} // } TitleStorage
} // } Services
} // } Xbox
} // } Microsoft
} // } ABI

USING_RTC_F_B(ABI::Windows::Foundation::Uri, ABI::Windows::Foundation::IUriRuntimeClass);

USING_RTC_S(ABI::Windows::ApplicationModel::Core::CoreApplication, ABI::Windows::ApplicationModel::Core::ICoreApplication);
//USING_RTC_S( ABI::Windows::ApplicationModel::Core::CoreApplicationContext, ABI::Windows::ApplicationModel::Core::ICoreApplicationContext );

USING_RTC_S_B(ABI::Windows::Data::Json::JsonObject, ABI::Windows::Data::Json::IJsonObject);
USING_RTC_B(ABI::Windows::Data::Json::JsonArray, ABI::Windows::Data::Json::IJsonArray);
USING_RTC_S_B(ABI::Windows::Data::Json::JsonValue, ABI::Windows::Data::Json::IJsonValue);

USING_RTC_F(ABI::Windows::Storage::Streams::Buffer, ABI::Windows::Storage::Streams::IBuffer);
USING_RTC_F_B(ABI::Windows::Storage::Streams::DataWriter, ABI::Windows::Storage::Streams::IDataWriter);
USING_RTC_F_B(ABI::Windows::Storage::Streams::DataReader, ABI::Windows::Storage::Streams::IDataReader);

USING_RTC_S_B(ABI::Windows::System::Launcher, ABI::Windows::System::ILauncher);

USING_RTC_S_B(ABI::Windows::Xbox::ApplicationModel::Store::Product, ABI::Windows::Xbox::ApplicationModel::Store::IProduct);

USING_RTC_S(ABI::Windows::Xbox::Input::Controller, ABI::Windows::Xbox::Input::IController);
USING_RTC_S(ABI::Windows::Xbox::Input::Gamepad, ABI::Windows::Xbox::Input::IGamepad);
USING_RTC_S_B(ABI::Windows::Xbox::Management::Deployment::PackageTransferManager, ABI::Windows::Xbox::Management::Deployment::IPackageTransferManager);
USING_RTC_S_B(ABI::Windows::Xbox::Multiplayer::Party, ABI::Windows::Xbox::Multiplayer::IParty);
USING_RTC_F_B(ABI::Windows::Xbox::Multiplayer::MultiplayerSessionReference, ABI::Windows::Xbox::Multiplayer::IMultiplayerSessionReference);
USING_RTC_B(ABI::Windows::Xbox::Storage::ConnectedStorageContainer, ABI::Windows::Xbox::Storage::IConnectedStorageContainer);
USING_RTC_S_B(ABI::Windows::Xbox::Storage::ConnectedStorageSpace, ABI::Windows::Xbox::Storage::IConnectedStorageSpace);
USING_RTC_S_B(ABI::Windows::Xbox::System::User, ABI::Windows::Xbox::System::IUser);

USING_RTC_S_B(ABI::Windows::Xbox::Networking::SecureDeviceAssociationTemplate, ABI::Windows::Xbox::Networking::ISecureDeviceAssociationTemplate);
USING_RTC_S_B(ABI::Windows::Xbox::Networking::SecureDeviceAssociation, ABI::Windows::Xbox::Networking::ISecureDeviceAssociation);
USING_RTC_S_B(ABI::Windows::Xbox::Networking::SecureDeviceAddress, ABI::Windows::Xbox::Networking::ISecureDeviceAddress);

USING_RTC_F_B(ABI::Microsoft::Xbox::Services::XboxLiveContext, ABI::Microsoft::Xbox::Services::IXboxLiveContext);
USING_RTC_F_B(ABI::Microsoft::Xbox::Services::Multiplayer::MultiplayerSession, ABI::Microsoft::Xbox::Services::Multiplayer::IMultiplayerSession);
USING_RTC_F_B(ABI::Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference, ABI::Microsoft::Xbox::Services::Multiplayer::IMultiplayerSessionReference);
USING_RTC_F_B(ABI::Microsoft::Xbox::Services::Presence::PresenceData, ABI::Microsoft::Xbox::Services::Presence::IPresenceData);
USING_RTC_F_B(ABI::Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata, ABI::Microsoft::Xbox::Services::TitleStorage::ITitleStorageBlobMetadata);
#endif
