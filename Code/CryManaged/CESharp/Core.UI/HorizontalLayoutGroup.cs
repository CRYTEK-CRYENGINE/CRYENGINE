using CryEngine.UI.Components;
using System.Collections.Generic;

namespace CryEngine.UI
{
	/// <summary>
	/// A LayoutGroup that creates a horizontal layout.
	/// </summary>
	public class HorizontalLayoutGroup : LayoutGroup
	{
		/// <summary>
		/// Called when the layout needs to be updated.
		/// </summary>
		/// <param name="items">Items.</param>
		protected override void OnUpdateLayout(List<UIElement> items)
		{
			RectTransform.Width = Spacing;

			for (int i = 0; i < items.Count; i++)
			{
				if (i != 0)
				{
					// Add spacing between each button
					items[i].RectTransform.Padding.Left = items[i - 1].RectTransform.Padding.Left + items[i - 1].RectTransform.Width + Spacing;
				}
				else
				{
					// Add initial spacing to the first button
					items[i].RectTransform.Padding.Left = Spacing;
				}

				RectTransform.Width += items[i].RectTransform.Width + Spacing;
			}
		}
	}
}
