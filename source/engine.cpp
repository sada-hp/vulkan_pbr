#include "pch.hpp"
#include "engine.hpp"

std::string exec_path = "";

namespace GR
{
	GrayEngine::GrayEngine(const std::string& ExecutablePath, ApplicationSettings& Settings)
	{
		assert(ExecutablePath != "");

		exec_path = ExecutablePath;

#ifdef INCLUDE_GUI
		GuiContext = ImGui::CreateContext();
		ImGui::SetCurrentContext(GuiContext);
#endif

		listener = new EventListener();
		window = new Window(Settings.ApplicationName.c_str(), Settings.WindowExtents.x, Settings.WindowExtents.y);

		context = { listener , renderer, this };

		renderer = new VulkanBase(window->glfwWindow, registry);

		glfwSetWindowUserPointer(window->glfwWindow, &context);
		glfwSetWindowSizeCallback(window->glfwWindow, glfw_resize);
		glfwSetKeyCallback(window->glfwWindow, glfw_key_press);
		glfwSetMouseButtonCallback(window->glfwWindow, glfw_mouse_press);
		glfwSetCursorPosCallback(window->glfwWindow, glfw_mouse_move);
	}

	GrayEngine::~GrayEngine()
	{
		delete listener;
		delete renderer;
		delete window;
		registry.clear();
	}

	void GrayEngine::StartGameLoop()
	{
		while (!glfwWindowShouldClose(window->glfwWindow))
		{
			glfwPollEvents();

			double frame_time = GetTime();
			delta = frame_time - total_time;
			total_time = frame_time;

#ifdef INCLUDE_GUI
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
#endif

			std::for_each(inputs.begin(), inputs.end(), [&](void(*func)(GrayEngine*, double))
			{
				func(this, delta);
			});

#ifdef INCLUDE_GUI
			ImGui::Render();
#endif

			renderer->Step(delta);

			glfwSwapBuffers(window->glfwWindow);
		}
	}

	void GR::GrayEngine::AddInputFunction(void(*function)(GrayEngine*, double))
	{
		inputs.push_back(function);
	}

	Entity GrayEngine::LoadModel(const std::string& MeshPath, const std::string* TextureCollection)
	{
		return renderer->AddGraphicsObject(MeshPath, "");
	}

	double GrayEngine::GetTime() const
	{
		return glfwGetTime();
	}
};