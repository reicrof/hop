#include "ModalWindow.h"
#include "imgui/imgui.h"

#include <mutex>

static std::mutex modalWindowLock;

static hop::ModalType modalType = hop::MODAL_TYPE_ERROR;
static bool modalWindowOpen = false;
static bool shouldOpenModalWindow = false;
static bool shouldCloseModalWindow = false;
static const char* modalWindowMessage = nullptr;

// Callbacks
static std::function<void()> noArgModalFct;
static std::function<void(const char*)> stringModalFct;

namespace hop
{
void renderModalWindow()
{
   bool isOpen, shouldClose;
   hop::ModalType type;
   const char* localMsg;
   {
      std::lock_guard<std::mutex> g( modalWindowLock );
      if ( shouldOpenModalWindow )
      {
         modalWindowOpen = true;
         shouldOpenModalWindow = false;
         return;  // Next time around, we will display the modal window
      }

      type = modalType;
      isOpen = modalWindowOpen;
      localMsg = modalWindowMessage;
      shouldClose = shouldCloseModalWindow;

      // Let's reset the shared state here while being protected
      shouldCloseModalWindow = false;
   }

   if ( isOpen )
   {
      const bool enterPressed = ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_Enter ), false );
      const bool escPressed = ImGui::IsKeyPressed( ImGui::GetKeyIndex( ImGuiKey_Escape ), false );
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
                  if ( noArgModalFct )
                  {
                     noArgModalFct();
                     noArgModalFct = nullptr;
                  }
               }
               ImGui::EndPopup();
               break;
            }
            case MODAL_TYPE_ERROR:
            {
               ImGui::OpenPopup( "Error!" );
               if ( ImGui::BeginPopupModal(
                        "Error!", NULL, ImGuiWindowFlags_AlwaysAutoResize ) )
               {
                  ImGui::Text( "%s", modalWindowMessage );
                  ImGui::Separator();
                  if ( ImGui::Button( "Close", ImVec2( 120, 0 ) ) || enterPressed )
                  {
                     ImGui::CloseCurrentPopup();
                     std::lock_guard<std::mutex> g( modalWindowLock );
                     modalWindowMessage = nullptr;
                     modalWindowOpen = false;
                     if ( noArgModalFct )
                     {
                        noArgModalFct();
                        noArgModalFct = nullptr;
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
                  else if (
                      ImGui::SameLine(), ImGui::Button( "No", ImVec2( 120, 0 ) ) || escPressed )
                  {
                     closing = true;
                  }
                  ImGui::EndPopup();

                  if ( closing )
                  {
                     ImGui::CloseCurrentPopup();
                     std::lock_guard<std::mutex> g( modalWindowLock );
                     modalWindowMessage = nullptr;
                     modalWindowOpen = false;
                  }
                  if ( execCallback && noArgModalFct )
                  {
                     noArgModalFct();
                     noArgModalFct = nullptr;
                  }
               }
               break;
            }
            case MODAL_TYPE_STRING_OK_CANCEL:
            {
               static char textField[512] = {};
               ImGui::OpenPopup( modalWindowMessage );
               if ( ImGui::BeginPopupModal(
                        modalWindowMessage, NULL, ImGuiWindowFlags_AlwaysAutoResize ) )
               {
                  bool closing = false;
                  bool execCallback = false;

                  // Set focus on next input text
                  if( ImGui::IsWindowFocused() && !ImGui::IsAnyItemActive() )
                     ImGui::SetKeyboardFocusHere();

                  const bool textEnterPressed = ImGui::InputText(
                      "",
                      textField,
                      sizeof( textField ),
                      ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue );

                  ImGui::Separator();

                  if ( ImGui::Button( "Ok", ImVec2( 120, 0 ) ) || textEnterPressed )
                  {
                     closing = true;
                     execCallback = true;
                  }
                  else if (
                      ImGui::SameLine(), ImGui::Button( "Cancel", ImVec2( 120, 0 ) ) || escPressed )
                  {
                     closing = true;
                  }
                  ImGui::EndPopup();

                  if ( closing )
                  {
                     ImGui::CloseCurrentPopup();
                     std::lock_guard<std::mutex> g( modalWindowLock );
                     modalWindowMessage = nullptr;
                     modalWindowOpen = false;
                  }
                  if ( execCallback && stringModalFct )
                  {
                     stringModalFct( textField );
                     stringModalFct = nullptr;
                     memset( textField, 0, sizeof( textField ) );
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
   assert( modalWindowOpen == false && shouldCloseModalWindow == false );

   modalType = type;
   shouldOpenModalWindow = true;
   shouldCloseModalWindow = false;
   modalWindowMessage = message;
}

void displayModalWindow( const char* message, ModalType type, std::function<void()> fctToExec )
{
   // Copy callback and call other overload
   {
      std::lock_guard<std::mutex> g( modalWindowLock );
      noArgModalFct = fctToExec;
   }

   assert( type != MODAL_TYPE_STRING_OK_CANCEL );
   displayModalWindow( message, type );
}

void displayStringInputModalWindow(
    const char* message,
    std::function<void( const char* )> fctToExec )
{
   // Copy callback and call other overload
   {
      std::lock_guard<std::mutex> g( modalWindowLock );
      stringModalFct = fctToExec;
   }

   displayModalWindow( message, MODAL_TYPE_STRING_OK_CANCEL );
}

bool modalWindowShowing() { return modalWindowOpen || shouldOpenModalWindow; }

void closeModalWindow()
{
   std::lock_guard<std::mutex> g( modalWindowLock );

   assert( modalWindowOpen == true );

   shouldCloseModalWindow = true;
}
}  // namespace hop