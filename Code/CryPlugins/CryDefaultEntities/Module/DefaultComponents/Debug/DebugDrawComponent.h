#pragma once

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IShader.h>

#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>

class CPlugin_CryDefaultEntities;

namespace Cry
{
	namespace DefaultComponents
	{
		class CDebugDrawComponent
			: public IEntityComponent
		{
		protected:
			friend CPlugin_CryDefaultEntities;
			static void Register(Schematyc::CEnvRegistrationScope& componentScope);

			// IEntityComponent
			virtual void ProcessEvent(const SEntityEvent& event) final;
			virtual uint64 GetEventMask() const final;
			// ~IEntityComponent

		public:
			virtual ~CDebugDrawComponent() {}

			static void ReflectType(Schematyc::CTypeDesc<CDebugDrawComponent>& desc)
			{
				desc.SetGUID("{9C255A6C-B3E2-4AF2-BDB4-9ADCCC9E07D3}"_cry_guid);
				desc.SetEditorCategory("Debug");
				desc.SetLabel("Debug Draw");
				desc.SetDescription("Allows drawing debug information to the screen");
				desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::ClientOnly });

				desc.AddMember(&CDebugDrawComponent::m_drawPersistent, 'draw', "DrawPersistent", "Draw Persistent Text", "Whether or not to draw the persistent text to screen", false);
				desc.AddMember(&CDebugDrawComponent::m_persistentText, 'text', "PersistentText", "Persistent Text", "Persistent text to draw to screen", "");
				desc.AddMember(&CDebugDrawComponent::m_persistentTextColor, 'tcol', "PersistentTextCol", "Persistent Text Color", "Color of the text to draw to screen", ColorF(1, 1, 1));
				desc.AddMember(&CDebugDrawComponent::m_shouldScaleWithCameraDistance, 'scal', "ScaleWithCam", "Scale With Camera Distance", "Whether the text should scale with font size" 
					"depending on the camera distance or always draw the font in the same size.", false);
				desc.AddMember(&CDebugDrawComponent::m_persistentViewDistance, 'view', "PersistentDist", "Persistent View Distance", "View distance of the persistent text", 100.f);
				desc.AddMember(&CDebugDrawComponent::m_persistentFontSize, 'font', "PersistentSize", "Persistent Font Size", "Size of the persistent font", 1.2f);
			}

			virtual void DrawSphere(const Vec3& pos, float radius, const ColorF& color, float duration);
			virtual void DrawDirection(const Vec3& pos, float radius, const Vec3& dir, const ColorF& color, float duration);
			virtual void DrawLine(const Vec3& posStart, const Vec3& posEnd, const ColorF& color, float duration);
			virtual void DrawPlanarDisc(const Vec3& pos, float innerRadius, float outerRadius, const ColorF& color, float duration);
			virtual void DrawCone(const Vec3& pos, const Vec3& dir, float baseRadius, float height, const ColorF& color, float duration);
			virtual void DrawCylinder(const Vec3& pos, const Vec3& dir, float radius, float height, const ColorF& color, float duration);
			virtual void Draw2DText(Schematyc::CSharedString text, float size, const ColorF& color, float duration);
			virtual void DrawText(Schematyc::CSharedString text, float x, float y, float size, const ColorF& color, float duration);
			virtual void DrawText3D(Schematyc::CSharedString text, const Vec3& pos, float size, const ColorF& color, float duration);
			virtual void Draw2DLine(float x1, float y1, float x2, float y2, const ColorF& color, float duration);

			bool IsPersistentTextEnabled() const { return m_drawPersistent; }
			virtual void EnablePersistentText(bool bEnable) { m_drawPersistent = bEnable; }

			virtual void SetPersistentTextColor(const ColorF& color) { m_persistentTextColor = color; }
			const ColorF& GetPersistentTextColor() const { return m_persistentTextColor; }

			virtual void SetPersistentTextViewDistance(float viewDistance) { m_persistentViewDistance = viewDistance; }
			float GetPersistentTextViewDistance() const { return m_persistentViewDistance; }

			virtual void SetPersistentTextFontSize(float fontSize) { m_persistentFontSize = fontSize; }
			float GetPersistentTextFontSize() const { return m_persistentFontSize; }

			virtual void SetPersistentText(const char* szText);
			const char* GetPersistentText() const { return m_persistentText.c_str(); }

		protected:
			bool m_drawPersistent = false;
			bool m_shouldScaleWithCameraDistance = false;
			ColorF m_persistentTextColor = ColorF(1, 1, 1);
			Schematyc::Range<0, 255> m_persistentViewDistance = 100.f;
			Schematyc::Range<0, 100> m_persistentFontSize = 1.2f;
			
			Schematyc::CSharedString m_persistentText;
		};
	}
}