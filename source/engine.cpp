#include "pch.hpp"
#include "engine.hpp"

std::string exec_path = "";

namespace GR
{
	GrayEngine::GrayEngine(const std::string& ExecutablePath, ApplicationSettings& Settings)
	{
		assert(ExecutablePath != "");

		srand(time(NULL));

		exec_path = ExecutablePath;

		listener = new EventListener();
		window = new Window(Settings.ApplicationName.c_str(), Settings.WindowExtents.x, Settings.WindowExtents.y);
		renderer = new VulkanBase(window->glfwWindow, registry);

		context = { listener , renderer, this };

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

			std::for_each(inputs.begin(), inputs.end(), [&](void(*func)(GrayEngine*, double))
			{
				func(this, delta);
			});

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