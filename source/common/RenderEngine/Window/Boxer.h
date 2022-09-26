#ifndef TESTAPP_BOXER_H
#define TESTAPP_BOXER_H

#include <string_view>

namespace RenderEngine{

    class Boxer{
    public:
        enum class Style {
            Info,
            Warning,
            Error,
            Question
        };

        enum class Buttons {
            OK,
            OKCancel,
            YesNo
        };

        enum class Selection {
            OK,
            Cancel,
            Yes,
            No,
            NONE
        };


        static constexpr const Style DEFAULT_STYLE = Style::Info;
        static constexpr const Buttons DEFAULT_BUTTONS = Buttons::OK;

        static Selection show(std::string_view const& message, std::string_view const& title, Style style, Buttons buttons);

        static inline Selection show(std::string_view const& message, std::string_view const& title, Style style) {
            return show(message, title, style, DEFAULT_BUTTONS);
        }

        static inline Selection show(std::string_view const& message, std::string_view const& title, Buttons buttons) {
            return show(message, title, DEFAULT_STYLE, buttons);
        }

        static inline Selection show(std::string_view const& message, std::string_view const& title) {
            return show(message, title, DEFAULT_STYLE, DEFAULT_BUTTONS);
        }

    };
}
#endif //TESTAPP_BOXER_H
