/*
 * IPCWindow.cpp
 * -------------
 * Purpose: Hidden window to receive file open commands from another OpenMPT instance
 * Notes  : mfcless twin of upstream's mptrack/IPCWindow.cpp. See the header:
 *          a QLocalServer per instance in $XDG_RUNTIME_DIR/openmpt-ipc (or
 *          the temporary folder). A request is the function and the data's
 *          size (two 32-bit numbers) and the data; the answer is a 64-bit
 *          result. Strings are sent as UTF-8.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "IPCWindow.h"
#include "mfcless/QtString.h"
#include "Mainfrm.h"
#include "ModDocTemplate.h"
#include "Mptrack.h"
#include "MPTrackUtil.h"

#include "../common/version.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtNetwork/QLocalServer>
#include <QtNetwork/QLocalSocket>

#include <cstring>


OPENMPT_NAMESPACE_BEGIN

namespace IPCWindow
{

	static QLocalServer *ipcServer = nullptr;
	static uint64 lastUsed = 0;

	static constexpr int TimeoutMs = 5000;

	static QString SocketFolder()
	{
		QString base = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
		if(base.isEmpty())
			base = QDir::tempPath();
		return QDir(base).filePath(QStringLiteral("openmpt-ipc"));
	}

	static mpt::ustring DataToString(const QByteArray &data)
	{
		return mpt::ToUnicode(mpt::Charset::UTF8, std::string(data.constData(), static_cast<std::size_t>(data.size())));
	}

	static std::string PathToBytes(const mpt::PathString &path)
	{
		return mpt::ToCharset(mpt::Charset::UTF8, path.ToUnicode());
	}

	static int64 Architecture()
	{
		return static_cast<int64>(sizeof(void *) * 8);
	}

	static int64 HandleRequest(Function function, const QByteArray &data)
	{
		int64 result = 0;
		switch(function)
		{
		case Function::Open:
			result = theApp.OpenDocumentFile(mpt::PathString::FromUnicode(DataToString(data))) ? 1 : 2;
			break;
		case Function::SetWindowForeground:
			{
				auto mainWnd = CMainFrame::GetMainFrame();
				if(mainWnd)
				{
					if(mainWnd->isMinimized())
					{
						mainWnd->showNormal();
					}
					mainWnd->raise();
					mainWnd->activateWindow();
					result = 1;
				} else
				{
					result = 0;
				}
			}
			break;
		case Function::GetVersion:
			result = Version::Current().GetRawVersion();
			break;
		case Function::GetArchitecture:
			result = Architecture();
			break;
		case Function::HasSameBinaryPath:
			result = (theApp.GetInstallBinArchPath().ToUnicode() == DataToString(data)) ? 1 : 0;
			break;
		case Function::HasSameSettingsPath:
			result = (theApp.GetConfigPath().ToUnicode() == DataToString(data)) ? 1 : 0;
			break;
		case Function::PlayCurrent:
			if(CMainFrame::GetMainFrame())
			{
				if(CMainFrame::GetMainFrame()->PlayMod(CMainFrame::GetMainFrame()->GetActiveDoc()))
					result = 1;
			}
			break;
		case Function::GetLastUsed:
			result = static_cast<int64>(lastUsed);
			break;
		default:
			result = 0;
			break;
		}
		return result;
	}

	static void OnConnection()
	{
		while(QLocalSocket *socket = ipcServer->nextPendingConnection())
		{
			QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
			QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket]() {
				for(;;)
				{
					uint32 header[2];
					if(socket->bytesAvailable() < static_cast<qint64>(sizeof(header)))
						return;
					socket->peek(reinterpret_cast<char *>(header), sizeof(header));
					if(socket->bytesAvailable() < static_cast<qint64>(sizeof(header) + header[1]))
						return;
					socket->read(reinterpret_cast<char *>(header), sizeof(header));
					const QByteArray data = socket->read(header[1]);
					const int64 result = HandleRequest(static_cast<Function>(header[0]), data);
					socket->write(reinterpret_cast<const char *>(&result), sizeof(result));
					socket->flush();
				}
			});
		}
	}

	void Open()
	{
		if(ipcServer)
			return;
		const QString folder = SocketFolder();
		QDir().mkpath(folder);
		QFile::setPermissions(folder, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
		const QString name = QDir(folder).filePath(QString::number(QCoreApplication::applicationPid()));
		QLocalServer::removeServer(name);
		ipcServer = new QLocalServer(qApp);
		ipcServer->setSocketOptions(QLocalServer::UserAccessOption);
		QObject::connect(ipcServer, &QLocalServer::newConnection, ipcServer, &OnConnection);
		if(!ipcServer->listen(name))
		{
			delete ipcServer;
			ipcServer = nullptr;
		}
	}

	void Close()
	{
		if(ipcServer)
		{
			ipcServer->close();
			delete ipcServer;
			ipcServer = nullptr;
		}
	}

	void UpdateLastUsed()
	{
		lastUsed = Util::GetTickCount64() / 100;
	}

	intptr_t SendIPC(const QString &ipcWnd, Function function, mpt::const_byte_span data)
	{
		if(ipcWnd.isEmpty())
		{
			return 0;
		}
		if(!mpt::in_range<uint32>(data.size()))
		{
			return 0;
		}
		QLocalSocket socket;
		socket.connectToServer(ipcWnd);
		if(!socket.waitForConnected(TimeoutMs))
			return -1;
		const uint32 header[2] = {static_cast<uint32>(function), static_cast<uint32>(data.size())};
		socket.write(reinterpret_cast<const char *>(header), sizeof(header));
		if(!data.empty())
			socket.write(reinterpret_cast<const char *>(data.data()), static_cast<qint64>(data.size()));
		socket.flush();
		// As SendMessage, opening a file waits for the other instance however long it takes.
		const int timeout = (function == Function::Open) ? -1 : TimeoutMs;
		while(socket.bytesAvailable() < static_cast<qint64>(sizeof(int64)))
		{
			if(!socket.waitForReadyRead(timeout))
				return -1;
		}
		int64 result = 0;
		socket.read(reinterpret_cast<char *>(&result), sizeof(result));
		return static_cast<intptr_t>(result);
	}

	static intptr_t SendIPC(const QString &ipcWnd, Function function, const std::string &data)
	{
		return SendIPC(ipcWnd, function, mpt::const_byte_span(reinterpret_cast<const std::byte *>(data.data()), data.size()));
	}

	QString FindIPCWindow(FlagSet<InstanceRequirements> require)
	{
		const QString ownName = ipcServer ? ipcServer->fullServerName() : QString();
		QDir folder(SocketFolder());
		QString result;
		int64 resultLastActive = 0;
		for(const QString &entry : folder.entryList(QDir::System | QDir::Files | QDir::NoDotAndDotDot))
		{
			const QString hwnd = folder.filePath(entry);
			if(hwnd == ownName)
				continue;
			const int64 version = SendIPC(hwnd, Function::GetVersion);
			if(version == -1)
			{
				// Left behind by an instance that has gone
				QLocalSocket probe;
				probe.connectToServer(hwnd);
				if(!probe.waitForConnected(100) && probe.error() == QLocalSocket::ConnectionRefusedError)
					QLocalServer::removeServer(hwnd);
				continue;
			}
			if(require[SameVersion])
			{
				if(Version(static_cast<uint32>(version)) != Version::Current())
				{
					continue;
				}
			}
			if(require[SameArchitecture])
			{
				if(SendIPC(hwnd, Function::GetArchitecture) != Architecture())
				{
					continue;
				}
			}
			if(require[SamePath])
			{
				if(SendIPC(hwnd, Function::HasSameBinaryPath, PathToBytes(theApp.GetInstallBinArchPath())) != 1)
				{
					continue;
				}
			}
			if(require[SameSettings])
			{
				if(SendIPC(hwnd, Function::HasSameSettingsPath, PathToBytes(theApp.GetConfigPath())) != 1)
				{
					continue;
				}
			}
			const int64 lastActive = SendIPC(hwnd, Function::GetLastUsed);
			if(result.isEmpty() || lastActive >= resultLastActive)
			{
				result = hwnd;
				resultLastActive = lastActive;
			}
		}
		return result;
	}

	bool SendToIPC(const std::vector<mpt::PathString> &filenames, bool autoplay)
	{
		const QString ipcWnd = FindIPCWindow();
		if(ipcWnd.isEmpty())
		{
			return false;
		}
		SendIPC(ipcWnd, Function::SetWindowForeground);
		for(const auto &filename : filenames)
		{
			const intptr_t result = SendIPC(ipcWnd, Function::Open, PathToBytes(filename));
			if(result == 0 || result == -1)
			{
				return false;
			}
		}
		if(autoplay)
		{
			SendIPC(ipcWnd, Function::PlayCurrent);
		}
		return true;
	}

}

OPENMPT_NAMESPACE_END
