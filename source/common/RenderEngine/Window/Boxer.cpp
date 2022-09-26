#include "Boxer.h"

using namespace RenderEngine;

#ifdef __linux__
#if 0
#include <gtk/gtk.h>

namespace {

GtkMessageType getMessageType(Boxer::Style style) {
   switch (style) {
      case Boxer::Style::Info:
         return GTK_MESSAGE_INFO;
      case Boxer::Style::Warning:
         return GTK_MESSAGE_WARNING;
      case Boxer::Style::Error:
         return GTK_MESSAGE_ERROR;
      case Boxer::Style::Question:
         return GTK_MESSAGE_QUESTION;
      default:
         return GTK_MESSAGE_INFO;
   }
}

GtkButtonsType getButtonsType(Boxer::Buttons buttons) {
   switch (buttons) {
      case Boxer::Buttons::OK:
         return GTK_BUTTONS_OK;
      case Boxer::Buttons::OKCancel:
         return GTK_BUTTONS_OK_CANCEL;
      case Boxer::Buttons::YesNo:
         return GTK_BUTTONS_YES_NO;
      default:
         return GTK_BUTTONS_OK;
   }
}

Boxer::Selection getSelection(gint response) {
   switch (response) {
      case GTK_RESPONSE_OK:
         return Boxer::Selection::OK;
      case GTK_RESPONSE_CANCEL:
         return Boxer::Selection::Cancel;
      case GTK_RESPONSE_YES:
         return Boxer::Selection::Yes;
      case GTK_RESPONSE_NO:
         return Boxer::Selection::No;
      default:
         return Boxer::Selection::None;
   }
}

} // namespace
#endif
#elif defined _WIN32

#include <windows.h>

namespace {

    UINT getIcon(Boxer::Style style) {
        switch (style) {
            case Boxer::Style::Info:
                return MB_ICONINFORMATION;
            case Boxer::Style::Warning:
                return MB_ICONWARNING;
            case Boxer::Style::Error:
                return MB_ICONERROR;
            case Boxer::Style::Question:
                return MB_ICONQUESTION;
            default:
                return MB_ICONINFORMATION;
        }
    }

    UINT getButtons(Boxer::Buttons buttons) {
        switch (buttons) {
            case Boxer::Buttons::OK:
                return MB_OK;
            case Boxer::Buttons::OKCancel:
                return MB_OKCANCEL;
            case Boxer::Buttons::YesNo:
                return MB_YESNO;
            default:
                return MB_OK;
        }
    }

    Boxer::Selection getSelection(int response) {
        switch (response) {
            case IDOK:
                return Boxer::Selection::OK;
            case IDCANCEL:
                return Boxer::Selection::Cancel;
            case IDYES:
                return Boxer::Selection::Yes;
            case IDNO:
                return Boxer::Selection::No;
            default:
                return Boxer::Selection::None;
        }
    }

} // namespace
#else

#error "unsupported platform"

#endif
RenderEngine::Boxer::Selection
RenderEngine::Boxer::show(std::string_view const& message, std::string_view const& title, RenderEngine::Boxer::Style style,
                          RenderEngine::Boxer::Buttons buttons) {
#ifdef __linux__
#if 0
    if (!gtk_init_check(0, NULL)) {
      return Boxer::Selection::None;
   }

   GtkWidget *dialog = gtk_message_dialog_new(NULL,
                                              GTK_DIALOG_MODAL,
                                              getMessageType(style),
                                              getButtonsType(buttons),
                                              "%s",
                                              message.data());
   gtk_window_set_title(GTK_WINDOW(dialog), title.data());
   Selection selection = getSelection(gtk_dialog_run(GTK_DIALOG(dialog)));

   gtk_widget_destroy(GTK_WIDGET(dialog));
   while (g_main_context_iteration(NULL, false));

   return selection;
#endif
#elif defined _WIN32
    UINT flags = MB_TASKMODAL;

    flags |= getIcon(style);
    flags |= getButtons(buttons);

    return getSelection(MessageBox(NULL, message.data(), title.data(), flags));
#endif
    return Selection::NONE;
}
