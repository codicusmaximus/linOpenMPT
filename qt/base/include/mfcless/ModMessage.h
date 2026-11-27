// mfcless base: OpenMPT's custom window messages (WM_MOD_*, VIEWMSG_*,
// CTRLMSG_* in mptrack/WindowMessages.h) as Qt events.
//
// Where upstream posts or sends a window message to a window, the twins and
// the mfcless patches post or send a ModMessage to the QObject that stands
// for the window; its event() hands it to the same handler upstream's
// message map names.

#pragma once

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <cstdint>

namespace mfcless
{

class ModMessage : public QEvent
{
public:
	static QEvent::Type EventType()
	{
		static const QEvent::Type type = static_cast<QEvent::Type>(QEvent::registerEventType());
		return type;
	}

	ModMessage(uint32_t msg, uintptr_t wParam, intptr_t lParam)
		: QEvent(EventType()), message(msg), wParam(wParam), lParam(lParam) { }

	uint32_t message;
	uintptr_t wParam;
	intptr_t lParam;
	// Set by the receiver for SendModMessage (a window procedure's result).
	intptr_t result = 0;
};

// Queued, like PostMessage. Does nothing without a target.
inline void PostModMessage(QObject *target, uint32_t msg, uintptr_t wParam = 0, intptr_t lParam = 0)
{
	if(target)
		QCoreApplication::postEvent(target, new ModMessage(msg, wParam, lParam));
}

// Immediate, like SendMessage, on the calling thread. Returns the receiver's
// result.
inline intptr_t SendModMessage(QObject *target, uint32_t msg, uintptr_t wParam = 0, intptr_t lParam = 0)
{
	if(!target)
		return 0;
	ModMessage message(msg, wParam, lParam);
	QCoreApplication::sendEvent(target, &message);
	return message.result;
}

} // namespace mfcless
