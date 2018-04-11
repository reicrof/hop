#ifndef MODAL_WINDOW_H_
#define MODAL_WINDOW_H_

namespace hop
{
   // Open the modal window
   void displayModalWindow( const char* message, bool closeable );

   // Does the actual rendering of the modal window
   void renderModalWindow();

   // Close the modal window
   void closeModalWindow();
}

#endif // MODAL_WINDOW_H_