using CryEngine.Common;

namespace CryEngine.Debugging
{
	public static class DebugDraw
	{
		public static void Sphere(Vector3 position, float radius, Color color, float timeout)
		{
			var persistantDebug = Global.gEnv.pGameFramework.GetIPersistantDebug();
			persistantDebug.Begin("DebugDraw", false);

			persistantDebug.AddSphere(position, radius, color, timeout);
		}

		public static void Direction(Vector3 position, float radius, Vector3 direction, Color color, float timeout)
		{
			var persistantDebug = Global.gEnv.pGameFramework.GetIPersistantDebug();
			persistantDebug.Begin("DebugDraw", false);

			persistantDebug.AddDirection(position, radius, direction, color, timeout);
		}

		public static void Line(Vector3 start, Vector3 end, Color color, float timeout)
		{
			var persistantDebug = Global.gEnv.pGameFramework.GetIPersistantDebug();
			persistantDebug.Begin("DebugDraw", false);

			persistantDebug.AddLine(start, end, color, timeout);
		}

		public static void BBox(Vector3 min, Vector3 max, Color color, float timeout)
		{
			var persistantDebug = Global.gEnv.pGameFramework.GetIPersistantDebug();
			persistantDebug.Begin("DebugDraw", false);

			persistantDebug.AddAABB(min, max, color, timeout);
		}

		public static void BBox(AABB bbox, Color color, float timeout)
		{
			BBox(bbox.min, bbox.max, color, timeout);
		}

		public static void Text(Vector3 position, string text, float size, Color color, float timeout)
		{
			var persistantDebug = Global.gEnv.pGameFramework.GetIPersistantDebug();
			persistantDebug.Begin("DebugDraw", false);

			persistantDebug.AddText3D(position, size, color, timeout, text);
		}

		public static void Text(Vector2 screenPosition, string text, float size, Color color, float timeout)
		{
			var persistantDebug = Global.gEnv.pGameFramework.GetIPersistantDebug();
			persistantDebug.Begin("DebugDraw", false);

			persistantDebug.AddText(screenPosition.x, screenPosition.y, size, color, timeout, text);
		}
	}
}
