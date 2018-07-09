#include <CryEntitySystem/IEntitySystem.h>

class CMinimalComponent final : public IEntityComponent
{
public:
	// Provide a virtual destructor, ensuring correct destruction of IEntityComponent members
	virtual ~CMinimalComponent() = default;
	
	// Reflect type to set a unique identifier for this component
	// and provide additional information to expose it in the sandbox
	static void ReflectType(Schematyc::CTypeDesc<CMinimalComponent>& desc)
	{
		// Provide a globally unique identifier for the component, can be generated in Visual Studio via Tools -> Create GUID (in registry format).
		desc.SetGUID("{35EC6526-3D24-4376-9509-806908FF4698}"_cry_guid);
		desc.SetEditorCategory("MyComponents");
		desc.SetLabel("MyComponent");
	}
};