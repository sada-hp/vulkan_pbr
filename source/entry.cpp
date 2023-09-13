#include "pch.hpp"
#include "renderer.hpp"

std::string exe_path;

int main(int argc, char** argv)
{
	if (argc > 0)
	{
		exe_path = argv[0];
		exe_path = exe_path.substr(0, exe_path.find_last_of('\\') + 1);
	}

	GLFWwindow* pWindow = nullptr;
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	pWindow = glfwCreateWindow(1024, 720, "Vulkan PBR", nullptr, nullptr);

	assert(glfwVulkanSupported());

	VulkanBase render(pWindow);

	glfwSetWindowUserPointer(pWindow, &render);
	glfwSetWindowSizeCallback(pWindow, [](GLFWwindow* window, int width, int height)
		{
			VulkanBase* rndr = static_cast<VulkanBase*>(glfwGetWindowUserPointer(window));
			rndr->HandleResize();
		});

	unsigned long long frame_count = 0;
	auto count_start = std::chrono::steady_clock::now();
	while (!glfwWindowShouldClose(pWindow))
	{
		glfwPollEvents();
		glfwSwapBuffers(pWindow);
		render.Step();
		frame_count++;
		auto count_end = std::chrono::steady_clock::now();

		if (std::chrono::duration_cast<std::chrono::seconds>(count_end - count_start).count() >= 1L)
		{
			long long latency = std::chrono::duration_cast<std::chrono::seconds>(count_end - count_start).count() / frame_count;
			glfwSetWindowTitle(pWindow, ("Vulkan PBR " + std::to_string(frame_count)).c_str());

			count_start = count_end;
			frame_count = 0;
		}
	}

	glfwDestroyWindow(pWindow);
	glfwTerminate();

	return 0;
}