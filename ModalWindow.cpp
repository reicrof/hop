#include "ModalWindow.h"
#include "imgui/imgui.h"

#include <mutex>

static std::mutex modalWindowLock;

static bool modalWindowOpen = false;
static hop::ModalType modalType = hop::MODAL_TYPE_CLOSE;
static bool shouldCloseModalWindow = false;
static const char* modalWindowMessage = nullptr;
static std::function<void()> modalFct;

namespace hop
{
   void renderModalWindow()
   {
      bool isOpen, shouldClose;
      hop::ModalType type;
      const char* localMsg;
      {
         std::lock_guard<std::mutex> g( modalWindowLock );
         isOpen = modalWindowOpen;
         type = modalType;
         shouldClose = shouldCloseModalWindow;
         localMsg = modalWindowMessage;
      }

      if ( isOpen )
      {
         const bool enterPressed = ImGui::IsKeyPressed( ImGuiKey_Enter );
         switch ( type )
         {
            case MODAL_TYPE_NO_CLOSE:
            {
               ImGui::OpenPopup( localMsg );
               if ( ImGui::BeginPopupModal( localMsg, NULL, ImGuiWindowFlags_AlwaysAutoResize ) )
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
                     if( modalFct )
                     {
                        modalFct();
                        modalFct = nullptr;
                     }
                  }
                  ImGui::EndPopup();
                  break;
               }
               case MODAL_TYPE_CLOSE:
               {
                  ImGui::OpenPopup( modalWindowMessage );
                  if ( ImGui::BeginPopupModal(
                           modalWindowMessage, NULL, ImGuiWindowFlags_AlwaysAutoResize ) )
                  {
                     if ( ImGui::Button( "Close", ImVec2( 120, 0 ) ) || enterPressed )
                     {
                        ImGui::CloseCurrentPopup();
                        std::lock_guard<std::mutex> g( modalWindowLock );
                        modalWindowMessage = nullptr;
                        modalWindowOpen = false;
                        shouldCloseModalWindow = false;
                        if( modalFct )
                        {
                           modalFct();
                           modalFct = nullptr;
                        }
                     }
                     ImGui::EndPopup();
                  }
                  break;
               }
               case MODAL_TYPE_YES_NO:
               {
                  ImGui::OpenPopup( modalWindowMessage );
                  if ( ImGui::BeginPopupModal(
                           modalWindowMessage, NULL, ImGuiWindowFlags_AlwaysAutoResize ) )
                  {
                     bool closing = false;
                     bool execCallback = false;
                     if ( ImGui::Button( "Yes", ImVec2( 120, 0 ) ) || enterPressed )
                     {
                        closing = true;
                        execCallback = true;
                     }
                     else if( ImGui::SameLine(), ImGui::Button( "No", ImVec2( 120, 0 ) ) )
                     {
                        closing = true;
                     }
                     ImGui::EndPopup();

                     if( closing )
                     {
                        ImGui::CloseCurrentPopup();
                        std::lock_guard<std::mutex> g( modalWindowLock );
                        modalWindowMessage = nullptr;
                        modalWindowOpen = false;
                        shouldCloseModalWindow = false;
                     }
                     if( execCallback && modalFct )
                     {
                        modalFct();
                        modalFct = nullptr;
                     }
                  }
                  break;
               }
            }
         }
      }
   }

   void displayModalWindow( const char* message, ModalType type )
   {
      std::lock_guard<std::mutex> g( modalWindowLock );

      // Call to display modal window while it is already open
      assert( modalWindowOpen == false && shouldCloseModalWindow != true );

      modalWindowOpen = true;
      modalType = type;
      shouldCloseModalWindow = false;
      modalWindowMessage = message;
   }

   void displayModalWindow( const char* message, ModalType type, std::function<void()> fctToExec )
   {
      // Copy callback and call other overload
      {
         std::lock_guard<std::mutex> g( modalWindowLock );
         modalFct = fctToExec;
      }

      displayModalWindow( message, type );
   }

   bool modalWindowShowing()
   {
      return modalWindowOpen;
   }

   void closeModalWindow()
   {
      std::lock_guard<std::mutex> g( modalWindowLock );

      assert( modalWindowOpen == true );

      shouldCloseModalWindow = true;
   }
}