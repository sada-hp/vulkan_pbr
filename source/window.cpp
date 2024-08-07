#include "pch.hpp"
#include "window.hpp"

#ifdef INCLUDE_GUI
#include "imgui/imgui.h"
#include "imgui/imgui_impl_vulkan.h"
#include "imgui/imgui_impl_glfw.h"
#endif

namespace GR
{
	Window::Window(const char* title, int width, int height)
	{
		glfwInit();
		assert(glfwVulkanSupported());
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
		m_GlfwWindow = glfwCreateWindow(width, height, title, nullptr, nullptr);

#ifdef INCLUDE_GUI
		ImGui_ImplGlfw_InitForVulkan(m_GlfwWindow, false);
#endif
	}

	Window::~Window()
	{
#ifdef INCLUDE_GUI
		ImGui_ImplGlfw_Shutdown();
#endif

		glfwDestroyWindow(m_GlfwWindow);
		glfwTerminate();
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

	TIVec2 Window::GetWindowSize() const
	{
		int width = 0, height = 0;
		glfwGetWindowSize(m_GlfwWindow, &width, &height);
		return TIVec2{ width, height };
	}

	TVec2 Window::GetCursorPos() const
	{
		double xpos = 0.0, ypos = 0.0;
		glfwGetCursorPos(m_GlfwWindow, &xpos, &ypos);
		return TVec2{ xpos, ypos };
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
};