// linOpenMPT platform layer: the Linux side of upstream's crash handling
// (mptrack/ExceptionHandler.h). Upstream writes minidumps with dbghelp and
// saves the open modules next to them; here a fatal signal or an unhandled
// C++ exception is reported on stderr, with the description of whatever the
// tracker was doing (ExceptionHandler::Context), and the process then dies
// as it would have. Assertions are reported the same way. Before that, the
// modified modules are saved into a crash folder, as upstream does
// (CrashRescue.h).
//
// Shared by the application and the headless tools.

#include "stdafx.h"
#include "ExceptionHandler.h"
#include "CrashRescue.h"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>

#include <execinfo.h>
#include <spawn.h>
#include <sys/stat.h>
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

linopenmpt::CrashRescue g_rescue = nullptr;
std::atomic<bool> g_rescuing{false};

bool MakeDirectories(const std::string &path)
{
	for(std::size_t slash = path.find('/', 1); slash != std::string::npos; slash = path.find('/', slash + 1))
	{
		const std::string parent = path.substr(0, slash);
		if(::mkdir(parent.c_str(), 0755) != 0 && errno != EEXIST)
			return false;
	}
	return true;
}

// Saves the modified modules into
// $XDG_DATA_HOME/OpenMPT/Crash Files/<date and time>/ (upstream uses the
// temporary folder, which Linux empties at boot). None of this is
// async-signal-safe, as on Windows: the process is going down, and the
// modules are worth the risk. Only once, should saving crash too.
void RescueModules()
{
	if(g_rescue == nullptr || g_rescuing.exchange(true))
		return;
	std::string folder;
	if(const char *data = std::getenv("XDG_DATA_HOME"); data != nullptr && data[0] == '/')
		folder = data;
	else if(const char *home = std::getenv("HOME"); home != nullptr && home[0] == '/')
		folder = std::string(home) + "/.local/share";
	else
		return;
	char timestamp[32] = {};
	const std::time_t now = std::time(nullptr);
	std::tm local{};
	::localtime_r(&now, &local);
	std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H.%M.%S", &local);
	folder += "/OpenMPT/Crash Files/";
	folder += timestamp;
	folder += '/';
	if(!MakeDirectories(folder))
		return;
	const int saved = g_rescue(mpt::PathString::FromUTF8(folder));
	if(saved <= 0)
	{
		::rmdir(folder.c_str());
		return;
	}
	const std::string message = std::to_string(saved) + (saved == 1 ? " modified module was" : " modified modules were")
		+ " rescued to " + folder + ", but it cannot be guaranteed that " + (saved == 1 ? "it is" : "they are") + " still intact.";
	WriteRaw(("linOpenMPT: " + message + "\n").c_str());
	// Started from a desktop, nobody reads stderr: a desktop notification
	// too, where there is one.
	if(std::getenv("DISPLAY") != nullptr || std::getenv("WAYLAND_DISPLAY") != nullptr)
	{
		const std::string body = message;
		char *argv[] = {const_cast<char *>("notify-send"), const_cast<char *>("--urgency=critical"), const_cast<char *>("OpenMPT crashed"), const_cast<char *>(body.c_str()), nullptr};
		pid_t pid = 0;
		::posix_spawnp(&pid, "notify-send", nullptr, nullptr, argv, environ);
	}
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
	RescueModules();
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
	RescueModules();
	if(g_oldTerminate != nullptr)
		g_oldTerminate();
	std::abort();
}

} // namespace

void linopenmpt::SetCrashRescue(CrashRescue rescue)
{
	g_rescue = rescue;
}

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
