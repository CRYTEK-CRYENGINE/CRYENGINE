#include <CrySystem/VR/IHMDManager.h>
#include <CrySystem/VR/IHMDDevice.h>
#include <CryEntitySystem/IEntitySystem.h>

class CLateCameraInjectionEventComponent final : public IEntityComponent
{
	// Should be called, from main thread, on each frame.
	static bool PrepareLateCameraInjection()
	{
		// Keep the camera at the default rotation (identity matrix)
		const Quat cameraRotation = IDENTITY;
		// Position the camera at the world-space coordinates {50,50,50}
		const Vec3 cameraPosition = Vec3(50, 50, 50);

		if (IHmdDevice* pDevice = gEnv->pSystem->GetHmdManager()->GetHmdDevice())
		{
			pDevice->EnableLateCameraInjectionForCurrentFrame(std::make_pair(cameraRotation, cameraPosition));

			// Indicate that the late camera injection was prepared successfully for current frame
			return true;
		}

		return false;
	}

public:
	// Provide a virtual destructor, ensuring correct destruction of IEntityComponent members
	virtual ~CLateCameraInjectionEventComponent() = default;

	static void ReflectType(Schematyc::CTypeDesc<CLateCameraInjectionEventComponent>& desc) { /* Reflect the component GUID in here. */ }

	// Override the ProcessEvent function to receive the callback whenever an event specified in GetEventMask is triggered
	virtual void ProcessEvent(const SEntityEvent& event) override
	{
		// Check if this is the update event
		if (event.event == ENTITY_EVENT_UPDATE)
		{
			/* Handle update logic here */
			PrepareLateCameraInjection();
		}
	}

	// As an optimization, components have to specify a bitmask of the events that they want to handle
	// This is called once at component creation, and then only if IEntity::UpdateComponentEventMask is called, to support dynamic change of desired events (useful for disabling update when you don't need it)
	virtual uint64 GetEventMask() const override { return BIT64((uint64)ENTITY_EVENT_UPDATE); }
};