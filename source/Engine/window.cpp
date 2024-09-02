#include "pch.hpp"
#include "window.hpp"
#include "event_listener.hpp"

#ifdef INCLUDE_GUI
#include "imgui/imgui.h"
#include "imgui/imgui_impl_vulkan.h"
#include "imgui/imgui_impl_glfw.h"
#endif

namespace GR
{
	void Window::glfw_key_press(GLFWwindow* window, int key, int scancode, int action, int mods)
	{
		EventContext* context = static_cast<EventContext*>(glfwGetWindowUserPointer(window));
		context->evnt->Register(GREvent::KeyPress((EKey)key, (EAction)action));

#ifdef INCLUDE_GUI
		ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
#endif
	}

	void Window::glfw_mouse_press(GLFWwindow* window, int button, int action, int mods)
	{
		EventContext* context = static_cast<EventContext*>(glfwGetWindowUserPointer(window));
		context->evnt->Register(GREvent::MousePress((EMouse)button, (EAction)action));
	
#ifdef INCLUDE_GUI
		ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
#endif
	}

	void Window::glfw_mouse_move(GLFWwindow* window, double xpos, double ypos)
	{
		EventContext* context = static_cast<EventContext*>(glfwGetWindowUserPointer(window));
		context->evnt->Register(GREvent::MousePosition(xpos, ypos));

#ifdef INCLUDE_GUI
		ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
#endif
	}

	void Window::glfw_resize(GLFWwindow* window, int width, int height)
	{
		EventContext* context = static_cast<EventContext*>(glfwGetWindowUserPointer(window));
		context->ctx->m_Renderer->_handleResize();
	}

	void Window::glfw_scroll(GLFWwindow* window, double dx, double dy)
	{
		EventContext* context = static_cast<EventContext*>(glfwGetWindowUserPointer(window));
		context->evnt->Register(GREvent::ScrollDelta(dx, dy));

#ifdef INCLUDE_GUI
		ImGui_ImplGlfw_ScrollCallback(window, dx, dy);
#endif
	}

	Window::Window(int width, int height, const char* title)
	{
		glfwInit();
		assert(glfwVulkanSupported());
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
		m_GlfwWindow = glfwCreateWindow(width, height, title, nullptr, nullptr);

		m_WindowPointer.ctx = this;
		glfwSetWindowUserPointer(m_GlfwWindow, &m_WindowPointer);
		glfwSetWindowSizeCallback(m_GlfwWindow, glfw_resize);

		m_Renderer = new VulkanBase(m_GlfwWindow);
	}

	Window::~Window()
	{
		if (m_GlfwWindow)
		{
			delete m_Renderer;
			glfwDestroyWindow(m_GlfwWindow);
			glfwTerminate();

			m_GlfwWindow = nullptr;
			m_Renderer = nullptr;
		}
	}

	void Window::SetUpEvents(EventListener& listener)
	{
		m_WindowPointer.evnt = &listener;
		glfwSetKeyCallback(m_GlfwWindow, glfw_key_press);
		glfwSetMouseButtonCallback(m_GlfwWindow, glfw_mouse_press);
		glfwSetCursorPosCallback(m_GlfwWindow, glfw_mouse_move);
		glfwSetScrollCallback(m_GlfwWindow, glfw_scroll);
	}

	void Window::SetTitle(const char* title)
	{
		glfwSetWindowTitle(m_GlfwWindow, title);
	}

	void Window::SetWindowSize(int width, int height)
	{
		glfwSetWindowSize(m_GlfwWindow, width, height);
	}

	void Window::MinimizeWindow()
	{
		glfwIconifyWindow(m_GlfwWindow);
	}

	glm::ivec2 Window::GetWindowSize() const
	{
		int width = 0, height = 0;
		glfwGetWindowSize(m_GlfwWindow, &width, &height);
		return glm::ivec2{ width, height };
	}

	glm::vec2 Window::GetCursorPos() const
	{
		double xpos = 0.0, ypos = 0.0;
		glfwGetCursorPos(m_GlfwWindow, &xpos, &ypos);
		return glm::vec2{ xpos, ypos };
	}

	GRAPI double Window::GetAspectRatio() const
	{
		int width = 0, height = 0;
		glfwGetWindowSize(m_GlfwWindow, &width, &height);

		return double(width) / double(height);
	}

	void Window::SetCursorPos(double xpos, double ypos)
	{
		glfwSetCursorPos(m_GlfwWindow, xpos, ypos);
	}

	void Window::ShowCursor(bool bShow)
	{
		glfwSetInputMode(m_GlfwWindow, GLFW_CURSOR, bShow ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
	}

	void Window::DisableCursor(bool bDisabled)
	{
		glfwSetInputMode(m_GlfwWindow, GLFW_CURSOR, bDisabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
	}

	void Window::SetAttribute(int attrib, int value)
	{
		glfwSetWindowAttrib(m_GlfwWindow, attrib, value);
	}

	Renderer& Window::GetRenderer() const
	{
		return *static_cast<Renderer*>(m_Renderer);
	}

	bool Window::IsAlive() const
	{
		return !glfwWindowShouldClose(m_GlfwWindow);
	}

	void Window::ProcessEvents() const
	{
		glfwPollEvents();
	}
};