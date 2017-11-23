#pragma once

#include "IGamePlatform.h"

namespace Cry
{
	namespace GamePlatform
	{
		//! Interface to one instance of user generated content (UGC) to be represented on the platform's servers
		struct IUserGeneratedContent
		{
			//! Unique identifier of the UGC
			using Identifier = uint64;

			//! Type of content
			enum class EType
			{
				Community,
				Microtransaction,
				Collection,
				Art,
				Video,
				Screenshot,
				Game,
				Software,
				Concept,
				WebGuide,
				IntegratedGuide,
				Merch,
				ControllerBinding,
				SteamworksAccessInvite,
				SteamVideo,
			};

			//! Visibility of the UGC
			enum class EVisibility
			{
				Public,
				FriendsOnly,
				Private
			};

			virtual ~IUserGeneratedContent() {}

			//! Gets the unique platform-specific identifier of the UGC
			virtual Identifier GetIdentifier() = 0;

			//! Sets the title of the content
			virtual bool SetTitle(const char* newTitle) = 0;
			//! Sets the description of the content
			virtual bool SetDescription(const char* newDescription) = 0;
			//! Sets the visibility of the content
			virtual bool SetVisibility(EVisibility visibility) = 0;
			//! Sets the tags of the content
			virtual bool SetTags(const char** pTags, int numTags) = 0;
			//! Sets the content / body of the content
			virtual bool SetContent(const char* contentFolderPath) = 0;
			//! Sets the preview of the content
			virtual bool SetPreview(const char* previewFilePath) = 0;
			//! Commits the changed information to the platform services, along with a change log
			virtual void CommitChanges(const char* changeNote) = 0;
		};

		//! Interface to the UGC manager, allowing creation of UGC
		struct IUserGeneratedContentManager
		{
			//! Listener providing callbacks on asynchronous UGC events
			struct IListener
			{
				//! Called when content has been successfully created
				virtual void OnContentCreated(IUserGeneratedContent* pContent, bool bRequireLegalAgreementAccept) = 0;
			};

			virtual ~IUserGeneratedContentManager() {}

			//! Adds a UGC event listener
			virtual void AddListener(IListener& listener) = 0;
			//! Removes a UGC event listener
			virtual void RemoveListener(IListener& listener) = 0;

			// Creates a new UGC item, if successful calls OnContentCreated listener event
			virtual void Create(ApplicationIdentifier appId, IUserGeneratedContent::EType type) = 0;

			//! Creates a new UGC item and updates its information immediately
			virtual void CreateDirect(ApplicationIdentifier appId, IUserGeneratedContent::EType type,
				const char* title, const char* desc, IUserGeneratedContent::EVisibility visibility,
				const char** pTags, int numTags, const char* contentFolderPath, const char* previewPath) = 0;
		};
	}
}