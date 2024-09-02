#pragma once
#include "core.hpp"
#include "Vulkan/renderer.hpp"

namespace GR
{
	class EventListener;

	class Window
	{
	private:
		GLFWwindow* m_GlfwWindow = nullptr;
		VulkanBase* m_Renderer = nullptr;

		struct EventContext
		{
			Window* ctx;
			EventListener* evnt;
		} m_WindowPointer;

	private:
		static void glfw_key_press(GLFWwindow* window, int, int, int, int);

		static void glfw_mouse_press(GLFWwindow* window, int, int, int);

		static void glfw_mouse_move(GLFWwindow* window, double, double);

		static void glfw_resize(GLFWwindow* window, int, int);

		static void glfw_scroll(GLFWwindow* window, double, double);

	public:
		GRAPI Window(int width, int height, const char* title);

		GRAPI ~Window();
		/*
		* !@brief Connect this window to in-engine event listener
		*/
		GRAPI void SetUpEvents(EventListener& listener);
		/*
		* !@brief Sets the title of the window
		* 
		* @param[in] title - new title string
		*/
		GRAPI void SetTitle(const char* title);
		/*
		* !@brief Set the size of the window
		* 
		* @param[in] width - new width of the window
		* @param[in] height - new height of the window
		*/
		GRAPI void SetWindowSize(int width, int height);
		/*
		* !@brief Minimizes (iconifies) the window
		*/
		GRAPI void MinimizeWindow();
		/*
		* !@brief Get the size of the window
		* 
		* @return Vector containing integer width, height of the window
		*/
		GRAPI glm::ivec2 GetWindowSize() const;
		/*
		* !@brief Get the curent position of the cursor, relative to window
		* 
		* @return Vector containing x, y coordinate of cursor
		*/
		GRAPI glm::vec2 GetCursorPos() const;
		/*
		* !@brief Get aspect ration of the window
		* 
		* @return Double representing the ratio between width and height of the window
		*/
		GRAPI double GetAspectRatio() const;
		/*
		* !@brief Set cursor position on the window
		* 
		* @param[in] xpos - new x coordinate of the cursor
		* @param[in] ypos - new y coordinate of the cursor
		*/
		GRAPI void SetCursorPos(double xpos, double ypos);
		/*
		* !@brief Hides/Shows cursor when it hovers the window
		* 
		* @param[in] bShow - new state of the cursor
		*/
		GRAPI void ShowCursor(bool bShow);
		/*
		* !@brief Hides/Shows and locks/unlocks the cursor
		* 
		* @param[in] bDisabled - new state of the cursor
		*/
		GRAPI void DisableCursor(bool bDisabled);
		/*
		* !@brief Sets other GLFW attributes for the window
		* 
		* @param[in] attrib - GLFW attribute
		* @param[in] value - value of the attribute
		*/
		GRAPI void SetAttribute(int attrib, int value);
		/*
		* !@brief Get renderer interface reference
		*/
		inline GRAPI Renderer& GetRenderer() const;
		/*
		* !@brief Check if window was queued for closure
		*/
		GRAPI bool IsAlive() const;
		/*
		* !@brief Process window event queue
		*/
		GRAPI void ProcessEvents() const;
	};
};