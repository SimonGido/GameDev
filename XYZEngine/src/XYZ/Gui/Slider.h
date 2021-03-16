#pragma once
#include "XYZ/ECS/Component.h"
#include "XYZ/Event/GuiEvent.h"
#include "XYZ/Event/EventSystem.h"
#include "XYZ/FSM/StateMachine.h"

#include <glm/glm.hpp>

namespace XYZ {

	namespace SliderState {
		enum SliderState
		{
			Dragged,
			Released,
			NumStates
		};
	}

	class Slider : public EventSystem<ClickEvent, ReleaseEvent, DraggedEvent>,
		public IComponent
	{
	public:
		Slider(const glm::vec4& hooverColor);

		glm::vec4 HooverColor;
		float	  Value = 0.0f;

		StateMachine<SliderState::NumStates> Machine;
	};
}