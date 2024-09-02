#pragma once
#include "core.hpp"
#include "glm/glm.hpp"
#include "Engine/enums.hpp"

namespace GREvent
{
	struct MousePosition { double x; double y; };

	struct ScrollDelta { double x; double y; };

	struct MousePress { GR::EMouse key; GR::EAction action; };

	struct KeyPress { GR::EKey key; GR::EAction action; };
}

namespace GR
{
	class EventListener
	{
	private:
		friend class Window;

		std::vector<void(*)(GREvent::KeyPress, void*)> m_KeyPressEvents;
		std::vector<void(*)(GREvent::MousePress, void*)> m_MousePressEvents;
		std::vector<void(*)(GREvent::MousePosition, void*)> m_MouseMoveEvents;
		std::vector<void(*)(GREvent::ScrollDelta, void*)> m_ScrollEvents;

		void* m_UserPointer = nullptr;

	public:
		EventListener() = default;

		~EventListener() = default;

		GRAPI void SetUserPointer(void* pointer);

		// !@brief Subscribe to keyboard key press events
		GRAPI void Subscribe(void(*key_press_callback_func)(GREvent::KeyPress, void*));

		// !@brief Subscribe to mouse button press events
		GRAPI void Subscribe(void(*mouse_press_callback_func)(GREvent::MousePress, void*));

		// !@brief Subscribe to mouse pointer move events
		GRAPI void Subscribe(void(*mouse_move_callback_func)(GREvent::MousePosition, void*));

		// !@brief Subscribe to mouse scroll events
		GRAPI void Subscribe(void(*scroll_callback_fun)(GREvent::ScrollDelta, void*));

		GRAPI void Register(GREvent::KeyPress);

		GRAPI void Register(GREvent::MousePress);

		GRAPI void Register(GREvent::MousePosition);

		GRAPI void Register(GREvent::ScrollDelta);
	};
};