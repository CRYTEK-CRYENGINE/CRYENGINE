#pragma once

#include <CrySystem/VR/IHMDDevice.h>
#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IStereoRenderer.h>

namespace CryVR
{
	namespace VRgineers
	{
		struct TextureDesc
		{
			uint32 width;
			uint32 height;
			uint32 format;
		};

		struct IVRgineersDevice : public IHmdDevice
		{
		public:
			virtual void GetRenderTargetSize(uint& w, uint& h) = 0;

			virtual bool InitRenderer(ID3D11Device* d3dDevice, ID3D11DeviceContext* d3dContext) = 0;

			virtual void OnSetupEyeTargets(ID3D11Texture2D* leftEyeHandle, ID3D11Texture2D* rightEyeHandle) = 0;

			virtual void OnPrepare() = 0;

			virtual void SubmitFrame(uint64 frameId) = 0;
			
			virtual void OnPostPresent() = 0;
			
		protected:
			virtual ~IVRgineersDevice()
			{
			}
		};
	}      // namespace VRgineers
}      // namespace CryVR
