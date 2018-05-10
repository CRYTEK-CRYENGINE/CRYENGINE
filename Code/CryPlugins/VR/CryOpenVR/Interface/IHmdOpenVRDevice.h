// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/VR/IHMDDevice.h>
#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IStereoRenderer.h>

namespace CryVR
{
namespace OpenVR
{

struct TextureDesc
{
	uint32 width;
	uint32 height;
	uint32 format;
};

enum ERenderAPI
{
	eRenderAPI_DirectX = 0,
	eRenderAPI_OpenGL  = 1,
};

enum ERenderColorSpace
{
	eRenderColorSpace_Auto   = 0,
	eRenderColorSpace_Gamma  = 1,
	eRenderColorSpace_Linear = 2,
};

struct IOpenVRDevice : public IHmdDevice
{
public:
	virtual void OnSetupEyeTargets(ERenderAPI api, ERenderColorSpace colorSpace, void* leftEyeHandle, void* rightEyeHandle) = 0;
	virtual void OnSetupOverlay(int id, ERenderAPI api, ERenderColorSpace colorSpace, void* overlayTextureHandle) = 0;
	virtual void OnDeleteOverlay(int id) = 0;
	virtual void SubmitOverlay(int id, const RenderLayer::CProperties* pOverlayProperties) = 0;
	virtual void SubmitFrame() = 0;
	virtual void OnPrepare() = 0;
	virtual void OnPostPresent() = 0;
	virtual bool IsActiveOverlay(int id) const = 0;
	virtual void GetRenderTargetSize(uint& w, uint& h) = 0;
	virtual void GetMirrorImageView(EEyeType eye, void* resource, void** mirrorTextureView) = 0;
protected:
	virtual ~IOpenVRDevice() {}
};
}      // namespace OpenVR
}      // namespace CryVR
