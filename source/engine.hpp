#pragma once

#include "core.hpp"
#include "glm/glm.hpp"
#include "entt/entt.hpp"
#include "window.hpp"
#include "event_listener.hpp"
#include "math.hpp"
#include "renderer.hpp"

#ifdef INCLUDE_GUI
#include "imgui/imgui.h"
#include "imgui/imgui_impl_vulkan.h"
#include "imgui/imgui_impl_glfw.h"
#endif

namespace GR
{
	using Entity = entt::entity;

	class GrayEnigne;
	/*
	* !@brief Struct describing settings for initial application launch
	*/
	struct ApplicationSettings
	{
		std::string ApplicationName;
		TIVec2 WindowExtents;
	};
	/*
	* !@brief Main engine class
	*/
	class GrayEngine
	{
		TVector<void(*)(GrayEngine*, double)> inputs;
		entt::registry registry;
		EventListener* listener;
		VulkanBase* renderer;
		Window* window;
		double delta = 0.0;
		double total_time = 0;

		struct EngineContext
		{
			EventListener* listener;
			VulkanBase* renderer;
			GrayEngine* engine;
		} context;

		static void glfw_key_press(GLFWwindow* window, int key, int scancode, int action, int mods);

		static void glfw_mouse_press(GLFWwindow* window, int button, int action, int mods);

		static void glfw_mouse_move(GLFWwindow* window, double xpos, double ypos);

		static void glfw_resize(GLFWwindow* window, int width, int height);

#ifdef INCLUDE_GUI
		ImGuiContext* GuiContext;
#endif
	public:
#ifdef INCLUDE_GUI
		GRAPI ImGuiContext* GetGUIContext()
		{
			return GuiContext;
		}
#endif
		/*
		* !@brief User defined pointer, renderer itself does not reference it
		*/
		void* userPointer1 = nullptr;
		/*
		* !@brief User defined pointer, renderer itself does not reference it
		*/
		void* userPointer2 = nullptr;
		/*
		* !@brief User defined pointer, renderer itself does not reference it
		*/
		void* userPointer3 = nullptr;
		/*
		* !@brief User defined pointer, renderer itself does not reference it
		*/
		void* userPointer4 = nullptr;

		GRAPI GrayEngine(const std::string& ExecutablePath, ApplicationSettings& Settings);

		GRAPI ~GrayEngine();

		GRAPI void StartGameLoop();
		/*
		* !@brief These functions will be called once every frame
		*
		* @param[in] Engine contex from which the call was made
		* @param[in] Time elapsed since last frame
		*
		*/
		GRAPI void AddInputFunction(void(*function)(GrayEngine*, double));

		GRAPI Entity LoadModel(const std::string& MeshPath, const std::string* TextureCollection);

		GRAPI double GetTime() const;

		GRAPI Window& GetWindow() const
		{
			return *window;
		}

		GRAPI Camera& GetMainCamera() const
		{
			return renderer->camera;
		}

		GRAPI EventListener& GetEventListener()
		{
			return *listener;
		}

		GRAPI VulkanBase& GetRenderer()
		{
			return *renderer;
		}

		template<typename Type, typename... Args>
		GRAPI decltype(auto) EmplaceComponent(Entity ent, Args&& ...args)
		{
			return registry.emplace_or_replace<Type>(ent, args...);
		}

		template<typename... Type>
		GRAPI decltype(auto) GetComponent(const Entity entt)
		{
			return registry.get<Type...>(entt);
		}
	};
};