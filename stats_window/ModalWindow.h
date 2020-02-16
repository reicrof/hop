#ifndef MODAL_WINDOW_H_
#define MODAL_WINDOW_H_

#include <functional>

namespace hop
{
   enum ModalType
   {
      MODAL_TYPE_NO_CLOSE,
      MODAL_TYPE_ERROR,
      MODAL_TYPE_YES_NO,
      MODAL_TYPE_STRING_OK_CANCEL
   };

   // Open the modal window
   void displayModalWindow( const char* message, ModalType type );

   // Open the modal window and set the callback to call if ok was pressed
   void displayModalWindow( const char* message, ModalType type, std::function<void()> fctToExec );

   // Open the modal window with a text input field and a callback to call with the text
   void displayStringInputModalWindow( const char* message, std::function<void(const char*)> fctToExec );

   // Does the actual rendering of the modal window
   void renderModalWindow();

   // Returns wether or not a modal window is open
   bool modalWindowShowing();

   // Close the modal window
   void closeModalWindow();
}

#endif // MODAL_WINDOW_H_