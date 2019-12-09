#pragma once

#include <CryMath/Cry_Math.h>

namespace CryVR
{
	namespace VRgineers
	{
		struct Frustum
		{
			float nearCP;
			float farCP;

			float nearLeft;
			float nearRight;
			float nearTop;
			float nearBottom;

			float farLeft;
			float farRight;
			float farTop;
			float farBottom;
		};

		struct Fov
		{
			float left;
			float right;
			float top;
			float bottom;
		};

		class VRgineersUtils
		{
		public:
			static Matrix44 CreateCryMatrix44FromFloatArray(const float* m);

			static Vec3 VRGPositionToCry(const Vec3& vrgPos);

			static Quat VRGOrientationToCry(const Quat vrgQuat);

			static Frustum GetFrustumFromProjectionMatrix(const Matrix44& pm);

			static Fov GetFovFromProjectionMatrix(const Matrix44& pm);
		};
	}
}
