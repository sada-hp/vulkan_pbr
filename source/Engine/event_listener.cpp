#include "pch.hpp"
#include "event_listener.hpp"

namespace GR
{
	void EventListener::SetUserPointer(void* pointer)
	{
		m_UserPointer = pointer;
	}

	void EventListener::Register(Events::KeyPress e) const
	{
		std::for_each(m_KeyPressEvents.begin(), m_KeyPressEvents.end(), [=](void(*f)(Events::KeyPress, void*))
		{
			f(e, m_UserPointer);
		});
	}

	void EventListener::Register(Events::MousePress e) const
	{
		std::for_each(m_MousePressEvents.begin(), m_MousePressEvents.end(), [=](void(*f)(Events::MousePress, void*))
		{
			f(e, m_UserPointer);
		});
	}

	void EventListener::Register(Events::MousePosition e) const
	{
		std::for_each(m_MouseMoveEvents.begin(), m_MouseMoveEvents.end(), [=](void(*f)(Events::MousePosition, void*))
		{
			f(e, m_UserPointer);
		});
	}

	void EventListener::Register(Events::ScrollDelta e) const
	{
		std::for_each(m_ScrollEvents.begin(), m_ScrollEvents.end(), [=](void(*f)(Events::ScrollDelta, void*))
		{
			f(e, m_UserPointer);
		});
	}

	void EventListener::Subscribe(void(*f)(Events::KeyPress, void*))
	{
		m_KeyPressEvents.push_back(f);
	}

	void EventListener::Subscribe(void(*f)(Events::MousePress, void*))
	{
		m_MousePressEvents.push_back(f);
	}

	void EventListener::Subscribe(void(*f)(Events::MousePosition, void*))
	{
		m_MouseMoveEvents.push_back(f);
	}

	void EventListener::Subscribe(void(*f)(Events::ScrollDelta, void*))
	{
		m_ScrollEvents.push_back(f);
	}
};