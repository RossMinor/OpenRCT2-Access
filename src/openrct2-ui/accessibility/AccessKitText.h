/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <cstddef>
#include <string_view>

namespace OpenRCT2::Ui::Accessibility
{
    // Exposes the game's text fields to the operating system's accessibility API through AccessKit
    // (https://accesskit.dev), so a screen reader reads them the way it reads a real text box.
    //
    // This is a DIFFERENT MODEL from the rest of the mod's speech and solves a problem that model
    // cannot. Prism PUSHES: the mod hands it a string and it is spoken, and the reader learns
    // nothing about what that string was. That is right for the map cursor and the menus, which are
    // not OS controls and never will be. It is wrong for a text field, because the things a reader
    // does with a text field - review it by character or word, show it on a braille display and
    // keep it there, move the caret with a display's routing keys - all need the reader to KNOW the
    // field and its contents, not to be told a sentence about it after the fact. Pushing one
    // keystroke at a time is why a braille display could only ever show the last letter typed.
    //
    // AccessKit EXPOSES instead: the mod publishes a small tree - a window containing a text input
    // containing its text - and the reader queries it. On Windows that surfaces as UI Automation,
    // on macOS NSAccessibility, on Linux AT-SPI, from the same tree.
    //
    // Nothing else in the mod is affected. Prism keeps doing everything it already does.

    // Attaches the platform adapter to the game window.
    //
    // MUST be called while the window is still HIDDEN: the Windows adapter works by subclassing the
    // window procedure, and the library aborts if asked to do that to a window that has already
    // been shown. UiContext::CreateWindow creates the window hidden for this reason and shows it
    // once this has run.
    //
    // Safe to call when the library is missing; the game then simply has no accessible text fields,
    // exactly as it did before, rather than failing to start.
    void AccessKitInit(void* sdlWindow);

    void AccessKitShutdown();

    // A text field has opened. `caretBytes` is a BYTE offset into `text`, as the game's text
    // session stores it; the conversion to the character index the accessibility tree wants happens
    // here, in one place.
    void AccessKitTextFieldOpen(std::string_view label, std::string_view text, size_t caretBytes);

    // The field's contents or caret changed.
    void AccessKitTextFieldUpdate(std::string_view text, size_t caretBytes);

    // The field has closed; focus returns to the window itself.
    void AccessKitTextFieldClose();
} // namespace OpenRCT2::Ui::Accessibility
