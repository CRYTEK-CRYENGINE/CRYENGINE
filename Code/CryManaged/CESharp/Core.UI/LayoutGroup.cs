using System.Collections.Generic;
using System.Linq;

namespace CryEngine.UI
{
	/// <summary>
	/// Lays out all active UIElements together as a group.
	/// </summary>
	public abstract class LayoutGroup : Panel
	{
		private float _spacing;
		private List<UIElement> _items;

		public float Spacing
		{
			get
			{
				return _spacing;
			}
			set
			{
				_spacing = value;
				UpdateLayout();
			}
		}

		public override void OnAwake()
		{
			base.OnAwake();
			_items = new List<UIElement>();
		}

		public void Add(UIElement item)
		{
			if (!_items.Contains(item))
			{
				_items.Add(item);
				item.ActiveChanged += OnItemActiveChanged;
				UpdateLayout();
			}
		}

		public void Remove(UIElement item)
		{
			if (_items.Contains(item))
			{
				_items.Remove(item);
				item.ActiveChanged -= OnItemActiveChanged;
				UpdateLayout();
			}
		}

		public void Clear(bool updateLayout = true)
		{
			_items.ForEach(x => x.ActiveChanged -= OnItemActiveChanged);
			_items.Clear();

			if (updateLayout)
				UpdateLayout();
		}

		public override void OnDestroy()
		{
			Clear(false);
		}

		void UpdateLayout()
		{
			OnUpdateLayout(_items.Where(x => x.Active).ToList());
		}

		void OnItemActiveChanged(SceneObject arg)
		{
			UpdateLayout();
		}

		/// <summary>
		/// Called whenever an item is added or removed.
		/// </summary>
		/// <param name="items">All currently active UIElements.</param>
		protected abstract void OnUpdateLayout(List<UIElement> items);
	}
}

