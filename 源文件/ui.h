#ifndef UI_H
#define UI_H
#include "../ImGui/imgui.h"
#include "../ImGui/imgui_impl_dx11.h"
#include "../ImGui/imgui_impl_win32.h"
#include "../ImGui/imgui_stdlib.h"
#include <functional>
#include <unordered_map>
#include <memory>
#include <sys/stat.h>
#include <stdio.h>
#include <string>
#include <fstream>
#include <filesystem>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include "../DDS/DDSTextureLoader11.h"
#include "d3dApp.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <wrl/client.h>
#include <wincodec.h>
#include <cstdlib>
#include <ctime>
#include <algorithm> 
#include "Audio.h"
using namespace std;
using namespace DirectX;
namespace fs = std::filesystem;

namespace event {
    struct worldphoto { ID3D11ShaderResourceView* srv; worldphoto(ID3D11ShaderResourceView* s) : srv(s) {} };
    struct worldname { string name; worldname(string s) : name(s) {} };
    struct setcamera { XMFLOAT3 pos; setcamera(XMFLOAT3 p) : pos(p) {} };
    struct rightmousedown { int type; XMFLOAT3 pos; rightmousedown(int t, XMFLOAT3 p) : type(t), pos(p) {} };//type:0-攻击，1-放置
    struct leftmousedown {
        std::tuple<int, int, int> pos;
        leftmousedown(std::tuple<int, int, int> p) : pos(p) {}
    };//破坏方块事件，参数为方块坐标tuple
    struct showsettings {bool show;
         showsettings(bool s) : show(s) {}
     };//显示设置界面事件
    struct updateworld {//更新世界事件，参数为世界名称
        string name;
        updateworld(string s) : name(s) {}
    };
    struct playaudio {
        int type;//0-界面音乐1-游戏音乐2-方块破坏3-方块放置4-玩家攻击
        playaudio(int t) : type(t) {}
    };//播放音效事件，参数为音效类型
    struct getobj { int type;getobj(int t) : type(t) {} };//获得物品事件，参数为物品类型
    struct putobj { int type;putobj(int t) : type(t) {} };//放置物品事件，参数为物品类型
    struct showbag { bool pad;showbag(bool p) : pad(p) {}  };//显示背包界面事件
    struct npcdialog {bool show; npcdialog(bool s) : show(s) {} };//显示NPC对话事件
}

struct bagobj {
    int type;//物品类型
    int count;//物品数量
    bagobj() : type(0), count(0) {}               // 默认构造函数
    bagobj(int t, int c) :type(t), count(c) {}
};

class EventBus {
public:
    template<typename EventType>
    static void Publish(const EventType& event) {
        auto listeners = GetListenerMap<EventType>();
        for (auto& kv : listeners) kv.second(event);
    }

    template<typename EventType>
    static std::unordered_map<size_t, std::function<void(const EventType&)>>& GetListenerMap() {
        static std::unordered_map<size_t, std::function<void(const EventType&)>> map;
        return map;
    }

    template<typename EventType>
    static void Unsubscribe(size_t token) { GetListenerMap<EventType>().erase(token); }

    struct Subscription { virtual ~Subscription() = default; };

    template<typename EventType>
    struct SubscriptionImpl : Subscription { size_t token; SubscriptionImpl(size_t t) : token(t) {} ~SubscriptionImpl() override { Unsubscribe<EventType>(token); } };

    template<typename EventType>
    static std::unique_ptr<Subscription> Subscribe(std::function<void(const EventType&)> listener) {
        static size_t nextToken = 0; size_t token = nextToken++; GetListenerMap<EventType>()[token] = listener; return std::make_unique<SubscriptionImpl<EventType>>(token);
    }
};

template <class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

class ui {
public:
    void Render();
    ID3D11Device* m_device = nullptr;
    void Init(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context);
    ui(ID3D11Device* m_device);
    ~ui();
    std::vector<std::unique_ptr<EventBus::Subscription>> m_subscriptions;
    //选择世界
    void chooseworld();
    ComPtr<ID3D11ShaderResourceView> m_worldsrv;
    bool m_showChooseWorld = TRUE;
    vector<string> worldnames;
    void creatworld(string s);
    string worldname;//当前世界名称
    //******//
    //设置界面******//
    static bool isshowsettings;//是否显示设置界面
    void showsettings();
     //******//
    //背包界面******//
    ID3D11ShaderResourceView* m_grasssrv;//草地贴图资源
    ID3D11ShaderResourceView* m_stonesrv;//石头贴图资源
    ID3D11ShaderResourceView* m_shuijinsrv;//水晶贴图资源
    void init_bag_texture(ID3D11ShaderResourceView* grass, ID3D11ShaderResourceView* stone, ID3D11ShaderResourceView* shuijin, vector<bagobj>* bag);
    bool isshowbag = false;//是否显示背包界面
    void showbag();
    vector<bagobj>* bagobjs;//背包物品列表
    //******//
    //NPC对话界面******//
    bool isshownpcdialog = false;//是否显示NPC提示对话界面
    bool isshownpclog = false;//是否显示NPC对话内容界面
    void shownpcdialog();
    void shownpclog(int count);
     //******//
};

#endif // UI_H
