
#include "CCNetCentre.h"

//#include "CCLuaEngine.h"
#include "editor-support/cocostudio/CocoStudio.h"

#include "json/document.h"
#include "json/writer.h"
#include "json/stringbuffer.h"

using namespace rapidjson;
using namespace cocostudio;

NS_CC_BEGIN

CCNetCentre::CCNetCentre()
: m_fSoTimeout(SOCKET_SOTIMEOUT)
, m_eStatus(eSocketIoClosed)
, m_fConnectingDuration(0.0f)
, m_bRunSchedule(false)
, m_pTarget(NULL)
, m_oCallfunc(NULL)
{
	
}

CCNetCentre::~CCNetCentre()
{
	m_oSocket.ccClose();

	while(!m_lSendBuffers.empty())
	{
		CC_SAFE_DELETE_ARRAY(m_lSendBuffers.front().pBuffer);
		m_lSendBuffers.pop_front();
	}
}

void CCNetCentre::setInetAddress(const CCInetAddress& oInetAddress)
{
	m_oInetAddress = oInetAddress;
}

const CCInetAddress& CCNetCentre::getInetAddress() const
{
	return m_oInetAddress;
}

void CCNetCentre::setSoTimeout(float fSoTimeout)
{
	m_fSoTimeout = fSoTimeout;
}

float CCNetCentre::getSoTimeout() const
{
	return m_fSoTimeout;
}

void CCNetCentre::send(char* pBuffer, unsigned int uLen)
{
	if( !pBuffer || uLen == 0 || !isConnected() )
		return;
	
#if USING_PACKAGE_HEAD_LENGTH
	CCBuffer* pBuf = new CCBuffer(pBuffer, uLen);
	pBuf->autorelease();
	send(pBuf);
#else
	char* pTemp = new char[uLen];
	memcpy(pTemp, pBuffer, uLen);

	_SENDBUFFER tBuf;
	tBuf.pBuffer = pTemp;
	tBuf.nLength = (int)uLen;
	tBuf.nOffset = 0;

	m_lSendBuffers.push_back(tBuf);
#endif
}

void CCNetCentre::send(CCBuffer* pBuffer)
{
	if( pBuffer->empty() || !isConnected() )
		return;

#if USING_PACKAGE_HEAD_LENGTH
	unsigned int u_len = pBuffer->length();
	pBuffer->moveRight(sizeof(unsigned int));
	pBuffer->moveWriterIndexToFront();
	pBuffer->writeUInt(u_len);
#endif

	pBuffer->moveReaderIndexToFront();
	char* pData = pBuffer->readWholeData();
	int nLength = (int)pBuffer->length();
	pBuffer->moveReaderIndexToFront();

	_SENDBUFFER tBuf;
	tBuf.pBuffer = pData;
	tBuf.nLength = nLength;
	tBuf.nOffset = 0;

	m_lSendBuffers.push_back(tBuf);
}

bool CCNetCentre::isConnected()
{
	return m_eStatus == eSocketConnected;
}

bool CCNetCentre::connect()
{
	if( m_eStatus != eSocketConnected && m_eStatus != eSocketConnecting )
	{
		m_oSocket.setInetAddress(m_oInetAddress);
		if( m_oSocket.ccConnect() )
		{
			registerScheduler();
			m_eStatus = eSocketConnecting;
			return true;
		}
		else
		{
			m_oSocket.ccClose();
			m_eStatus = eSocketConnectFailed;
			onExceptionCaught(eSocketConnectFailed);
		}
	}
	return false;
}

void CCNetCentre::disconnect()
{
	if( m_eStatus == eSocketConnected )
	{
		unregisterScheduler();
		m_oSocket.ccDisconnect();
		m_eStatus = eSocketDisconnected;
		onDisconnected();
	}
}

void CCNetCentre::close()
{
	if( m_eStatus == eSocketConnected )
	{
		unregisterScheduler();
		m_oSocket.ccClose();
		m_eStatus = eSocketIoClosed;
		onDisconnected();
	}
}

void CCNetCentre::runSchedule(float dt)
{
	switch( m_eStatus )
	{
	case eSocketConnecting:
		{
			switch( m_oSocket.ccIsConnected() )
			{
			case eSocketConnected:
				{
					m_eStatus = eSocketConnected;
					onConnected();
				}
				break;
			case eSocketConnectFailed:
				{
					unregisterScheduler();
                    m_oSocket.ccClose();
					m_eStatus = eSocketConnectFailed;
					onExceptionCaught(eSocketConnectFailed);
				}
				break;
			case eSocketConnecting:
				{
					if( m_fConnectingDuration > m_fSoTimeout )
					{
						unregisterScheduler();
						m_oSocket.ccDisconnect();
						m_eStatus = eSocketDisconnected;
						onConnectTimeout();
						m_fConnectingDuration = 0.0f;
					}
					else
					{
						m_fConnectingDuration += dt;
					}
				}
				break;
			default:
				break;
			}
		}
		break;
	case eSocketConnected:
		{
#if HANDLE_ON_SINGLE_FRAME
			while( m_oSocket.ccIsReadable() )
#else
			if( m_oSocket.ccIsReadable() )
#endif
			{
				if( this->runRead() ) return;
			}

#if HANDLE_ON_SINGLE_FRAME
			while( m_oSocket.ccIsWritable() && !m_lSendBuffers.empty() )
#else
			if( m_oSocket.ccIsWritable() && !m_lSendBuffers.empty() )
#endif
			{
				if( this->runWrite() ) return;
			}
		}
		break;
	default:
		break;
	}
}

bool CCNetCentre::runRead()
{
	int nRet = m_oSocket.ccRead(m_pReadBuffer, SOCKET_READ_BUFFER_SIZE);
	if( nRet == eSocketIoError || nRet == eSocketIoClosed )
	{
		unregisterScheduler();
		m_oSocket.ccClose();
		m_eStatus = eSocketIoClosed;
		onDisconnected();
		return true;
	}
	else
	{
#if USING_SOCKET_PRINT_DEBUG
		printf("\nSOCKET READ LENGTH: %d\n", nRet);
		for (int i = 0; i < nRet; ++i)
		{
			printf("0x%02X ", m_pReadBuffer[i]);
		}
		printf("\n\n");
#endif
		m_oReadBuffer.writeData(m_pReadBuffer, (unsigned int)nRet);
#if USING_PACKAGE_HEAD_LENGTH
		while( m_oReadBuffer.isReadable(sizeof(int)) )
		{
			m_oReadBuffer.moveReaderIndexToFront();
			int n_head_len = m_oReadBuffer.readInt();
			if( n_head_len <= 0 )
			{
				cocos2d::log("invalidate head length");
				m_oReadBuffer.moveLeft(sizeof(int));
			}

			int n_content_len = (int)m_oReadBuffer.length();
			if( n_content_len - (int)(sizeof(int)) >= n_head_len )
			{
				m_oReadBuffer.moveLeft(sizeof(unsigned int));
				CCBuffer* pData = m_oReadBuffer.readData(n_head_len);
				m_oReadBuffer.moveLeft(n_head_len);
				m_oReadBuffer.moveReaderIndexToFront();
				m_oReadBuffer.moveWriterIndexToBack();

				onMessageReceived(*pData);
			}
			else
			{
				break;
			}
		}
#else
		/*CCBuffer* pData = (CCBuffer*) m_oReadBuffer.copy();
		pData->autorelease();
		m_oReadBuffer.clear();
		
		onMessageReceived(*pData);*/

		// 包头四个ASC英文字符占4个byte，后面跟4byte包长度(包长度包括长度自身所占4byte)，共8byte
		// |HDZY(4byte)|len(int;4byte)|cmd(string;short+data;2byte+data)|data(Nbyte)|
		unsigned int protocol_header_size = 0; //strlen(PROTOCOL_HEADER_SIGN_ASC);
		unsigned int protocol_length_size = 2; //sizeof(uint16_t);
		unsigned int header_and_length_size = protocol_header_size + protocol_length_size;
		while( m_oReadBuffer.isReadable(header_and_length_size) )
		{
			m_oReadBuffer.moveReaderIndexToFront();
			//m_oReadBuffer.skipData(protocol_header_size); //skip protocol_header_size
			unsigned short tmp_len = m_oReadBuffer.readUShort();
			unsigned int n_head_len = ntohs(tmp_len);
			if( n_head_len == 0 )
			{
				CCASSERT(false, "invalidate head length");
				return true;
			}

			unsigned int n_buffer_len = m_oReadBuffer.length();
			if( n_buffer_len - protocol_header_size >= n_head_len )
			{
				m_oReadBuffer.moveReaderIndexToFront();
				CCBuffer* pData = m_oReadBuffer.readData(n_head_len + protocol_header_size + 2);
				m_oReadBuffer.moveLeft(n_head_len + protocol_header_size + 2); //接收缓存区左移，删除已经读取的数据
				m_oReadBuffer.moveReaderIndexToFront();
				m_oReadBuffer.moveWriterIndexToBack();

				onMessageReceived(*pData);
			}
			else
			{
				break;
			}
		}
#endif
	}
	return false;
}

bool CCNetCentre::runWrite()
{
	_SENDBUFFER& tBuffer = m_lSendBuffers.front();

	int nRet = m_oSocket.ccWrite(tBuffer.pBuffer + tBuffer.nOffset, tBuffer.nLength - tBuffer.nOffset);
#if USING_SOCKET_PRINT_DEBUG
	printf("\nSOCKET WRITE LENGTH: %d\n", nRet);

	char *buf = tBuffer.pBuffer + tBuffer.nOffset;
	int size = tBuffer.nLength - tBuffer.nOffset;
	for (int i = 0; i < size; ++i)
	{
		printf("0x%02X ", buf[i]);
	}
	printf("\n\n");
#endif
	if( nRet == eSocketIoError )
	{
		unregisterScheduler();
		m_oSocket.ccClose();
		m_eStatus = eSocketIoClosed;
		onDisconnected();
		return true;
	}
	else if( nRet == tBuffer.nLength - tBuffer.nOffset )
	{
		CC_SAFE_DELETE_ARRAY(tBuffer.pBuffer);
		m_lSendBuffers.pop_front();
	}
	else
	{
		tBuffer.nOffset += nRet;
	}
	return false;
}

void CCNetCentre::registerScheduler()
{
	if( m_bRunSchedule )
		return;

	Director::getInstance()->getScheduler()->schedule(
		schedule_selector(CCNetCentre::runSchedule), 
		this, 
		0.0f, 
		false
	);
	m_bRunSchedule = true;
}

void CCNetCentre::unregisterScheduler()
{
	if( !m_bRunSchedule )
		return;

	Director::getInstance()->getScheduler()->unschedule(
		schedule_selector(CCNetCentre::runSchedule),
		this
	);
	m_bRunSchedule = false;
}

#ifdef WIN32
std::string get_data()
{
	time_t t  = time(NULL);
	struct tm *timeinfo = localtime(&t);

	char pbuf[128] = {0};
	strftime(pbuf,sizeof(pbuf), "%c", timeinfo);

	return std::string(pbuf);
}

void save_file(const char *log)
{
	std::string path = "d:/cocos2d-ddz.log";

	FILE *fp = fopen(path.c_str(), "a+");

	if (! fp)
	{
		cocos2d::log("can't create file %s", path.c_str());
		return;
	}

	std::string pbuf = "\n";
	pbuf.append(get_data());
	pbuf.append(" : ");
	pbuf.append(log);

	fwrite(pbuf.c_str(), 1, pbuf.size(), fp);

	fclose(fp);
}
#endif // WIN32

void CCNetCentre::onMessageReceived(CCBuffer& oBuffer)
{
	unsigned short size = ntohs(oBuffer.readUShort());
	std::string msg = oBuffer.readString(size);
	cocos2d::log("onMessageReceived: %s", msg.c_str());

#ifdef WIN32
	save_file(msg.c_str());
#endif

	Document doc;
	doc.Parse<0>(msg.c_str());

	if(doc.HasParseError())
	{
		cocos2d::log("GetParseError%s\n", doc.GetParseError());
		return;
	}

	std::string cmd = DICTOOL->getStringValue_json(doc, "cmd");

	unsigned int idx = oBuffer.getReaderIndex();
	
	std::map<std::string, MsgCppHandlerMap>::iterator itor1 = m_oMessageCppHandlers.begin();
	for (; itor1 != m_oMessageCppHandlers.end(); itor1++)
	{
		if (itor1->second.find(cmd) != itor1->second.end())
		{
			Ref *target = itor1->second[cmd].target;
			MessageHandler callfunc = itor1->second[cmd].callfunc;
			if (target != NULL && callfunc != NULL)
			{
				oBuffer.moveReaderIndexToFront();
				(target->*callfunc)(oBuffer);
				oBuffer.setReaderIndex(idx);
			}
		}
	}
	
	//std::map<std::string, MsgLuaHandlerMap>::iterator itor2 = m_oMessageLuaHandlers.begin();
	//for (; itor2 != m_oMessageLuaHandlers.end(); itor2++)
	//{
	//	if (itor2->second.find(cmd) != itor2->second.end())
	//	{
	//		CCLuaStack *pStack = CCLuaEngine::defaultEngine()->getLuaStack();

	//		pStack->pushCCObject(&oBuffer, "CCBuffer");
	//		pStack->executeFunctionByHandler(itor2->second[cmd], 1);
	//		pStack->clean();

	//		oBuffer.setReaderIndex(idx);
	//	}
	//}
}

void CCNetCentre::onConnected()
{
	cocos2d::log("socket connected");
	if (m_pTarget != NULL && m_oCallfunc != NULL)
	{
		(m_pTarget->*m_oCallfunc)(eSocketConnected);
	}
}

void CCNetCentre::onConnectTimeout()
{
	cocos2d::log("socket connect timeout");
	if (m_pTarget != NULL && m_oCallfunc != NULL)
	{
		(m_pTarget->*m_oCallfunc)(eSocketConnectTimeout);
	}
}

void CCNetCentre::onDisconnected()
{
	cocos2d::log("socket disconnect");
	if (m_pTarget != NULL && m_oCallfunc != NULL)
	{
		(m_pTarget->*m_oCallfunc)(eSocketDisconnected);
	}
}

void CCNetCentre::onExceptionCaught(CCSocketStatus eStatus)
{
	cocos2d::log("socket exception");
	if (m_pTarget != NULL && m_oCallfunc != NULL)
	{
		(m_pTarget->*m_oCallfunc)(eSocketCreateFailed);
	}
}

void CCNetCentre::addMessageLuaHandler(const std::string &scene, const std::string &type, LUA_HANDLER handler)
{
	if (m_oMessageLuaHandlers.find(scene) == m_oMessageLuaHandlers.end())
	{
		MsgLuaHandlerMap handlers;
		m_oMessageLuaHandlers.insert(std::make_pair(scene, handlers));
	}

	std::map<std::string, MsgLuaHandlerMap>::iterator itor = m_oMessageLuaHandlers.find(scene);
	if (itor != m_oMessageLuaHandlers.end())
	{
		if (itor->second.find(type) != itor->second.end())
		{
			CCAssert(false, CCString::createWithFormat("please don't repeat add it -> %s (lua)", type.c_str())->getCString());
			return;
		}

		itor->second.insert(std::make_pair(type, handler));
	}
}

void CCNetCentre::removeMessageLuaHandler(const std::string &scene, const std::string &type)
{
	std::map<std::string, MsgLuaHandlerMap>::iterator handlers = m_oMessageLuaHandlers.find(scene);
	if (handlers != m_oMessageLuaHandlers.end())
	{
		if (handlers->second.find(type) != handlers->second.end())
		{
			handlers->second.erase(type);
		}
	}
}

void CCNetCentre::clearAllMessageLuaHandler(const std::string &scene)
{
	if (m_oMessageLuaHandlers.find(scene) != m_oMessageLuaHandlers.end())
	{
		m_oMessageLuaHandlers.erase(scene);
	}
}

void CCNetCentre::addMessageLuaHandler(const std::string &type, LUA_HANDLER handler)
{
	addMessageLuaHandler("LUA", type, handler);
}

void CCNetCentre::removeMessageLuaHandler(const std::string &type)
{
	removeMessageLuaHandler("LUA", type);
}

void CCNetCentre::addMessageHandler(const std::string &scene, const std::string &type, Ref *target, MessageHandler callfunc)
{
	if (m_oMessageCppHandlers.find(scene) == m_oMessageCppHandlers.end())
	{
		MsgCppHandlerMap handlers;
		m_oMessageCppHandlers.insert(std::make_pair(scene, handlers));
	}

	std::map<std::string, MsgCppHandlerMap>::iterator itor = m_oMessageCppHandlers.find(scene);
	if (itor != m_oMessageCppHandlers.end())
	{
		if (itor->second.find(type) != itor->second.end())
		{
			//CCAssert(false, CCString::createWithFormat("please don't repeat add it -> %s (cpp)", type.c_str())->getCString());
			//return;
			itor->second.erase(type);
		}

		CPP_HANDLER handler(target, callfunc);
		itor->second.insert(std::make_pair(type, handler));
	}
}

void CCNetCentre::removeMessageHandler(const std::string &scene, const std::string &type)
{
	std::map<std::string, MsgCppHandlerMap>::iterator handlers = m_oMessageCppHandlers.find(scene);
	if (handlers != m_oMessageCppHandlers.end())
	{
		if (handlers->second.find(type) != handlers->second.end())
		{
			handlers->second.erase(type);
		}
	}
}

void CCNetCentre::clearMessageHandler(const std::string &scene)
{
	if (m_oMessageCppHandlers.find(scene) != m_oMessageCppHandlers.end())
	{
		m_oMessageCppHandlers.erase(scene);
	}
}

void CCNetCentre::setSocketStatusHandler(Ref *target, SocketStatusHandler callfunc)
{
	m_pTarget = target;
	m_oCallfunc = callfunc;
}

void CCNetCentre::removeSocketStatusHandler()
{
	m_pTarget = NULL;
	m_oCallfunc = NULL;
}

void CCNetCentre::sendMessage(CCBuffer* body)
{

}

void CCNetCentre::forceSendMessage(CCBuffer* body)
{
	sendMessage(body);
	if (!m_lSendBuffers.empty())
	{
		runSchedule(0.f);
	}
}

CCNetCentre& CCNetCentre::getInstance()
{
	static CCNetCentre thiz;
	return thiz;
}


NS_CC_END
