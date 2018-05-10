using CryEngine.UI.Components;
using System.Collections.Generic;

namespace CryEngine.UI
{
	public class HorizontalLayoutGroup : LayoutGroup
	{
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
