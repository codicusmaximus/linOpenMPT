/*
 * ForeignWindows.h
 * ----------------
 * Purpose: Other programs' and libraries' windows inside ours: plugin
 *          editors. A Linux VST2 plugin opens its editor as a child of an
 *          X11 window it is given (effEditOpen); a Windows plugin's editor,
 *          run by the Wine bridge, is an X11 window that gets put into ours.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include <cstdint>

class QWidget;

namespace mfcless
{

// Only X11 (also XWayland) has window ids that another toolkit in the
// process can parent its windows to.
bool CanEmbedForeignWindows();
// The X11 window of a widget, for a plugin to open its editor in (0 if
// there is none). The widget gets a native window of its own.
uintptr_t NativeWindowId(QWidget *w);
// Puts the X11 window child into parent and keeps it at parent's top left;
// moved hears where parent is on the screen (for Wine, which places its
// windows itself).
bool EmbedX11Window(uintptr_t parent, uintptr_t child, void (*moved)(void *context, int x, int y), void *context);
void UnembedX11Window(uintptr_t child);
// Runs fn on the UI thread, later.
void PostToMainThread(void (*fn)(void *context), void *context);

} // namespace mfcless
