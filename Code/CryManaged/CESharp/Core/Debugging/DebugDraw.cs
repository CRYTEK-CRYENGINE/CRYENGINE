// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using CryEngine.Common;

namespace CryEngine.Debugging
{
	/// <summary>
	/// Helper class for drawing debug information.
	/// </summary>
	public static class DebugDraw
	{
		/// <summary>
		/// Draw a sphere with a radius and color at a position for a set amount of time.
		/// If timeout is smaller or equal to 0, it will be drawn forever until Clear is called.
		/// </summary>
		/// <param name="position"></param>
		/// <param name="radius"></param>
		/// <param name="color"></param>
		/// <param name="timeout"></param>
		public static void Sphere(Vector3 position, float radius, Color color, float timeout)
		{
			var persistantDebug = Global.gEnv.pGameFramework.GetIPersistantDebug();
			persistantDebug.Begin("DebugDraw", false);

			persistantDebug.AddSphere(position, radius, color, timeout);
		}

		/// <summary>
		/// Draw a ray with a radius and color at a position in a direction for a set amount of time.
		/// If timeout is smaller or equal to 0, it will be drawn forever until Clear is called.
		/// </summary>
		/// <param name="position"></param>
		/// <param name="radius"></param>
		/// /// <param name="direction"></param>
		/// <param name="color"></param>
		/// <param name="timeout"></param>
		public static void Direction(Vector3 position, float radius, Vector3 direction, Color color, float timeout)
		{
			var persistantDebug = Global.gEnv.pGameFramework.GetIPersistantDebug();
			persistantDebug.Begin("DebugDraw", false);

			persistantDebug.AddDirection(position, radius, direction, color, timeout);
		}

		/// <summary>
		/// Draw a line with a color at a position up to the end position for a set amount of time.
		/// If timeout is smaller or equal to 0, it will be drawn forever until Clear is called.
		/// </summary>
		/// <param name="start"></param>
		/// <param name="end"></param>
		/// <param name="color"></param>
		/// <param name="timeout"></param>
		public static void Line(Vector3 start, Vector3 end, Color color, float timeout)
		{
			var persistantDebug = Global.gEnv.pGameFramework.GetIPersistantDebug();
			persistantDebug.Begin("DebugDraw", false);

			persistantDebug.AddLine(start, end, color, timeout);
		}

		/// <summary>
		/// Draw a box with a color for a set amount of time. The size and position can be defined by setting the min and max.
		/// If timeout is smaller or equal to 0, it will be drawn forever until Clear is called.
		/// </summary>
		/// <param name="min"></param>
		/// <param name="max"></param>
		/// <param name="color"></param>
		/// <param name="timeout"></param>
		public static void BBox(Vector3 min, Vector3 max, Color color, float timeout)
		{
			var persistantDebug = Global.gEnv.pGameFramework.GetIPersistantDebug();
			persistantDebug.Begin("DebugDraw", false);

			persistantDebug.AddAABB(min, max, color, timeout);
		}

		/// <summary>
		/// Draw a box with a color for a set amount of time. The size and position can be defined by setting it in the bbox.
		/// If timeout is smaller or equal to 0, it will be drawn forever until Clear is called.
		/// </summary>
		/// <param name="bbox"></param>
		/// <param name="color"></param>
		/// <param name="timeout"></param>
		public static void BBox(AABB bbox, Color color, float timeout)
		{
			BBox(bbox.min, bbox.max, color, timeout);
		}

		/// <summary>
		/// Draw text at a position in the world with a color and a size.
		/// If timeout is smaller or equal to 0, it will be drawn forever until Clear is called.
		/// </summary>
		/// <param name="position"></param>
		/// <param name="text"></param>
		/// <param name="size"></param>
		/// <param name="color"></param>
		/// <param name="timeout"></param>
		public static void Text(Vector3 position, string text, float size, Color color, float timeout)
		{
			var persistantDebug = Global.gEnv.pGameFramework.GetIPersistantDebug();
			persistantDebug.Begin("DebugDraw", false);

			persistantDebug.AddText3D(position, size, color, timeout, text);
		}

		/// <summary>
		/// Draw text at a position the screen with a color and a size.
		/// If timeout is smaller or equal to 0, it will be drawn forever until Clear is called.
		/// </summary>
		/// <param name="screenPosition"></param>
		/// <param name="text"></param>
		/// <param name="size"></param>
		/// <param name="color"></param>
		/// <param name="timeout"></param>
		public static void Text(Vector2 screenPosition, string text, float size, Color color, float timeout)
		{
			var persistantDebug = Global.gEnv.pGameFramework.GetIPersistantDebug();
			persistantDebug.Begin("DebugDraw", false);

			persistantDebug.AddText(screenPosition.x, screenPosition.y, size, color, timeout, text);
		}

		/// <summary>
		/// Clears all objects drawn with DebugDrawn.
		/// </summary>
		public static void Clear()
		{
			var persistantDebug = Global.gEnv.pGameFramework.GetIPersistantDebug();
			persistantDebug.Reset();
		}
	}
}
