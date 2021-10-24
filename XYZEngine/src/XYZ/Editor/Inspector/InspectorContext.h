#pragma once
#include "XYZ/Scene/SceneEntity.h"
#include "XYZ/Renderer/EditorRenderer.h"
#include "XYZ/Renderer/Material.h"
#include "XYZ/Renderer/SubTexture.h"

namespace XYZ {
	namespace Editor {

		class InspectorContext
		{
		public:
			virtual ~InspectorContext() = default;

			virtual void OnImGuiRender(Ref<EditorRenderer> renderer) = 0;
		};
	}
}