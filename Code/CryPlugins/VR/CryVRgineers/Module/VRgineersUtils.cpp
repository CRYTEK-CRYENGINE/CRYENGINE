#include "VRgineersUtils.h"

namespace CryVR
{
	namespace VRgineers
	{
		Matrix44 VRgineersUtils::CreateCryMatrix44FromFloatArray(const float* m)
		{
			Matrix44 result(
				m[0], m[1], m[2], m[3],
				m[4], m[5], m[6], m[7],
				m[8], m[9], m[10], m[11],
				m[12], m[13], m[14], m[15]
			);

			return result;
		}

		Vec3 VRgineersUtils::VRGPositionToCry(const Vec3& vrgPos)
		{
			return Vec3(vrgPos.x, -vrgPos.z, vrgPos.y);
		}

		Quat VRgineersUtils::VRGOrientationToCry(const Quat& vrgQuat)
		{
			Matrix33 m33(vrgQuat);
			Vec3 column1 = -vrgQuat.GetColumn2();
			m33.SetColumn2(m33.GetColumn1());
			m33.SetColumn1(column1);
			return (Quat::CreateRotationX(gf_PI * 0.5f) * Quat(m33).Normalize()).Normalize();
		}

		Frustum VRgineersUtils::GetFrustumFromProjectionMatrix(const Matrix44& pm)
		{
			float sngA = std::fabsf(pm[2][2]);
			float sngB = std::fabsf(pm[3][2]);

			float nearCP = std::fabsf(-sngB / sngA);
			float farCP = std::fabsf(sngB / (1.0f - sngA));

			//float nearCP = pm[2][3] / (pm[2][2] - 1);
			//float farCP = pm[2][3] / (pm[2][2] + 1);

			float leftNear = nearCP * (pm[2][0] - 1) / pm[0][0];
			float rightNear = nearCP * (pm[2][0] + 1) / pm[0][0];
			float topNear = nearCP * (pm[2][1] + 1) / pm[1][1];
			float bottomNear = nearCP * (pm[2][1] - 1) / pm[1][1];

			float leftFar = farCP * (pm[2][0] - 1) / pm[0][0];
			float rightFar = farCP * (pm[2][0] + 1) / pm[0][0];
			float topFar = farCP * (pm[2][1] + 1) / pm[1][1];
			float bottomFar = farCP * (pm[2][1] - 1) / pm[1][1];

			return Frustum{ nearCP, farCP, leftNear, rightNear, topNear, bottomNear, leftFar, rightFar, topFar, bottomFar };
		}

		Fov VRgineersUtils::GetFovFromFrustum(const Frustum& frustum)
		{
			float fovLeft = std::atan2f(frustum.farLeft, frustum.farCP);
			float fovRight = std::atan2f(frustum.farRight, frustum.farCP);
			float fovTop = std::atan2f(frustum.farTop, frustum.farCP);
			float fovBottom = std::atan2f(frustum.farBottom, frustum.farCP);

			return Fov{ fovLeft, fovRight, fovTop, fovBottom };
		}

		Fov VRgineersUtils::GetFovFromProjectionMatrix(const Matrix44& pm)
		{
			const auto frustum = GetFrustumFromProjectionMatrix(pm);

			return GetFovFromFrustum(frustum);
		}

		Matrix44 VRgineersUtils::CreateProjectionMatrix(const float left, const float right, const float top, float const bottom, const float nearCP, const float farCP)
		{
			const float tanAngleWidth = right - left;
			const float tanAngleHeight = top - bottom;
			const float frustumDepth = farCP - nearCP;

			Matrix44 result(
				2 / tanAngleWidth,								              0,								0,  0,
				0,											 2 / tanAngleHeight,							    0,  0,
				(right + left) / tanAngleWidth, (top + bottom) / tanAngleHeight,		    -farCP / frustumDepth, -1,
				0,														      0, -(farCP * nearCP) / frustumDepth,  0
			);

			return result;
		}

		Matrix44 VRgineersUtils::GetProjectionMatrixFromFov(const Fov& fov, const float nearClippingPlane, const float farClippingPlane)
		{
			const float tanLeft = tanf(fov.left);
			const float tanRight = tanf(fov.right);

			const float tanTop = tanf(fov.top);
			const float tanBottom = tanf(fov.bottom);

			return CreateProjectionMatrix(tanLeft, tanRight, tanTop, tanBottom, nearClippingPlane, farClippingPlane);
		}
	}
}