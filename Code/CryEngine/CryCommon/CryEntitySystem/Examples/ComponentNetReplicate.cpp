#include <CryEntitySystem/IEntitySystem.h>

class CMyReplicatedComponent final : public IEntityComponent
{
public:
	static void ReflectType(Schematyc::CTypeDesc<CMyReplicatedComponent>& desc) { /* Reflect the component GUID in here. */}

	// Implement NetReplicateSerialize, called on the server when an entity with the component is spawned
	virtual void NetReplicateSerialize(TSerialize ser) override 
	{
		ser.Value("health", m_health);
	}

	void SetHealth(float health) { m_health = health; }

protected:
	float m_health = 100.f;
};

void SpawnEntityOnAllClients()
{
	CRY_ASSERT(gEnv->bServer);

	// Spawn a new entity, but do *not* automatically initialize it
	// This is delayed as want to add our component before network replication occurs
	if (IEntity* pEntity = gEnv->pEntitySystem->SpawnEntity(SEntitySpawnParams(), false))
	{
		// Create an instance of our component and attach it to the entity
		CMyReplicatedComponent* pComponent = pEntity->CreateComponentClass<CMyReplicatedComponent>();
		// Change the initial health, as an example of replication a new state on the remote clients
		pComponent->SetHealth(0.f);

		// Now initialize the entity, resulting in CMyComponent::NetReplicateSerialize first being called on the server (this instance) to write the data to network, and then on all clients to deserialize.
		gEnv->pEntitySystem->InitEntity(pEntity, SEntitySpawnParams());
	}
}