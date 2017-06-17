#include "im_prof.h"

#include <Windows.h>
#include "imgui.h"
#include <gl/GL.h>

#include <chrono>
#include <vector>
#include <assert.h>

namespace
{
	static std::chrono::time_point<std::chrono::system_clock> g_Time = std::chrono::system_clock::now();
	static GLuint       g_FontTexture = 0;
	std::vector<ImProfiler> _profilers;

	// This is the main rendering function that you have to implement and provide to ImGui (via setting up 'RenderDrawListsFn' in the ImGuiIO structure)
	// If text or lines are blurry when integrating ImGui in your engine:
	// - in your Render function, try translating your projection matrix by (0.5f,0.5f) or (0.375f,0.375f)
	void renderDrawlist(ImDrawData* draw_data)
	{
		// Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
		ImGuiIO& io = ImGui::GetIO();
		int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
		int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
		if (fb_width == 0 || fb_height == 0)
			return;
		draw_data->ScaleClipRects(io.DisplayFramebufferScale);

		// We are using the OpenGL fixed pipeline to make the example code simpler to read!
		// Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled, vertex/texcoord/color pointers.
		GLint last_texture; glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
		GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
		GLint last_scissor_box[4]; glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
		glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TRANSFORM_BIT);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_SCISSOR_TEST);
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
		glEnable(GL_TEXTURE_2D);
		//glUseProgram(0); // You may want this if using this code in an OpenGL 3+ context

		// Setup viewport, orthographic projection matrix
		glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glOrtho(0.0f, io.DisplaySize.x, io.DisplaySize.y, 0.0f, -1.0f, +1.0f);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();

		// Render command lists
#define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
		for (int n = 0; n < draw_data->CmdListsCount; n++)
		{
			const ImDrawList* cmd_list = draw_data->CmdLists[n];
			const ImDrawVert* vtx_buffer = cmd_list->VtxBuffer.Data;
			const ImDrawIdx* idx_buffer = cmd_list->IdxBuffer.Data;
			glVertexPointer(2, GL_FLOAT, sizeof(ImDrawVert), (const GLvoid*)((const char*)vtx_buffer + OFFSETOF(ImDrawVert, pos)));
			glTexCoordPointer(2, GL_FLOAT, sizeof(ImDrawVert), (const GLvoid*)((const char*)vtx_buffer + OFFSETOF(ImDrawVert, uv)));
			glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(ImDrawVert), (const GLvoid*)((const char*)vtx_buffer + OFFSETOF(ImDrawVert, col)));

			for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
			{
				const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
				if (pcmd->UserCallback)
				{
					pcmd->UserCallback(cmd_list, pcmd);
				}
				else
				{
					glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId);
					glScissor((int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
					glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idx_buffer);
				}
				idx_buffer += pcmd->ElemCount;
			}
		}
#undef OFFSETOF

		// Restore modified state
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
		glBindTexture(GL_TEXTURE_2D, (GLuint)last_texture);
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glPopAttrib();
		glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
		glScissor(last_scissor_box[0], last_scissor_box[1], (GLsizei)last_scissor_box[2], (GLsizei)last_scissor_box[3]);
	}

	void createResources()
	{
		// Build texture atlas
		ImGuiIO& io = ImGui::GetIO();
		unsigned char* pixels;
		int width, height;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);   // Load as RGBA 32-bits (75% of the memory is wasted, but default font is so small) because it is more likely to be compatible with user's existing shaders. If your ImTextureId represent a higher-level concept than just a GL texture id, consider calling GetTexDataAsAlpha8() instead to save on GPU memory.

																  // Upload texture to graphics system
		GLint last_texture;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
		glGenTextures(1, &g_FontTexture);
		glBindTexture(GL_TEXTURE_2D, g_FontTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

		// Store our identifier
		io.Fonts->TexID = (void *)(intptr_t)g_FontTexture;

		// Restore state
		glBindTexture(GL_TEXTURE_2D, last_texture);
	}

	bool dummy = false;
	bool drawTrace(const ImProfiler::ProfRes& trace)
	{
		bool isOpen = ImGui::TreeNode(trace.name, "%s :    %f", trace.name, trace.deltaTime);
		ImGui::SameLine();
		ImGui::Checkbox("Read-only", &dummy);
		return isOpen;
	}
}


void imProfNewFrame( int width, int height, int mouseX, int mouseY, bool leftMouseButtonPressed )
{
	if (!g_FontTexture)
		createResources();

	ImGuiIO& io = ImGui::GetIO();

	// Setup display size (every frame to accommodate for window resizing)
	io.DisplaySize = ImVec2((float)width, (float)height);
	//io.DisplayFramebufferScale = ImVec2(w > 0 ? ((float)display_w / w) : 0, h > 0 ? ((float)display_h / h) : 0);

	// Setup time step
	auto curTime = std::chrono::system_clock::now();
	io.DeltaTime = static_cast< float >( std::chrono::duration_cast<std::chrono::milliseconds>((curTime - g_Time)).count() );
	g_Time = curTime;

	// Mouse position in screen coordinates (set to -1,-1 if no mouse / on another screen, etc.)
	io.MousePos = ImVec2((float)mouseX, (float)mouseY);

	io.MouseDown[0] = leftMouseButtonPressed;

	//io.MouseWheel = g_MouseWheel;
	//g_MouseWheel = 0.0f;

	// Start the frame
	ImGui::NewFrame();
}

void imProfDraw()
{
	for (const auto& p : _profilers)
	{
		p.draw();
	}

	ImGui::Render();

	for (auto& p : _profilers)
	{
		p.clear();
	}
}


void imProfInit()
{
	ImGuiIO& io = ImGui::GetIO();

	// Init keys ?
	// Init callbacks
	io.RenderDrawListsFn = renderDrawlist;      // Alternatively you can set this to NULL and call ImGui::GetDrawData() after ImGui::Render() to get the same ImDrawData pointer.
												// Clipboard stuff ?
	// Mouse callbacks	
	//glfwSetMouseButtonCallback(window, ImGui_ImplGlfw_MouseButtonCallback);
	//glfwSetScrollCallback(window, ImGui_ImplGlfw_ScrollCallback);
}

ImProfiler* imNewProfiler( const char* name )
{
	ImProfiler prof{ name };
	_profilers.emplace_back(std::move(prof));
	return &_profilers.back();
}

ImProfiler::ImProfiler(const char* name) : _name(name)
{
	_traceStack.reserve(20);
}

void ImProfiler::draw() const
{
	assert(currentLevel == -1);
	ImGui::SetNextWindowSize(ImVec2(300, 100), ImGuiSetCond_Once);
	if (!ImGui::Begin(_name))
	{
		ImGui::End();
		return;
	}

	// Draw the traces
	int level = 0;
	for (size_t i = 0; i < _tracesResults.size(); ++i)
	{
		const auto& trace = _tracesResults[i];
		if (trace.level == level)
		{
			if (drawTrace(trace))
			{
				++level;
			}
		}
		else if (trace.level < level)
		{
			ImGui::TreePop();
			if (!drawTrace(trace))
			{
				--level;
			}
		}
	}

	while (level-- > 0)
	{
		ImGui::TreePop();
	}

	ImGui::End();
}

void ImProfiler::pushProfTrace(const char * traceName)
{
	_tracesResults.emplace_back(ProfRes{ traceName, -1.0f, ++currentLevel });
	_traceStack.emplace_back(ProfTrace{ traceName, std::chrono::system_clock::now(), &_tracesResults.back().deltaTime });
}

void ImProfiler::popProfTrace()
{
	assert(_traceStack.size() > 0 );
	--currentLevel;
	auto& trace = _traceStack.back();
	*trace.deltaTimeRef = static_cast< float >(std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now() - trace.startTime)).count());;
	_traceStack.pop_back();
}
