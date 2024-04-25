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
		glfwWindow = glfwCreateWindow(width, height, title, nullptr, nullptr);

#ifdef INCLUDE_GUI
		ImGui_ImplGlfw_InitForVulkan(glfwWindow, false);
#endif
	}

	Window::~Window()
	{
#ifdef INCLUDE_GUI
		ImGui_ImplGlfw_Shutdown();
#endif

		glfwTerminate();
		glfwDestroyWindow(glfwWindow);
	}

	void Window::SetTitle(const char* title)
	{
		glfwSetWindowTitle(glfwWindow, title);
	}

	void Window::SetWindowSize(int width, int height)
	{
		glfwSetWindowSize(glfwWindow, width, height);
	}

	void Window::MinimizeWindow()
	{
		glfwIconifyWindow(glfwWindow);
	}

	TIVec2 Window::GetWindowSize() const
	{
		int width = 0, height = 0;
		glfwGetWindowSize(glfwWindow, &width, &height);
		return TIVec2{ width, height };
	}

	TVec2 Window::GetCursorPos() const
	{
		double xpos = 0.0, ypos = 0.0;
		glfwGetCursorPos(glfwWindow, &xpos, &ypos);
		return TVec2{ xpos, ypos };
	}

	GRAPI double Window::GetAspectRatio() const
	{
		int width = 0, height = 0;
		glfwGetWindowSize(glfwWindow, &width, &height);

		return double(width) / double(height);
	}

	void Window::SetCursorPos(double xpos, double ypos)
	{
		glfwSetCursorPos(glfwWindow, xpos, ypos);
	}

	void Window::ShowCursor(bool bShow)
	{
		glfwSetInputMode(glfwWindow, GLFW_CURSOR, bShow ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
	}

	void Window::DisableCursor(bool bDisabled)
	{
		glfwSetInputMode(glfwWindow, GLFW_CURSOR, bDisabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
	}

	void Window::SetAttribute(int attrib, int value)
	{
		glfwSetWindowAttrib(glfwWindow, attrib, value);
	}
};