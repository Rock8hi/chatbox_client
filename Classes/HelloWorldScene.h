#ifndef __HELLOWORLD_SCENE_H__
#define __HELLOWORLD_SCENE_H__

#include "cocos2d.h"
#include "CocosNet/cocos-net.h"

class HelloWorld : public cocos2d::Layer
{
public:
	HelloWorld();
	~HelloWorld();

    static cocos2d::Scene* createScene();

    virtual bool init();
    
    // a selector callback
    void menuCloseCallback(cocos2d::Ref* pSender);
    
    // implement the "static create()" method manually
    CREATE_FUNC(HelloWorld);

	// socket×´Ì¬»Øµ÷
	void onSocketStatusCallback(cocos2d::CCSocketStatus status);

	// ´´½¨ÓÃ»§
	void onCreateUserCallback(cocos2d::CCBuffer& oBuffer);

	// ÍÆËÍÒÆ³ýÓÃ»§
	void onNotifyRemoveUserCallback(cocos2d::CCBuffer& oBuffer);

	// »ñÈ¡ÓÃ»§ÁÐ±í
	void onGetUserListCallback(cocos2d::CCBuffer& oBuffer);

	// ÍÆËÍÓÃ»§ÁÐ±í
	void onNotifyUserListCallback(cocos2d::CCBuffer& oBuffer);

	// ÍÆËÍÁÄÌìÄÚÈÝ
	void onNotifyChatMessageCallback(cocos2d::CCBuffer& oBuffer);

private:
	cocos2d::__LayerRGBA *m_pRoot;
};

#endif // __HELLOWORLD_SCENE_H__
