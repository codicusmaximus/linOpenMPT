// linOpenMPT: the protocol between the tracker and its Windows plugin
// bridge (linopenmpt-vstbridge.exe), which runs one Windows VST2 plugin in a
// Wine process. Upstream's own bridge talks through Windows shared memory,
// named events and window messages, none of which reach from a Wine process
// to a native Linux one; this one talks through three local TCP
// connections, which both sides have:
//   - control: the editor thread's calls (dispatcher, mostly), served by the
//     bridge's window thread, so plugin editors work;
//   - audio: process, parameter and MIDI calls, served by a thread of its own;
//   - notify: what the plugin tells the host on its own (a knob turned in its
//     editor), one way, from the bridge to the host.
// A call is a request followed by its reply. While the plugin handles a
// request it may call back into the host (audioMaster...): those callbacks
// come back on the same connection and the host answers them there, before
// the reply, as an in-process host would answer them on the same thread.
//
// Shared by the host (Linux) and the bridge (Windows, built with MinGW), so
// it uses no OpenMPT or platform types. The bridge is 64-bit or 32-bit (as
// the plugin is), the host 64-bit: messages use fixed-size fields only, and
// VST structures travel as they are only when they hold no pointers (which
// have the same layout on both, VstDefinitions.h packing them to 8 bytes as
// the Windows compilers do).
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace vstbridge
{

inline constexpr uint32_t kProtocolVersion = 2;
// Sent first on each connection, so a stray connection to the port is
// refused: the token the host passed the bridge on its command line.
inline constexpr std::size_t kTokenLength = 32;

enum class Connection : uint32_t
{
	control = 0,
	audio = 1,
	notify = 2,
};

enum class MsgType : uint32_t
{
	hello = 1,     // bridge to host, first on each connection: HelloPayload
	loaded,        // bridge to host on control, after hello: LoadedPayload (+ error text)
	dispatch,      // host to bridge: DispatchPayload (+ data)
	process,       // host to bridge: ProcessPayload (+ input samples)
	setParameter,  // host to bridge: ParameterPayload
	getParameter,  // host to bridge: ParameterPayload
	reply,         // either way: ReplyPayload (+ data)
	callback,      // bridge to host, during a request: DispatchPayload (+ data)
	notify,        // bridge to host on notify: DispatchPayload (+ data), no reply
	quit,          // host to bridge, on control
	editorPosition,  // host to bridge, on control: EditorPositionPayload
};

struct Header
{
	uint32_t type;  // MsgType
	uint32_t size;  // of the payload that follows
};

struct HelloPayload
{
	uint32_t version;
	uint32_t connection;  // Connection
	char token[kTokenLength];
};

// The AEffect fields the host mirrors. Sent with every reply, as a plugin
// may change them at any time (audioMasterIOChanged).
struct EffectInfo
{
	int32_t numPrograms;
	int32_t numParams;
	int32_t numInputs;
	int32_t numOutputs;
	int32_t flags;
	int32_t initialDelay;
	int32_t uniqueID;
	int32_t version;
};

struct LoadedPayload
{
	int32_t ok;
	int32_t pad;
	EffectInfo info;
	// Not ok: the error text follows.
};

struct DispatchPayload
{
	int32_t opcode;
	int32_t index;
	int64_t value;
	int64_t ptr;  // an X11 window for effEditOpen, else unused
	float opt;
	uint32_t pad;
	// The data [ptr] points to follows, as the opcode needs (DataKind).
};

struct ProcessPayload
{
	int32_t numInputs;
	int32_t numOutputs;
	int32_t frames;
	int32_t pad;
	// numInputs * frames input samples follow; the reply carries
	// numOutputs * frames output samples.
};

struct ParameterPayload
{
	int32_t index;
	float value;
};

// Where the editor's window is on the screen (inside the tracker's), for
// Wine to translate mouse positions; the first one also shows it.
struct EditorPositionPayload
{
	int32_t x;
	int32_t y;
};

struct ReplyPayload
{
	int64_t result;
	float value;  // getParameter
	uint32_t pad;
	EffectInfo info;
	// The data [ptr] points to follows, as the opcode needs.
};

// What [ptr] (or the result) of an opcode is, and so what travels with the
// request and the reply.
enum class DataKind
{
	none,
	stringIn,        // char * read by the callee
	stringOut,       // char[256] written by the callee
	stringInOut,     // both (effString2Parameter)
	structIn,        // a struct of the given size, read
	structOut,       // a struct of the given size, written
	structInOut,     // both
	special,         // handled by name (chunks, events, editor, pointers returned)
};

// Strings returned through [ptr] get this much room, more than VST asks for.
inline constexpr std::size_t kStringSize = 256;

// Serialised VstEvents: count, then per event its type, its size and its
// bytes (a VstEvent of that size); a SysEx event's dump follows it, and its
// pointer is rebuilt on the other side. Same layout on 32 and 64 bits.
struct EventHeader
{
	int32_t type;
	int32_t byteSize;
	int32_t dumpSize;  // SysEx bytes following the event, else 0
	int32_t pad;
};

// Socket I/O for both sides. SocketType is int (Linux) or SOCKET (Windows);
// the functions below are implemented by each side for its own sockets.
template <typename T>
inline void Append(std::vector<char> &out, const T &value)
{
	const char *p = reinterpret_cast<const char *>(&value);
	out.insert(out.end(), p, p + sizeof(T));
}

inline void AppendBytes(std::vector<char> &out, const void *data, std::size_t size)
{
	const char *p = static_cast<const char *>(data);
	out.insert(out.end(), p, p + size);
}

// A message: header and payload, built in one buffer.
inline std::vector<char> MakeMessage(MsgType type, const void *payload, std::size_t payloadSize, const void *data = nullptr, std::size_t dataSize = 0)
{
	std::vector<char> out;
	out.reserve(sizeof(Header) + payloadSize + dataSize);
	Header header{static_cast<uint32_t>(type), static_cast<uint32_t>(payloadSize + dataSize)};
	Append(out, header);
	AppendBytes(out, payload, payloadSize);
	if(dataSize)
		AppendBytes(out, data, dataSize);
	return out;
}

} // namespace vstbridge
