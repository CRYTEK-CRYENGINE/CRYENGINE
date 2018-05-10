// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using CryEngine.Resources;

namespace CryEngine.UI.Components
{
	/// <summary>
	/// Input controller for a ComboBox.
	/// </summary>
	public class ComboBoxCtrl : UIComponent
	{
		/// <summary>
		/// Raised if the ComboBox focus was entered.
		/// </summary>
		public event Action OnFocusEnter;

		/// <summary>
		/// Raised if the ComboBox focus was left.
		/// </summary>
		public event Action OnFocusLost;

		/// <summary>
		/// Raised if the selected item changed.
		/// </summary>
		public event Action<object> SelectedItemChanged;

		private int _idx = -1;
		private Text _text;
		private object _selectedItem;
		private UIElement _choiceFrame;
		private Panel _choice;
		private readonly List<Text> _choiceLabels = new List<Text>();
		private List<object> _items = new List<object>();
		private SceneObject _choiceRoot;

		/// <summary>
		/// List of items which are shown by the ComboBox.
		/// </summary>
		/// <value>The items.</value>
		public List<object> Items
		{
			get
			{
				return _items;
			}
			set
			{
				_items = value;
				SelectedItem = null;
				ConstructChoiceList();
			}
		}

		/// <summary>
		/// Currently selected item.
		/// </summary>
		/// <value>The selected item.</value>
		public object SelectedItem
		{
			get
			{
				return _selectedItem;
			}
			set
			{
				_selectedItem = value == null ? null : Items.FirstOrDefault(x => x.Equals(value));
				_idx = _selectedItem == null ? -1 : Items.IndexOf(_selectedItem);
				_text.Content = _idx < 0 ? "" : _selectedItem.ToString();

				SelectedItemChanged?.Invoke(_selectedItem);
			}
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnAwake()
		{
			var comboBox = Owner as ComboBox;
			if(comboBox != null)
			{
				_text = comboBox.BgPanel.AddComponent<Text>();
				_text.Offset = new Point(5, 1);
			}

			_choiceFrame = SceneObject.Instantiate<UIElement>(Owner);
			_choiceFrame.RectTransform.Alignment = Alignment.TopHStretch;

			_choiceRoot = SceneObject.Instantiate(null, "ChoiceRoot");
			var canvas = SceneObject.Instantiate<Canvas>(_choiceRoot);

			var pc = Owner.GetParentWithType<Canvas>();
			if(pc != null)
			{	
				canvas.SetupTargetEntity(pc.TargetEntity, pc.TargetTexture);
			}

			_choice = SceneObject.Instantiate<Panel>(canvas);
			_choice.RectTransform.Alignment = Alignment.TopLeft;
			_choice.Background.Source = ResourceManager.ImageFromFile(Path.Combine(UIElement.DataDirectory, "button.png"));
			_choice.Background.IgnoreClamping = true;
			_choice.Background.SliceType = SliceType.Nine;
			_choice.RectTransform.ClampMode = ClampMode.Self;
			_choice.RectTransform.Spacing = new Rect(0, 0, 6, 0);
			_choice.Background.Color = Color.Gray;
			_choice.Active = false;
		}

		/// <summary>
		/// Called by framework. Do not call directly.
		/// </summary>
		protected override void OnDestroy()
		{
			_choiceRoot.Destroy();
		}

		/// <summary>
		/// Called by Canvas. Do not call directly.
		/// </summary>
		protected override void OnEnterFocus()
		{
			
			OnFocusEnter?.Invoke();
		}

		/// <summary>
		/// Called by Canvas. Do not call directly.
		/// </summary>
		protected override void OnLeaveFocus()
		{
			
			OnFocusLost?.Invoke();
			_choice.Active = false;
		}

		/// <summary>
		/// Called by Canvas. Do not call directly.
		/// </summary>
		protected override void OnLeftMouseDown(int x, int y)
		{
			if(Items.Count > 0)
			{
				var bounds = _choiceFrame.RectTransform.Bounds;
				_choice.RectTransform.Padding = new Padding(bounds.x + bounds.w / 2, bounds.y + bounds.h / 2);
				_choice.RectTransform.Size = new Point(bounds.w, bounds.h);
				_choice.Active = !_choice.Active;

				if(!_choice.Active)
				{
					OnFocusLost?.Invoke();
				}
				else
				{
					OnFocusEnter?.Invoke();
				}
			}
		}

		/// <summary>
		/// Called by Canvas. Do not call directly.
		/// </summary>
		protected override void OnKey(InputEvent e)
		{
			if(Items.Count > 0)
			{
				if(e.KeyPressed(KeyId.Down))
				{
					_idx = Math.Min(Items.Count - 1, _idx + 1);
				}
				if(e.KeyPressed(KeyId.Up))
				{
					_idx = Math.Max(0, _idx - 1);
				}
			}
			else
			{
				_idx = -1;
			}

			if(e.KeyName.Length == 1)
			{
				var key = e.KeyName.ToUpper();
				var chosen = Items.FirstOrDefault(x => x.ToString().ToUpper().StartsWith(key, StringComparison.InvariantCultureIgnoreCase));
				if(chosen != null)
				{
					_idx = Items.IndexOf(chosen);
				}
			}
			SelectedItem = _idx < 0 ? null : Items[_idx];
		}

		void ConstructChoiceList()
		{
			int yOfs = 5;
			foreach(var str in _items)
			{
				var l = _choice.AddComponent<Text>();
				l.Content = str.ToString();
				l.Alignment = Alignment.TopLeft;
				l.Offset = new Point(5, yOfs);
				l.Height = 12;
				yOfs += 13;
				_choiceLabels.Add(l);
			}

			var cfRt = _choiceFrame.RectTransform;
			cfRt.Padding = new Padding(1, 21 + yOfs / 2, 3, 0);
			cfRt.Size = new Point(0, yOfs + 6);
		}
	}
}
