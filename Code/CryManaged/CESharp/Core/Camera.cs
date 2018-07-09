// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.Runtime.CompilerServices;
using CryEngine.Common;
using CryEngine.Rendering;

namespace CryEngine
{
	/// <summary>
	/// Static class exposing access to the current view camera
	/// </summary>
	public static class Camera
	{
		/// <summary>
		/// Set or get the position of the current view camera.
		/// </summary>
		public static Vector3 Position
		{
			get
			{
				return Engine.System.GetViewCamera().GetPosition();
			}
			set
			{
				Engine.System.GetViewCamera().SetPosition(value);
			}
		}

		/// <summary>
		/// Get or set the facing direction of the current view camera.
		/// </summary>
		public static Vector3 ForwardDirection
		{
			get
			{
				return Engine.System.GetViewCamera().GetMatrix().GetColumn1();
			}
			set
			{
				var camera = Engine.System.GetViewCamera();
				var newRotation = new Quaternion(value);

				camera.SetMatrix(new Matrix3x4(Vector3.One, newRotation, camera.GetPosition()));
			}
		}

		/// <summary>
		/// Get or set the transformation matrix of the current view camera.
		/// </summary>
		public static Matrix3x4 Transform
		{
			get
			{
				return Engine.System.GetViewCamera().GetMatrix();
			}
			set
			{
				Engine.System.GetViewCamera().SetMatrix(value);
			}
		}

		/// <summary>
		/// Get or set the rotation of the current view camera
		/// </summary>
		public static Quaternion Rotation
		{
			get
			{
				return new Quaternion(Engine.System.GetViewCamera().GetMatrix());
			}
			set
			{
				var camera = Engine.System.GetViewCamera();

				camera.SetMatrix(new Matrix3x4(Vector3.One, value, camera.GetPosition()));
			}
		}

		/// <summary>
		/// The amount of horizontal pixels this camera is currently rendering.
		/// </summary>
		public static int RenderWidth
		{
			get
			{
				var camera = Engine.System.GetViewCamera();
				return camera.GetViewSurfaceX();
			}
		}

		/// <summary>
		/// The amount of vertical pixels this camera is currently rendering.
		/// </summary>
		public static int RenderHeight
		{
			get
			{
				var camera = Engine.System.GetViewCamera();
				return camera.GetViewSurfaceZ();
			}
		}

		/// <summary>
		/// Gets or sets the field of view of the view camera in degrees.
		/// </summary>
		/// <value>The field of view.</value>
		public static float FieldOfView
		{
			get
			{
				return MathHelpers.RadiansToDegrees(Engine.System.GetViewCamera().GetFov());
			}
			set
			{
				var camera = Engine.System.GetViewCamera();

				camera.SetFrustum(camera.GetViewSurfaceX(), camera.GetViewSurfaceZ(), MathHelpers.DegreesToRadians(value));
			}
		}

		/// <summary>
		/// Converts a viewport point to a position in world-space.
		/// </summary>
		/// <param name="x">Horizontal viewport position.</param>
		/// <param name="y">Vertical viewport position.</param>
		/// <param name="depth">Depth of the viewport point.</param>
		/// <param name="position">Position of the viewport point in world-space.</param>
		/// <returns><c>true</c> if the viewport point could be converted, <c>false</c> otherwise.</returns>
		public static bool ViewportPointToWorldPoint(float x, float y, float depth, out Vector3 position)
		{
			var camera = Engine.System.GetViewCamera();
			int width = camera.GetViewSurfaceX();
			int height = camera.GetViewSurfaceZ();
			int screenX = (int)(width * x + 0.5f);
			int screenY = (int)(height * y + 0.5f);

			Vec3 result = new Vec3();
			bool visible = camera.Unproject(new Vec3(x, height - y, depth), result);
			position = result;
			return visible;
		}

		/// <summary>
		/// Converts a viewport point to a position in world-space.
		/// </summary>
		/// <param name="viewportPoint">Viewport point that will be converted.</param>
		/// <param name="depth">Depth of the viewport point.</param>
		/// <param name="position">Position of the viewport point in world-space.</param>
		/// <returns><c>true</c> if the viewport point could be converted, <c>false</c> otherwise.</returns>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static bool ViewportPointToWorldPoint(Vector2 viewportPoint, float depth, out Vector3 position)
		{
			return ViewportPointToWorldPoint(viewportPoint.x, viewportPoint.y, depth, out position);
		}

		/// <summary>
		/// Converts a viewport point to a direction in world-space.
		/// </summary>
		/// <param name="x">Horizontal viewport position.</param>
		/// <param name="y">Vertical viewport position.</param>
		/// <param name="direction">Direction into world-space.</param>
		/// <returns><c>true</c> if the viewport point could be converted, <c>false</c> otherwise.</returns>
		public static bool ViewportPointToDirection(float x, float y, out Vector3 direction)
		{
			var camera = Engine.System.GetViewCamera();
			int width = camera.GetViewSurfaceX();
			int height = camera.GetViewSurfaceZ();
			int screenX = (int)(width * x + 0.5f);
			int screenY = (int)(height * y + 0.5f);

			Vec3 result = new Vec3();
			bool visible = camera.Unproject(new Vec3(x, height - y, 1), result);
			var position = result;
			direction = (position - Position).Normalized;
			return visible;
		}

		/// <summary>
		/// Converts a viewport point to a direction in world-space.
		/// </summary>
		/// <param name="viewportPoint">Viewport point that will be converted.</param>
		/// <param name="direction">Direction into world-space.</param>
		/// <returns><c>true</c> if the viewport point could be converted, <c>false</c> otherwise.</returns>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static bool ViewportPointToDirection(Vector2 viewportPoint, out Vector3 direction)
		{
			return ViewportPointToDirection(viewportPoint.x, viewportPoint.y, out direction);
		}

		/// <summary>
		/// Converts a screen position to a position in world-space.
		/// </summary>
		/// <param name="x">Horizontal position on the screen.</param>
		/// <param name="y">Vertical position on the screen.</param>
		/// <param name="depth">Depth of the screenpoint.</param>
		/// <param name="position">Position of the screenpoint in world-space.</param>
		/// <returns><c>true</c> if the screenpoint could be converted, <c>false</c> otherwise.</returns>
		public static bool ScreenPointToWorldPoint(int x, int y, float depth, out Vector3 position)
		{
			var camera = Engine.System.GetViewCamera();
			Vec3 result = new Vec3();
			bool visible = camera.Unproject(new Vec3(x, camera.GetViewSurfaceZ() - y, depth), result);
			position = result;
			return visible;
		}

		/// <summary>
		/// Converts a screen position to a direction in world-space.
		/// </summary>
		/// <param name="x">Horizontal position on the screen.</param>
		/// <param name="y">Vertical position on the screen.</param>
		/// <param name="direction">Direction into world-space.</param>
		/// <returns><c>true</c> if the screenpoint could be converted, <c>false</c> otherwise.</returns>
		public static bool ScreenPointToDirection(int x, int y, out Vector3 direction)
		{
			var camera = Engine.System.GetViewCamera();
			Vec3 result = new Vec3();
			bool visible = camera.Unproject(new Vec3(x, camera.GetViewSurfaceZ() - y, 1), result);
			var position = result;
			direction = (position - Position).Normalized;
			return visible;
		}

		/// <summary>
		/// Converts a point in world-space to the camera's screen-space.
		/// </summary>
		/// <returns><c>true</c>, if the point is visible, <c>false</c> otherwise.</returns>
		/// <param name="position">Position of the point in world-space.</param>
		/// <param name="screenPosition">Position of the point in the camera's screen-space.</param>
		public static bool WorldPointToScreenPoint(Vector3 position, out Vector3 screenPosition)
		{
			var camera = Engine.System.GetViewCamera();
			Vec3 result = new Vec3();
			var visible = camera.Project(position, result);
			screenPosition = result;
			return visible;
		}

		/// <summary>
		/// Converts a point in world-space to the camera's viewport-space.
		/// </summary>
		/// <returns><c>true</c>, if the point is visible, <c>false</c> otherwise.</returns>
		/// <param name="position">Position of the point in world-space.</param>
		/// <param name="viewportPosition">Position of the point in the camera's viewport-space.</param>
		public static bool WorldPointToViewportPoint(Vector3 position, out Vector3 viewportPosition)
		{
			var camera = Engine.System.GetViewCamera();
			Vec3 result = new Vec3();
			var visible = camera.Project(position, result);
			viewportPosition = result;
			viewportPosition.x /= camera.GetViewSurfaceX();
			viewportPosition.y /= camera.GetViewSurfaceZ();
			return visible;
		}

		/// <summary>
		/// Transforms a direction from world space to local space of the camera.
		/// </summary>
		/// <param name="direction"></param>
		/// <returns></returns>
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Vector3 TransformDirection(Vector3 direction)
		{
			return Rotation * direction;
		}
	}
}
