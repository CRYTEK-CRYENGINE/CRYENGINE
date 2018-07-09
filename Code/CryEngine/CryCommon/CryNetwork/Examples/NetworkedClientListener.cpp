#include <CryNetwork/INetwork.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryGame/IGameFramework.h>
#include <unordered_map>

// Simple example of how to handle client connection events, and spawning entities for players automatically
class CMyNetworkedClientListener final : public INetworkedClientListener
{
public:
	CMyNetworkedClientListener()
	{
		// Listen for client connection events, in order to create the local player
		gEnv->pGameFramework->AddNetworkedClientListener(*this);
	}

	virtual ~CMyNetworkedClientListener()
	{
		// Remove any registered listeners before 'this' becomes invalid
		gEnv->pGameFramework->RemoveNetworkedClientListener(*this);
	}

protected:
	// Store the player channel identifier and entity identifiers side-by-side
	std::unordered_map<int, EntityId> m_clientEntityIdLookupMap;

	// Sent to the server when a new client has started connecting
	// Return false to disallow the connection
	virtual bool OnClientConnectionReceived(int channelId, bool bIsReset) override
	{
		// Connection received from a client, create a player entity
		SEntitySpawnParams spawnParams;
		// Assign a default name for the player
		// This is not required, but can be useful for future debugging
		const string playerName = string().Format("Player(%i)", channelId);
		spawnParams.sName = playerName;
		// Notify the network system that this is a fully dynamic entity
		spawnParams.nFlags |= ENTITY_FLAG_NEVER_NETWORK_STATIC;

		// Determine if we are currently spawning the local player
		const bool isLocalPlayer = m_clientEntityIdLookupMap.empty() && !gEnv->IsDedicated();

		// If we are the local player, use the predefined local player identifier and set the ENTITY_FLAG_LOCAL_PLAYER flag
		if (isLocalPlayer)
		{
			spawnParams.id = LOCAL_PLAYER_ENTITY_ID;
			spawnParams.nFlags |= ENTITY_FLAG_LOCAL_PLAYER;
		}

		// Now spawn the player entity
		if (IEntity* pPlayerEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams))
		{
			// Set the local player entity channel id, and bind it to the network so that it can support Multiplayer contexts
			pPlayerEntity->GetNetEntity()->SetChannelId(channelId);
			pPlayerEntity->GetNetEntity()->BindToNetwork();

			// Push the entity identifier into our map, with the channel id as the key
			m_clientEntityIdLookupMap.emplace(std::make_pair(channelId, pPlayerEntity->GetId()));

			return true;
		}

		// Player entity spawning failed, disallow the connection
		return false;
	}

	// Sent to the server when a new client has finished connecting and is ready for gameplay
	// Return false to disallow the connection and kick the player
	virtual bool OnClientReadyForGameplay(int channelId, bool bIsReset) override
	{
		/* This is where gameplay logic should start occurring */
		return true;
	}

	//! Sent to the server when a client is disconnected
	virtual void OnClientDisconnected(int channelId, EDisconnectionCause cause, const char* description, bool bKeepClient) override
	{
		// Check if an entity was created for this client
		decltype(m_clientEntityIdLookupMap)::const_iterator clientIterator = m_clientEntityIdLookupMap.find(channelId);
		if (clientIterator != m_clientEntityIdLookupMap.end())
		{
			// An entity had been created for this client, remove it from the scene
			const EntityId clientEntityId = clientIterator->second;
			gEnv->pEntitySystem->RemoveEntity(clientEntityId);
		}
	}

	// Sent to the server when a client is timing out (no packets for X seconds)
	// Return true to allow disconnection, otherwise false to keep client.
	virtual bool OnClientTimingOut(int channelId, EDisconnectionCause cause, const char* description) override { return true; }

	// Sent to the local client on disconnect
	virtual void OnLocalClientDisconnected(EDisconnectionCause cause, const char* description) override {}
};