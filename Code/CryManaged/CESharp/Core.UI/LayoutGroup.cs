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

		/// <summary>
		/// Spacing around the elements in the  LayoutGroup.
		/// </summary>
		/// <value>The spacing.</value>
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

		/// <summary>
		/// Called when the LayoutGroup is created.
		/// </summary>
		protected override void OnAwake()
		{
			base.OnAwake();
			_items = new List<UIElement>();
		}

		/// <summary>
		/// Add an UIElement to the LayoutGroup.
		/// </summary>
		/// <param name="item">The UIElement to add.</param>
		public void Add(UIElement item)
		{
			if (!_items.Contains(item))
			{
				_items.Add(item);
				item.ActiveChanged += OnItemActiveChanged;
				UpdateLayout();
			}
		}

		/// <summary>
		/// Remove an UIElement from the LayoutGroup.
		/// </summary>
		/// <param name="item">The UIElement to remove.</param>
		public void Remove(UIElement item)
		{
			if (_items.Contains(item))
			{
				_items.Remove(item);
				item.ActiveChanged -= OnItemActiveChanged;
				UpdateLayout();
			}
		}

		/// <summary>
		/// Removes all UIElements from this LayoutGroup.
		/// </summary>
		/// <returns>The clear.</returns>
		/// <param name="updateLayout">If set to <c>true</c> update layout.</param>
		public void Clear(bool updateLayout = true)
		{
			_items.ForEach(x => x.ActiveChanged -= OnItemActiveChanged);
			_items.Clear();

			if(updateLayout)
			{
				UpdateLayout();
			}
		}

		/// <summary>
		/// Called when the LayoutGroup is being destroyed.
		/// </summary>
		protected override void OnDestroy()
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

