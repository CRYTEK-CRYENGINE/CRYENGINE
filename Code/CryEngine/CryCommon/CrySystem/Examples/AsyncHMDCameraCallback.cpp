#include <CrySystem/VR/IHMDManager.h>
#include <CrySystem/VR/IHMDDevice.h>

// Minimal example for how to implement asynchronous camera injection for VR on the game side
class CMyAsyncCameraCallback final : public IHmdDevice::IAsyncCameraCallback
{
	CMyAsyncCameraCallback()
	{
		// If there is an HMD device available, set the async camera callback to this object
		if (IHmdDevice* pDevice = gEnv->pSystem->GetHmdManager()->GetHmdDevice())
		{
			pDevice->SetAsyncCameraCallback(this);
		}
	}

	virtual ~CMyAsyncCameraCallback()
	{
		// Clear the async camera callback on the HMD, assuming that we set it in our constructor
		if (IHmdDevice* pDevice = gEnv->pSystem->GetHmdManager()->GetHmdDevice())
		{
			pDevice->SetAsyncCameraCallback(nullptr);
		}
	}

	virtual bool OnAsyncCameraCallback(const HmdTrackingState& state, IHmdDevice::AsyncCameraContext& context) override
	{
		// For the sake of this example we choose not to scale our camera
		const Vec3 cameraScale = Vec3(1.f);
		// Keep the camera at the default rotation (identity matrix)
		const Quat cameraRotation = IDENTITY;
		// Position the camera at the world-space coordinates {50,50,50}
		const Vec3 cameraPosition = Vec3(50, 50, 50);

		// Create a matrix from the units specified above
		const Matrix34 cameraMatrix = Matrix34::Create(cameraScale, cameraRotation, cameraPosition);

		// Apply the latest HMD orientation to our desired camera matrix (thus allowing the user to turn their head around)
		context.outputCameraMatrix = cameraMatrix * Matrix34::Create(Vec3(1.f), state.pose.orientation, state.pose.position);

		// Indicate that we returned a valid output camera matrix
		return true;
	}
};