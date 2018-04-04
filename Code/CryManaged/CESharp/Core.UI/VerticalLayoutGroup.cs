using System.Collections.Generic;

namespace CryEngine.UI
{
	/// <summary>
	/// A LayoutGroup that creates a vertical layout.
	/// </summary>
	public class VerticalLayoutGroup : LayoutGroup
	{
		/// <summary>
		/// Called when the layout needs to be updated.
		/// </summary>
		/// <param name="items">Items.</param>
		protected override void OnUpdateLayout(List<UIElement> items)
		{
			RectTransform.Height = Spacing;

			for (int i = 0; i < items.Count; i++)
			{
				if (i != 0)
				{
					// Add spacing between each button
					items[i].RectTransform.Padding.Top = items[i - 1].RectTransform.Padding.Top + items[i - 1].RectTransform.Height + Spacing;
				}
				else
				{
					// Add initial spacing to the first button
					items[i].RectTransform.Padding.Top = Spacing;
				}

				RectTransform.Height += items[i].RectTransform.Height + Spacing;
			}
		}
	}
}
