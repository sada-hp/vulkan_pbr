#include "pch.hpp"
#include "event_listener.hpp"

namespace GR
{
	void EventListener::SetUserPointer(void* pointer)
	{
		m_UserPointer = pointer;
	}

	void EventListener::Register(GREvent::KeyPress e)
	{
		std::for_each(m_KeyPressEvents.begin(), m_KeyPressEvents.end(), [=](void(*f)(GREvent::KeyPress, void*))
		{
			f(e, m_UserPointer);
		});
	}

	void EventListener::Register(GREvent::MousePress e)
	{
		std::for_each(m_MousePressEvents.begin(), m_MousePressEvents.end(), [=](void(*f)(GREvent::MousePress, void*))
		{
			f(e, m_UserPointer);
		});
	}

	void EventListener::Register(GREvent::MousePosition e)
	{
		std::for_each(m_MouseMoveEvents.begin(), m_MouseMoveEvents.end(), [=](void(*f)(GREvent::MousePosition, void*))
		{
			f(e, m_UserPointer);
		});
	}

	void EventListener::Register(GREvent::ScrollDelta e)
	{
		std::for_each(m_ScrollEvents.begin(), m_ScrollEvents.end(), [=](void(*f)(GREvent::ScrollDelta, void*))
		{
			f(e, m_UserPointer);
		});
	}

	void EventListener::Subscribe(void(*f)(GREvent::KeyPress, void*))
	{
		m_KeyPressEvents.push_back(f);
	}

	void EventListener::Subscribe(void(*f)(GREvent::MousePress, void*))
	{
		m_MousePressEvents.push_back(f);
	}

	void EventListener::Subscribe(void(*f)(GREvent::MousePosition, void*))
	{
		m_MouseMoveEvents.push_back(f);
	}

	void EventListener::Subscribe(void(*f)(GREvent::ScrollDelta, void*))
	{
		m_ScrollEvents.push_back(f);
	}
};