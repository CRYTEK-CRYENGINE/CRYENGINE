using System.Collections.Generic;

namespace CryEngine.UI
{
	public class VerticalLayoutGroup : LayoutGroup
	{
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
