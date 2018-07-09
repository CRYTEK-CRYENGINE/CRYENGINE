#include <CryEntitySystem/IEntitySystem.h>

class CMyNetSerializedComponent final : public IEntityComponent
{
public:
	// Provide a virtual destructor, ensuring correct destruction of IEntityComponent members
	virtual ~CMyNetSerializedComponent() = default;

	static void ReflectType(Schematyc::CTypeDesc<CMyNetSerializedComponent>& desc) { /* Reflect the component GUID in here. */}

	// Define the aspect / stream to use for this aspect, in our case we want a server stream since only the server can change health
	static constexpr EEntityAspects HealthAspect = eEA_GameServerA;
	// Override GetNetSerializeAspectMask to specify that we will handle HealthAspect
	virtual NetworkAspectType GetNetSerializeAspectMask() const override { return HealthAspect; }

	// Override NetSerialize, called on the server when the aspect has been marked dirty, and then on the client to read the data
	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags) override
	{
		// Check that we are processing the desired aspect
		if (aspect == HealthAspect)
		{
			// Serialize health, either to network (on the server) or from network (on a client)
			ser.Value("health", m_health);
		}
	}

	// Helper utility that game code should call to set the health of this component
	void SetHealth(float newHealth)
	{
		CRY_ASSERT_MESSAGE(gEnv->bServer, "Health can only be modified on the server!");
		// Update the local state
		m_health = newHealth;
		// Notify the network that the health aspect has changed
		// This will result in NetSerialize being called to serialize data over the network
		NetMarkAspectsDirty(HealthAspect);
	}

protected:
	float m_health = 100.f;
};