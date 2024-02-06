#include "Poco/Net/SocketReactor.h"
#include "Poco/Net/SocketAcceptor.h"
#include "Poco/Net/SocketNotification.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/NObserver.h"
#include "Poco/Exception.h"
#include "Poco/Thread.h"
#include "Poco/FIFOBuffer.h"
#include "Poco/Delegate.h"
#include "Poco/Util/ServerApplication.h"
#include "Poco/Util/Option.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/HelpFormatter.h"

#include <iostream>
#include <algorithm>


using Poco::Net::SocketReactor;
using Poco::Net::SocketAcceptor;
using Poco::Net::ReadableNotification;
using Poco::Net::WritableNotification;
using Poco::Net::ShutdownNotification;
using Poco::Net::ServerSocket;
using Poco::Net::StreamSocket;
using Poco::NObserver;
using Poco::AutoPtr;
using Poco::Thread;
using Poco::FIFOBuffer;
using Poco::delegate;
using Poco::Util::ServerApplication;
using Poco::Util::Application;
using Poco::Util::Option;
using Poco::Util::OptionSet;
using Poco::Util::HelpFormatter;


class ReversedEchoServiceHandler
{
public:
	ReversedEchoServiceHandler(StreamSocket& socket, SocketReactor& reactor):
		_socket(socket),
		_reactor(reactor),
		_fifoIn(BUFFER_SIZE, true),
		_fifoOut(BUFFER_SIZE, true)
	{
		Application& app = Application::instance();
		app.logger().information("Connection from " + socket.peerAddress().toString());

		_reactor.addEventHandler(_socket, NObserver<ReversedEchoServiceHandler, ReadableNotification>(*this, &ReversedEchoServiceHandler::onSocketReadable));
		_reactor.addEventHandler(_socket, NObserver<ReversedEchoServiceHandler, ShutdownNotification>(*this, &ReversedEchoServiceHandler::onSocketShutdown));

		_fifoOut.readable += delegate(this, &ReversedEchoServiceHandler::onFIFOOutReadable);
		_fifoIn.writable += delegate(this, &ReversedEchoServiceHandler::onFIFOInWritable);


		std::string greeting = "Welcome to POCO TCP server. Enter your string:\n";
		_fifoOut.write(&greeting[0], greeting.size());
	}

	~ReversedEchoServiceHandler()
	{
		Application& app = Application::instance();
		try
		{
			app.logger().information("Disconnecting " + _socket.peerAddress().toString());
		}
		catch (...)
		{
		}
		_reactor.removeEventHandler(_socket, NObserver<ReversedEchoServiceHandler, ReadableNotification>(*this, &ReversedEchoServiceHandler::onSocketReadable));
		_reactor.removeEventHandler(_socket, NObserver<ReversedEchoServiceHandler, WritableNotification>(*this, &ReversedEchoServiceHandler::onSocketWritable));
		_reactor.removeEventHandler(_socket, NObserver<ReversedEchoServiceHandler, ShutdownNotification>(*this, &ReversedEchoServiceHandler::onSocketShutdown));

		_fifoOut.readable -= delegate(this, &ReversedEchoServiceHandler::onFIFOOutReadable);
		_fifoIn.writable -= delegate(this, &ReversedEchoServiceHandler::onFIFOInWritable);
	}

	void onFIFOOutReadable(bool& b)
	{
		if (b)
			_reactor.addEventHandler(_socket, NObserver<ReversedEchoServiceHandler, WritableNotification>(*this, &ReversedEchoServiceHandler::onSocketWritable));
		else
			_reactor.removeEventHandler(_socket, NObserver<ReversedEchoServiceHandler, WritableNotification>(*this, &ReversedEchoServiceHandler::onSocketWritable));
	}

	void onFIFOInWritable(bool& b)
	{
		if (b)
			_reactor.addEventHandler(_socket, NObserver<ReversedEchoServiceHandler, ReadableNotification>(*this, &ReversedEchoServiceHandler::onSocketReadable));
		else
			_reactor.removeEventHandler(_socket, NObserver<ReversedEchoServiceHandler, ReadableNotification>(*this, &ReversedEchoServiceHandler::onSocketReadable));
	}

	void onSocketReadable(const AutoPtr<ReadableNotification>& pNf)
	{
		try
		{
			int len = _socket.receiveBytes(_fifoIn);
			if (len > 0)
			{
				std::string inputStr (&_fifoIn.buffer()[0], _fifoIn.used());
				std::reverse(inputStr.begin(), --inputStr.end());
				_fifoIn.drain(_fifoOut.write(&inputStr[0], inputStr.size()));
			}
			else
			{
				delete this;
			}
		}
		catch (Poco::Exception& exc)
		{
			Application& app = Application::instance();
			app.logger().log(exc);
			delete this;
		}
	}

	void onSocketWritable(const AutoPtr<WritableNotification>& pNf)
	{
		try
		{
			_socket.sendBytes(_fifoOut);
		}
		catch (Poco::Exception& exc)
		{
			Application& app = Application::instance();
			app.logger().log(exc);
			delete this;
		}
	}

	void onSocketShutdown(const AutoPtr<ShutdownNotification>& pNf)
	{
		delete this;
	}

private:
	enum
	{
		BUFFER_SIZE = 256
	};

	StreamSocket   _socket;
	SocketReactor& _reactor;
	FIFOBuffer     _fifoIn;
	FIFOBuffer     _fifoOut;
};


class ReversedEchoServer: public Poco::Util::ServerApplication
{
public:
	ReversedEchoServer(): _helpRequested(false)
	{
	}

	~ReversedEchoServer()
	{
	}

protected:
	void initialize(Application& self)
	{
		ServerApplication::initialize(self);
	}

	void uninitialize()
	{
		ServerApplication::uninitialize();
	}

	int main(const std::vector<std::string>& args)
	{
		unsigned short port = (unsigned short) config().getInt("ReversedEchoServer.port", 28888);

		// set-up a server socket
		ServerSocket svs(port);
		// set-up a SocketReactor...
		SocketReactor reactor;
		// ... and a SocketAcceptor
		SocketAcceptor<ReversedEchoServiceHandler> acceptor(svs, reactor);
		// run the reactor in its own thread so that we can wait for
		// a termination request
		Thread thread;
		thread.start(reactor);
		// wait for CTRL-C or kill
		waitForTerminationRequest();
		// Stop the SocketReactor
		reactor.stop();
		thread.join();

		return Application::EXIT_OK;
	}

private:
	bool _helpRequested;
};


int main(int argc, char** argv)
{
	ReversedEchoServer app;
	return app.run(argc, argv);
}