// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IPlatformBase.h"

namespace Cry
{
	namespace GamePlatform
	{
		//! Interface of a game platform service (Steam, PSN, Discord etc.)
		struct IService : public IBase
		{
			//! Listener to check for general game platform events
			//! See IService::AddListener and RemoveListener
			struct IListener
			{
				enum class EPersonaChangeFlags
				{
					Name = BIT(0),
					Status = BIT(1),
					CameOnline = BIT(2),
					WentOffline = BIT(3),
					GamePlayed = BIT(4),
					GameServer = BIT(5),
					ChangeAvatar = BIT(6),
					JoinedSource = BIT(7),
					LeftSource = BIT(8),
					RelationshipChanged = BIT(9),
					NameFirstSet = BIT(10),
					FacebookInfo = BIT(11),
					Nickname = BIT(12),
					SteamLevel = BIT(13),
				};

				virtual ~IListener() {}
				//! Called when the in-game platform layer is opened (usually by the user)
				virtual void OnOverlayActivated(const ServiceIdentifier& serviceId, bool active) = 0;
				//! Called when an avatar requested using RequestUserInformation has become available
				virtual void OnAvatarImageLoaded(const AccountIdentifier& accountId) = 0;
				//! Called when the service is about to shut down
				virtual void OnShutdown(const ServiceIdentifier& serviceId) = 0;
				//! Called when an account has been added
				virtual void OnAccountAdded(IAccount& account) = 0;
				//! Called right before removing an account
				virtual void OnAccountRemoved(IAccount& account) = 0;
				//! Called when the persona state was updated
				virtual void OnPersonaStateChanged(const IAccount& account, CEnumFlags<EPersonaChangeFlags> changeFlags) = 0;
				//! Called when a steam auth ticket request received a response
				virtual void OnGetSteamAuthTicketResponse(bool success, uint32 authTicket) = 0;
				//! Called when a token request received a response
				virtual void OnAuthTokenReceived(bool success, const char* szToken) = 0;
				//! Called when the connection status to the platform service changed
				virtual void OnNetworkStatusChanged(const EConnectionStatus& connectionStatus) = 0;
			};

			enum class EPermission
			{
				Communication,
				Multiplayer,
				ViewProfiles,
				WebBrowser,

				Count // Internal use
			};

			enum class EPrivacyPermission
			{
				VoiceCommunication,
				TextCommunication,
				PlayMultiplayer,

				Count // Internal use
			};

			virtual ~IService() {}

			//! Adds a service event listener
			virtual void AddListener(IListener& listener) = 0;
			//! Removes a service event listener
			virtual void RemoveListener(IListener& listener) = 0;

			//! Called by core platform plugin before it's going to be unloaded
			virtual void Shutdown() = 0;

			//! Gets the unique identifier of this service
			virtual ServiceIdentifier GetServiceIdentifier() const = 0;

			//! Returns the platform identifier of the build the player is running, usually the trivial version of the application version
			virtual int GetBuildIdentifier() const = 0;

			//! Checks if the local user owns the specified identifier
			//! \param id The platform-specific identifier for the application or DLC
			virtual bool OwnsApplication(ApplicationIdentifier id) const = 0;

			//! Gets the platform-specific identifier for the running application
			virtual ApplicationIdentifier GetApplicationIdentifier() const = 0;

			//! Gets an IAccount representation of the local user, useful for getting local information such as user name
			virtual IAccount* GetLocalAccount() const = 0;
			//! Gets local user's friend accounts
			virtual const DynArray<IAccount*>& GetFriendAccounts() const = 0;
#if CRY_GAMEPLATFORM_EXPERIMENTAL
			//! Gets local user's blocked accounts
			virtual const DynArray<IAccount*>& GetBlockedAccounts() const = 0;
			//! Gets local user's muted accounts
			virtual const DynArray<IAccount*>& GetMutedAccounts() const = 0;
			//! Allows retrieval of platform-specific information that can't be easily added to the IService interface without bloating it
			//! \param szVarName The variable name
			//! \param valueOut This is where the variable value will be stored (if any)
			//! \retval true Value was found and stored in output variable
			//! \retval false Unknown variable name
			virtual bool GetEnvironmentValue(const char* szVarName, string& valueOut) const = 0;
#endif // CRY_GAMEPLATFORM_EXPERIMENTAL
			//! Gets an IAccount representation of another user by account id
			virtual IAccount* GetAccountById(const AccountIdentifier& accountId) const = 0;
			//! Adds the account into a lobby or match context for interaction relevant updates (HasPrivacyPermission)
			virtual void AddAccountToLocalSession(const AccountIdentifier& accountId) = 0;
			//! Removes the account from a lobby or match context
			virtual void RemoveAccountFromLocalSession(const AccountIdentifier& accountId) = 0;

			//! Checks if the local user has the other user in their friends list for this service
			virtual bool IsFriendWith(const AccountIdentifier& otherAccountId) const = 0;
			//! Gets the relationship status between the local user and another user
			virtual EFriendRelationship GetFriendRelationship(const AccountIdentifier& otherAccountId) const = 0;
			//! Opens a known dialog targeted at a specific user id via the platform's overlay
			virtual bool OpenDialogWithTargetUser(EUserTargetedDialog dialog, const AccountIdentifier& otherAccountId) const = 0;

			//! Opens a known dialog via the platform's overlay
			virtual bool OpenDialog(EDialog dialog) const = 0;
			//! Opens a browser window via the platform's overlay
			virtual bool OpenBrowser(const char* szURL) const = 0;
			//! Checks whether we are able to open the overlay used for purchasing assets
			virtual bool CanOpenPurchaseOverlay() const = 0;

			//! Check if information about a user (e.g. personal name, avatar...) is available, otherwise download it.
			//! \note It is recommended to limit requests for bulky data (like avatars) to a minimum as some platforms may have bandwidth or other limitations.
			//! \retval true Information is not yet available and listeners will be notified once retrieved.
			//! \retval false Information is already available and there's no need for a download.
			virtual bool RequestUserInformation(const AccountIdentifier& accountId, UserInformationMask info) = 0;

			//! CHeck if there is an active connection to the service's backend
			virtual bool IsLoggedIn() const = 0;

			//! Check if a user has permission to perform certain actions on the platform
			virtual bool HasPermission(const AccountIdentifier& accountId, EPermission permission) const = 0;

			//! Check if the user's privacy settings grant permission to perform certain actions with the target user
			virtual bool HasPrivacyPermission(const AccountIdentifier& accountId, const AccountIdentifier& targetAccountId, EPrivacyPermission permission) const = 0;
		};
	}
}