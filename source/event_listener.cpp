#include "pch.hpp"
#include "event_listener.hpp"

namespace GR
{
	EventListener::EventListener()
	{
	}

	EventListener::~EventListener()
	{
	}

	void EventListener::Register(GrayEngine* c, GREvent::KeyPress e)
	{
		std::for_each(m_KeyPressEvents.begin(), m_KeyPressEvents.end(), [=](void(*f)(GrayEngine*, GREvent::KeyPress))
		{
			f(c, e);
		});
	}

	void EventListener::Register(GrayEngine* c, GREvent::MousePress e)
	{
		std::for_each(m_MousePressEvents.begin(), m_MousePressEvents.end(), [=](void(*f)(GrayEngine*, GREvent::MousePress))
		{
			f(c, e);
		});
	}

	void EventListener::Register(GrayEngine* e, GREvent::MousePosition x)
	{
		std::for_each(m_MouseMoveEvents.begin(), m_MouseMoveEvents.end(), [=](void(*f)(GrayEngine*, GREvent::MousePosition))
			{
				f(e, x);
			});
	}

	void EventListener::Register(GrayEngine* e, GREvent::ScrollDelta x)
	{
		std::for_each(m_ScrollEvents.begin(), m_ScrollEvents.end(), [=](void(*f)(GrayEngine*, GREvent::ScrollDelta))
			{
				f(e, x);
			});
	}

	void EventListener::Subscribe(void(*f)(GrayEngine*, GREvent::KeyPress))
	{
		m_KeyPressEvents.push_back(f);
	}

	void EventListener::Subscribe(void(*f)(GrayEngine*, GREvent::MousePress))
	{
		m_MousePressEvents.push_back(f);
	}

	void EventListener::Subscribe(void(*f)(GrayEngine*, GREvent::MousePosition))
	{
		m_MouseMoveEvents.push_back(f);
	}

	void EventListener::Subscribe(void(*f)(GrayEngine*, GREvent::ScrollDelta))
	{
		m_ScrollEvents.push_back(f);
	}
};