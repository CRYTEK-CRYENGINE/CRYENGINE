// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using CryEngine.Common;
using CryEngine.Rendering;
using CryEngine.Resources;
using CryEngine.UI.Components;

namespace CryEngine.UI
{
	/// <summary>
	/// A Canvas element functions as the root element for an UI entity. There is no restriction to the amount of UI entities.
	/// The Canvas Element processes input and delegates it to the element in focus. Drawing of the UI to screen or render textures is handled by the Canvas.
	/// </summary>
	public class Canvas : UIElement
	{
		UIComponent _cmpUnderMouse;
		UIComponent _currentFocus;
		DateTime _mouseMovePickTime = DateTime.MinValue;
		Graphic _targetTexture;
		Entity _targetEntity;
		bool _targetInFocus;
		IMaterial _targetMaterial;
		IMaterial _originalMaterial;

		/// <summary>
		/// True if the entity which is currently in focus is targeted by the mouse cursor.
		/// </summary>
		/// <value><c>true</c> if target in focus; otherwise, <c>false</c>.</value>
		public bool TargetInFocus
		{
			get
			{
				return _targetInFocus;
			}
			private set
			{
				if(_targetInFocus != value)
				{
					if(!value)
					{
						OnWindowLeave(-1, -1);
					}
				}
				_targetInFocus = value;
			}
		}

		/// <summary>
		/// Last UIComponent in hierarchy which is hit by the current cursor position. 
		/// </summary>
		/// <value>The component under mouse.</value>
		public UIComponent ComponentUnderMouse { get { return _cmpUnderMouse; } }

		/// <summary>
		/// UIComponent which is currently in focus.
		/// </summary>
		/// <value>The current focus.</value>
		public UIComponent CurrentFocus
		{
			get
			{
				return _currentFocus;
			}
			set
			{
				if(_currentFocus == value)
				{
					return;
				}

				_currentFocus?.InvokeOnLeaveFocus();
				_currentFocus = value;
				_currentFocus?.InvokeOnEnterFocus();
			}
		}

		/// <summary>
		/// Texture for all child elements to be drawn to. elements are drawn to screen otherwise.
		/// </summary>
		/// <value>The target texture.</value>
		public Graphic TargetTexture
		{
			get
			{
				return _targetTexture;
			}
			private set
			{
				_targetTexture = value;
				if(value != null)
				{
					RectTransform.Width = _targetTexture.Width;
					RectTransform.Height = _targetTexture.Height;
				}
			}
		}

		/// <summary>
		/// Entity which is supposed to provide UV info for cursor interaction on the Canvas' UI. Must only be used in conjunction with TargetTexture.
		/// </summary>
		/// <value>The target entity.</value>
		public Entity TargetEntity
		{
			get
			{
				return _targetEntity;
			}
			private set
			{
				_targetEntity = value;
			}
		}

		/// <summary>
		/// Initializes a new instance of the <see cref="Canvas"/> class.
		/// </summary>
		public Canvas()
		{
			RectTransform.Width = Renderer.ScreenWidth;
			RectTransform.Height = Renderer.ScreenHeight;

			Mouse.OnLeftButtonDown += OnLeftMouseDown;
			Mouse.OnLeftButtonUp += OnLeftMouseUp;
			Mouse.OnWindowLeave += OnWindowLeave;
			Mouse.OnMove += OnMouseMove;

			Renderer.ResolutionChanged += ResolutionChanged;
			Input.OnKey += OnKey;

			SceneManager.RegisterCanvas(this);
		}

		/// <summary>
		/// Used by a Texture to draw itself on TargetTexture. Do not use directly.
		/// </summary>
		internal void PushTexturePart(Texture texture, float x, float y, float w, float h, float u0, float v0, float u1, float v1, float a, Color c)
		{
			if(TargetTexture == null)
			{
				var id = texture.ID;
				v0 = 1.0f - v0;
				v1 = 1.0f - v1;
				IRenderAuxImage.Draw2dImage(x, y, w, h, id, u0, v0, u1, v1, a, c);
			}
			else
			{
				var width = texture.Width;
				var height = texture.Height;
				int regionX = (int)Math.Round(u0 * width);
				int regionY = (int)Math.Round(v0 * height);
				int regionW = (int)Math.Round((u1 - u0) * width);
				int regionH = (int)Math.Round((v1 - v0) * height);
				var srcRegion = new Region(regionX, regionY, regionW, regionH);

				regionX = (int)Math.Round(x);
				regionY = (int)Math.Round(y);
				regionW = (int)Math.Round(w);
				regionH = (int)Math.Round(h);
				var dstRegion = new Region(regionX, regionY, regionW, regionH);

				Texture.BlendTextureRegion(texture, srcRegion, TargetTexture, dstRegion, c);
			}
		}

		/// <summary>
		/// Adapts the UI Resolution, in case the Canvas target is not a Texture.
		/// </summary>
		/// <param name="w">The new width.</param>
		/// <param name="h">The new height.</param>
		private void ResolutionChanged(int w, int h)
		{
			if(TargetTexture == null)
			{
				RectTransform.Width = w;
				RectTransform.Height = h;
			}
		}

		/// <summary>
		/// Set this Canvas up to draw the UI onto another Entity.
		/// </summary>
		/// <returns><c>true</c>, if target entity was setup correctly, <c>false</c> otherwise.</returns>
		/// <param name="target">Target.</param>
		/// <param name="targetTexture">Target texture.</param>
		internal bool SetupTargetEntity(Entity target, Graphic targetTexture)
		{
			if(target == null || targetTexture == null)
			{
				return false;
			}

			if(TargetTexture != null)
			{
				TargetTexture.Destroy();
			}

			TargetTexture = targetTexture;

			return SetupRenderMaterial(target);
		}

		/// <summary>
		/// Set this Canvas up to draw the UI onto another Entity.
		/// </summary>
		/// <returns><c>true</c>, if target entity was setup correctly, <c>false</c> otherwise.</returns>
		/// <param name="target">Target Entity.</param>
		/// <param name="resolution">Resolution of UI texture in 3D space.</param>
		public bool SetupTargetEntity(Entity target, int resolution = 768)
		{
			if(target == null)
			{
				return false;
			}

			byte[] data = new byte[resolution * resolution * 4];

			if(TargetTexture != null)
			{
				TargetTexture.UpdateData(resolution, resolution, data);
				RectTransform.Width = _targetTexture.Width;
				RectTransform.Height = _targetTexture.Height;
			}
			else
			{
				TargetTexture = new Graphic(resolution, resolution, data, false, true, true, Name + "_RenderTexture");
			}

			return SetupRenderMaterial(target);
		}

		private bool SetupRenderMaterial(Entity target)
		{
			// If this Canvas was already rendering to an Entity previously, 
			// remove the material from that entity and apply the original material to it again.
			if(_targetMaterial != null)
			{
				if(TargetEntity != null && _originalMaterial != null)
				{
					TargetEntity.Material = _originalMaterial;
				}
				
				_targetMaterial.Release();
				_targetMaterial.Dispose();
			}

			// In case the material is used on multiple entities it is duplicated here.
			// This prevents conflicts with entities with the same materials and/or textures.
			_originalMaterial = target.Material;
			var material = CopyMaterial(_originalMaterial);
			_targetMaterial = material;

			material.SetTexture(TargetTexture.ID);
			target.Material = material;

			TargetEntity = target;
			return true;
		}

		private IMaterial CopyMaterial(IMaterial source)
		{
			var materialManager = Global.gEnv.p3DEngine.GetMaterialManager();

			//Assign a material first, and use clone to get a new instance. Also set a new name to prevent problems in the lookup.
			var target = materialManager.CloneMaterial(source);
			target.SetName(string.Format("{0}_copy", source.GetName()));

			//Copy the values over to the destination material. This prevents the shader-resources error.
			materialManager.CopyMaterial(source, target, EMaterialCopyFlags.MTL_COPY_DEFAULT);

			target.AddRef();

			return target;
		}

		void TryAdaptMouseInput(ref int x, ref int y)
		{
			if(TargetEntity != null)
			{
				if(TargetEntity.Id == Mouse.HitEntityId)
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
			if(TargetEntity == null)
			{
				return e.IsCursorInside;
			}

			int u = (int)Mouse.CursorPosition.x, v = (int)Mouse.CursorPosition.y;
			TryAdaptMouseInput(ref u, ref v);
			return e.RectTransform.Bounds.Contains(u, v);
		}

		void OnLeftMouseDown(int x, int y)
		{
			TryAdaptMouseInput(ref x, ref y);
			CurrentFocus = HitTestFocusable(x, y);
			if(CurrentFocus != null)
			{
				CurrentFocus.InvokeOnLeftMouseDown(x, y);
			}
		}

		void OnWindowLeave(int x, int y)
		{
			if(_cmpUnderMouse != null)
			{
				_cmpUnderMouse.InvokeOnMouseLeave(x, y);
			}
			_cmpUnderMouse = null;
		}

		void OnLeftMouseUp(int x, int y)
		{
			TryAdaptMouseInput(ref x, ref y);
			var cmpUnderMouse = HitTestFocusable(x, y);
			if(CurrentFocus != null)
			{
				CurrentFocus.InvokeOnLeftMouseUp(x, y, cmpUnderMouse == CurrentFocus);
			}
		}

		void OnMouseMove(int x, int y)
		{
			TryAdaptMouseInput(ref x, ref y);
			if(CurrentFocus != null)
			{
				CurrentFocus.InvokeOnMouseMove(x, y);
			}

			if(_mouseMovePickTime > DateTime.Now)
			{
				return;
			}
			_mouseMovePickTime = DateTime.Now.AddSeconds(0.2f);

			var lastUnder = _cmpUnderMouse;
			_cmpUnderMouse = HitTestFocusable(x, y);
			if(lastUnder != _cmpUnderMouse)
			{
				if(lastUnder != null)
				{
					lastUnder.InvokeOnMouseLeave(x, y);
				}

				if(_cmpUnderMouse != null)
				{
					_cmpUnderMouse.InvokeOnMouseEnter(x, y);
				}
			}
		}

		UIComponent HitTestFocusable(int x, int y)
		{
			UIComponent result = null;
			ForEachComponentReverse(c =>
			{
				if(c.IsFocusable && c.Enabled && c.HitTest(x, y))
				{
					result = c;
				}
				return result != null;
			});
			return result;
		}

		void FocusNextComponent()
		{
			UIComponent result = null;
			bool currentFound = false;
			ForEachComponent(c =>
			{
				if(!c.IsFocusable || !c.Enabled)
				{
					return false;
				}

				if(result == null)
				{
					result = c;
				}

				if(CurrentFocus == null || currentFound)
				{
					result = c;
					return true;
				}

				if(CurrentFocus == c)
				{
					currentFound = true;
				}

				return false;
			});
			CurrentFocus = result;
		}

		void FocusPreviousComponent()
		{
			UIComponent result = null;
			UIComponent prevComponent = null;
			ForEachComponent(c =>
			{
				if(!c.IsFocusable || !c.Enabled)
				{
					return false;
				}
				result = c;
				if(CurrentFocus == null)
				{
					return true;
				}
				if(CurrentFocus == c && prevComponent != null)
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
		public void OnKey(InputEvent e)
		{
			if(!Active)
			{
				return;
			}

			if((e.KeyPressed(KeyId.Tab) && Input.ShiftDown) || e.KeyPressed(KeyId.XI_DPadUp) || e.KeyPressed(KeyId.XI_DPadLeft))
			{
				FocusPreviousComponent();
			}
			else if(e.KeyPressed(KeyId.Tab) || e.KeyPressed(KeyId.XI_DPadDown) || e.KeyPressed(KeyId.XI_DPadRight))
			{
				FocusNextComponent();
			}
			else if(CurrentFocus != null)
			{
				CurrentFocus.InvokeOnKey(e);
			}
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnDestroy()
		{
			SceneManager.UnregisterCanvas(this);
			Mouse.OnLeftButtonDown -= OnLeftMouseDown;
			Mouse.OnLeftButtonUp -= OnLeftMouseUp;
			Mouse.OnWindowLeave -= OnWindowLeave;
			Mouse.OnMove -= OnMouseMove;
			Input.OnKey -= OnKey;

			if(_targetMaterial != null)
			{
				if(TargetEntity != null && TargetEntity.Exists && _originalMaterial != null)
				{
					TargetEntity.Material = _originalMaterial;
				}

				_targetMaterial.Release();
				_targetMaterial.Dispose();
			}

			if(TargetTexture != null)
			{
				TargetTexture.Destroy();
			}
		}

		internal void ClearRenderTarget()
		{
			// The equals operator is overloaded, so do a normal if check instead of using ?. operator.
			if(TargetTexture != null)
			{
				TargetTexture.Clear(new Color());
			}
		}
	}
}
