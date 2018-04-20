#ifndef MODAL_WINDOW_H_
#define MODAL_WINDOW_H_

#include <functional>

namespace hop
{
   enum ModalType
   {
      MODAL_TYPE_NO_CLOSE,
      MODAL_TYPE_CLOSE,
      MODAL_TYPE_YES_NO
   };

   // Open the modal window
   void displayModalWindow( const char* message, ModalType type );

   // Open the modal window and set the callback to call if ok was pressed
   void displayModalWindow( const char* message, ModalType type, std::function<void()> fctToExec );

   // Does the actual rendering of the modal window
   void renderModalWindow();

   // Returns wether or not a modal window is open
   bool modalWindowShowing();

   // Close the modal window
   void closeModalWindow();
}

#endif // MODAL_WINDOW_H_