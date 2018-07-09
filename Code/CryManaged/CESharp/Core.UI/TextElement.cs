using CryEngine.UI.Components;

namespace CryEngine.UI
{
	/// <summary>
	/// A UIElement that has a Text component.
	/// </summary>
	public class TextElement : UIElement
	{
		/// <summary>
		/// The Text component.
		/// </summary>
		/// <value>The text.</value>
		public Text Text { get; private set; }

		/// <summary>
		/// Sets the height of both the RectTransform and the Text.
		/// </summary>
		public float Height { set { RectTransform.Height = value; } }

		/// <summary>
		/// Called when a TextElement is created.
		/// </summary>
		protected override void OnAwake()
		{
			Text = AddComponent<Text>();
			var parentRect = Parent.GetComponent<RectTransform>();

			RectTransform.Alignment = Alignment.Center;
			RectTransform.Size = parentRect == null ? new Point(0,0) : new Point(parentRect.Width, parentRect.Height);
		}
	}
}