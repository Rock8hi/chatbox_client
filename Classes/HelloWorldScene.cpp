#include "HelloWorldScene.h"
#include "CocosNet/cocos-net.h"

#include "ui/CocosGUI.h"
#include "editor-support/cocostudio/CocoStudio.h"

#include "json/document.h"
#include "json/writer.h"
#include "json/stringbuffer.h"

using namespace rapidjson;
using namespace cocostudio;
using namespace cocos2d;
using namespace cocos2d::ui;

static cocos2d::Node* seekWidgetByName(cocos2d::Node* root, const std::string& name)
{
	if (!root)
	{
		return nullptr;
	}
	if (root->getName() == name)
	{
		return root;
	}
	const auto& arrayRootChildren = root->getChildren();
	for (auto& subWidget : arrayRootChildren)
	{
		cocos2d::Node* child = dynamic_cast<cocos2d::Node*>(subWidget);
		if (child)
		{
			cocos2d::Node* res = seekWidgetByName(child,name);
			if (res != nullptr)
			{
				return res;
			}
		}
	}
	return nullptr;
}

HelloWorld::HelloWorld()
{
	CCNetCentre::getInstance().setSocketStatusHandler(this, socket_status_selector(HelloWorld::onSocketStatusCallback));
	CCNetCentre::getInstance().addMessageHandler("HelloWorld", "create_user", this, message_selector(HelloWorld::onCreateUserCallback));
	CCNetCentre::getInstance().addMessageHandler("HelloWorld", "notify_remove_user", this, message_selector(HelloWorld::onNotifyRemoveUserCallback));
	CCNetCentre::getInstance().addMessageHandler("HelloWorld", "notify_new_user", this, message_selector(HelloWorld::onNotifyUserListCallback));
	CCNetCentre::getInstance().addMessageHandler("HelloWorld", "get_user_list", this, message_selector(HelloWorld::onGetUserListCallback));
	CCNetCentre::getInstance().addMessageHandler("HelloWorld", "notify_message", this, message_selector(HelloWorld::onNotifyChatMessageCallback));
}

HelloWorld::~HelloWorld()
{
	CCNetCentre::getInstance().removeSocketStatusHandler();
	CCNetCentre::getInstance().clearMessageHandler("HelloWorld");
}

Scene* HelloWorld::createScene()
{
    // 'scene' is an autorelease object
    auto scene = Scene::create();
    
    // 'layer' is an autorelease object
    auto layer = HelloWorld::create();

    // add layer as a child to scene
    scene->addChild(layer);

    // return the scene
    return scene;
}

// on "init" you need to initialize your instance
bool HelloWorld::init()
{
    //////////////////////////////
    // 1. super init first
    if ( !Layer::init() )
    {
        return false;
    }

	m_pRoot = static_cast<__LayerRGBA*>(CSLoader::getInstance()->createNode("chat_box/ChatBoxLayer.csb"));
	this->addChild(m_pRoot);

	auto pTextField = static_cast<TextField*>(seekWidgetByName(m_pRoot, "TextField_Input"));
	//pTextField->addEventListener(CC_CALLBACK_2(LoginLayer::onInputCallback, this));

	auto btnSend = static_cast<Layout*>(seekWidgetByName(m_pRoot, "Button_Send"));
	btnSend->addClickEventListener([=](Ref *pSender){
		std::string msg = pTextField->getString();
		if (msg.empty())
			return;

		char buffer[256];
		memset(buffer, 0x0, sizeof(buffer));
		sprintf(buffer+2, "{\"cmd\":\"send_message\",\"chat\":\"%s\"}", msg.c_str());
		int16_t len = strlen(buffer+2);
		*((int16_t*)buffer) = htons(len);

		CCNetCentre::getInstance().send(buffer, len + 2);

		pTextField->setString("");
	});

	CCNetCentre::getInstance().setInetAddress(CCInetAddress("115.28.107.191", 8888));
	CCNetCentre::getInstance().connect();
    
    return true;
}

void HelloWorld::onSocketStatusCallback(cocos2d::CCSocketStatus status)
{
	cocos2d::log("onZSocketStatusCallback: %d", status);
	if (status == eSocketConnected)
	{
		char buffer[256];
		memset(buffer, 0x0, sizeof(buffer));
		sprintf(buffer+2, "{\"cmd\":\"create_user\",\"name\":\"user_%d\"}", cocos2d::random());
		int16_t len = strlen(buffer+2);
		*((int16_t*)buffer) = htons(len);

		CCNetCentre::getInstance().send(buffer, len + 2);
	}
}

void HelloWorld::onCreateUserCallback(cocos2d::CCBuffer& oBuffer)
{
	unsigned short size = ntohs(oBuffer.readUShort());
	std::string msg = oBuffer.readString(size);

	Document doc;
	doc.Parse<0>(msg.c_str());

	if(doc.HasParseError())
	{
		cocos2d::log("GetParseError%s\n", doc.GetParseError());
		return;
	}

	const char *code = DICTOOL->getStringValue_json(doc, "code");
	if (strcmp(code, "ok"))
	{
		cocos2d::log("create user fail, error code: %s", code);
		return;
	}

	const char *get_user_list = "{\"cmd\":\"get_user_list\"}";

	char buffer[256];
	memset(buffer, 0x0, sizeof(buffer));
	sprintf(buffer+2, "%s", get_user_list);
	int16_t len = strlen(get_user_list);
	*((int16_t*)buffer) = htons(len);

	CCNetCentre::getInstance().send(buffer, len + 2);
}

void HelloWorld::onNotifyUserListCallback(cocos2d::CCBuffer& oBuffer)
{
	unsigned short size = ntohs(oBuffer.readUShort());
	std::string msg = oBuffer.readString(size);

	Document doc;
	doc.Parse<0>(msg.c_str());

	if(doc.HasParseError())
	{
		cocos2d::log("GetParseError%s\n", doc.GetParseError());
		return;
	}

	const char *code = DICTOOL->getStringValue_json(doc, "code");
	if (strcmp(code, "ok"))
	{
		return;
	}

	auto pListViewUser = static_cast<ListView*>(seekWidgetByName(m_pRoot, "ListView_UserList"));

	const rapidjson::Value& sub = DICTOOL->getSubDictionary_json(doc, "users");
	const char *name = DICTOOL->getStringValue_json(sub, "name");
	int userid = DICTOOL->getIntValue_json(sub, "userid");

	auto pName = Text::create(name, "", 36.f);
	pName->setTag(userid);

	pListViewUser->pushBackCustomItem(pName);
	pListViewUser->forceDoLayout();
	pListViewUser->jumpToBottom();
}

void HelloWorld::onGetUserListCallback(cocos2d::CCBuffer& oBuffer)
{
	unsigned short size = ntohs(oBuffer.readUShort());
	std::string msg = oBuffer.readString(size);

	Document doc;
	doc.Parse<0>(msg.c_str());

	if(doc.HasParseError())
	{
		cocos2d::log("GetParseError%s\n", doc.GetParseError());
		return;
	}

	const char *code = DICTOOL->getStringValue_json(doc, "code");
	if (strcmp(code, "ok"))
	{
		return;
	}

	auto pListViewUser = static_cast<ListView*>(seekWidgetByName(m_pRoot, "ListView_UserList"));
	pListViewUser->removeAllItems();

	int len = DICTOOL->getArrayCount_json(doc, "users");
	for (int i = 0; i < len; ++i)
	{
		const rapidjson::Value& sub = DICTOOL->getSubDictionary_json(doc, "users", i);
		const char *name = DICTOOL->getStringValue_json(sub, "name");
		int userid = DICTOOL->getIntValue_json(sub, "userid");

		auto pName = Text::create(name, "", 36.f);
		pName->setTag(userid);

		pListViewUser->pushBackCustomItem(pName);
	}
	pListViewUser->forceDoLayout();
	pListViewUser->jumpToBottom();
}

void HelloWorld::onNotifyChatMessageCallback(cocos2d::CCBuffer& oBuffer)
{
	unsigned short size = ntohs(oBuffer.readUShort());
	std::string msg = oBuffer.readString(size);

	Document doc;
	doc.Parse<0>(msg.c_str());

	if(doc.HasParseError())
	{
		cocos2d::log("GetParseError%s\n", doc.GetParseError());
		return;
	}

	const char *code = DICTOOL->getStringValue_json(doc, "code");
	if (strcmp(code, "ok"))
	{
		return;
	}

	auto pListViewUser = static_cast<ListView*>(seekWidgetByName(m_pRoot, "ListView_ChatMessage"));

	const char *chat = DICTOOL->getStringValue_json(doc, "chat");
	auto pChat = Text::create(chat, "", 36.f);
	pListViewUser->pushBackCustomItem(pChat);
	pListViewUser->forceDoLayout();
	pListViewUser->jumpToBottom();
}

void HelloWorld::onNotifyRemoveUserCallback(cocos2d::CCBuffer& oBuffer)
{
	unsigned short size = ntohs(oBuffer.readUShort());
	std::string msg = oBuffer.readString(size);

	Document doc;
	doc.Parse<0>(msg.c_str());

	if(doc.HasParseError())
	{
		cocos2d::log("GetParseError%s\n", doc.GetParseError());
		return;
	}

	const char *code = DICTOOL->getStringValue_json(doc, "code");
	if (strcmp(code, "ok"))
	{
		return;
	}

	const rapidjson::Value& sub = DICTOOL->getSubDictionary_json(doc, "users");
	int userid = DICTOOL->getIntValue_json(sub, "userid");
	const char *name = DICTOOL->getStringValue_json(sub, "name");

	auto pListViewUser = static_cast<ListView*>(seekWidgetByName(m_pRoot, "ListView_UserList"));
	auto items = pListViewUser->getItems();
	for (auto item : items)
	{
		int _userid = item->getTag();
		if (_userid == userid)
		{
			pListViewUser->removeItem(pListViewUser->getIndex(item));
			break;
		}
	}
}
