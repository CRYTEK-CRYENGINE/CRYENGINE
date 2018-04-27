using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using CryEngine.Common;
using CryEngine.EntitySystem;
using CryEngine.Rendering;

namespace CryEngine
{
	/// <summary>
	/// Central point for mouse access. Listens to CryEngine sided mouse callbacks and generates events from them.
	/// </summary>
	public class Mouse : IHardwareMouseEventListener, IGameUpdateReceiver
	{
		public interface IMouseOverride
		{
			event MouseEventHandler LeftButtonDown;
			event MouseEventHandler LeftButtonUp;
			event MouseEventHandler RightButtonDown;
			event MouseEventHandler RightButtonUp;
			event MouseEventHandler Move;
		}

		public delegate void MouseEventHandler(int x, int y); ///< Used by all Mouse events.
		public static event MouseEventHandler OnLeftButtonDown;
		public static event MouseEventHandler OnLeftButtonUp;
		public static event MouseEventHandler OnRightButtonDown;
		public static event MouseEventHandler OnRightButtonUp;
		public static event MouseEventHandler OnMove;
		public static event MouseEventHandler OnWindowLeave;
		public static event MouseEventHandler OnWindowEnter;

		internal static Mouse Instance { get; set; }

		private static IMouseOverride s_override = null;
		private static float _lmx = 0;
		private static float _lmy = 0;
		private static bool _updateLeftDown = false;
		private static bool _updateLeftUp = false;
		private static bool _updateRightDown = false;
		private static bool _updateRightUp = false;
		private static uint _hitEntityId = 0;
		private static Vector2 _hitEntityUV = new Vector2();
		private static bool _cursorVisible = false;

		public static Point CursorPosition { get { return new Point(_lmx, _lmy); } } ///< Current Mouse Cursor Position, refreshed before update loop.
		public static bool LeftDown { get; private set; } ///< Indicates whether left mouse button is Down during one update phase.
		public static bool LeftUp { get; private set; } ///< Indicates whether left mouse button is Released during one update phase.
		public static bool RightDown { get; private set; } ///< Indicates whether right mouse button is Down during one update phase.
		public static bool RightUp { get; private set; } ///< Indicates whether right mouse button is Released during one update phase.
		public static uint HitEntityId ///< ID of IEntity under cursor position.
		{
			get { return _hitEntityId; }
			set { _hitEntityId = value; }
		}
		public static Vector2 HitEntityUV ///< UV of IEntity under cursor position.
		{
			get { return _hitEntityUV; }
			set { _hitEntityUV = value; }
		}
		public static Entity HitEntity
		{
			get
			{
				return Entity.Get(HitEntityId);
			}
		}

		public static void ShowCursor()
		{
			if (!_cursorVisible)
				Global.gEnv.pHardwareMouse.IncrementCounter();
			_cursorVisible = true;
		}

		public static void HideCursor()
		{
			if (_cursorVisible)
				Global.gEnv.pHardwareMouse.DecrementCounter();
			_cursorVisible = false;
		}

		/// <summary>
		/// Called by CryEngine. Do not call directly.
		/// </summary>
		/// <param name="x">The x coordinate.</param>
		/// <param name="y">The y coordinate.</param>
		/// <param name="eHardwareMouseEvent">Event struct.</param>
		/// <param name="wheelDelta">Wheel delta.</param>
		public override void OnHardwareMouseEvent(int x, int y, EHARDWAREMOUSEEVENT eHardwareMouseEvent, int wheelDelta)
		{
			switch (eHardwareMouseEvent)
			{
				case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_LBUTTONDOWN:
					{
						_updateLeftDown = true;
						HitScenes(x, y);
						if (OnLeftButtonDown != null)
							OnLeftButtonDown(x, y);
						break;
					}
				case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_LBUTTONUP:
					{
						_updateLeftUp = true;
						HitScenes(x, y);
						if (OnLeftButtonUp != null)
							OnLeftButtonUp(x, y);
						break;
					}
				case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_RBUTTONDOWN:
					{
						_updateRightDown = true;
						HitScenes(x, y);
						if (OnRightButtonDown != null)
							OnRightButtonDown(x, y);
						break;
					}
				case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_RBUTTONUP:
					{
						_updateRightUp = true;
						HitScenes(x, y);
						if (OnRightButtonUp != null)
							OnRightButtonUp(x, y);
						break;
					}
			}
		}

		private static void OnOverrideLeftButtonDown(int x, int y)
		{
			_updateLeftDown = true;
			if (OnLeftButtonDown != null)
				OnLeftButtonDown(x, y);
		}

		private static void OnOverrideLeftButtonUp(int x, int y)
		{
			_updateLeftUp = true;
			if (OnLeftButtonUp != null)
				OnLeftButtonUp(x, y);
		}

		private static void OnOverrideRightButtonDown(int x, int y)
		{
			_updateRightDown = true;
			if (OnRightButtonDown != null)
				OnRightButtonDown(x, y);
		}

		private static void OnOverrideRightButtonUp(int x, int y)
		{
			_updateRightUp = true;
			if (OnRightButtonUp != null)
				OnRightButtonUp(x, y);
		}

		private static void OnOverrideMove(int x, int y)
		{
			if (OnMove != null)
				OnMove(x, y);
		}

		public static void SetOverride(IMouseOverride mouseOverride)
		{
			if (s_override != null)
			{
				s_override.LeftButtonDown -= OnOverrideLeftButtonDown;
				s_override.LeftButtonUp -= OnOverrideLeftButtonUp;
				s_override.RightButtonDown -= OnOverrideRightButtonDown;
				s_override.RightButtonUp -= OnOverrideRightButtonUp;
				s_override.Move -= OnOverrideMove;
			}
			s_override = mouseOverride;
			if (s_override != null)
			{
				s_override.LeftButtonDown += OnOverrideLeftButtonDown;
				s_override.LeftButtonUp += OnOverrideLeftButtonUp;
				s_override.RightButtonDown += OnOverrideRightButtonDown;
				s_override.RightButtonUp += OnOverrideRightButtonUp;
				s_override.Move += OnOverrideMove;
			}

		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public void OnUpdate()
		{
			LeftDown = _updateLeftDown;
			LeftUp = _updateLeftUp;
			RightDown = _updateRightDown;
			RightUp = _updateRightUp;

			_updateLeftDown = false;
			_updateLeftUp = false;
			_updateRightDown = false;
			_updateRightUp = false;

			float x = 0, y = 0;
			Global.gEnv.pHardwareMouse.GetHardwareMouseClientPosition(ref x, ref y);

			var w = Renderer.ScreenWidth;
			var h = Renderer.ScreenHeight;
			bool wasInside = _lmx >= 0 && _lmy >= 0 && _lmx < w && _lmy < h;
			bool isInside = x >= 0 && y >= 0 && x < w && y < h;
			_lmx = x; _lmy = y;

			HitScenes((int)x, (int)y);
			if (wasInside && isInside)
			{
				if (OnMove != null)
					OnMove((int)x, (int)y);
			}
			else if (wasInside != isInside && isInside)
			{
				if (OnWindowEnter != null)
					OnWindowEnter((int)x, (int)y);
			}
			else if (wasInside != isInside && !isInside)
			{
				if (OnWindowLeave != null)
					OnWindowLeave((int)x, (int)y);
			}
		}

		public static void HitScenes(int x, int y)
		{
            if (!Global.gEnv.pGameFramework.GetILevelSystem().IsLevelLoaded())
                return;

			HitEntityId = 0;
			float u = 0, v = 0;
			var mouseDir = Camera.Unproject(x, y);
			HitEntityId = (uint)Global.gEnv.pRenderer.RayToUV(Camera.Position, mouseDir, ref u, ref v);
			_hitEntityUV.x = u;
			_hitEntityUV.y = v;
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		internal Mouse()
		{
			AddListener();

			Engine.EndReload += AddListener;

			HitEntityId = 0;
			HitEntityUV = new Vector2();
		}

		void AddListener()
		{
			GameFramework.RegisterForUpdate(this);
			Global.gEnv.pHardwareMouse.AddListener(this);
		}

		public override void Dispose()
		{
			GameFramework.UnregisterFromUpdate(this);
			Global.gEnv.pHardwareMouse.RemoveListener(this);

			base.Dispose();
		}
	}
}
