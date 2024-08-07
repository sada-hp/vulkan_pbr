#pragma once
#include "core.hpp"
#include "glm/glm.hpp"
#include "enums.hpp"

namespace GREvent
{
	struct MousePosition { double x; double y; };

	struct ScrollDelta { double x; double y; };

	struct MousePress { GR::EMouse key; GR::EAction action; };

	struct KeyPress { GR::EKey key; GR::EAction action; };
}

namespace GR
{
	class GrayEngine;

	class EventListener
	{
		friend class GrayEngine;

		EventListener();

		~EventListener();

		std::vector<void(*)(GrayEngine*, GREvent::KeyPress)> m_KeyPressEvents;
		std::vector<void(*)(GrayEngine*, GREvent::MousePress)> m_MousePressEvents;
		std::vector<void(*)(GrayEngine*, GREvent::MousePosition)> m_MouseMoveEvents;
		std::vector<void(*)(GrayEngine*, GREvent::ScrollDelta)> m_ScrollEvents;

		void Register(GrayEngine*, GREvent::KeyPress);

		void Register(GrayEngine*, GREvent::MousePress);

		void Register(GrayEngine*, GREvent::MousePosition);

		void Register(GrayEngine*, GREvent::ScrollDelta);

	public:
		// !@brief Subscribe to keyboard key press events
		GRAPI void Subscribe(void(*key_press_callback_func)(GrayEngine*, GREvent::KeyPress));

		// !@brief Subscribe to mouse button press events
		GRAPI void Subscribe(void(*mouse_press_callback_func)(GrayEngine*, GREvent::MousePress));

		// !@brief Subscribe to mouse pointer move events
		GRAPI void Subscribe(void(*mouse_move_callback_func)(GrayEngine*, GREvent::MousePosition));

		// !@brief Subscribe to mouse scroll events
		GRAPI void Subscribe(void(*scroll_callback_fun)(GrayEngine*, GREvent::ScrollDelta));
	};
};