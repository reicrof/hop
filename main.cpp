#include <GLFW/glfw3.h>

#include "im_prof.h"
#include "imgui.h"
#include <stdio.h>
#include <Windows.h>

int main(void)
{
	GLFWwindow* window;

	/* Initialize the library */
	if (!glfwInit())
		return -1;

	/* Create a windowed mode window and its OpenGL context */
	window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		return -1;
	}

	/* Make the window's context current */
	glfwMakeContextCurrent(window);

	imProfInit();

	bool show_test_window = true;
	bool show_another_window = false;
	ImVec4 clear_color = ImColor(114, 144, 154);

	ImProfiler* prof = imNewProfiler("My Profiler");

	/* Loop until the user closes the window */
	while (!glfwWindowShouldClose(window))
	{	
		prof->pushProfTrace("Main");
		Sleep(2);
		int w, h;
		glfwGetWindowSize(window, &w, &h);
		double mouseX, mouseY;
		glfwGetCursorPos(window, &mouseX, &mouseY);
		imProfNewFrame(w,h, (int)mouseX, (int)mouseY,  glfwGetMouseButton(window, 0));
		/* Poll for and process events */
		glfwPollEvents();

		ImGui::ShowTestWindow(&show_test_window);

		prof->pushProfTrace("A trace");
		prof->pushProfTrace("Another one##23233");
		prof->pushProfTrace("Another one##23232");
		prof->pushProfTrace("Another one##2323");
		prof->popProfTrace();
		prof->pushProfTrace("Another one##232");
		prof->popProfTrace();
		prof->popProfTrace();
		prof->popProfTrace();
		prof->popProfTrace();

		prof->popProfTrace();
		prof->pushProfTrace("Another one");
		prof->popProfTrace();

		// Rendering
		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);

		imProfDraw();

		/* Swap front and back buffers */
		glfwSwapBuffers(window);
	}

	glfwTerminate();
	return 0;
}