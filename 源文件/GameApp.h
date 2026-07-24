#ifndef GAMEAPP_H
#define GAMEAPP_H

#include "shader.h"

class GameApp : public D3DApp
{
public:
    struct VertexPosColor
    {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT4 color;
        static const D3D11_INPUT_ELEMENT_DESC inputLayout[2];
    };

public:
    // GameApp.h 中添加成员变量记录上一帧状态
    bool m_prevMouseShow = true;
    std::vector<std::unique_ptr<EventBus::Subscription>> m_subscriptions;//事件订阅
    GameApp(HINSTANCE hInstance, const std::wstring& windowName, int initWidth, int initHeight);
    ~GameApp();

    bool Init();
    void OnResize();
    void UpdateScene(float dt);
    void DrawScene();
    //游戏ui
    bool ismouseshow = true;//是否显示鼠标
    void LockCursor();
    ui m_ui;
    //渲染
    Shader m_shader;
    //相机
    Camera m_camera;
    //方块
    Block m_block;
    //玩家
    player m_player;
    Drop m_drop;
    bool isstart = false;//是否开始游戏
    sw m_sw;
    Audio m_audio;
    Bag m_bag;
};
#endif