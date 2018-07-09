// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Interactive objects are registered here with required params

-------------------------------------------------------------------------
History:
- 10:12:2009: Created by Benito G.R.

*************************************************************************/

#pragma once

#ifndef __INTERACTIVEOBJECT_REGISTRY_H__
#define __INTERACTIVEOBJECT_REGISTRY_H__

#include "../Utility/CryHash.h"
#include "ItemString.h"

#include <ICryMannequinDefs.h>
#include "InteractiveObjectEnums.h"
#include "ICryMannequinProceduralClipFactory.h"

class IActionController;
class IAnimationDatabase;
struct SControllerDef;

class CInteractiveObjectRegistry
{
public : 

	struct SInteraction
	{
		const char *helper;
		TagID			  targetStateTagID;
	};

	struct SMannequinParams
	{
		TagID interactionTagID;
		TagID stateTagID;

		SMannequinParams()
			: interactionTagID(TAG_ID_INVALID)
			, stateTagID(TAG_ID_INVALID)
		{

		}

		void GetMemoryUsage(ICrySizer *pSizer) const
		{

		}
	};

private:

	typedef std::map< EntityId, SMannequinParams > TEntityToTagMap;

public:

	void Init();
	void Shutdown();

	void OnInteractiveObjectSutdown(const EntityId entityId);

	TagID GetInteractionTag(const char* interactionName) const;
	const SMannequinParams GetInteractionTagForEntity(const EntityId entityId, const int interactionIndex) const;

	void QueryInteractiveActionOptions( const EntityId entityId, const TagID interactionTag, const TagID stateTagId, std::vector<SInteraction> &options);
	void ApplyInteractionToObject(IEntity *pEntity, const TagID interactionTypeTag, const int interactionIndex) const;

	void GetMemoryUsage(ICrySizer *pSizer) const
	{
		pSizer->AddContainer(m_entityToTag);
	}

private:

	TEntityToTagMap			m_entityToTag;
	IProceduralClipFactory::THash m_clipLocatorHash;

	FragmentID m_interactFragID;
	const IAnimationDatabase *m_pDatabasePlayer;
	const IAnimationDatabase *m_pDatabaseObject;
	const SControllerDef *m_pControllerDef;
};

#endif
