// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryInput/IInput.h>

#include <CryMath/Cry_Geo.h>

#include <CryCore/optional.h>

// TODO: Remove when full VR device implementation (incl. renderer) is in plugin
enum EHmdClass
{
	eHmdClass_Oculus,
	eHmdClass_OpenVR,
	eHmdClass_Osvr
};

enum EHmdStatus
{
	eHmdStatus_OrientationTracked = 0x0001,
	eHmdStatus_PositionTracked    = 0x0002,
	// Potentially deprecated since Oculus SDK 1.0 - Need to double check with HTC Vive and PS VR
	eHmdStatus_CameraPoseTracked  = 0x0004, // deprecated?
	eHmdStatus_PositionConnected  = 0x0008, // deprecated?
	eHmdStatus_HmdConnected       = 0x0010, // deprecated?
	//HS_ValidReferencePosition = 0x0020,

	eHmdStatus_IsUsable = /*eHmdStatus_HmdConnected | */ eHmdStatus_OrientationTracked
};

enum EEyeType
{
	eEyeType_LeftEye = 0,
	eEyeType_RightEye,
	eEyeType_NumEyes
};

enum EVRComponent
{
	eVRComponent_Hmd = BIT(0),
	eVRComponent_Controller = BIT(1),
	eVRComponent_All = eVRComponent_Hmd | eVRComponent_Controller
};

struct HmdDeviceInfo
{
	HmdDeviceInfo()
		: productName(0)
		, manufacturer(0)
		, screenWidth(0)
		, screenHeight(0)
		, fovH(0)
		, fovV(0)
	{
	}

	const char*  productName;
	const char*  manufacturer;

	unsigned int screenWidth;
	unsigned int screenHeight;

	float        fovH;
	float        fovV;
};

enum class EHmdSocialScreen
{
	Off = -1,
	DistortedDualImage = 0,
	UndistortedDualImage,
	UndistortedLeftEye,
	UndistortedRightEye,

	// helpers
	FirstInvalidIndex
};

enum class EHmdSocialScreenAspectMode
{
	Stretch = 0,
	Center = 1,
	Fill = 2,

	FirstInvalidIndex
};

// Specifies the type of coordinates that will be returned from HMD devices
// See the hmd_tracking_origin CVar for changing this at runtime
enum class EHmdTrackingOrigin
{
	// Origin at floor height, meaning that the player in-game become as tall as they expect from real life.
	// Recentering tracking origin will not affect the height when this is set.
	Standing = 0,
	// Origin at pre-set player eye height
	// Requires recentering on application start, or when switching users
	// Designed for seated experiences where the player only moves the head, e.g. cockpit, camera attached to an entity, etc..
	Seated = 1,
};

struct HmdPoseState
{
	HmdPoseState()
		: orientation(Quat::CreateIdentity())
		, position(ZERO)
		, angularVelocity(ZERO)
		, linearVelocity(ZERO)
		, angularAcceleration(ZERO)
		, linearAcceleration(ZERO)
	{
	}

	Quat orientation;
	Vec3 position;
	Vec3 angularVelocity;
	Vec3 linearVelocity;
	Vec3 angularAcceleration;
	Vec3 linearAcceleration;
};

struct HmdTrackingState
{
	HmdTrackingState()
		: pose()
		, statusFlags(0)
	{
	}

	bool CheckStatusFlags(unsigned int checked, unsigned int wanted) const { return (statusFlags & checked) == wanted; }
	bool CheckStatusFlags(unsigned int checkedAndWanted) const             { return CheckStatusFlags(checkedAndWanted, checkedAndWanted); }

	HmdPoseState pose;

	unsigned int statusFlags;
};

enum EHmdController
{
	// Oculus
	eHmdController_OculusLeftHand                 = 0,
	eHmdController_OculusRightHand                = 1,
	eHmdController_MaxNumOculusControllers        = 2,
	// OpenVR
	eHmdController_OpenVR_1                       = 0,
	eHmdController_OpenVR_2                       = 1,
	eHmdController_OpenVR_MaxNumOpenVRControllers = 2,
};

enum EHmdProjection
{
	eHmdProjection_Stereo = 0,
	eHmdProjection_Mono_Cinema,
	eHmdProjection_Mono_HeadLocked,
};

struct HMDCameraSetup
{
	// Pupil distance
	float ipd;

	// Camera asymmetries
	float l, r, t, b;

	float sfov;

	static HMDCameraSetup fromProjectionMatrix(const Matrix44A &projectionMatrix, float projRatio, float fnear)
	{
		HMDCameraSetup params;

		const double x = static_cast<double>(projectionMatrix(0, 0));
		const double y = static_cast<double>(projectionMatrix(1, 1));
		const double z = static_cast<double>(projectionMatrix(0, 2));
		const double w = static_cast<double>(projectionMatrix(1, 2));

		const double n_x = static_cast<double>(fnear) / x;
		const double n_y = static_cast<double>(fnear) / y;
		params.sfov = static_cast<float>(2.0 * atan(1.0 / y));

		// Compute symmetric frustum
		const double st = n_y;
		const double sb = -st;
		const double sr = st * static_cast<double>(projRatio);
		const double sl = -sr;

		// Compute asymmetries
		const double l = (z - 1) * n_x;
		const double b = (w - 1) * n_y;
		const double r = 2 * n_x + l;
		const double t = 2 * n_y + b;

		params.l = static_cast<float>(l - sl);
		params.r = static_cast<float>(r - sr);
		params.b = static_cast<float>(b - sb);
		params.t = static_cast<float>(t - st);

		return params;
	}

	static HMDCameraSetup fromAsymmetricFrustum(float r, float t, float l, float b, float projRatio, float fnear)
	{
		HMDCameraSetup params;

		const double n = static_cast<double>(fnear);
		const double y = 2.0 / (static_cast<double>(t) + static_cast<double>(b));

		params.sfov = static_cast<float>(2.0 * atan(1.0 / y));

		// Compute symmetric frustum
		const double st = n / y;
		const double sb = -st;
		const double sr = st * static_cast<double>(projRatio);
		const double sl = -sr;

		params.l = static_cast<float>(-l * n - sl);
		params.r = static_cast<float>( r * n - sr);
		params.b = static_cast<float>(-b * n - sb);
		params.t = static_cast<float>( t * n - st);

		return params;
	}
};

struct IHmdController
{
	typedef uint32 TLightColor;

	enum ECaps
	{
		eCaps_Buttons       = BIT(0),
		eCaps_Tracking      = BIT(1),
		eCaps_Sticks        = BIT(2),
		eCaps_Capacitors    = BIT(3),
		eCaps_Gestures      = BIT(4),
		eCaps_ForceFeedback = BIT(5),
		eCaps_Color         = BIT(6),
	};

	virtual bool IsConnected(EHmdController id) const = 0;

	// TODO: refactor FG node to keep track of the button state internally w/o using this functions.
	//! In general action listeners should be used instead of calling directly this functions.
	//! These functions are still used in FG nodes used for quick prototyping by design.
	//! \return State of the current buttons/triggers/gestures.
	virtual bool  IsButtonPressed(EHmdController id, EKeyId button)  const = 0;
	virtual bool  IsButtonTouched(EHmdController id, EKeyId button)  const = 0;
	virtual bool  IsGestureTriggered(EHmdController id, EKeyId gesture) const = 0;
	virtual float GetTriggerValue(EHmdController id, EKeyId trigger) const = 0;
	virtual Vec2  GetThumbStickValue(EHmdController id, EKeyId stick)   const = 0;

	//! \return Tracking state in the Hmds internal coordinates system.
	virtual const HmdTrackingState& GetNativeTrackingState(EHmdController controller)      const = 0;

	//! \return Tracking state in the Hmd's local tracking space using CRYENGINE's coordinate system.
	virtual const HmdTrackingState& GetLocalTrackingState(EHmdController controller) const = 0;

	//! Apply a signal to the force-feedback motors of the selected controller.
	virtual void ApplyForceFeedback(EHmdController id, float freq, float amplitude) = 0;

	//! Access to the light color of the some controllers.
	virtual void        SetLightColor(EHmdController id, TLightColor color) = 0;
	virtual TLightColor GetControllerColor(EHmdController id) const = 0;

	//! \return Capabilities of the selected controller.
	virtual uint32 GetCaps(EHmdController id) const = 0;

	//! \returns the hardware device handle(if applicable) of the selected controller
	virtual void* GetDeviceHandle(EHmdController id) const { return nullptr; };

protected:
	virtual ~IHmdController() noexcept {}
};

//! Represents a head-mounted device (Virtual Reality) connected to the system
struct IHmdDevice
{
private:
	stl::optional<std::pair<Quat, Vec3>> m_orientationForLateCameraInjection;

protected:
	void OnEndFrame() { m_orientationForLateCameraInjection = stl::nullopt; }

public:
	enum EInternalUpdate
	{
		eInternalUpdate_DebugInfo = 0,
	};

	virtual void      AddRef() = 0;
	virtual void      Release() = 0;

	virtual EHmdClass GetClass() const = 0;
	virtual void      GetDeviceInfo(HmdDeviceInfo& info) const = 0;

	virtual void      GetCameraSetupInfo(float& fov, float& aspectRatioFactor) const = 0;
	virtual HMDCameraSetup GetHMDCameraSetup(int nEye, float projRatio, float fnear) const = 0;

	virtual void      UpdateInternal(EInternalUpdate) = 0;
	virtual void      RecenterPose() = 0;
	virtual void      UpdateTrackingState(EVRComponent, uint64_t frameId) = 0;

	//! \return Tracking state in Hmd's internal coordinates system.
	virtual const HmdTrackingState& GetNativeTrackingState() const = 0;

	//! \return Tracking state in the Hmd's local tracking space using CRYENGINE's coordinate system.
	virtual const HmdTrackingState& GetLocalTrackingState() const = 0;

	//! \return Quad (four corners) of the Play Area in which the player can move around safely. A value of zero indicates that this HMD does not handle room-scale functionality.
	virtual Quad GetPlayArea() const = 0;
	//! \return 2D width and height of the play area in which the player can move around safely. A value of zero indicates that this HMD does not handle room-scale functionality.
	virtual Vec2 GetPlayAreaSize() const = 0;

	virtual const IHmdController*   GetController() const = 0;
	virtual int                     GetControllerCount() const = 0;

	//useful for querying preferred rendering resolution for devices where the screen resolution is not the resolution the SDK wants for rendering (OSVR)
	virtual void GetPreferredRenderResolution(unsigned int& width, unsigned int& height) = 0;
	//Disables & Resets the tracking state to identity. Useful for benchmarking where we want the HMD to behave normally except that we want to force the direction of the camera.
	virtual void DisableHMDTracking(bool disable) = 0;

	//! Enables a late camera injection to the render thread based on updated HMD tracking feedback, must be called from main thread.
	void EnableLateCameraInjectionForCurrentFrame(const std::pair<Quat, Vec3> &currentOrientation) { m_orientationForLateCameraInjection = (stl::optional<std::pair<Quat, Vec3>>) currentOrientation; }
	const stl::optional<std::pair<Quat, Vec3>>& GetOrientationForLateCameraInjection() const { return m_orientationForLateCameraInjection; }

	//! Can be called from any thread to retrieve most up to date camera transformation
	virtual stl::optional<Matrix34> RequestAsyncCameraUpdate(uint64_t frameId, const Quat& q, const Vec3 &p) = 0;

protected:
	virtual ~IHmdDevice() noexcept {}
};

