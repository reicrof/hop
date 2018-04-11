#include "ModalWindow.h"
#include "imgui/imgui.h"

#include <mutex>

static std::mutex modalWindowLock;

static bool modalWindowOpen = false;
static bool closeableModalWindow = false;
static bool shouldCloseModalWindow = false;
static const char* modalWindowMessage = nullptr;

namespace hop
{
void renderModalWindow()
{
   bool isOpen, isCloseable, shouldClose;
   const char* localMsg;
   {
      std::lock_guard<std::mutex> g( modalWindowLock );
      isOpen = modalWindowOpen;
      isCloseable = closeableModalWindow;
      shouldClose = shouldCloseModalWindow;
      localMsg = modalWindowMessage;
   }

   if ( isOpen )
   {
      if ( isCloseable )
      {
         ImGui::OpenPopup( modalWindowMessage );
         if ( ImGui::BeginPopupModal( modalWindowMessage ) )
         {
            ImGui::Text( "%s", modalWindowMessage );
            if ( ImGui::Button( "Close", ImVec2( 120, 0 ) ) )
            {
               ImGui::CloseCurrentPopup();
               std::lock_guard<std::mutex> g( modalWindowLock );
               modalWindowMessage = nullptr;
               modalWindowOpen = false;
               shouldCloseModalWindow = false;
            }
            ImGui::EndPopup();
         }
      }
      else
      {
         ImGui::OpenPopup( localMsg );
         if ( ImGui::BeginPopupModal( localMsg ) )
         {
            char buf[64];
            sprintf( buf, "%c %s", "|/-\\"[(int)( ImGui::GetTime() / 0.25f ) & 3], localMsg );
            ImGui::Text( "%s", buf );
            if ( shouldClose )
            {
               ImGui::CloseCurrentPopup();
               std::lock_guard<std::mutex> g( modalWindowLock );
               modalWindowMessage = nullptr;
               modalWindowOpen = false;
               shouldCloseModalWindow = false;
            }
            ImGui::EndPopup();
         }
      }
   }
}

   void displayModalWindow( const char* message, bool closeable )
   {
      std::lock_guard<std::mutex> g( modalWindowLock );

      // I don't know how this could happen, but it should not...
      assert( modalWindowOpen == false );

      modalWindowOpen = true;
      closeableModalWindow = closeable;
      shouldCloseModalWindow = false;
      modalWindowMessage = message;
   }

   void closeModalWindow()
   {
      std::lock_guard<std::mutex> g( modalWindowLock );

      assert( modalWindowOpen == true );

      shouldCloseModalWindow = true;
   }
}