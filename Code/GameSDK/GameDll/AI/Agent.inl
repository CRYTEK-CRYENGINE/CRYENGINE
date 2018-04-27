// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef Agent_inl
#define Agent_inl


// =======================================================================
//	Query if the agent is dead.
//
//	Returns:	True if dead; otherwise false.
//
inline bool Agent::IsDead() const
{
	const IActor* actor = GetActor();
	assert(actor);

	if (actor)
		return actor->IsDead();
	else
		return true;
}


// =======================================================================
//	Query if the agent is hidden.
//
//	Also see IEntity::IsHidden()
//
//	Returns:	True if the agent is hidden; otherwise false.
//
inline bool Agent::IsHidden() const
{
	assert(GetEntity() != NULL);
	return GetEntity()->IsHidden();
}


// =======================================================================
//	Get the actor interface.
//
//	Returns:	The actor interface (or NULL if it could not be obtained).
//
inline const IActor* Agent::GetActor() const
{
	return g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(GetEntityID());
}


#endif // Agent_inl
