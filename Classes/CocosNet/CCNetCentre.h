
#ifndef __CCNET_CENTRE_H__
#define __CCNET_CENTRE_H__

#include "cocos2d.h"
#include "CCNetMacros.h"
#include "CCSocket.h"

NS_CC_BEGIN

typedef void (Ref::*SocketStatusHandler)(CCSocketStatus status);
#define socket_status_selector(_SELECTOR) (SocketStatusHandler)(&_SELECTOR)

typedef void (Ref::*MessageHandler)(CCBuffer& oBuffer);
#define message_selector(_SELECTOR) (MessageHandler)(&_SELECTOR)

typedef struct _CPP_HANDLER
{
	Ref          *target;
	MessageHandler    callfunc;

	_CPP_HANDLER(Ref *h, MessageHandler m) : target(h) , callfunc(m) { }

	_CPP_HANDLER() : target(NULL) , callfunc(NULL) { }
} CPP_HANDLER;

typedef std::map<std::string, CPP_HANDLER> MsgCppHandlerMap;

typedef int LUA_HANDLER;
typedef std::map<std::string, LUA_HANDLER> MsgLuaHandlerMap;

/**
 * class  : CCNetCentre
 * author : Jason lee
 * email  : jason.lee.c@foxmail.com
 * descpt : the net delegate, use it as connector
 */
class CCNetCentre : public Ref
{
public:
	CCNetCentre();
	virtual ~CCNetCentre();

	static CCNetCentre& getInstance();

public:
	// set target address
	void setInetAddress(const CCInetAddress& oInetAddress);

	// get target address
	const CCInetAddress& getInetAddress() const;
	
	// the time out of connecting
	void setSoTimeout(float fSoTimeout);

	// get the time out value
	float getSoTimeout() const;

	// send package to target address
	void send(char* pBuffer, unsigned int uLen);

	// send package to target address
	void send(CCBuffer* pBuffer);

	// check the net status
	bool isConnected();

	// close the socket
	void close();

	// connect to target address
	bool connect();

	// disconnect as close for now
	void disconnect();

private:
	// read data on every frame, if needed
	bool runRead();

	// send data on every frame, if needed
	bool runWrite();

	// frame call
	void runSchedule(float dt);
	
	// registe the function of frame called
	void registerScheduler();

	// unregiste the function of frame called
	void unregisterScheduler();

private:

	/**
	 * struct : _SENDBUFFER
	 * author : Jason lee
	 * email  : jason.lee.c@foxmail.com
	 * descpt : send data
	 */
	struct _SENDBUFFER
	{
		char* pBuffer;       // the data
		int nOffset;         // the send data offset
		int nLength;         // data's length
	};
	
private:
	bool                   m_bRunSchedule;
	float                  m_fConnectingDuration;
	float                  m_fSoTimeout;
	std::list<_SENDBUFFER> m_lSendBuffers;
	CCBuffer               m_oReadBuffer;
	CCInetAddress          m_oInetAddress;
	CCSocket               m_oSocket;
	char                   m_pReadBuffer[SOCKET_READ_BUFFER_SIZE];

protected:
	CCSocketStatus         m_eStatus;

private:
	// will calling when a package is coming
	void onMessageReceived(CCBuffer& oBuffer);

	// when connected will calling
	void onConnected();

	// when connect time out will calling
	void onConnectTimeout();

	// on disconnected will call
	void onDisconnected();

	// on exception
	void onExceptionCaught(CCSocketStatus eStatus);

public:
	void addMessageLuaHandler(const std::string &scene, const std::string &type, LUA_HANDLER handler);
	void removeMessageLuaHandler(const std::string &scene, const std::string &type);
	void clearAllMessageLuaHandler(const std::string &scene);

	void addMessageLuaHandler(const std::string &type, LUA_HANDLER handler);
	void removeMessageLuaHandler(const std::string &type);

	void addMessageHandler(const std::string &scene, const std::string &type, Ref *target, MessageHandler callfunc);
	void removeMessageHandler(const std::string &scene, const std::string &type);
	void clearMessageHandler(const std::string &scene);

	void setSocketStatusHandler(Ref *target, SocketStatusHandler callfunc);
	void removeSocketStatusHandler();

	void sendMessage(CCBuffer* pBuffer);
	void forceSendMessage(CCBuffer* pBuffer);

private:
	std::map<std::string, MsgCppHandlerMap> m_oMessageCppHandlers;
	std::map<std::string, MsgLuaHandlerMap> m_oMessageLuaHandlers;

	Ref *m_pTarget;
	SocketStatusHandler m_oCallfunc;
};

NS_CC_END

#endif //__CCNET_CENTRE_H__