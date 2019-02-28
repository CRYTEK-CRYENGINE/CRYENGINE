// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System.Collections.Generic;
using CryEngine.Common;
using CryEngine.Rendering;

namespace CryEngine
{
	/// <summary>
	/// Central point for mouse access. Listens to CryEngine sided mouse callbacks and generates events from them.
	/// </summary>
	public class Mouse
	{
		private struct MouseEvent
		{
			public int X;
			public int Y;
			public EHARDWAREMOUSEEVENT HardwareEvent;
			public int WheelDelta;

			public MouseEvent(int x, int y, EHARDWAREMOUSEEVENT hardwareEvent, int wheelDelta)
			{
				X = x;
				Y = y;
				HardwareEvent = hardwareEvent;
				WheelDelta = wheelDelta;
			}
		}

		private class DeferredMouseListener : IHardwareMouseEventListener, IGameUpdateReceiver
		{
			private Mouse _mouse;
			private readonly Queue<MouseEvent> _eventQueue = new Queue<MouseEvent>();

			public DeferredMouseListener(Mouse mouse)
			{
				_mouse = mouse;
				GameFramework.RegisterForUpdate(this);
				Global.gEnv.pHardwareMouse.AddListener(this);
			}

			/// <summary>
			/// Disposes this instance.
			/// </summary>
			public override void Dispose()
			{
				GameFramework.UnregisterFromUpdate(this);
				Global.gEnv.pHardwareMouse.RemoveListener(this);
				_mouse = null;

				base.Dispose();
			}

			public override void OnHardwareMouseEvent(int iX, int iY, EHARDWAREMOUSEEVENT eHardwareMouseEvent, int wheelDelta)
			{
				MouseEvent mouseEvent = new MouseEvent(iX, iY, eHardwareMouseEvent, wheelDelta);
				_eventQueue.Enqueue(mouseEvent);
			}

			public override void OnHardwareMouseEvent(int iX, int iY, EHARDWAREMOUSEEVENT eHardwareMouseEvent)
			{
				OnHardwareMouseEvent(iX, iY, eHardwareMouseEvent, 0);
			}

			public void OnUpdate()
			{
				var queue = new Queue<MouseEvent>(_eventQueue);
				_eventQueue.Clear();
				foreach(var mouseEvent in queue)
				{
					// If a mouse event triggered an engine unload the mouse can be null.
					if(_mouse == null)
					{
						break;
					}
					_mouse.HandleMouseEvent(mouseEvent);
				}

				_mouse?.Update();
			}
		}

		/// <summary>
		/// Interface for overriding mouse input.
		/// </summary>
		public interface IMouseOverride
		{
			/// <summary>
			/// Called when the left mouse button is pressed down.
			/// </summary>
			event MouseEventHandler LeftButtonDown;

			/// <summary>
			/// Called when the left mouse button was pressed down is now up again.
			/// </summary>
			event MouseEventHandler LeftButtonUp;

			/// <summary>
			/// Called when the right mouse button is pressed down.
			/// </summary>
			event MouseEventHandler RightButtonDown;

			/// <summary>
			/// Called when the right mouse button was pressed down is now up again.
			/// </summary>
			event MouseEventHandler RightButtonUp;

			/// <summary>
			/// Called when the mouse has moved.
			/// </summary>
			event MouseEventHandler Move;
		}

		/// <summary>
		/// Used by all Mouse events.
		/// </summary>
		public delegate void MouseEventHandler(int x, int y);
		#region Left mouse button
		/// <summary>
		/// Invoked when the left mouse button is pressed down.
		/// </summary>
		public static event MouseEventHandler OnLeftButtonDown;

		/// <summary>
		/// Invoked when the left mouse button is released after being pressed down.
		/// </summary>
		public static event MouseEventHandler OnLeftButtonUp;

		/// <summary>
		/// Invoked when the left mouse button is double clicked.
		/// </summary>
		public static event MouseEventHandler OnLeftButtonDoubleClicked;
		#endregion

		#region Right mouse button
		/// <summary>
		/// Invoked when the right mouse button is pressed down.
		/// </summary>
		public static event MouseEventHandler OnRightButtonDown;

		/// <summary>
		/// Invoked when the right mouse button is released after being pressed down.
		/// </summary>
		public static event MouseEventHandler OnRightButtonUp;

		/// <summary>
		/// Invoked when the right mouse button is double clicked.
		/// </summary>
		public static event MouseEventHandler OnRightButtonDoubleClicked;
		#endregion

		#region Middle mouse button
		/// <summary>
		/// Invoked when the middle mouse button is pressed down.
		/// </summary>
		public static event MouseEventHandler OnMiddleButtonDown;

		/// <summary>
		/// Invoked when the middle mouse button is released after being pressed down.
		/// </summary>
		public static event MouseEventHandler OnMiddleButtonUp;

		/// <summary>
		/// Invoked when the middle mouse button is double clicked.
		/// </summary>
		public static event MouseEventHandler OnMiddleButtonDoubleClicked;
		#endregion

		/// <summary>
		/// Invoked when the mouse wheel value has changed.
		/// </summary>
		public static event System.Action<int> OnMouseWheelChanged;

		/// <summary>
		/// Invoked when the mouse has moved inside the window of the engine.
		/// </summary>
		public static event MouseEventHandler OnMove;

		/// <summary>
		/// Invoked when the mouse moves outside of the window of the engine.
		/// </summary>
		public static event MouseEventHandler OnWindowLeave;

		/// <summary>
		/// Invoked when the mouse enters the window of the engine.
		/// </summary>
		public static event MouseEventHandler OnWindowEnter;

		internal static Mouse Instance { get; set; }

		private static IMouseOverride s_override = null;
		private static bool _updateLeftDown = false;
		private static bool _updateLeftUp = false;
		private static bool _updateRightDown = false;
		private static bool _updateRightUp = false;
		private static bool _updateMiddleDown = false;
		private static bool _updateMiddleUp = false;
		private static bool _cursorVisible = false;

		private DeferredMouseListener _mouseListener;

		/// <summary>
		/// Current mouse cursor position, refreshed before update loop.
		/// </summary>
		public static Vector2 CursorPosition { get; private set; }

		/// <summary>
		/// Indicates whether left mouse button is down during one update phase.
		/// </summary>
		public static bool LeftDown { get; private set; }

		/// <summary>
		/// Indicates whether left mouse button is released during one update phase.
		/// </summary>
		public static bool LeftUp { get; private set; }

		/// <summary>
		/// Indicates whether right mouse button is down during one update phase.
		/// </summary>
		public static bool RightDown { get; private set; }

		/// <summary>
		/// Indicates whether right mouse button is released during one update phase.
		/// </summary>
		public static bool RightUp { get; private set; }

		/// <summary>
		/// Indicates whether middle mouse button is down during one update phase.
		/// </summary>
		public static bool MiddleDown { get; private set; }

		/// <summary>
		/// Indicates whether middle mouse button is released during one update phase.
		/// </summary>
		public static bool MiddleUp { get; private set; }

		/// <summary>
		/// ID of the Entity currently under the cursor position.
		/// </summary>
		public static uint HitEntityId { get; set; }

		/// <summary>
		/// UV-coordinates where the mouse-cursor is hitting an Entity.
		/// </summary>
		public static Vector2 HitEntityUV { get; set; }

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

		private void HandleMouseEvent(MouseEvent mouseEvent)
		{
			int x = mouseEvent.X;
			int y = mouseEvent.Y;

			switch(mouseEvent.HardwareEvent)
			{
				case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_LBUTTONDOWN:
				{
					_updateLeftDown = true;
					HitScenes(x, y);
					OnLeftButtonDown?.Invoke(x, y);
					break;
				}

				case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_LBUTTONUP:
				{
					_updateLeftUp = true;
					HitScenes(x, y);
					OnLeftButtonUp?.Invoke(x, y);
					break;
				}

				case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_RBUTTONDOWN:
				{
					_updateRightDown = true;
					HitScenes(x, y);
					OnRightButtonDown?.Invoke(x, y);
					break;
				}

				case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_RBUTTONUP:
				{
					_updateRightUp = true;
					HitScenes(x, y);
					OnRightButtonUp?.Invoke(x, y);
					break;
				}

				case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_MOVE:
				{
					// OnMove will be called from the Update method.
					break;
				}

				case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_LBUTTONDOUBLECLICK:
				{
					OnLeftButtonDoubleClicked?.Invoke(x, y);
					break;
				}

				case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_RBUTTONDOUBLECLICK:
				{
					OnRightButtonDoubleClicked?.Invoke(x, y);
					break;
				}

				case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_MBUTTONDOWN:
				{
					_updateMiddleDown = true;
					HitScenes(x, y);
					OnMiddleButtonDown?.Invoke(x, y);
					break;
				}

				case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_MBUTTONUP:
				{
					_updateMiddleUp = true;
					HitScenes(x, y);
					OnMiddleButtonUp?.Invoke(x, y);
					break;
				}

				case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_MBUTTONDOUBLECLICK:
				{
					OnMiddleButtonDoubleClicked?.Invoke(x, y);
					break;
				}

				case EHARDWAREMOUSEEVENT.HARDWAREMOUSEEVENT_WHEEL:
				{
					OnMouseWheelChanged?.Invoke(mouseEvent.WheelDelta);	
					break;
				}
					
				default:
					throw new System.NotImplementedException();
			}
		}

		private static void OnOverrideLeftButtonDown(int x, int y)
		{
			_updateLeftDown = true;
			OnLeftButtonDown?.Invoke(x, y);
		}

		private static void OnOverrideLeftButtonUp(int x, int y)
		{
			_updateLeftUp = true;
			OnLeftButtonUp?.Invoke(x, y);
		}

		private static void OnOverrideRightButtonDown(int x, int y)
		{
			_updateRightDown = true;
			OnRightButtonDown?.Invoke(x, y);
		}

		private static void OnOverrideRightButtonUp(int x, int y)
		{
			_updateRightUp = true;
			OnRightButtonUp?.Invoke(x, y);
		}

		private static void OnOverrideMove(int x, int y)
		{
			OnMove?.Invoke(x, y);
		}

		/// <summary>
		/// Overrides the mouse input.
		/// </summary>
		/// <param name="mouseOverride"></param>
		public static void SetOverride(IMouseOverride mouseOverride)
		{
			if(s_override != null)
			{
				s_override.LeftButtonDown -= OnOverrideLeftButtonDown;
				s_override.LeftButtonUp -= OnOverrideLeftButtonUp;
				s_override.RightButtonDown -= OnOverrideRightButtonDown;
				s_override.RightButtonUp -= OnOverrideRightButtonUp;
				s_override.Move -= OnOverrideMove;
			}
			s_override = mouseOverride;
			if(s_override != null)
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
		private void Update()
		{
			LeftDown = _updateLeftDown;
			LeftUp = _updateLeftUp;
			RightDown = _updateRightDown;
			RightUp = _updateRightUp;
			MiddleDown = _updateMiddleDown;
			MiddleUp = _updateMiddleUp;

			_updateLeftDown = false;
			_updateLeftUp = false;
			_updateRightDown = false;
			_updateRightUp = false;
			_updateMiddleDown = false;
			_updateMiddleUp = false;

			float x = 0, y = 0;
			Global.gEnv.pHardwareMouse.GetHardwareMouseClientPosition(ref x, ref y);

			var w = Renderer.ScreenWidth;
			var h = Renderer.ScreenHeight;
			var pos = CursorPosition;
			bool wasInside = pos.X >= 0 && pos.Y >= 0 && pos.X < w && pos.Y < h;
			bool isInside = x >= 0 && y >= 0 && x < w && y < h;
			CursorPosition = new Vector2(x, y);

			HitScenes((int)x, (int)y);
			if(wasInside && isInside)
			{
				OnMove?.Invoke((int)x, (int)y);
			}
			else if(wasInside != isInside && isInside)
			{
				OnWindowEnter?.Invoke((int)x, (int)y);
			}
			else if(wasInside != isInside && !isInside)
			{
				OnWindowLeave?.Invoke((int)x, (int)y);
			}
		}

		/// <summary>
		/// Fires a raycast to check if UI is hit at the specified coordinates.
		/// </summary>
		/// <param name="x"></param>
		/// <param name="y"></param>
		public static void HitScenes(int x, int y)
		{
			if(!Global.gEnv.pGameFramework.GetILevelSystem().IsLevelLoaded())
			{
				return;
			}

			HitEntityId = 0;
			if(Camera.ScreenPointToDirection(x, y, out Vector3 direction)
				&& Physics.Raycast(Camera.Position, direction, 100, out RaycastHit hit))
			{
				HitEntityId = hit.EntityId;
				HitEntityUV = hit.UvPoint;
			}
			else
			{
				HitEntityUV = Vector2.Zero;
			}
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		internal Mouse()
		{
			_mouseListener = new DeferredMouseListener(this);
			Engine.StartReload += OnEngineUnload;
			Engine.EndReload += OnEngineReload;

			HitEntityId = 0;
			HitEntityUV = new Vector2();
		}

		private void OnEngineUnload()
		{
			_mouseListener?.Dispose();
			_mouseListener = null;
		}

		private void OnEngineReload()
		{
			_mouseListener = new DeferredMouseListener(this);
		}
	}
}
