#include "GameApp.h"
#include "d3dUtil.h"
#include "DXTrace.h"
using namespace DirectX;


GameApp::GameApp(HINSTANCE hInstance, const std::wstring& windowName, int initWidth, int initHeight)
    : D3DApp(hInstance, windowName, initWidth, initHeight), m_ui(m_pd3dDevice.Get()),m_camera(static_cast<float>(initWidth), static_cast<float>(initHeight))
{
}

GameApp::~GameApp()
{
}

bool GameApp::Init()
{
    if (!D3DApp::Init())
        return false;
    std::srand(static_cast<unsigned int>(std::time(nullptr)));// 设置随机种子
    m_ui.Init(MainWnd(), m_pd3dDevice.Get(), m_pd3dImmediateContext.Get());// 初始化UI
    m_shader.init(m_pd3dDevice.Get(), m_pd3dImmediateContext.Get(), static_cast<float>(m_ClientWidth) / m_ClientHeight);// 初始化渲染
    m_block.init();// 初始化方块
    m_player.init();// 初始化玩家
    m_drop.init();
    m_sw.init();
    m_audio.init();
    m_bag.init();
    m_ui.init_bag_texture(m_shader.m_grassSRV.Get(), m_shader.m_stoneSRV.Get(), m_shader.m_shuijintSRV.Get(), &m_bag.bagobjs);
    Model::Init(m_pd3dDevice.Get());
    m_subscriptions.push_back(EventBus::Subscribe<event::worldname>([this](const event::worldname& e) {
        ismouseshow= false;
        isstart= true;
    }));
    m_camera.projectionMatrix=XMMatrixPerspectiveFovLH(XM_PI / 3.0f, static_cast<float>(m_ClientWidth) / m_ClientHeight, 0.1f, 1000.0f);// 设置投影矩阵
    EventBus::Publish<event::playaudio>(event::playaudio(0));
    return true;
}

void GameApp::OnResize()
{
    D3DApp::OnResize();
}

void GameApp::UpdateScene(float dt)
{
    static bool isbagshow = false;
    static bool isset = false;
    m_ui.Render();
    if (isstart) {
        if (Input::IsKeyPressed(VK_ESCAPE)&&!isbagshow)//esc被按下
        {
            ismouseshow = !ismouseshow;
            isset = !isset;
            EventBus::Publish<event::showsettings>(event::showsettings(true));
        }
        if (!isset) {
            if (Input::IsKeyPressed('B'))//b被按下
            {
                isbagshow = !isbagshow;
                ismouseshow= !ismouseshow;
                EventBus::Publish<event::showbag>(event::showbag(isbagshow));
            }

            if (!isbagshow) {
                Shader::daytime += dt;
                m_bag.Update();
                m_camera.Update(dt);
                m_player.Update(dt);
                m_drop.Update();
                m_sw.Update(dt);
                m_shader.update_particles(dt);
                m_shader.update_view(m_camera.GetViewMatrix(), m_camera.GetPosition());
                m_shader.update_character_world_matrix(
                    m_player.m_transform_hand.GetWorldMatrix(),   // 手
                    m_player.m_transform_body.GetWorldMatrix(),   // 身体
                    m_player.m_transform_foot_left.GetWorldMatrix(),
                    m_player.m_transform_foot_right.GetWorldMatrix()
                );
            }
        }
    }
    LockCursor();
}

void GameApp::DrawScene()
{
    assert(m_pd3dImmediateContext);
    assert(m_pSwapChain);

    static float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };	// RGBA = (0,0,0,255)
    m_pd3dImmediateContext->ClearRenderTargetView(m_pRenderTargetView.Get(), black);
    m_pd3dImmediateContext->ClearDepthStencilView(m_pDepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    //绘制游戏
    m_shader.drawall();
    // 绘制UI
    if (!ismouseshow)//如果鼠标不可见，绘制准星
    {
        ImVec2 center(m_ClientWidth / 2.0f, m_ClientHeight / 2.0f);
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        draw->AddLine(ImVec2(center.x - 10, center.y), ImVec2(center.x - 3, center.y), IM_COL32(255, 255, 255, 200), 2.0f);
        draw->AddLine(ImVec2(center.x + 3, center.y), ImVec2(center.x + 10, center.y), IM_COL32(255, 255, 255, 200), 2.0f);
        draw->AddLine(ImVec2(center.x, center.y - 10), ImVec2(center.x, center.y - 3), IM_COL32(255, 255, 255, 200), 2.0f);
        draw->AddLine(ImVec2(center.x, center.y + 3), ImVec2(center.x, center.y + 10), IM_COL32(255, 255, 255, 200), 2.0f);
        draw->AddCircleFilled(center, 2.0f, IM_COL32(255, 255, 255, 200));
    }
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    HR(m_pSwapChain->Present(0, 0));
}

void GameApp::LockCursor() {
    // 只在状态改变时调用 ShowCursor
    if (ismouseshow != m_prevMouseShow) {
        if (ismouseshow) {
            // 强制显示：循环增加计数器直到返回 >= 0
            while (ShowCursor(TRUE) < 0);
            ClipCursor(nullptr);  // 解除鼠标限制
        }
        else {
            ShowCursor(FALSE);
            // 限制鼠标在窗口内
            RECT rc;
            GetClientRect(m_hMainWnd, &rc);
            ClientToScreen(m_hMainWnd, (POINT*)&rc.left);
            ClientToScreen(m_hMainWnd, (POINT*)&rc.right);
            ClipCursor(&rc);
        }
        m_prevMouseShow = ismouseshow;
    }

    if (!ismouseshow) {
        // 仅在隐藏状态下每帧居中
        Input::SetMousePosition(m_ClientWidth / 2, m_ClientHeight / 2);
    }
}


