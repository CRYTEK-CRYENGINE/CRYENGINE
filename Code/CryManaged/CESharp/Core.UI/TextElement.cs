using CryEngine.UI.Components;

namespace CryEngine.UI
{
	public class TextElement : UIElement
	{
		public Text Text { get; private set; }

		/// <summary>
		/// Sets the height of both the RectTransform and the Text.
		/// </summary>
		public float Height { set { RectTransform.Height = value; } }

		public override void OnAwake()
		{
			Text = AddComponent<Text>();
			var parentRect = Parent as UIElement;
			RectTransform.Alignment = Alignment.Center;
			RectTransform.Size = new Point(parentRect.RectTransform.Width, parentRect.RectTransform.Height);
		}
	}
}

