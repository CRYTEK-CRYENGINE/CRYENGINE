// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "PlatformIdentifier.h"
#include "CryExtension/CryTypeID.h"
#include <CrySerialization/CryStringsImpl.h>

struct ITexture;

namespace Cry
{
	namespace GamePlatform
	{
		struct IBase;
		struct IPlugin;
		struct IAccount;
		struct IAchievement;
		struct ILeaderboards;
		struct IUserLobby;
		struct IMatchmaking;
		struct INetworking;
		struct IRemoteStorage;
		struct IServer;
		struct IService;
		struct IStatistics;
		struct IUser;
		struct IUserGeneratedContent;
		class  UserIdentifier;

		struct SRichPresence
		{
			enum class ETimer
			{
				None = -1,
				Elapsed,
				Remaining
			};

			CryFixedStringT<128> activity;
			CryFixedStringT<128> headline;
			ETimer countdownTimer = ETimer::None;
			int64 seconds = 0;
			uint32 partySize = 0;
			uint32 partyMax = 0;

			bool operator==(const SRichPresence& other) const
			{
				return countdownTimer == other.countdownTimer
					&& partySize == other.partySize
					&& partyMax == other.partyMax
					&& seconds == other.seconds
					&& activity == other.activity
					&& headline == other.headline;
			}

			bool operator!=(const SRichPresence& other) const
			{
				const bool areEqual = this->operator==(other);
				return !areEqual;
			}
		};

		//! Type of in-game overlay dialog, used together with IPlugin::OpenDialog
		enum class EDialog
		{
			Friends,
			Community,
			Players,
			Settings,
			Group,
			Achievements,
			Stats
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

		enum class EFriendRelationship
		{
			None,
			Friend,
			RequestSent,
			RequestReceived,
			Blocked,
		};

		//! Determines the size of a user's avatar, used when retrieving the avatar as a texture (see GetAvatar)
		enum class EAvatarSize
		{
			Large,  //!< E.g. for Steam: 184x184
			Medium, //!< E.g. for Steam: 64x64
			Small   //!< E.g. for Steam: 32x32
		};

		//! Type of in-game overlay dialog with a target user, used together with IPlugin::OpenDialogWithTargetUser
		enum class EUserTargetedDialog
		{
			UserInfo,
			FriendAdd,
			FriendRemove,
			FriendRequestAccept,
			FriendRequestIgnore,
			Chat,
			JoinTrade,
			Stats,
			Achievements
		};

		//! Flags to be passed to platform service for querying/downloading
		//! information about users.
		enum EUserInformationFlags : uint8
		{
			eUIF_Name =   BIT8(0), //!< Personal name
			eUIF_Avatar = BIT8(1)  //!< Avatar image
		};

		//! Only purpose of this typedef is to make sure we always use the correct type
		//! to store combinations of EUserInformationFlags bits.
		using UserInformationMask = std::underlying_type<EUserInformationFlags>::type;

		using TextureId = int;
		constexpr TextureId NullTextureID = 0;

		//! Unique identifier for a service
		using ServiceIdentifier = CryGUID;

		constexpr ServiceIdentifier NullServiceID = CryGUID::Null();
		constexpr ServiceIdentifier SteamServiceID = CryClassID("{4DFCDCE3-9985-42E5-A702-5A64D849DC8F}"_cry_guid);
		constexpr ServiceIdentifier PSNServiceID = CryClassID("{F7729E26-464F-4BE9-A58E-06C72C03EAE1}"_cry_guid);
		constexpr ServiceIdentifier DiscordServiceID = CryClassID("{D68238FE-AA88-4C0C-9E9C-56A848AE0F37}"_cry_guid);
		constexpr ServiceIdentifier RailServiceID = CryClassID("{B536B2AE-E363-4765-8E15-7B021C356E9C}"_cry_guid);
		constexpr ServiceIdentifier XboxServiceID = CryClassID("{BE28931B-8843-419F-BDE4-C167F3EAAFBA}"_cry_guid);


		inline const char* GetServiceDebugName(const ServiceIdentifier& svcId)
		{
			return (svcId == SteamServiceID) ? "Steam" :
					((svcId == PSNServiceID) ? "PSN" :
					((svcId == XboxServiceID) ? "Xbox" :
					((svcId == DiscordServiceID) ? "Discord" :
					((svcId == RailServiceID) ? "Rail" :
					((svcId == NullServiceID) ? "Null" : "Unknown")))));
		}

		namespace Detail
		{
			using NumericIdentifierValue = uint64;
			using StringIdentifierValue = CryFixedStringT<48>;

			struct STraitsBase
			{
				using ServiceType = ServiceIdentifier;
				// Note: When adding types here make sure you update the code using stl::holds_alternative
				// and stl::get
				using ValueType = CryVariant<StringIdentifierValue, NumericIdentifierValue>;

				static ValueType Null()
				{
					return NumericIdentifierValue(0);
				}

				static const char* ToDebugString(const ServiceIdentifier& svcId, const char* szIdKind, const ValueType& value)
				{
					static stack_string debugStr;

					if (stl::holds_alternative<StringIdentifierValue>(value))
					{
						debugStr.Format("%s%s:%s", GetServiceDebugName(svcId), szIdKind, stl::get<StringIdentifierValue>(value).c_str());
					}
					else if (stl::holds_alternative<NumericIdentifierValue>(value))
					{
						debugStr.Format("%s%s:%" PRIu64, GetServiceDebugName(svcId), szIdKind, stl::get<NumericIdentifierValue>(value));
					}
					else
					{
						return debugStr.Format("%s%s:?",  GetServiceDebugName(svcId), szIdKind);
					}

					return debugStr.c_str();
				}

				static bool AsUint64(const ValueType& value, uint64& out)
				{
					if (stl::holds_alternative<StringIdentifierValue>(value))
					{
						char trailing; // attempt to parse trailing characters as we don't want them.
						const StringIdentifierValue& str = stl::get<StringIdentifierValue>(value);
						const int ok = sscanf_s(str.c_str(), "%" PRIu64 "%c", &out, &trailing);
						return ok == 1;
					}
					else if (stl::holds_alternative<NumericIdentifierValue>(value))
					{
						out = stl::get<NumericIdentifierValue>(value);
						return true;
					}

					return false;
				}

				template<class StrType>
				static bool AsString(const ValueType& value, StrType& out)
				{
					if (stl::holds_alternative<StringIdentifierValue>(value))
					{
						out = stl::get<StringIdentifierValue>(value).c_str();
						return true;
					}
					else if (stl::holds_alternative<NumericIdentifierValue>(value))
					{
						out.Format("%" PRIu64, stl::get<NumericIdentifierValue>(value));
						return true;
					}
					return false;
				}

				static void Serialize(ValueType& value, Serialization::IArchive& ar)
				{
					constexpr size_t strIdx = cry_variant::get_index<StringIdentifierValue, ValueType>::value;
					constexpr size_t numIdx = cry_variant::get_index<NumericIdentifierValue, ValueType>::value;

					if (ar.isOutput())
					{
						const size_t idx = value.index();
						switch (idx)
						{
						case strIdx:
							ar(idx, "type");
							ar(stl::get<StringIdentifierValue>(value), "value");
							break;
						case numIdx:
							ar(idx, "type");
							ar(stl::get<NumericIdentifierValue>(value), "value");
							break;
						default:
							ar(stl::variant_npos, "type");
						}

						return;
					}

					if (ar.isInput())
					{
						ValueType tmpVal;

						size_t idx = stl::variant_npos;
						ar(idx, "type");

						switch (idx)
						{
						case strIdx:
							{
								StringIdentifierValue str;
								ar(str, "value");
								tmpVal = str;
							}
							break;
						case numIdx:
							{
								NumericIdentifierValue num;
								ar(num, "value");
								tmpVal = num;
							}
							break;
						}

						if (tmpVal.index() != idx)
						{
							CRY_ASSERT_MESSAGE(tmpVal.index() == idx, "Variant deserialization failed!");
							return;
						}

						value.swap(tmpVal);
					}
				}
			};
			struct SAccountTraits : public STraitsBase
			{
				static const char* ToDebugString(const ServiceIdentifier& svcId, const ValueType& value)
				{
					return STraitsBase::ToDebugString(svcId, "Account", value);
				}
			};

			struct SLobbyTraits : public STraitsBase
			{
				static const char* ToDebugString(const ServiceIdentifier& svcId, const ValueType& value)
				{
					return STraitsBase::ToDebugString(svcId, "Lobby", value);
				}
			};

			struct SApplicationTraits : public STraitsBase
			{
				static const char* ToDebugString(const ServiceIdentifier& svcId, const ValueType& value)
				{
					return STraitsBase::ToDebugString(svcId, "Application", value);
				}
			};
		}

		//! Identifies a game platform user on a specific service.
		using AccountIdentifier = Identifier<Detail::SAccountTraits>;

		//! Identifies a game platform lobby.
		using LobbyIdentifier = Identifier<Detail::SLobbyTraits>;

		//! Identifies a game or DLC.
		using ApplicationIdentifier = Identifier<Detail::SApplicationTraits>;
	}
}
