#include "renderer.hpp"

int main()
{
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

	while (!glfwWindowShouldClose(pWindow))
	{
		glfwPollEvents();
		glfwSwapBuffers(pWindow);
		render.Step();
	}

	glfwDestroyWindow(pWindow);
	glfwTerminate();

	return 0;
}