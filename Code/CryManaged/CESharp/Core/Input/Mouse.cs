using CryEngine.Common;
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

		/// <summary>
		/// Used by all Mouse events.
		/// </summary>
		public delegate void MouseEventHandler(int x, int y);
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

		/// <summary>
		/// Current Mouse Cursor Position, refreshed before update loop.
		/// </summary>
		public static Point CursorPosition { get { return new Point(_lmx, _lmy); } }

		/// <summary>
		/// Indicates whether left mouse button is Down during one update phase.
		/// </summary>
		public static bool LeftDown { get; private set; }

		/// <summary>
		/// Indicates whether left mouse button is Released during one update phase.
		/// </summary>
		public static bool LeftUp { get; private set; }

		/// <summary>
		/// Indicates whether right mouse button is Down during one update phase.
		/// </summary>
		public static bool RightDown { get; private set; }

		/// <summary>
		/// Indicates whether right mouse button is Released during one update phase.
		/// </summary>
		public static bool RightUp { get; private set; }

		/// <summary>
		/// ID of the Entity currently under the cursor position.
		/// </summary>
		public static uint HitEntityId
		{
			get { return _hitEntityId; }
			set { _hitEntityId = value; }
		}

		/// <summary>
		/// UV-coordinates where the mouse-cursor is hitting an Entity.
		/// </summary>
		public static Vector2 HitEntityUV
		{
			get { return _hitEntityUV; }
			set { _hitEntityUV = value; }
		}

		/// <summary>
		/// The Entity currently under the cursor position
		/// </summary>
		public static Entity HitEntity
		{
			get
			{
				return Entity.Get(HitEntityId);
			}
		}

		/// <summary>
		/// Show the mouse-cursor
		/// </summary>
		public static void ShowCursor()
		{
			if(!_cursorVisible)
			{
				Global.gEnv.pHardwareMouse.IncrementCounter();
			}
			_cursorVisible = true;
		}

		/// <summary>
		/// Hide the mouse-cursor
		/// </summary>
		public static void HideCursor()
		{
			if(_cursorVisible)
			{
				Global.gEnv.pHardwareMouse.DecrementCounter();
			}
			_cursorVisible = false;
		}

		/// <summary>
		/// Called by CryEngine. Do not call directly.
		/// </summary>
		/// <param name="iX">The x coordinate.</param>
		/// <param name="iY">The y coordinate.</param>
		/// <param name="eHardwareMouseEvent">Event struct.</param>
		/// <param name="wheelDelta">Wheel delta.</param>
		public override void OnHardwareMouseEvent(int iX, int iY, EHARDWAREMOUSEEVENT eHardwareMouseEvent, int wheelDelta)
		{
			switch (eHardwareMouseEvent)
			{
				case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_LBUTTONDOWN:
					{
						_updateLeftDown = true;
						HitScenes(iX, iY);
						if (OnLeftButtonDown != null)
							OnLeftButtonDown(iX, iY);
						break;
					}
				case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_LBUTTONUP:
					{
						_updateLeftUp = true;
						HitScenes(iX, iY);
						if (OnLeftButtonUp != null)
							OnLeftButtonUp(iX, iY);
						break;
					}
				case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_RBUTTONDOWN:
					{
						_updateRightDown = true;
						HitScenes(iX, iY);
						if (OnRightButtonDown != null)
							OnRightButtonDown(iX, iY);
						break;
					}
				case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_RBUTTONUP:
					{
						_updateRightUp = true;
						HitScenes(iX, iY);
						if (OnRightButtonUp != null)
							OnRightButtonUp(iX, iY);
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
			if(!Global.gEnv.pGameFramework.GetILevelSystem().IsLevelLoaded())
			{
				return;
			}

			HitEntityId = 0;
			var mouseDir = Camera.Unproject(x, y);
			RaycastHit hit;
			if(Physics.Raycast(Camera.Position, mouseDir, 100, out hit))
			{
				HitEntityId = hit.EntityId;
				_hitEntityUV = hit.UvPoint;
			}
			else
			{
				_hitEntityUV.x = 0;
				_hitEntityUV.y = 0;
			}
			
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
