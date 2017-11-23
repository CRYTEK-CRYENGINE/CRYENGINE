#pragma once

#include <CrySystem/ICryPlugin.h>

#include "IPlatformRemoteStorage.h"
#include "IPlatformLeaderboards.h"
#include "IPlatformStatistics.h"
#include "IPlatformLobby.h"

#include "IPlatformUser.h"
#include "IPlatformServer.h"

#include "IPlatformMatchmaking.h"
#include "IPlatformNetworking.h"

namespace Cry
{
	namespace GamePlatform
	{
		//! Platform-specific identifier of an application or DLC
		using ApplicationIdentifier = unsigned int;

		//! Type of game platform, can never change at runtime
		enum class EType
		{
			Steam,
			PSN
		};

		//! Type of in-game overlay dialog, used together with IPlugin::OpenDialog
		enum class EDialog
		{
			Friends,
			Community,
			Players,
			Settings,
			Group,
			Achievements
		};

		//! Represents the current connection to the platform's services
		enum class EConnectionStatus
		{
			Disconnected,
			Connecting,
			ObtainingIP,
			Connected
		};

		enum class ESignInStatus
		{
			Unknown,
			Offline,
			Online
		};

		//! The main interface to a game platform (Steam, PSN etc.)
		//! This can be queried via gEnv->pSystem->GetIPluginManager()->QueryPlugin<IGamePlatform>();
		struct IPlugin : public Cry::IEnginePlugin
		{
			//! Listener to check for general game platform events
			//! See IPlugin::AddListener and RemoveListener
			struct IListener
			{
				//! Called when the in-game platform layer is opened (usually by the user)
				virtual void OnOverlayActivated(bool active) = 0;
			};

			virtual ~IPlugin() {}

			CRYINTERFACE_DECLARE_GUID(IPlugin, "{7A7D908B-D12F-4827-B410-F50386018036}"_cry_guid);

			//! Gets the type of game platform that we are running, cannot change at run-time
			virtual EType GetType() const = 0;

			//! Whether or not the plug-in initialized correctly, and indicates whether we can use the platform services
			virtual bool IsInitialized() const = 0;

			// Returns the platform identifier of the build the player is running, usually the trivial version of the application version
			virtual int GetBuildIdentifier() const = 0;

			//! Adds a general game platform event listener
			virtual void AddListener(IListener& listener) = 0;
			//! Removes a general game platform event listener
			virtual void RemoveListener(IListener& listener) = 0;

			//! Gets an IUser representation of the local user, useful for getting local user information such as user name
			virtual IUser* GetLocalClient() = 0;
			//! Gets an IUser representation of another user by platform user id
			//! Note that this function cannot fail, and will always return a valid pointer even if the id is invalid - no server checks are made.
			virtual IUser* GetUserById(IUser::Identifier id) = 0;

			//! Checks if the local user has the other user in their friends list
			virtual bool IsFriendWith(IUser::Identifier otherUserId) const = 0;

			//! Starts a server on the platform, registering it with the network - thus allowing future discovery through the matchmaking systems
			virtual IServer* CreateServer(bool bLocal) = 0;
			//! Gets the currently running platform server started via CreateServer
			virtual IServer* GetLocalServer() const = 0;

			//! Gets the platform's leaderboard implementation, allowing querying and posting to networked leaderboards
			virtual ILeaderboards* GetLeaderboards() const = 0;
			//! Gets the platform's statistics implementation, allowing querying and posting to networked statistics
			virtual IStatistics* GetStatistics() const = 0;
			//! Gets the platform's remote storage implementation, allowing for storage of player data over the network
			virtual IRemoteStorage* GetRemoteStorage() const = 0;
			//! Gets the platform's matchmaking implementation, allowing querying of servers and lobbies
			virtual IMatchmaking* GetMatchmaking() const = 0;
			//! Gets the platform's networking implementation, allowing for sending packets to specific users
			virtual INetworking* GetNetworking() const = 0;

			//! Checks if the local user owns the specified identifier
			//! \param id The platform-specific identifier for the application or DLC
			virtual bool OwnsApplication(ApplicationIdentifier id) const = 0;
			//! Gets the platform-specific identifier for the running application
			virtual ApplicationIdentifier GetApplicationIdentifier() const = 0;
			
			//! Opens a known dialog via the platforms's overlay
			virtual bool OpenDialog(EDialog dialog) const = 0;
			//! Opens a known dialog by platform-specific string via the platform's overlay
			virtual bool OpenDialog(const char* szPage) const = 0;
			//! Opens a browser window via the platform's overlay
			virtual bool OpenBrowser(const char* szURL) const = 0;
			//! Checks whether we are able to open the overlay used for purchasing assets
			virtual bool CanOpenPurchaseOverlay() const = 0;

			// Retrieves the authentication token required to communicate with web servers
			virtual bool GetAuthToken(string& tokenOut, int& issuerId) = 0;

			//! Checks the connection to the platform services
			virtual EConnectionStatus GetConnectionStatus() const = 0;
			//! Asynchronously checks whether the user can play Multiplayer
			//! The callback can be executed either during or after CanAcessMultiplayerServices
			//! \param asynchronousCallback The callback to execute when the platform services have responded
			virtual void CanAccessMultiplayerServices(std::function<void(bool authorized)> asynchronousCallback) = 0;
		};
	}
}