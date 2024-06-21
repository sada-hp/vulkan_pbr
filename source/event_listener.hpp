#pragma once
#include "core.hpp"
#include "glm/glm.hpp"
#include "enums.hpp"

namespace GREvent
{
	struct MousePosition { double x; double y; };

	struct ScrollDelta { double x; double y; };
}

namespace GR
{
	class GrayEngine;

	class EventListener
	{
		friend class GrayEngine;

		EventListener();

		~EventListener();

		std::vector<void(*)(GrayEngine*, EKey, EAction)> keypress;
		std::vector<void(*)(GrayEngine*, EMouse, EAction)> mousepress;
		std::vector<void(*)(GrayEngine*, GREvent::MousePosition)> mousemove;
		std::vector<void(*)(GrayEngine*, GREvent::ScrollDelta)> scroll;

		void Register(GrayEngine*, EKey, EAction);

		void Register(GrayEngine*, EMouse, EAction);

		void Register(GrayEngine*, GREvent::MousePosition);

		void Register(GrayEngine*, GREvent::ScrollDelta);

	public:
		GRAPI void Subscribe(void(*key_press_callback_func)(GrayEngine*, EKey, EAction));

		GRAPI void Subscribe(void(*mouse_press_callback_func)(GrayEngine*, EMouse, EAction));

		GRAPI void Subscribe(void(*mouse_move_callback_func)(GrayEngine*, GREvent::MousePosition));

		GRAPI void Subscribe(void(*scroll_callback_fun)(GrayEngine*, GREvent::ScrollDelta));
	};
};