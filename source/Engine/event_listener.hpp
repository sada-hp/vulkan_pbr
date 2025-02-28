#pragma once
#include "core.hpp"
#include "glm/glm.hpp"
#include "Engine/enums.hpp"

namespace GR
{
	namespace Events
	{
		struct MousePosition { double x; double y; };

		struct ScrollDelta { double x; double y; };

		struct MousePress { GR::Enums::EMouse key; GR::Enums::EAction action; };

		struct KeyPress { GR::Enums::EKey key; GR::Enums::EAction action; };
	}


	class EventListener
	{
	private:
		friend class Window;

		std::vector<void(*)(Events::KeyPress, void*)> m_KeyPressEvents = {};
		std::vector<void(*)(Events::MousePress, void*)> m_MousePressEvents = {};
		std::vector<void(*)(Events::MousePosition, void*)> m_MouseMoveEvents = {};
		std::vector<void(*)(Events::ScrollDelta, void*)> m_ScrollEvents = {};

		void* m_UserPointer = nullptr;

	public:
		EventListener()
		{
			m_KeyPressEvents.resize(0);
			m_MousePressEvents.resize(0);
			m_MouseMoveEvents.resize(0);
			m_ScrollEvents.resize(0);
		}

		~EventListener() = default;

		GRAPI void SetUserPointer(void* pointer);

		// !@brief Subscribe to keyboard key press events
		GRAPI void Subscribe(void(*key_press_callback_func)(Events::KeyPress, void*));

		// !@brief Subscribe to mouse button press events
		GRAPI void Subscribe(void(*mouse_press_callback_func)(Events::MousePress, void*));

		// !@brief Subscribe to mouse pointer move events
		GRAPI void Subscribe(void(*mouse_move_callback_func)(Events::MousePosition, void*));

		// !@brief Subscribe to mouse scroll events
		GRAPI void Subscribe(void(*scroll_callback_fun)(Events::ScrollDelta, void*));

		GRAPI void Register(Events::KeyPress) const;

		GRAPI void Register(Events::MousePress) const;

		GRAPI void Register(Events::MousePosition) const;

		GRAPI void Register(Events::ScrollDelta) const;
	};
};