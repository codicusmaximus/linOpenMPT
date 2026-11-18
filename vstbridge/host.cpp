// linOpenMPT: the tracker's side of the Windows plugin bridge (protocol.h).
// Upstream's VST host asks BridgeWrapper::Create for any plugin that is not
// native; here that starts linopenmpt-vstbridge.exe in Wine, which loads the
// plugin, and returns a stand-in AEffect whose calls travel to it. The host
// then uses the plugin as it would one loaded in-process.
// Compiled with upstream's VST host (mpt_tracker_vst).
#include "stdafx.h"

#ifdef MPT_WITH_VST

#include "pluginBridge/BridgeWrapper.h"
#include "mptrack/Vstplug.h"
#include "soundlib/plugins/PluginManager.h"

#include "protocol.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

OPENMPT_NAMESPACE_BEGIN
// The opcode tables, in terms of upstream's Vst:: names.
#include "opcodes.h"

namespace
{

using namespace vstbridge;

struct Message
{
	MsgType type{};
	std::vector<char> payload;

	const char *Data(std::size_t offset) const { return payload.data() + offset; }
	std::size_t DataSize(std::size_t offset) const { return payload.size() > offset ? payload.size() - offset : 0; }
	template <typename T>
	T Get() const
	{
		T value{};
		std::memcpy(&value, payload.data(), std::min(sizeof(T), payload.size()));
		return value;
	}
};

bool SendAll(int fd, const char *data, std::size_t size)
{
	while(size > 0)
	{
		const ssize_t sent = ::send(fd, data, size, MSG_NOSIGNAL);
		if(sent < 0 && errno == EINTR)
			continue;
		if(sent <= 0)
			return false;
		data += sent;
		size -= static_cast<std::size_t>(sent);
	}
	return true;
}

bool RecvAll(int fd, char *data, std::size_t size)
{
	while(size > 0)
	{
		const ssize_t got = ::recv(fd, data, size, 0);
		if(got < 0 && errno == EINTR)
			continue;
		if(got <= 0)
			return false;
		data += got;
		size -= static_cast<std::size_t>(got);
	}
	return true;
}

bool Receive(int fd, Message &message)
{
	Header header;
	if(!RecvAll(fd, reinterpret_cast<char *>(&header), sizeof(header)))
		return false;
	message.type = static_cast<MsgType>(header.type);
	message.payload.resize(header.size);
	return header.size == 0 || RecvAll(fd, message.payload.data(), header.size);
}

bool WaitReadable(int fd, int timeoutMs)
{
	pollfd p{fd, POLLIN, 0};
	int result;
	do
	{
		result = ::poll(&p, 1, timeoutMs);
	} while(result < 0 && errno == EINTR);
	return result > 0;
}

std::string FindInPath(const std::string &name)
{
	if(name.find('/') != std::string::npos)
		return ::access(name.c_str(), X_OK) == 0 ? name : std::string();
	const char *path = std::getenv("PATH");
	std::string dirs = path ? path : "/usr/local/bin:/usr/bin:/bin";
	std::size_t start = 0;
	while(start <= dirs.size())
	{
		const std::size_t end = std::min(dirs.find(':', start), dirs.size());
		const std::string candidate = dirs.substr(start, end - start) + "/" + name;
		if(end > start && ::access(candidate.c_str(), X_OK) == 0)
			return candidate;
		start = end + 1;
	}
	return std::string();
}

// The bridge for a plugin of this many bits: linopenmpt-vstbridge.exe (64)
// or linopenmpt-vstbridge32.exe (32). $LINOPENMPT_VSTBRIDGE (or
// $LINOPENMPT_VSTBRIDGE32) if set, else the one next to it, else next to the
// tracker, else where it is installed.
std::string FindBridge(int bits)
{
	const std::string name = bits == 32 ? "linopenmpt-vstbridge32.exe" : "linopenmpt-vstbridge.exe";
	std::vector<std::string> candidates;
	if(const char *path = std::getenv(bits == 32 ? "LINOPENMPT_VSTBRIDGE32" : "LINOPENMPT_VSTBRIDGE"); path != nullptr && *path)
		return ::access(path, R_OK) == 0 ? path : std::string();
	if(const char *path = std::getenv("LINOPENMPT_VSTBRIDGE"); bits == 32 && path != nullptr && *path)
	{
		const std::string other = path;
		const auto slash = other.find_last_of('/');
		candidates.push_back((slash == std::string::npos ? std::string(".") : other.substr(0, slash)) + "/" + name);
	}
	char self[4096];
	const ssize_t length = ::readlink("/proc/self/exe", self, sizeof(self) - 1);
	if(length > 0)
	{
		std::string dir(self, static_cast<std::size_t>(length));
		dir = dir.substr(0, dir.find_last_of('/'));
		candidates.push_back(dir + "/" + name);
		candidates.push_back(dir + "/../lib/linopenmpt/" + name);
		candidates.push_back(dir + "/../vstbridge/" + name);  // build tree
	}
#ifdef LINOPENMPT_VSTBRIDGE_INSTALL_DIR
	candidates.push_back(std::string(LINOPENMPT_VSTBRIDGE_INSTALL_DIR "/") + name);
#endif
	for(const auto &candidate : candidates)
	{
		if(::access(candidate.c_str(), R_OK) == 0)
			return candidate;
	}
	return std::string();
}

// What kind of Windows program a file is: 64 or 32 (bits), or 0 if it is not
// one.
int WindowsBinaryBits(const mpt::PathString &path)
{
	std::ifstream file(path.AsNative().c_str(), std::ios::binary);
	unsigned char dos[64] = {};
	if(!file.read(reinterpret_cast<char *>(dos), sizeof(dos)) || dos[0] != 'M' || dos[1] != 'Z')
		return 0;
	const uint32 peOffset = dos[60] | (dos[61] << 8) | (dos[62] << 16) | (static_cast<uint32>(dos[63]) << 24);
	unsigned char pe[6] = {};
	file.seekg(peOffset);
	if(!file.read(reinterpret_cast<char *>(pe), sizeof(pe)) || pe[0] != 'P' || pe[1] != 'E' || pe[2] != 0 || pe[3] != 0)
		return 0;
	const uint16 machine = static_cast<uint16>(pe[4] | (pe[5] << 8));
	if(machine == 0x8664)  // IMAGE_FILE_MACHINE_AMD64
		return 64;
	if(machine == 0x14C)  // IMAGE_FILE_MACHINE_I386
		return 32;
	return 0;
}

std::string RandomToken()
{
	unsigned char bytes[kTokenLength / 2] = {};
	std::ifstream random("/dev/urandom", std::ios::binary);
	random.read(reinterpret_cast<char *>(bytes), sizeof(bytes));
	static const char hex[] = "0123456789abcdef";
	std::string token;
	for(unsigned char b : bytes)
	{
		token += hex[b >> 4];
		token += hex[b & 15];
	}
	token.resize(kTokenLength - 1);
	return token;
}

class BridgedPlugin;
std::mutex g_pluginsMutex;
std::set<BridgedPlugin *> g_plugins;  // alive, for notifications that arrive late

class BridgedPlugin
{
public:
	Vst::AEffect effect{};

	BridgedPlugin() = default;
	BridgedPlugin(const BridgedPlugin &) = delete;
	BridgedPlugin &operator=(const BridgedPlugin &) = delete;

	~BridgedPlugin()
	{
		if(m_editorWindow != 0)
			LinOpenMPTUnembedX11Window(m_editorWindow);
		{
			std::lock_guard<std::mutex> lock(g_pluginsMutex);
			g_plugins.erase(this);
		}
		for(int *fd : {&m_control, &m_audio, &m_notify})
		{
			if(*fd >= 0)
			{
				::shutdown(*fd, SHUT_RDWR);
				::close(*fd);
				*fd = -1;
			}
		}
		if(m_notifyThread.joinable())
			m_notifyThread.join();
		if(m_pid > 0)
		{
			// Let the bridge end on its own, then make sure it does.
			const pid_t pid = m_pid;
			std::thread([pid]() {
				for(int i = 0; i < 50; i++)
				{
					if(::waitpid(pid, nullptr, WNOHANG) == pid)
						return;
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
				::kill(pid, SIGKILL);
				::waitpid(pid, nullptr, 0);
			}).detach();
		}
	}

	void Start(const VSTPluginLib &lib)
	{
		// Upstream may ask for the bridge for any plugin (a plugin set to
		// use it, the first load of a new one); only Windows plugins need
		// it here. For the rest, a missing bridge: upstream then loads them
		// in-process.
		if(BridgeWrapper::IsPluginNative(lib.dllPath))
			throw BridgeWrapper::BridgeNotFoundException();
		if(::access(lib.dllPath.AsNative().c_str(), R_OK) != 0)
			throw BridgeWrapper::BridgeException(U_("The plugin file cannot be found: ") + lib.dllPath.ToUnicode());
		const int bits = WindowsBinaryBits(lib.dllPath);
		if(bits != 32 && bits != 64)
			throw BridgeWrapper::BridgeException(U_("This file is neither a Linux nor a Windows (32-bit or 64-bit) plugin."));
		const std::string bridge = FindBridge(bits);
		if(bridge.empty())
		{
			// No bridge at all: as for any missing bridge. Only no 32-bit
			// one: say so.
			if(bits == 32 && !FindBridge(64).empty())
				throw BridgeWrapper::BridgeException(U_("This is a 32-bit Windows plugin, and this build of OpenMPT has no bridge for 32-bit plugins (linopenmpt-vstbridge32.exe)."));
			throw BridgeWrapper::BridgeNotFoundException();
		}
		const char *wineName = std::getenv("LINOPENMPT_WINE");
		const std::string wine = FindInPath(wineName && *wineName ? wineName : "wine");
		if(wine.empty())
			throw BridgeWrapper::BridgeException(U_("Windows plugins run in Wine, which is not installed. Install Wine (the wine or wine64 package) and try again."));
		m_bits = bits;

		// Listen on a port of our own, on this machine only.
		const int listener = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		socklen_t addressLength = sizeof(address);
		if(listener < 0 || ::bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0
		   || ::listen(listener, 4) != 0 || ::getsockname(listener, reinterpret_cast<sockaddr *>(&address), &addressLength) != 0)
		{
			if(listener >= 0)
				::close(listener);
			throw BridgeWrapper::BridgeException(U_("Cannot open a local connection for the plugin bridge."));
		}
		struct ListenerGuard
		{
			int fd;
			~ListenerGuard() { ::close(fd); }
		} listenerGuard{listener};

		const std::string token = RandomToken();
		const std::string port = std::to_string(ntohs(address.sin_port));
		const std::string pluginPath = lib.dllPath.ToUTF8();
		const std::string shellId = std::to_string(static_cast<int32>(lib.shellPluginID));
		std::vector<std::string> args{wine, bridge, port, token, pluginPath, shellId};
		std::vector<char *> argv;
		for(auto &arg : args)
			argv.push_back(arg.data());
		argv.push_back(nullptr);
		// Wine's own messages (fixme:...) stay quiet unless asked for.
		std::vector<std::string> env;
		bool hasWineDebug = false;
		for(char **e = environ; *e; e++)
		{
			env.emplace_back(*e);
			hasWineDebug = hasWineDebug || env.back().rfind("WINEDEBUG=", 0) == 0;
		}
		if(!hasWineDebug)
			env.emplace_back("WINEDEBUG=-all");
		std::vector<char *> envp;
		for(auto &e : env)
			envp.push_back(e.data());
		envp.push_back(nullptr);
		if(::posix_spawn(&m_pid, wine.c_str(), nullptr, nullptr, argv.data(), envp.data()) != 0)
		{
			m_pid = 0;
			throw BridgeWrapper::BridgeException(U_("Cannot start Wine for the plugin bridge."));
		}

		// Its three connections. Wine may take a while the first time, when
		// it sets up its folder.
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(90);
		int connected = 0;
		while(connected < 3)
		{
			if(std::chrono::steady_clock::now() > deadline)
				throw BridgeWrapper::BridgeException(U_("The plugin bridge did not start in time."));
			if(::waitpid(m_pid, nullptr, WNOHANG) == m_pid)
			{
				m_pid = 0;
				if(m_bits == 32)
					throw BridgeWrapper::BridgeException(U_("The plugin bridge stopped while starting. 32-bit Windows plugins need Wine's 32-bit part (the wine32 package on Debian and Ubuntu, wine.i686 on Fedora)."));
				throw BridgeWrapper::BridgeException(U_("The plugin bridge stopped while starting."));
			}
			if(!WaitReadable(listener, 200))
				continue;
			const int fd = ::accept4(listener, nullptr, nullptr, SOCK_CLOEXEC);
			if(fd < 0)
				continue;
			int one = 1;
			::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
			Message hello;
			if(!WaitReadable(fd, 5000) || !Receive(fd, hello) || hello.type != MsgType::hello || hello.payload.size() < sizeof(HelloPayload))
			{
				::close(fd);
				continue;
			}
			const auto payload = hello.Get<HelloPayload>();
			int *slot = nullptr;
			switch(static_cast<Connection>(payload.connection))
			{
			case Connection::control: slot = &m_control; break;
			case Connection::audio: slot = &m_audio; break;
			case Connection::notify: slot = &m_notify; break;
			}
			if(payload.version != kProtocolVersion || std::strncmp(payload.token, token.c_str(), kTokenLength) != 0 || slot == nullptr || *slot >= 0)
			{
				::close(fd);
				continue;
			}
			*slot = fd;
			connected++;
		}

		// Then the plugin, loaded or not.
		Message loaded;
		if(!WaitReadable(m_control, 60000) || !Receive(m_control, loaded) || loaded.type != MsgType::loaded || loaded.payload.size() < sizeof(LoadedPayload))
			throw BridgeWrapper::BridgeException(U_("The plugin bridge did not answer."));
		const auto result = loaded.Get<LoadedPayload>();
		if(!result.ok)
		{
			const std::string error(loaded.Data(sizeof(LoadedPayload)), strnlen(loaded.Data(sizeof(LoadedPayload)), loaded.DataSize(sizeof(LoadedPayload))));
			throw BridgeWrapper::BridgeException(mpt::ToUnicode(mpt::Charset::UTF8, error.empty() ? std::string("The plugin could not be loaded.") : error));
		}

		effect.magic = Vst::kEffectMagic;
		effect.dispatcher = DispatcherThunk;
		effect.process = ProcessThunk;
		effect.processReplacing = ProcessReplacingThunk;
		effect.processDoubleReplacing = nullptr;
		effect.setParameter = SetParameterThunk;
		effect.getParameter = GetParameterThunk;
		effect.ioRatio = 1.0f;
		effect.object = this;
		UpdateInfo(result.info);
		{
			std::lock_guard<std::mutex> lock(g_pluginsMutex);
			g_plugins.insert(this);
		}
		m_notifyThread = std::thread([this]() { NotifyLoop(); });
	}

private:
	int m_control = -1, m_audio = -1, m_notify = -1;
	pid_t m_pid = 0;
	int m_bits = 64;  // the plugin's, and so its bridge's
	std::recursive_mutex m_controlMutex, m_audioMutex;
	std::thread m_notifyThread;
	std::atomic<bool> m_dead{false};

	// What [ptr] points to on return, kept until the next such call.
	std::vector<char> m_chunk;
	Vst::ERect m_rect{};
	Vst::VstSpeakerArrangement m_speakers[2]{};
	std::vector<float> m_outputs;
	uintptr_t m_editorWindow = 0;  // the embedded editor's X11 window

	void UpdateInfo(const EffectInfo &info)
	{
		effect.numPrograms = info.numPrograms;
		effect.numParams = static_cast<uint32>(std::max(info.numParams, 0));
		effect.numInputs = info.numInputs;
		effect.numOutputs = info.numOutputs;
		// Our stand-in always replaces, and only in single precision.
		effect.flags = static_cast<Vst::VstFlags>((info.flags | Vst::effFlagsCanReplacing) & ~Vst::effFlagsCanDoubleReplacing);
		effect.initialDelay = info.initialDelay;
		effect.uniqueID = info.uniqueID;
		effect.version = info.version;
	}

	// A request, and the callbacks the plugin makes while it handles it.
	bool Call(int fd, std::recursive_mutex &mutex, const std::vector<char> &request, Message &reply)
	{
		if(m_dead || fd < 0)
			return false;
		std::lock_guard<std::recursive_mutex> lock(mutex);
		if(!SendAll(fd, request.data(), request.size()))
		{
			m_dead = true;
			return false;
		}
		for(;;)
		{
			if(!Receive(fd, reply))
			{
				m_dead = true;
				return false;
			}
			if(reply.type == MsgType::reply)
			{
				if(reply.payload.size() < sizeof(ReplyPayload))
					return false;
				UpdateInfo(reply.Get<ReplyPayload>().info);
				return true;
			}
			if(reply.type == MsgType::callback)
				HandleCallback(fd, reply);
		}
	}

	// The plugin calls the host (audioMaster...) during a request.
	void HandleCallback(int fd, const Message &message)
	{
		const auto request = message.Get<DispatchPayload>();
		const char *data = message.Data(sizeof(DispatchPayload));
		const std::size_t dataSize = message.DataSize(sizeof(DispatchPayload));
		std::size_t size = 0;
		const DataKind kind = HostOpcodeData(request.opcode, size);
		std::string in;
		std::vector<char> buffer, out, events;
		void *ptr = nullptr;
		if(kind == DataKind::stringIn)
		{
			in.assign(data, strnlen(data, dataSize));
			ptr = in.data();
		} else if(kind == DataKind::stringOut)
		{
			buffer.assign(size, 0);
			ptr = buffer.data();
		} else if(request.opcode == Vst::audioMasterProcessEvents)
		{
			ptr = DeserializeEvents(data, dataSize, events);
		}
		intptr_t result = CVstPlugin::MasterCallBack(&effect, static_cast<Vst::VstOpcodeToHost>(request.opcode), request.index, static_cast<intptr_t>(request.value), ptr, request.opt);
		if(kind == DataKind::stringOut)
		{
			buffer.back() = 0;
			out.assign(buffer.data(), buffer.data() + std::strlen(buffer.data()) + 1);
		} else if(request.opcode == Vst::audioMasterGetTime)
		{
			if(result != 0)
				AppendBytes(out, reinterpret_cast<const void *>(result), sizeof(Vst::VstTimeInfo));
			result = result != 0 ? 1 : 0;
		} else if(request.opcode == Vst::audioMasterGetDirectory)
		{
			if(result != 0)
				AppendBytes(out, reinterpret_cast<const char *>(result), std::strlen(reinterpret_cast<const char *>(result)) + 1);
			result = result != 0 ? 1 : 0;
		}
		ReplyPayload reply{};
		reply.result = result;
		if(!SendAll(fd, MakeMessage(MsgType::reply, &reply, sizeof(reply), out.data(), out.size()).data(), sizeof(Header) + sizeof(reply) + out.size()))
			m_dead = true;
	}

	// What the plugin tells the host on its own (its editor), on the
	// editor's thread, as an in-process plugin's editor would.
	void NotifyLoop()
	{
		Message message;
		while(Receive(m_notify, message))
		{
			if(message.type != MsgType::notify || message.payload.size() < sizeof(DispatchPayload))
				continue;
			auto *copy = new std::pair<BridgedPlugin *, Message>(this, message);
			LinOpenMPTPostToMainThread([](void *context) {
				std::unique_ptr<std::pair<BridgedPlugin *, Message>> item(static_cast<std::pair<BridgedPlugin *, Message> *>(context));
				{
					std::lock_guard<std::mutex> lock(g_pluginsMutex);
					if(!g_plugins.count(item->first))
						return;
				}
				item->first->HandleNotification(item->second);
			}, copy);
		}
	}

	void HandleNotification(const Message &message)
	{
		const auto request = message.Get<DispatchPayload>();
		if(std::getenv("LINOPENMPT_BRIDGE_TRACE")) std::fprintf(stderr, "bridge notify opcode %d index %d opt %f\n", request.opcode, request.index, request.opt);
		std::vector<char> events;
		void *ptr = nullptr;
		if(request.opcode == Vst::audioMasterProcessEvents)
			ptr = DeserializeEvents(message.Data(sizeof(DispatchPayload)), message.DataSize(sizeof(DispatchPayload)), events);
		CVstPlugin::MasterCallBack(&effect, static_cast<Vst::VstOpcodeToHost>(request.opcode), request.index, static_cast<intptr_t>(request.value), ptr, request.opt);
	}

	intptr_t Dispatch(int32 opcode, int32 index, intptr_t value, void *ptr, float opt)
	{
		if(opcode == Vst::effClose)
		{
			Message reply;
			DispatchPayload request{};
			request.opcode = opcode;
			Call(m_control, m_controlMutex, MakeMessage(MsgType::dispatch, &request, sizeof(request)), reply);
			Header quit{static_cast<uint32_t>(MsgType::quit), 0};
			if(m_control >= 0)
				SendAll(m_control, reinterpret_cast<const char *>(&quit), sizeof(quit));
			delete this;
			return 1;
		}

		if(opcode == Vst::effEditClose && m_editorWindow != 0)
		{
			LinOpenMPTUnembedX11Window(m_editorWindow);
			m_editorWindow = 0;
		}

		std::size_t size = 0;
		const DataKind kind = PluginOpcodeData(opcode, size);
		DispatchPayload request{};
		request.opcode = opcode;
		request.index = index;
		request.value = value;
		request.opt = opt;
		std::vector<char> data;
		char *ptrC = static_cast<char *>(ptr);
		switch(kind)
		{
		case DataKind::stringIn:
		case DataKind::stringInOut:
			if(ptrC != nullptr)
				AppendBytes(data, ptrC, std::strlen(ptrC) + 1);
			break;
		case DataKind::structIn:
		case DataKind::structInOut:
			if(ptrC == nullptr)
				return 0;
			AppendBytes(data, ptrC, size);
			break;
		case DataKind::special:
			switch(opcode)
			{
			case Vst::effEditOpen:
				request.ptr = static_cast<int64_t>(reinterpret_cast<intptr_t>(ptr));
				break;
			case Vst::effSetChunk:
				if(ptrC == nullptr || value <= 0)
					return 0;
				AppendBytes(data, ptrC, static_cast<std::size_t>(value));
				break;
			case Vst::effProcessEvents:
				if(ptr == nullptr)
					return 0;
				SerializeEvents(data, *static_cast<const Vst::VstEvents *>(ptr));
				break;
			case Vst::effSetSpeakerArrangement:
				if(ptr == nullptr || value == 0)
					return 0;
				AppendBytes(data, ptr, sizeof(Vst::VstSpeakerArrangement));
				AppendBytes(data, reinterpret_cast<const void *>(value), sizeof(Vst::VstSpeakerArrangement));
				request.value = 0;
				break;
			case Vst::effGetSpeakerArrangement:
				request.value = 0;
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}

		// MIDI goes with the audio; the rest comes from the editor's thread.
		const bool audio = opcode == Vst::effProcessEvents || opcode == Vst::effStartProcess || opcode == Vst::effStopProcess;
		Message reply;
		if(!Call(audio ? m_audio : m_control, audio ? m_audioMutex : m_controlMutex, MakeMessage(MsgType::dispatch, &request, sizeof(request), data.data(), data.size()), reply))
			return 0;
		const auto result = reply.Get<ReplyPayload>();
		const char *out = reply.Data(sizeof(ReplyPayload));
		const std::size_t outSize = reply.DataSize(sizeof(ReplyPayload));

		switch(kind)
		{
		case DataKind::stringOut:
		case DataKind::stringInOut:
			if(ptrC != nullptr && outSize > 0)
			{
				const std::size_t length = strnlen(out, outSize);
				std::memcpy(ptrC, out, length);
				ptrC[length] = '\0';
			}
			break;
		case DataKind::structOut:
		case DataKind::structInOut:
			if(ptrC != nullptr)
				std::memcpy(ptrC, out, std::min(outSize, size));
			break;
		case DataKind::special:
			switch(opcode)
			{
			case Vst::effEditGetRect:
				if(ptr != nullptr)
				{
					if(outSize >= sizeof(Vst::ERect))
					{
						std::memcpy(&m_rect, out, sizeof(m_rect));
						*static_cast<Vst::ERect **>(ptr) = &m_rect;
					} else
					{
						*static_cast<Vst::ERect **>(ptr) = nullptr;
					}
				}
				break;
			case Vst::effEditOpen:
				if(result.result != 0 && outSize >= sizeof(int64_t) + 2 * sizeof(int32_t))
				{
					int64_t window = 0;
					std::memcpy(&window, out, sizeof(window));
					if(window == 0 || !LinOpenMPTEmbedX11Window(reinterpret_cast<uintptr_t>(ptr), static_cast<uintptr_t>(window), EditorMovedThunk, this))
						return 0;
					m_editorWindow = static_cast<uintptr_t>(window);
				}
				break;
			case Vst::effGetChunk:
				m_chunk.assign(out, out + outSize);
				if(ptr != nullptr)
					*static_cast<void **>(ptr) = m_chunk.empty() ? nullptr : m_chunk.data();
				return static_cast<intptr_t>(m_chunk.size());
			case Vst::effGetSpeakerArrangement:
				if(outSize >= sizeof(m_speakers) && result.result != 0 && ptr != nullptr && value != 0)
				{
					std::memcpy(m_speakers, out, sizeof(m_speakers));
					*static_cast<Vst::VstSpeakerArrangement **>(ptr) = &m_speakers[0];
					*reinterpret_cast<Vst::VstSpeakerArrangement **>(value) = &m_speakers[1];
				}
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
		return static_cast<intptr_t>(result.result);
	}

	void Process(float **inputs, float **outputs, int32 frames, bool replace)
	{
		const int32 numInputs = std::max(effect.numInputs, 0), numOutputs = std::max(effect.numOutputs, 0);
		ProcessPayload request{};
		request.numInputs = numInputs;
		request.numOutputs = numOutputs;
		request.frames = std::max(frames, 0);
		thread_local std::vector<char> message;
		message.clear();
		Header header{static_cast<uint32_t>(MsgType::process), static_cast<uint32_t>(sizeof(request) + sizeof(float) * numInputs * request.frames)};
		Append(message, header);
		Append(message, request);
		for(int32 i = 0; i < numInputs; i++)
			AppendBytes(message, inputs[i], sizeof(float) * request.frames);
		Message reply;
		const bool ok = Call(m_audio, m_audioMutex, message, reply);
		const char *out = ok ? reply.Data(sizeof(ReplyPayload)) : nullptr;
		const std::size_t outSize = ok ? reply.DataSize(sizeof(ReplyPayload)) : 0;
		for(int32 i = 0; i < numOutputs; i++)
		{
			const std::size_t offset = sizeof(float) * i * request.frames, bytes = sizeof(float) * request.frames;
			if(replace)
			{
				if(offset + bytes <= outSize)
					std::memcpy(outputs[i], out + offset, bytes);
				else
					std::fill(outputs[i], outputs[i] + request.frames, 0.0f);
			} else if(offset + bytes <= outSize)
			{
				const auto *samples = reinterpret_cast<const float *>(out + offset);
				for(int32 f = 0; f < request.frames; f++)
					outputs[i][f] += samples[f];
			}
		}
	}

	// Where the embedded editor is on the screen, for Wine (bridge.cpp).
	void EditorMoved(int x, int y)
	{
		EditorPositionPayload position{x, y};
		Message reply;
		Call(m_control, m_controlMutex, MakeMessage(MsgType::editorPosition, &position, sizeof(position)), reply);
	}

	static void EditorMovedThunk(void *context, int x, int y)
	{
		auto *plugin = static_cast<BridgedPlugin *>(context);
		{
			std::lock_guard<std::mutex> lock(g_pluginsMutex);
			if(!g_plugins.count(plugin))
				return;
		}
		plugin->EditorMoved(x, y);
	}

	void SetParameter(int32 index, float value)
	{
		ParameterPayload request{index, value};
		Message reply;
		Call(m_audio, m_audioMutex, MakeMessage(MsgType::setParameter, &request, sizeof(request)), reply);
	}

	float GetParameter(int32 index)
	{
		ParameterPayload request{index, 0.0f};
		Message reply;
		if(!Call(m_audio, m_audioMutex, MakeMessage(MsgType::getParameter, &request, sizeof(request)), reply))
			return 0.0f;
		return reply.Get<ReplyPayload>().value;
	}

	static BridgedPlugin *From(Vst::AEffect *e) { return static_cast<BridgedPlugin *>(e->object); }
	static intptr_t VSTCALLBACK DispatcherThunk(Vst::AEffect *e, Vst::VstOpcodeToPlugin opcode, int32 index, intptr_t value, void *ptr, float opt) { return From(e)->Dispatch(opcode, index, value, ptr, opt); }
	static void VSTCALLBACK ProcessThunk(Vst::AEffect *e, float **in, float **out, int32 frames) { From(e)->Process(in, out, frames, false); }
	static void VSTCALLBACK ProcessReplacingThunk(Vst::AEffect *e, float **in, float **out, int32 frames) { From(e)->Process(in, out, frames, true); }
	static void VSTCALLBACK SetParameterThunk(Vst::AEffect *e, int32 index, float value) { From(e)->SetParameter(index, value); }
	static float VSTCALLBACK GetParameterThunk(Vst::AEffect *e, int32 index) { return From(e)->GetParameter(index); }
};

} // namespace

Vst::AEffect *BridgeWrapper::Create(const VSTPluginLib &plugin, bool)
{
	auto *bridged = new BridgedPlugin;
	try
	{
		bridged->Start(plugin);
	} catch(...)
	{
		delete bridged;
		throw;
	}
	return &bridged->effect;
}

OPENMPT_NAMESPACE_END

#endif // MPT_WITH_VST
