/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// The mod's F1 help window. When a help text lists more than five commands, F1 opens this instead of
// speaking it: one command per row, arrowed through like any list, drawn like a native window so a
// sighted helper can follow. Escape or F1 closes it and the keyboard goes back where it was.

#include <algorithm>
#include <openrct2-ui/accessibility/ScreenReader.h>
#include <openrct2-ui/accessibility/graph/GraphBuilder.h>
#include <openrct2-ui/accessibility/graph/GraphScreens.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/interface/Window.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/drawing/ColourMap.h>
#include <openrct2/drawing/Drawing.String.h>
#include <openrct2/drawing/Font.h>
#include <openrct2/drawing/Rectangle.h>
#include <openrct2/drawing/RenderTarget.h>
#include <openrct2/drawing/Text.h>
#include <openrct2/ui/WindowManager.h>
#include <string>
#include <vector>

using namespace OpenRCT2::Drawing;

namespace OpenRCT2::Ui::Windows
{
    static constexpr ScreenSize kWindowSize = { 440, 280 };
    static constexpr int32_t kMaxSpokenCommands = 5;
    static constexpr int32_t kRowPadding = 3;

    enum WindowAccessibilityHelpWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_LIST,
    };

    // clang-format off
    static constexpr auto kWidgets = makeWidgets(
        makeWindowShim(kStringIdNone, kWindowSize),
        makeWidget({ 3, kTitleHeightNormal + 3 }, { kWindowSize.width - 6, kWindowSize.height - kTitleHeightNormal - 6 }, WidgetType::scroll, WindowColour::secondary, SCROLL_VERTICAL) // rows
    );
    // clang-format on

    // A help text split into rows: one per command ("Key: what it does."), with any sentence that
    // follows a command kept on its row ("Enter: Places the ride for viewing. Pressing enter again
    // will build it."). A sentence before the first command is a description row of its own - or,
    // when asked for, the text's name ("Map.", "Placing scenery."), which becomes the window title.
    struct HelpRows
    {
        std::string name;
        std::vector<std::string> rows;
        int32_t commands = 0;
    };

    static HelpRows SplitHelpText(const std::string& text, bool takeName)
    {
        // Sentences end at ". " before a capital letter or a digit ("Shift + 1 - 0: ...").
        std::vector<std::string> sentences;
        size_t start = 0;
        for (size_t i = 0; i + 2 < text.size(); i++)
        {
            const char next = text[i + 2];
            if (text[i] == '.' && text[i + 1] == ' ' && ((next >= 'A' && next <= 'Z') || (next >= '0' && next <= '9')))
            {
                sentences.push_back(text.substr(start, i + 1 - start));
                start = i + 2;
            }
        }
        if (start < text.size())
            sentences.push_back(text.substr(start));

        HelpRows out;
        bool lastWasCommand = false;
        for (auto& sentence : sentences)
        {
            // A command is "Key: ...". A page's description ("Generator page: the map's size...") has
            // the same shape but names no key.
            const auto colon = sentence.find(": ");
            const bool isCommand = colon != std::string::npos
                && !(colon >= 4 && sentence.compare(colon - 4, 4, "page") == 0);
            if (isCommand)
            {
                out.rows.push_back(sentence);
                out.commands++;
                lastWasCommand = true;
            }
            else if (lastWasCommand)
            {
                out.rows.back() += " " + sentence;
            }
            else if (takeName && out.name.empty() && out.rows.empty())
            {
                out.name = sentence.back() == '.' ? sentence.substr(0, sentence.size() - 1) : sentence;
            }
            else
            {
                out.rows.push_back(sentence);
            }
        }
        return out;
    }

    class AccessibilityHelpWindow final : public Window
    {
        std::string _title;
        std::vector<std::string> _rows;
        std::vector<int32_t> _rowHeights;
        int32_t _focusedRow = 0;
        int32_t _scrolledTo = -1;

        int32_t TextWidth() const
        {
            return widgets[WIDX_LIST].width() - kScrollBarWidth - 8;
        }

        const std::vector<int32_t>& RowHeights()
        {
            if (_rowHeights.size() != _rows.size())
            {
                _rowHeights.clear();
                for (const auto& row : _rows)
                {
                    int32_t lines = 0;
                    wrapString(row, TextWidth(), FontStyle::medium, nullptr, &lines);
                    _rowHeights.push_back((lines + 1) * FontGetLineHeight(FontStyle::medium) + kRowPadding);
                }
            }
            return _rowHeights;
        }

        int32_t RowTop(int32_t row)
        {
            int32_t top = 0;
            for (int32_t i = 0; i < row; i++)
                top += RowHeights()[i];
            return top;
        }

        // The focused row's box on screen, for the focus highlight. Also where the list learns which
        // row has the keyboard: it draws that row highlighted, and scrolls it into view when focus
        // first arrives - only then, so a helper scrolling with the mouse is not pulled back.
        Accessibility::Graph::GraphRect FocusRow(int32_t row)
        {
            const auto& list = widgets[WIDX_LIST];
            const int32_t viewHeight = list.height() - 2;
            const int32_t top = RowTop(row);
            const int32_t rowHeight = RowHeights()[row];
            if (_focusedRow != row || _scrolledTo != row)
            {
                _focusedRow = row;
                _scrolledTo = row;
                auto& scroll = scrolls[0];
                if (top < scroll.contentOffsetY)
                    scroll.contentOffsetY = top;
                else if (top + rowHeight > scroll.contentOffsetY + viewHeight)
                    scroll.contentOffsetY = std::max(0, top + rowHeight - viewHeight);
                widgetScrollUpdateThumbs(*this, WIDX_LIST);
                invalidate();
            }

            const int32_t viewTop = windowPos.y + list.top + 1;
            const int32_t rowTop = viewTop + top - scrolls[0].contentOffsetY;
            const int32_t clippedTop = std::max(rowTop, viewTop);
            const int32_t clippedBottom = std::min(rowTop + rowHeight, viewTop + viewHeight);
            const int32_t left = windowPos.x + list.left + 1;
            const int32_t boxWidth = list.width() - kScrollBarWidth - 2;
            if (clippedBottom <= clippedTop)
                return { left, viewTop, boxWidth, viewHeight };
            return { left, clippedTop, boxWidth, clippedBottom - clippedTop };
        }

    public:
        void setContent(std::string title, std::vector<std::string> rows)
        {
            _title = std::move(title);
            _rows = std::move(rows);
            _rowHeights.clear();
            _focusedRow = 0;
            _scrolledTo = -1;
            widgets[WIDX_TITLE].setString(_title.c_str());
            scrolls[0].contentOffsetY = 0;
            initScrollWidgets();
            invalidate();
        }

        const std::string& title() const
        {
            return _title;
        }

        void onOpen() override
        {
            setWidgets(kWidgets);
            initScrollWidgets();
        }

        void onMouseUp(WidgetIndex widgetIndex) override
        {
            if (widgetIndex == WIDX_CLOSE)
                close();
        }

        ScreenSize onScrollGetSize(int32_t /*scrollIndex*/) override
        {
            return { 0, RowTop(static_cast<int32_t>(_rows.size())) };
        }

        void onDraw(RenderTarget& rt) override
        {
            drawWidgets(rt);
        }

        void onScrollDraw(int32_t /*scrollIndex*/, RenderTarget& rt) override
        {
            auto rtCoords = ScreenCoordsXY{ rt.x, rt.y };
            Rectangle::fill(
                rt, { rtCoords, rtCoords + ScreenCoordsXY{ rt.width, rt.height } }, getColourMap(colours[1].colour).midLight);

            int32_t y = 0;
            const auto& heights = RowHeights();
            for (size_t i = 0; i < _rows.size(); i++)
            {
                if (static_cast<int32_t>(i) == _focusedRow)
                    Rectangle::filter(rt, { 0, y, TextWidth() + 6, y + heights[i] - 1 }, FilterPaletteID::paletteDarken1);
                drawTextWrapped(rt, { 3, y + 1 }, TextWidth(), _rows[i], { Colour::black });
                y += heights[i];
            }
        }

        // One list of rows. Enter does nothing; Escape and F1 close (see the screen below).
        void BuildAccessGraph(Accessibility::Graph::GraphBuilder& b)
        {
            using namespace Accessibility::Graph;
            for (size_t i = 0; i < _rows.size(); i++)
            {
                NodeVtable vt;
                vt.announcements.push_back(NodeAnnouncement::Static(_rows[i]));
                vt.focusRect = [this, row = static_cast<int32_t>(i)]() {
                    return std::optional<GraphRect>(FocusRow(row));
                };
                b.AddItem(ControlId::Structural("help:" + std::to_string(i)), std::move(vt));
            }
        }
    };

    bool AccessibilityHelpOpenIfLong(const std::string& title, const std::string& text, std::vector<std::string> leadRows)
    {
        auto split = SplitHelpText(text, title.empty());
        if (split.commands <= kMaxSpokenCommands)
            return false;

        std::string name = !title.empty() ? title : split.name;
        name = name.empty() ? std::string("Help") : name + " help";
        leadRows.insert(leadRows.end(), split.rows.begin(), split.rows.end());

        auto* windowMgr = GetWindowManager();
        if (windowMgr == nullptr)
            return false;
        auto* w = windowMgr->FocusOrCreate<AccessibilityHelpWindow>(
            WindowClass::accessibilityHelp, kWindowSize, WindowFlag::centreScreen);
        if (w == nullptr)
            return false;
        static_cast<AccessibilityHelpWindow*>(w)->setContent(std::move(name), std::move(leadRows));
        return true;
    }

    void RegisterAccessibilityHelpGraphScreen()
    {
        using namespace Accessibility::Graph;
        GraphScreen screen;
        screen.windowClass = WindowClass::accessibilityHelp;
        screen.screenName = []() {
            auto* windowMgr = GetWindowManager();
            auto* w = windowMgr != nullptr ? windowMgr->FindByClass(WindowClass::accessibilityHelp) : nullptr;
            return w != nullptr ? static_cast<AccessibilityHelpWindow*>(w)->title() : std::string();
        };
        screen.build = [](GraphBuilder& b, WindowBase& w) { static_cast<AccessibilityHelpWindow&>(w).BuildAccessGraph(b); };
        screen.f1Closes = true;
        screen.holdsEveryKey = true;
        RegisterGraphScreen(std::move(screen));
    }
} // namespace OpenRCT2::Ui::Windows
