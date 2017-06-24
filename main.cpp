#include <GLFW/glfw3.h>

#include <stdio.h>
#include "imdbg.h"

#include <cmath>

int main( void )
{
   GLFWwindow* window;

   /* Initialize the library */
   if ( !glfwInit() ) return -1;

   /* Create a windowed mode window and its OpenGL context */
   window = glfwCreateWindow( 640, 480, "Hello World", NULL, NULL );
   if ( !window )
   {
      glfwTerminate();
      return -1;
   }

   /* Make the window's context current */
   glfwMakeContextCurrent( window );

   imdbg::init();

   bool show_test_window = true;
   bool show_another_window = false;
   ImVec4 clear_color = ImColor( 114, 144, 154 );

   imdbg::Profiler* prof = imdbg::newProfiler( "My Profiler" );

   /* Loop until the user closes the window */
   while ( !glfwWindowShouldClose( window ) )
   {
      prof->pushTrace( "Main" );
      int w, h;
      glfwGetWindowSize( window, &w, &h );
      double mouseX, mouseY;
      glfwGetCursorPos( window, &mouseX, &mouseY );
      imdbg::onNewFrame( w, h, (int)mouseX, (int)mouseY, glfwGetMouseButton( window, 0 ), glfwGetMouseButton( window, 1 ) );
      /* Poll for and process events */
      glfwPollEvents();

      ImGui::ShowTestWindow( &show_test_window );
      
      if( mouseX > 500 )
      {
         prof->pushTrace( "A trace" );
         prof->popTrace();

         prof->pushTrace( "Another one" );
         prof->popTrace();
      }
      prof->popTrace();

      // Rendering   
      int display_w, display_h;
      glfwGetFramebufferSize( window, &display_w, &display_h );
      glViewport( 0, 0, display_w, display_h );
      glClearColor( clear_color.x, clear_color.y, clear_color.z, clear_color.w );
      glClear( GL_COLOR_BUFFER_BIT );

      imdbg::draw();

      /* Swap front and back buffers */
      glfwSwapBuffers( window );
   }

   glfwTerminate();
   return 0;
}