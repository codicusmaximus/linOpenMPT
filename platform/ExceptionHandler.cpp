// linOpenMPT platform layer: the Linux side of upstream's crash handling
// (mptrack/ExceptionHandler.h). Upstream writes minidumps with dbghelp and
// saves the open modules next to them; here a fatal signal or an unhandled
// C++ exception is reported on stderr, with the description of whatever the
// tracker was doing (ExceptionHandler::Context), and the process then dies
// as it would have. Assertions are reported the same way.
//
// Shared by the application and the headless tools.

#include "stdafx.h"
#include "ExceptionHandler.h"

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>

#include <execinfo.h>
#include <unistd.h>

OPENMPT_NAMESPACE_BEGIN

bool ExceptionHandler::fullMemDump = false;
bool ExceptionHandler::stopSoundDeviceOnCrash = true;
bool ExceptionHandler::stopSoundDeviceBeforeDump = false;
bool ExceptionHandler::delegateToWindowsHandler = false;
bool ExceptionHandler::debugExceptionHandler = false;
bool ExceptionHandler::useAnyCrashHandler = true;
bool ExceptionHandler::useImplicitFallbackSEH = false;
bool ExceptionHandler::useExplicitSEH = false;
bool ExceptionHandler::handleStdTerminate = true;
bool ExceptionHandler::handleStdUnexpected = true;
bool ExceptionHandler::handleMfcExceptions = true;

namespace
{

thread_local ExceptionHandler::Context *g_context = nullptr;
std::terminate_handler g_oldTerminate = nullptr;
bool g_registered = false;
bool g_systemHandlers = false;
const int g_fatalSignals[] = {SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT};
struct sigaction g_oldActions[std::size(g_fatalSignals)];

// Async-signal-safe output.
void WriteRaw(const char *text)
{
	if(text != nullptr)
		[[maybe_unused]] auto written = ::write(STDERR_FILENO, text, std::strlen(text));
}

void OnFatalSignal(int signal)
{
	WriteRaw("linOpenMPT: crashed (");
	WriteRaw(strsignal(signal));
	WriteRaw(")");
	if(g_context != nullptr)
	{
		// Converting allocates, which is not async-signal-safe; the process
		// is going down anyway, and the description is worth the risk.
		WriteRaw(" while: ");
		WriteRaw(mpt::ToCharset(mpt::Charset::UTF8, g_context->description).c_str());
	}
	WriteRaw("\n");
	// Where it happened, for bug reports (addr2line or gdb turn the
	// addresses into source lines).
	void *frames[64];
	const int count = ::backtrace(frames, 64);
	::backtrace_symbols_fd(frames, count, STDERR_FILENO);
	// Back to the default action, so the process dies (and dumps core) as
	// it would have without the handler.
	std::signal(signal, SIG_DFL);
	std::raise(signal);
}

void OnTerminate()
{
	std::cerr << "linOpenMPT: unhandled exception";
	if(std::exception_ptr e = std::current_exception())
	{
		try
		{
			std::rethrow_exception(e);
		} catch(const std::exception &ex)
		{
			std::cerr << ": " << ex.what();
		} catch(...)
		{
		}
	}
	if(g_context != nullptr)
		std::cerr << " while: " << mpt::ToCharset(mpt::Charset::UTF8, g_context->description);
	std::cerr << std::endl;
	if(g_oldTerminate != nullptr)
		g_oldTerminate();
	std::abort();
}

} // namespace

void ExceptionHandler::Register()
{
	if(g_registered || !useAnyCrashHandler)
		return;
	g_registered = true;
	if(handleStdTerminate)
		g_oldTerminate = std::set_terminate(OnTerminate);
}

void ExceptionHandler::ConfigureSystemHandler()
{
	if(g_systemHandlers || !useAnyCrashHandler || debugExceptionHandler)
		return;
	g_systemHandlers = true;
	struct sigaction action{};
	action.sa_handler = OnFatalSignal;
	sigemptyset(&action.sa_mask);
	action.sa_flags = SA_RESETHAND;
	for(std::size_t i = 0; i < std::size(g_fatalSignals); i++)
		sigaction(g_fatalSignals[i], &action, &g_oldActions[i]);
}

void ExceptionHandler::UnconfigureSystemHandler()
{
	if(!g_systemHandlers)
		return;
	g_systemHandlers = false;
	for(std::size_t i = 0; i < std::size(g_fatalSignals); i++)
		sigaction(g_fatalSignals[i], &g_oldActions[i], nullptr);
}

void ExceptionHandler::Unregister()
{
	if(!g_registered)
		return;
	g_registered = false;
	if(handleStdTerminate)
		std::set_terminate(g_oldTerminate);
	g_oldTerminate = nullptr;
}

void ExceptionHandler::TaintProcess(TaintReason)
{
}

ExceptionHandler::Context *ExceptionHandler::SetContext(Context *newContext) noexcept
{
	Context *old = g_context;
	g_context = newContext;
	return old;
}

void ExceptionHandler::UnhandledMFCException(CException *, const MSG *)
{
	// MFC's message pump caught an exception a handler threw. Upstream
	// treats it as a crash; here it is reported and the pump goes on, as MFC
	// itself does without a handler.
	std::cerr << "linOpenMPT: unhandled MFC exception";
	if(g_context != nullptr)
		std::cerr << " while: " << mpt::ToCharset(mpt::Charset::UTF8, g_context->description);
	std::cerr << std::endl;
}

MPT_NOINLINE void AssertHandler(const mpt::source_location &loc, const char *expr, const char *msg)
{
	std::cerr << "linOpenMPT: assertion failed: " << (expr ? expr : "") << " at "
	          << (loc.file_name() ? loc.file_name() : "?") << ":" << loc.line();
	if(msg)
		std::cerr << " (" << msg << ")";
	std::cerr << std::endl;
	if(!msg)
		std::abort();
}

void DebugTraceDump()
{
}

OPENMPT_NAMESPACE_END
