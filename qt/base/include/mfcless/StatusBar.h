/*
 * StatusBar.h
 * -----------
 * Purpose: The main window's status bar as upstream fills it: a message pane
 *          and indicator panes, each known by a command ID (its string from
 *          the string table is its first text) and set by index.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "mfcless/Text.h"
#include "mfcless/Window.h"

#include <QtWidgets/QStatusBar>

#include <cstdint>
#include <vector>

class QLabel;
class QMainWindow;

namespace mfcless
{

namespace pane
{
	inline constexpr uint32_t NoBorders = 0x0100, PopOut = 0x0200, Disabled = 0x04000000, Stretch = 0x08000000, Normal = 0;
}

class StatusBar : public Window<QStatusBar>
{
	Q_OBJECT
public:
	explicit StatusBar(QWidget *parent = nullptr);

	// Becomes the status bar of the main window.
	bool Create(QMainWindow *parent);
	// Panes for these command IDs; 0 (ID_SEPARATOR) is the message pane.
	bool SetIndicators(const uint32_t *ids, int count);
	int CommandToIndex(uint32_t id) const;
	uint32_t GetItemID(int index) const;
	void SetPaneInfo(int index, uint32_t id, uint32_t style, int width);
	bool SetPaneText(int index, const Text &text, bool update = true);
	QString GetPaneText(int index) const;
	// Asks target for the state of each pane's command (its ON_UPDATE_COMMAND_UI
	// handler sets the pane's text), as MFC does when the program is idle.
	void UpdatePanes(MessageMap *target);

protected:
	struct PaneInfo
	{
		uint32_t id = 0;
		QLabel *label = nullptr;
	};
	std::vector<PaneInfo> m_panes;
};

} // namespace mfcless
