// linOpenMPT plugin bridge: runs one Windows VST2 plugin for the Linux
// tracker, in a Wine process. A Windows program, built with MinGW.
//   linopenmpt-vstbridge.exe <port> <token> <plugin.dll> [shell plugin id]
// It connects to the tracker on 127.0.0.1:<port> (three connections, see
// protocol.h), loads the plugin and then serves the tracker's calls until
// it says to quit or goes away. The plugin's editor is a Wine window, whose
// X11 window the tracker embeds in its own editor window.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "openmpt/all/BuildSettings.hpp"

namespace vstdefs
{
using int8 = std::int8_t;
using uint8 = std::uint8_t;
using int16 = std::int16_t;
using uint16 = std::uint16_t;
using int32 = std::int32_t;
using uint32 = std::uint32_t;
using int64 = std::int64_t;
using uint64 = std::uint64_t;
#include "mptrack/plugins/VstDefinitions.h"
}  // namespace vstdefs

namespace Vst = vstdefs::Vst;

#include "opcodes.h"

using namespace vstbridge;

namespace
{

// Sockets -------------------------------------------------------------------

struct Channel
{
	SOCKET socket = INVALID_SOCKET;
	std::mutex sendMutex;  // the notify channel is written from any thread
};

Channel g_control, g_audio, g_notify;
std::atomic<bool> g_quit{false};

void Fail(const char *what)
{
	std::fprintf(stderr, "linopenmpt-vstbridge: %s\n", what);
}

// Waits until the socket can be read (or written), for sockets that
// WSAEventSelect made non-blocking.
bool WaitSocket(SOCKET s, bool write)
{
	fd_set set;
	FD_ZERO(&set);
	FD_SET(s, &set);
	return select(0, write ? nullptr : &set, write ? &set : nullptr, nullptr, nullptr) > 0;
}

bool SendAll(SOCKET s, const char *data, std::size_t size)
{
	while(size > 0)
	{
		const int sent = send(s, data, static_cast<int>(std::min<std::size_t>(size, 1 << 20)), 0);
		if(sent == SOCKET_ERROR)
		{
			if(WSAGetLastError() == WSAEWOULDBLOCK && WaitSocket(s, true))
				continue;
			return false;
		}
		data += sent;
		size -= sent;
	}
	return true;
}

bool RecvAll(SOCKET s, char *data, std::size_t size)
{
	while(size > 0)
	{
		const int got = recv(s, data, static_cast<int>(std::min<std::size_t>(size, 1 << 20)), 0);
		if(got == 0)
			return false;
		if(got == SOCKET_ERROR)
		{
			if(WSAGetLastError() == WSAEWOULDBLOCK && WaitSocket(s, false))
				continue;
			return false;
		}
		data += got;
		size -= got;
	}
	return true;
}

bool Send(Channel &channel, const std::vector<char> &message)
{
	std::lock_guard<std::mutex> lock(channel.sendMutex);
	return SendAll(channel.socket, message.data(), message.size());
}

struct Message
{
	MsgType type{};
	std::vector<char> payload;
};

bool Receive(Channel &channel, Message &message)
{
	Header header;
	if(!RecvAll(channel.socket, reinterpret_cast<char *>(&header), sizeof(header)))
		return false;
	message.type = static_cast<MsgType>(header.type);
	message.payload.resize(header.size);
	return header.size == 0 || RecvAll(channel.socket, message.payload.data(), header.size);
}

template <typename T>
bool Read(const Message &message, T &value, std::size_t offset = 0)
{
	if(message.payload.size() < offset + sizeof(T))
		return false;
	std::memcpy(&value, message.payload.data() + offset, sizeof(T));
	return true;
}

// The plugin ----------------------------------------------------------------

HMODULE g_library = nullptr;
Vst::AEffect *g_effect = nullptr;
int32_t g_shellPluginId = 0;
std::atomic<float> g_sampleRate{44100.0f};
std::atomic<int32_t> g_blockSize{1024};
Vst::VstTimeInfo g_lastTimeInfo{};
std::mutex g_timeInfoMutex;

HWND g_editor = nullptr;

// The channel whose request this thread is handling: the plugin's
// callbacks go back on it.
thread_local Channel *t_channel = nullptr;

EffectInfo GetEffectInfo()
{
	EffectInfo info{};
	if(g_effect)
	{
		info.numPrograms = g_effect->numPrograms;
		info.numParams = static_cast<int32_t>(g_effect->numParams);
		info.numInputs = g_effect->numInputs;
		info.numOutputs = g_effect->numOutputs;
		info.flags = static_cast<int32_t>(g_effect->flags);
		info.initialDelay = g_effect->initialDelay;
		info.uniqueID = g_effect->uniqueID;
		info.version = g_effect->version;
	}
	return info;
}

bool SendReply(Channel &channel, int64_t result, float value, const void *data = nullptr, std::size_t dataSize = 0)
{
	ReplyPayload reply{};
	reply.result = result;
	reply.value = value;
	reply.info = GetEffectInfo();
	return Send(channel, MakeMessage(MsgType::reply, &reply, sizeof(reply), data, dataSize));
}

void HandleRequest(Channel &channel, const Message &message);

// Waits for the reply to a callback, serving the host's requests that come
// first (the host may call the plugin while it answers).
bool WaitForReply(Channel &channel, Message &reply)
{
	for(;;)
	{
		if(!Receive(channel, reply))
			return false;
		if(reply.type == MsgType::reply)
			return true;
		HandleRequest(channel, reply);
	}
}

// What the host would answer, for callbacks outside of any request (from
// the plugin's editor or its own threads).
intptr_t AnswerLocally(int32_t opcode, int32_t index, intptr_t value, void *ptr)
{
	switch(opcode)
	{
	case Vst::audioMasterVersion:
		return Vst::kVstVersion;
	case Vst::audioMasterCurrentId:
		return g_shellPluginId;
	case Vst::audioMasterGetSampleRate:
		return static_cast<intptr_t>(g_sampleRate.load());
	case Vst::audioMasterGetBlockSize:
		return g_blockSize.load();
	case Vst::audioMasterGetTime:
	{
		thread_local Vst::VstTimeInfo timeInfo;
		std::lock_guard<std::mutex> lock(g_timeInfoMutex);
		timeInfo = g_lastTimeInfo;
		return reinterpret_cast<intptr_t>(&timeInfo);
	}
	case Vst::audioMasterGetVendorString:
		if(ptr)
			std::strcpy(static_cast<char *>(ptr), "OpenMPT project");
		return 1;
	case Vst::audioMasterGetProductString:
		if(ptr)
			std::strcpy(static_cast<char *>(ptr), "OpenMPT");
		return 1;
	case Vst::audioMasterGetCurrentProcessLevel:
		return 1;  // user thread
	case Vst::audioMasterCanDo:
		return 0;
	default:
		(void)index;
		(void)value;
		return 0;
	}
}

intptr_t VSTCALLBACK HostCallback(Vst::AEffect *, Vst::VstOpcodeToHost opcode, int32_t index, intptr_t value, void *ptr, float opt)
{
	if(opcode == Vst::audioMasterSizeWindow && g_editor != nullptr)
		SetWindowPos(g_editor, nullptr, 0, 0, index, static_cast<int>(value), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	if(opcode == Vst::audioMasterVersion)
		return Vst::kVstVersion;
	if(opcode == Vst::audioMasterCurrentId)
		return g_shellPluginId;
	if(g_effect == nullptr)
		return AnswerLocally(opcode, index, value, ptr);

	std::size_t size = 0;
	const DataKind kind = HostOpcodeData(opcode, size);
	std::vector<char> data;
	if(kind == DataKind::stringIn && ptr != nullptr)
		AppendBytes(data, ptr, std::strlen(static_cast<const char *>(ptr)) + 1);
	else if(opcode == Vst::audioMasterProcessEvents && ptr != nullptr)
		SerializeEvents(data, *static_cast<const Vst::VstEvents *>(ptr));

	DispatchPayload request{};
	request.opcode = opcode;
	request.index = index;
	request.value = value;
	request.opt = opt;

	Channel *channel = t_channel;
	if(channel == nullptr)
	{
		// Not during a request: tell the host, or answer for it.
		if(IsNotification(opcode))
		{
			Send(g_notify, MakeMessage(MsgType::notify, &request, sizeof(request), data.data(), data.size()));
			return 1;
		}
		return AnswerLocally(opcode, index, value, ptr);
	}

	if(!Send(*channel, MakeMessage(MsgType::callback, &request, sizeof(request), data.data(), data.size())))
		return AnswerLocally(opcode, index, value, ptr);
	Message reply;
	if(!WaitForReply(*channel, reply))
		return 0;
	ReplyPayload result{};
	Read(reply, result);
	const char *replyData = reply.payload.data() + sizeof(ReplyPayload);
	const std::size_t replySize = reply.payload.size() > sizeof(ReplyPayload) ? reply.payload.size() - sizeof(ReplyPayload) : 0;

	if(kind == DataKind::stringOut && ptr != nullptr)
	{
		std::memcpy(ptr, replyData, std::min(replySize, size));
		static_cast<char *>(ptr)[std::min(replySize, size - 1)] = '\0';
	} else if(opcode == Vst::audioMasterGetTime)
	{
		if(result.result == 0 || replySize < sizeof(Vst::VstTimeInfo))
			return 0;
		thread_local Vst::VstTimeInfo timeInfo;
		std::memcpy(&timeInfo, replyData, sizeof(timeInfo));
		{
			std::lock_guard<std::mutex> lock(g_timeInfoMutex);
			g_lastTimeInfo = timeInfo;
		}
		return reinterpret_cast<intptr_t>(&timeInfo);
	} else if(opcode == Vst::audioMasterGetDirectory)
	{
		if(result.result == 0 || replySize == 0)
			return 0;
		thread_local std::string directory;
		directory.assign(replyData, strnlen(replyData, replySize));
		return reinterpret_cast<intptr_t>(directory.c_str());
	}
	return static_cast<intptr_t>(result.result);
}

// The editor -----------------------------------------------------------------

LRESULT CALLBACK EditorWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if(message == WM_CLOSE)
		return 0;  // the tracker closes the editor
	return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool GetEditorSize(int &width, int &height)
{
	Vst::ERect *rect = nullptr;
	g_effect->dispatcher(g_effect, Vst::effEditGetRect, 0, 0, &rect, 0.0f);
	if(rect == nullptr || rect->right <= rect->left || rect->bottom <= rect->top)
		return false;
	width = rect->right - rect->left;
	height = rect->bottom - rect->top;
	return true;
}

// Opens the editor in a Wine window and returns its X11 window, which the
// tracker embeds. Wine believes the window is where SetWindowPos put it,
// and translates mouse positions with that: the tracker tells us where on
// the screen its window is (editorPosition), we move ours there, and the
// tracker puts the X window back inside its own, which Wine does not notice
// for a window no window manager handles (WS_POPUP).
int64_t OpenEditor(int &width, int &height)
{
	static bool registered = false;
	if(!registered)
	{
		WNDCLASSW windowClass{};
		windowClass.lpfnWndProc = EditorWindowProc;
		windowClass.hInstance = GetModuleHandleW(nullptr);
		windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
		windowClass.lpszClassName = L"LinOpenMPTPluginEditor";
		RegisterClassW(&windowClass);
		registered = true;
	}
	width = 320;
	height = 200;
	GetEditorSize(width, height);
	g_editor = CreateWindowExW(WS_EX_TOOLWINDOW, L"LinOpenMPTPluginEditor", L"Plugin editor", WS_POPUP, 0, 0, width, height, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	if(g_editor == nullptr)
		return 0;
	g_effect->dispatcher(g_effect, Vst::effEditOpen, 0, 0, g_editor, 0.0f);
	// Some plugins only know their size once open.
	if(GetEditorSize(width, height))
		SetWindowPos(g_editor, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	// Shown once the tracker has it inside its window (editorPosition).
	return reinterpret_cast<int64_t>(GetPropA(g_editor, "__wine_x11_whole_window"));
}

void CloseEditor()
{
	if(g_editor == nullptr)
		return;
	g_effect->dispatcher(g_effect, Vst::effEditClose, 0, 0, nullptr, 0.0f);
	DestroyWindow(g_editor);
	g_editor = nullptr;
}

// Requests --------------------------------------------------------------------

void HandleDispatch(Channel &channel, const Message &message)
{
	DispatchPayload request{};
	Read(message, request);
	const char *data = message.payload.data() + sizeof(DispatchPayload);
	const std::size_t dataSize = message.payload.size() > sizeof(DispatchPayload) ? message.payload.size() - sizeof(DispatchPayload) : 0;
	const auto opcode = static_cast<Vst::VstOpcodeToPlugin>(request.opcode);
	intptr_t value = static_cast<intptr_t>(request.value);

	std::size_t size = 0;
	const DataKind kind = PluginOpcodeData(request.opcode, size);
	std::vector<char> buffer;
	void *ptr = nullptr;
	std::vector<char> out;
	intptr_t result = 0;

	switch(kind)
	{
	case DataKind::none:
		result = g_effect->dispatcher(g_effect, opcode, request.index, value, nullptr, request.opt);
		break;
	case DataKind::stringIn:
		buffer.assign(data, data + dataSize);
		buffer.push_back('\0');
		result = g_effect->dispatcher(g_effect, opcode, request.index, value, buffer.data(), request.opt);
		break;
	case DataKind::stringOut:
	case DataKind::stringInOut:
	case DataKind::structOut:
	case DataKind::structIn:
	case DataKind::structInOut:
		buffer.assign(std::max(size, dataSize) + 1, 0);
		if(kind != DataKind::stringOut && kind != DataKind::structOut)
			std::memcpy(buffer.data(), data, dataSize);
		ptr = buffer.data();
		if(kind == DataKind::stringInOut && dataSize == 0)
			ptr = nullptr;  // effString2Parameter with no string asks whether it can
		result = g_effect->dispatcher(g_effect, opcode, request.index, value, ptr, request.opt);
		if(kind == DataKind::stringOut || kind == DataKind::stringInOut)
		{
			buffer[size - 1] = '\0';
			AppendBytes(out, buffer.data(), std::strlen(buffer.data()) + 1);
		} else if(kind != DataKind::structIn)
		{
			AppendBytes(out, buffer.data(), size);
		}
		break;
	case DataKind::special:
		switch(opcode)
		{
		case Vst::effEditGetRect:
		{
			Vst::ERect *rect = nullptr;
			result = g_effect->dispatcher(g_effect, opcode, 0, 0, &rect, 0.0f);
			if(rect != nullptr)
				AppendBytes(out, rect, sizeof(*rect));
			break;
		}
		case Vst::effEditOpen:
		{
			int width = 0, height = 0;
			const int64_t window = OpenEditor(width, height);
			result = window != 0 ? 1 : 0;
			Append(out, window);
			Append(out, static_cast<int32_t>(width));
			Append(out, static_cast<int32_t>(height));
			break;
		}
		case Vst::effGetChunk:
		{
			void *chunk = nullptr;
			result = g_effect->dispatcher(g_effect, opcode, request.index, 0, &chunk, 0.0f);
			if(chunk != nullptr && result > 0)
				AppendBytes(out, chunk, static_cast<std::size_t>(result));
			else
				result = 0;
			break;
		}
		case Vst::effSetChunk:
			buffer.assign(data, data + dataSize);
			result = g_effect->dispatcher(g_effect, opcode, request.index, static_cast<intptr_t>(dataSize), buffer.empty() ? nullptr : buffer.data(), 0.0f);
			break;
		case Vst::effProcessEvents:
		{
			// Plugins may keep the events until the next process call.
			thread_local std::vector<char> storage;
			Vst::VstEvents *events = DeserializeEvents(data, dataSize, storage);
			if(events != nullptr)
				result = g_effect->dispatcher(g_effect, opcode, 0, 0, events, 0.0f);
			break;
		}
		case Vst::effSetSpeakerArrangement:
		{
			Vst::VstSpeakerArrangement arrangements[2]{};
			std::memcpy(arrangements, data, std::min(dataSize, sizeof(arrangements)));
			result = g_effect->dispatcher(g_effect, opcode, 0, reinterpret_cast<intptr_t>(&arrangements[1]), &arrangements[0], 0.0f);
			break;
		}
		case Vst::effGetSpeakerArrangement:
		{
			Vst::VstSpeakerArrangement *input = nullptr, *output = nullptr;
			result = g_effect->dispatcher(g_effect, opcode, 0, reinterpret_cast<intptr_t>(&input), &output, 0.0f);
			Vst::VstSpeakerArrangement arrangements[2]{};
			if(output)
				arrangements[0] = *output;
			if(input)
				arrangements[1] = *input;
			if(input == nullptr || output == nullptr)
				result = 0;
			AppendBytes(out, arrangements, sizeof(arrangements));
			break;
		}
		default:
			// effVendorSpecific: what [ptr] is, only the vendor knows.
			result = g_effect->dispatcher(g_effect, opcode, request.index, value, nullptr, request.opt);
			break;
		}
		break;
	}

	switch(opcode)
	{
	case Vst::effSetSampleRate:
		g_sampleRate = request.opt;
		break;
	case Vst::effSetBlockSize:
		g_blockSize = static_cast<int32_t>(value);
		break;
	case Vst::effEditClose:
		CloseEditor();
		break;
	default:
		break;
	}
	SendReply(channel, result, 0.0f, out.data(), out.size());
}

void HandleProcess(Channel &channel, const Message &message)
{
	ProcessPayload request{};
	Read(message, request);
	const int32_t frames = std::max(request.frames, 0);
	const int32_t numInputs = std::clamp(request.numInputs, 0, 256), numOutputs = std::clamp(request.numOutputs, 0, 256);
	thread_local std::vector<float> inputs, outputs;
	thread_local std::vector<float *> inputPtrs, outputPtrs;
	inputs.assign(static_cast<std::size_t>(numInputs) * frames, 0.0f);
	outputs.assign(static_cast<std::size_t>(numOutputs) * frames, 0.0f);
	const std::size_t available = message.payload.size() - std::min(message.payload.size(), sizeof(ProcessPayload));
	std::memcpy(inputs.data(), message.payload.data() + sizeof(ProcessPayload), std::min(available, inputs.size() * sizeof(float)));
	// At least as many buffers as the plugin has, in case it ignores what
	// it was told.
	const int32_t inputCount = std::max(numInputs, g_effect->numInputs), outputCount = std::max(numOutputs, g_effect->numOutputs);
	thread_local std::vector<float> spare;
	spare.assign(static_cast<std::size_t>(frames) * 2 + 16, 0.0f);
	inputPtrs.resize(std::max(inputCount, 1));
	outputPtrs.resize(std::max(outputCount, 1));
	for(int32_t i = 0; i < inputCount; i++)
		inputPtrs[i] = i < numInputs ? inputs.data() + static_cast<std::size_t>(i) * frames : spare.data();
	for(int32_t i = 0; i < outputCount; i++)
		outputPtrs[i] = i < numOutputs ? outputs.data() + static_cast<std::size_t>(i) * frames : spare.data() + frames;
	if((g_effect->flags & Vst::effFlagsCanReplacing) && g_effect->processReplacing != nullptr)
		g_effect->processReplacing(g_effect, inputPtrs.data(), outputPtrs.data(), frames);
	else if(g_effect->process != nullptr)
		g_effect->process(g_effect, inputPtrs.data(), outputPtrs.data(), frames);
	SendReply(channel, 1, 0.0f, outputs.data(), outputs.size() * sizeof(float));
}

void HandleRequest(Channel &channel, const Message &message)
{
	Channel *previous = t_channel;
	t_channel = &channel;
	switch(message.type)
	{
	case MsgType::dispatch:
		HandleDispatch(channel, message);
		break;
	case MsgType::process:
		HandleProcess(channel, message);
		break;
	case MsgType::setParameter:
	{
		ParameterPayload request{};
		Read(message, request);
		g_effect->setParameter(g_effect, request.index, request.value);
		SendReply(channel, 0, 0.0f);
		break;
	}
	case MsgType::getParameter:
	{
		ParameterPayload request{};
		Read(message, request);
		SendReply(channel, 0, g_effect->getParameter(g_effect, request.index));
		break;
	}
	case MsgType::quit:
		g_quit = true;
		break;
	case MsgType::editorPosition:
	{
		EditorPositionPayload position{};
		Read(message, position);
		if(g_editor != nullptr)
		{
			SetWindowPos(g_editor, nullptr, position.x, position.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
			UpdateWindow(g_editor);
		}
		SendReply(channel, 1, 0.0f);
		break;
	}
	default:
		SendReply(channel, 0, 0.0f);
		break;
	}
	t_channel = previous;
}

// Start-up --------------------------------------------------------------------

bool Connect(Channel &channel, uint16_t port, const char *token, Connection which)
{
	channel.socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(channel.socket == INVALID_SOCKET)
		return false;
	BOOL noDelay = TRUE;
	setsockopt(channel.socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&noDelay), sizeof(noDelay));
	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if(connect(channel.socket, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0)
		return false;
	HelloPayload hello{};
	hello.version = kProtocolVersion;
	hello.connection = static_cast<uint32_t>(which);
	std::strncpy(hello.token, token, kTokenLength);
	return Send(channel, MakeMessage(MsgType::hello, &hello, sizeof(hello)));
}

// A Linux path as Wine sees it (drive Z: is the root), or a Windows path.
std::wstring ToWindowsPath(const std::string &path)
{
	std::string windows = path;
	if(!windows.empty() && windows[0] == '/')
	{
		windows = "Z:" + windows;
		std::replace(windows.begin(), windows.end(), '/', '\\');
	}
	const int length = MultiByteToWideChar(CP_UTF8, 0, windows.c_str(), -1, nullptr, 0);
	std::wstring wide(std::max(length, 1), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, windows.c_str(), -1, wide.data(), length);
	wide.resize(wcslen(wide.c_str()));
	return wide;
}

bool SendLoaded(bool ok, const std::string &error)
{
	LoadedPayload loaded{};
	loaded.ok = ok ? 1 : 0;
	loaded.info = GetEffectInfo();
	return Send(g_control, MakeMessage(MsgType::loaded, &loaded, sizeof(loaded), error.c_str(), error.size() + 1));
}

std::string LoadPlugin(const std::string &path)
{
	const std::wstring windowsPath = ToWindowsPath(path);
	g_library = LoadLibraryW(windowsPath.c_str());
	if(g_library == nullptr)
		return "Wine could not load " + path + " (error " + std::to_string(GetLastError()) + ").";
	using MainProc = Vst::AEffect *(VSTCALLBACK *)(Vst::AudioMasterCallbackFunc);
	auto main = reinterpret_cast<MainProc>(GetProcAddress(g_library, "VSTPluginMain"));
	if(main == nullptr)
		main = reinterpret_cast<MainProc>(GetProcAddress(g_library, "main"));
	if(main == nullptr)
		return path + " is not a VST2 plugin (it has no VSTPluginMain).";
	Vst::AEffect *effect = main(HostCallback);
	if(effect == nullptr || effect->magic != Vst::kEffectMagic || effect->dispatcher == nullptr)
		return path + " did not start (no valid plugin returned).";
	g_effect = effect;
	return std::string();
}

void AudioThread()
{
	Message message;
	while(!g_quit && Receive(g_audio, message))
		HandleRequest(g_audio, message);
	g_quit = true;
}

// Serves the control connection between window messages, so that plugin
// editors keep running.
void ControlLoop()
{
	WSAEVENT event = WSACreateEvent();
	WSAEventSelect(g_control.socket, event, FD_READ | FD_CLOSE);
	while(!g_quit)
	{
		const DWORD wait = MsgWaitForMultipleObjects(1, &event, FALSE, INFINITE, QS_ALLINPUT);
		if(wait == WAIT_OBJECT_0)
		{
			WSANETWORKEVENTS events{};
			WSAEnumNetworkEvents(g_control.socket, event, &events);
			// Everything that arrived, message by message.
			for(;;)
			{
				fd_set set;
				FD_ZERO(&set);
				FD_SET(g_control.socket, &set);
				timeval zero{};
				if(select(0, &set, nullptr, nullptr, &zero) <= 0)
					break;
				Message message;
				if(!Receive(g_control, message))
				{
					g_quit = true;
					break;
				}
				HandleRequest(g_control, message);
				if(g_quit)
					break;
			}
		} else
		{
			MSG msg;
			while(PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		}
	}
	WSACloseEvent(event);
}

}  // namespace

int main(int argc, char *argv[])
{
	if(argc < 4)
	{
		std::fprintf(stderr, "usage: linopenmpt-vstbridge.exe <port> <token> <plugin.dll> [shell plugin id]\n");
		return 2;
	}
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;
	const auto port = static_cast<uint16_t>(std::atoi(argv[1]));
	const char *token = argv[2];
	if(argc > 4)
		g_shellPluginId = static_cast<int32_t>(std::strtol(argv[4], nullptr, 10));
	if(!Connect(g_control, port, token, Connection::control) || !Connect(g_audio, port, token, Connection::audio) || !Connect(g_notify, port, token, Connection::notify))
	{
		Fail("cannot connect to the tracker");
		return 1;
	}
	const std::string error = LoadPlugin(argv[3]);
	SendLoaded(error.empty(), error);
	if(!error.empty())
		return 1;
	std::thread audio(AudioThread);
	ControlLoop();
	// The tracker has gone (or said to quit): unblock the audio thread.
	shutdown(g_audio.socket, SD_BOTH);
	closesocket(g_audio.socket);
	audio.join();
	if(g_editor)
		CloseEditor();
	// The tracker closed the plugin (effClose) before quitting; Windows
	// unloads the library with the process.
	closesocket(g_control.socket);
	closesocket(g_notify.socket);
	WSACleanup();
	std::fflush(stderr);
	ExitProcess(0);
}
