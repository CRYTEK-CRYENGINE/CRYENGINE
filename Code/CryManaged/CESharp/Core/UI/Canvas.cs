// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.Serialization;
using CryEngine.UI.Components;
using CryEngine.Common;
using CryEngine.Resources;
using CryEngine.Components;
using CryEngine.EntitySystem;

namespace CryEngine.UI
{
	/// <summary>
	/// A Canvas element functions as the root element for an UI entity. There is no restriction to the amount of UI entities.
	/// The Canvas Element processes input and delegates it to the element in focus. Drawing of the UI to screen or render textures is handled by the Canvas.
	/// </summary>
	[DataContract]
	public class Canvas : UIElement
	{
		Component _cmpUnderMouse;
		Component _currentFocus;
		DateTime _mouseMovePickTime = DateTime.MinValue;
		Texture _targetTexture;
		Entity _targetEntity;
		bool _targetInFocus;

		public bool TargetInFocus
		{
			private set 
			{
				if (_targetInFocus != value) 
				{
					if (!value)
						OnWindowLeave (-1, -1);
				}
				_targetInFocus = value;
			}
			get 
			{
				return _targetInFocus;
			}
		} ///< True if the entity which is currently in focus is targetet by the mouse cursor.

		public Component ComponentUnderMouse { get { return _cmpUnderMouse; } } ///< Last Component in hierarchy which is hit by the current cursor position. 

		public Component CurrentFocus 
		{
			set 
			{
				if (_currentFocus == value)
					return;
				if (_currentFocus != null)
					_currentFocus.InvokeOnLeaveFocus ();
				_currentFocus = value;
				if (_currentFocus != null)
					_currentFocus.InvokeOnEnterFocus ();
			}

			get
			{
				return _currentFocus;
			}
		} ///< Component which is currently in focus.

		public Texture TargetTexture 
		{ 
			get 
			{
				return _targetTexture;
			}

			set 
			{
				_targetTexture = value;
				if (value != null)
				{
					RectTransform.Width = _targetTexture.Width;
					RectTransform.Height = _targetTexture.Height;
				}
			}
		} ///< Texture for all child elements to be drawn to. elements are drawn to screen otherwise.

		public Entity TargetEntity
		{
			set 
			{
				_targetEntity = value;
			}
			get 
			{
				return _targetEntity;
			}
		} ///< Entity which is supposed to provide UV info for cursor interacation on the Canvas' UI. Must only be used in conjunction with TargetTexture.

		/// <summary>
		/// Initializes a new instance of the <see cref="CryEngine.UI.Canvas"/> class.
		/// </summary>
		public Canvas()
		{
			RectTransform.Width = Screen.Width;
			RectTransform.Height = Screen.Height;

			Mouse.OnLeftButtonDown += OnLeftMouseDown;
			Mouse.OnLeftButtonUp += OnLeftMouseUp;
			Mouse.OnWindowLeave += OnWindowLeave;
			Mouse.OnMove += OnMouseMove;

			Screen.ResolutionChanged += ResolutionChanged;
			Input.OnKey += OnKey;
		}

		/// <summary>
		/// Used by a Texture to draw itself on TargetTexture. Do not use directly.
		/// </summary>
		public void PushTexturePart(float x, float y, float w, float h, int id, float u0, float v0, float u1, float v1, float a, Color c)
		{
			if (TargetTexture == null) 
			{
				var ix = 800.0f / Env.Renderer.GetWidth ();
				var iy = 600.0f / Env.Renderer.GetHeight ();
				Env.Renderer.Push2dImage (x * ix, y * iy, w * ix, h * iy, id, u0, v0, u1, v1, a, c.R, c.G, c.B, c.A);
			} 
			else 
			{
				var tw = (float)TargetTexture.Width;
				var th = (float)TargetTexture.Height;
				Env.Renderer.PushUITexture (id, TargetTexture.ID, x / tw, y / th , w / tw, h / th, u0, v0, u1, v1, c.R, c.G, c.B, c.A);
			}
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public void OnUpdate()
		{
			if (TargetTexture != null) 
			{
				/*var img2 = Resource.ImageFromFile (Application.UIPath + "orion1K.png");
				Env.Renderer.PushUITexture (img2.Texture.ID, TargetTexture.ID, 0.1f, 0.1f, 0.8f, 0.8f, 0, 0, 1, 1, 1,1,1,1);*/
			}
		}

		/// <summary>
		/// Adapts the UI Resolution, in case the Canvas target is not a Texture.
		/// </summary>
		/// <param name="w">The new Width.</param>
		/// <param name="h">The new Height.</param>
		void ResolutionChanged(int w, int h)
		{
			if (TargetTexture == null) 
			{
				RectTransform.Width = w;
				RectTransform.Height = h;
			}
		}

		/// <summary>
		/// Expects an existing entity to be placed upon in 3D.
		/// </summary>
		/// <param name="target">Target Entity.</param>
		/// <param name="resolution">Resolution of UI texture in 3D space.</param>
		public bool SetupTargetEntity(EntitySystem.Entity target, int resolution=768)
		{
			if (target == null)
				return false;
			byte[] data = new byte[resolution * resolution * 4];
			TargetTexture = new Texture(resolution, resolution, data, true, false, true);
			target.Material.SetTexture(TargetTexture.ID);
			TargetEntity = target;
			return true;
		}

		void TryAdaptMouseInput(ref int x, ref int y)
		{
			if (TargetEntity != null) 
			{
				if (TargetEntity.ID == Mouse.HitEntityId) 
				{
					x = (int)(Mouse.HitEntityUV.x * RectTransform.Width);
					y = (int)(Mouse.HitEntityUV.y * RectTransform.Height);
					TargetInFocus = true;
				} 
				else
				{
					x = y = -1;
					TargetInFocus = false;
				}
			}
		}

		/// <summary>
		/// Checks if the mouse cursor is inside a specific UIElement, taking into account 3D UI Targets.
		/// </summary>
		/// <returns><c>True</c>, if Cursor is inside element, <c>false</c> otherwise.</returns>
		/// <param name="e">UIElement to be tested against.</param>
		public bool CursorInside(UIElement e)
		{
			if (TargetEntity == null)
				return Mouse.CursorInside (e);

			int u = (int)Mouse.CursorPosition.x, v = (int)Mouse.CursorPosition.y;
			TryAdaptMouseInput (ref u, ref v);
			return e.RectTransform.Bounds.Contains(new Point(u, v));
		}

		void OnLeftMouseDown(int x, int y)
		{
			TryAdaptMouseInput (ref x, ref y);
			CurrentFocus = HitTestFocusable (x, y);
			if (CurrentFocus != null) 
				CurrentFocus.InvokeOnLeftMouseDown(x, y);
		}

		void OnWindowLeave(int x, int y)
		{
			if (_cmpUnderMouse != null)
				_cmpUnderMouse.InvokeOnMouseLeave (x, y);
			_cmpUnderMouse = null;
		}

		void OnLeftMouseUp(int x, int y)
		{
			TryAdaptMouseInput (ref x, ref y);
			var cmpUnderMouse = HitTestFocusable (x, y);
			if (CurrentFocus != null) 
				CurrentFocus.InvokeOnLeftMouseUp(x, y, cmpUnderMouse == CurrentFocus);
		}

		void OnMouseMove(int x, int y)
		{
			TryAdaptMouseInput (ref x, ref y);
			if (CurrentFocus != null)
				CurrentFocus.InvokeOnMouseMove(x, y);
			
			if (_mouseMovePickTime > DateTime.Now)
				return;
			_mouseMovePickTime = DateTime.Now.AddSeconds (0.2f);

			var lastUnder = _cmpUnderMouse;
			_cmpUnderMouse = HitTestFocusable (x, y);
			if (lastUnder != _cmpUnderMouse) 
			{
				if (lastUnder != null)
					lastUnder.InvokeOnMouseLeave (x, y);
				if (_cmpUnderMouse != null)
					_cmpUnderMouse.InvokeOnMouseEnter (x, y);
			}
		}

		Component HitTestFocusable(int x, int y)
		{
			Component result = null;
			ForEachComponentReverse(c =>
				{
					if (c.IsFocusable && c.Enabled && c.HitTest(x, y))
						result = c;
					return result != null;
				});
			return result;
		}

		void FocusNextComponent()
		{
			Component result = null;
			bool currentFound = false;
			ForEachComponent(c =>
				{
					if (!c.IsFocusable || !c.Enabled)
						return false;
					if (result == null)
						result = c;
					if (CurrentFocus == null || currentFound)
					{
						result = c;
						return true;
					}
					if (CurrentFocus == c)
						currentFound = true;
					return false;
				});
			CurrentFocus = result;
		}

		void FocusPreviousComponent()
		{
			Component result = null;
			Component prevComponent = null;
			ForEachComponent(c =>
				{
					if (!c.IsFocusable || !c.Enabled)
						return false;
					result = c;
					if (CurrentFocus == null)
						return true;
					if (CurrentFocus == c && prevComponent != null)
					{
						result = prevComponent;
						return true;
					}
					prevComponent = c;
					return false;
				});
			CurrentFocus = result;
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public void OnKey(SInputEvent e)
		{
			if (!Active)
				return;

			if ((e.KeyPressed (EKeyId.eKI_Tab) && Input.ShiftDown) || e.KeyPressed (EKeyId.eKI_XI_DPadUp) || e.KeyPressed(EKeyId.eKI_XI_DPadLeft))
				FocusPreviousComponent ();
			else if (e.KeyPressed (EKeyId.eKI_Tab) || e.KeyPressed (EKeyId.eKI_XI_DPadDown) || e.KeyPressed(EKeyId.eKI_XI_DPadRight))
				FocusNextComponent ();
			else if(CurrentFocus != null)
				CurrentFocus.InvokeOnKey (e);
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		public void OnDestroy ()
		{
			Mouse.OnLeftButtonDown -= OnLeftMouseDown;
			Mouse.OnLeftButtonUp -= OnLeftMouseUp;
			Mouse.OnWindowLeave -= OnWindowLeave;
			Mouse.OnMove -= OnMouseMove;
			Input.OnKey -= OnKey;
		}
	}
}
