#include <CryEntitySystem/IEntitySystem.h>
#include <CryNetwork/Rmi.h>

class CMyRemoteMachineInvocationComponent final : public IEntityComponent
{
	// Provide a virtual destructor, ensuring correct destruction of IEntityComponent members
	virtual ~CMyRemoteMachineInvocationComponent() = default;

	static void ReflectType(Schematyc::CTypeDesc<CMyRemoteMachineInvocationComponent>& desc) { /* Reflect the component GUID in here. */ }

	// Define parameters that we will send with our RMI
	// The SerializeWith function will be called once to serialize, and then on the remote side to deserialize
	struct SJumpParameters
	{
		// In our example we will serialize a positional coordinate
		Vec3 position;

		void SerializeWith(TSerialize ser)
		{
			// Serialize the position with the 'wrld' compression policy
			// This policy can be tweaked to only support the maximum size of a project's level
			ser.Value("position", position, 'wrld');
		}
	};

	// The remote method, only executed on the server
	bool ServerJump(SJumpParameters&& p, INetChannel* pNetChannel)
	{
		// Client requested a jump, validate inputs here
		// If successful, run jump logic on the server
	}

	// Use a type definition to simply registering and invoking the RMI
	using ServerJumpRMI = SRmi<RMI_WRAP(&CMyRemoteMachineInvocationComponent::ServerJump)>;

	virtual void Initialize() override
	{
		// Whether or not this RMI is to be executed on the server
		// Otherwise it can only be invoked on clients
		const bool isServerCall = true;
		// Whether RMI is invoked before, after or independently from aspect serialization
		// Attaching before or after aspects can increase packet size, favor NoAttach when possible
		const ERMIAttachmentType attachmentType = eRAT_NoAttach;
		// Describes the reliability type for packet delivery
		// Reliable RMIs have significant overhead and should be used carefully
		const ENetReliabilityType reliability = eNRT_UnreliableUnordered;

		// Register the jump RMI so that it can be invoked later during the entity lifecycle
		ServerJumpRMI::Register(this, attachmentType, isServerCall, reliability);

		// Now bind the entity to network
		m_pEntity->GetNetEntity()->BindToNetwork();
	}

public:
	// For the sake of this example, this function should be called when a player wants to jump
	// If executed on the server, the jump can be handled immediately - otherwise an RMI is sent to the server
	void RequestJump()
	{
		if (gEnv->bServer)
		{
			// We were already on the server, so the code can execute safely
			/* ... */
		}
		else
		{
			// Client requested jump, invoke a remote method invocation on the server
			ServerJumpRMI::InvokeOnServer(this, SJumpParameters{ m_pEntity->GetPos() });
		}
	}
};