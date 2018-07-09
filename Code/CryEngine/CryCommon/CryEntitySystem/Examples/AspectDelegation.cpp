#include <CryEntitySystem/IEntitySystem.h>
#include <CryGame/IGameFramework.h>
#include <CryNetwork/Rmi.h>

// Example for how authority / control of an entity can be delegated to a specific client channel
// Note that INetEntity::EnableDelegatableAspect has to have been called during entity spawning for aspects we want to handle on the client
void DelegateAuthorityToClient(const EntityId controlledEntityId, const uint16 clientChannelId)
{
	INetChannel* pNetChannel = gEnv->pGameFramework->GetNetChannel(clientChannelId);
	gEnv->pGameFramework->GetNetContext()->DelegateAuthority(controlledEntityId, pNetChannel);
}

// Example for how authority / control of an entity can be delegated back to the server
void DelegateAuthorityToServer(const EntityId controlledEntityId)
{
	gEnv->pGameFramework->GetNetContext()->DelegateAuthority(controlledEntityId, nullptr);
}