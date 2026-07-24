#include"ui.h"

bool ui::isshowsettings = false;
void ui::Init(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context) {
    m_device = device;
    // 初始化 ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", 18.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull());
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(device, context);
    //读取世界文件夹下的所有世界名称
    ifstream worldfile("./world/worldname.txt");
    if (!worldfile.is_open()) {//读取世界
        MessageBoxA(nullptr, "Failed to open ./world/worldname.txt", "World init failed", MB_OK | MB_ICONERROR);
        return;
    }
    string line;
    while (getline(worldfile, line)) {
        worldnames.push_back(line);
    }
    worldfile.close();
    m_subscriptions.push_back(EventBus::Subscribe<event::showsettings>([this](event::showsettings e) {
        isshowsettings = !isshowsettings;
        }));
    m_subscriptions.push_back(EventBus::Subscribe<event::showbag>([this](event::showbag e) {
        isshowbag = !isshowbag;
        }));// 订阅显示设置界面事件
    m_subscriptions.push_back(EventBus::Subscribe<event::npcdialog>([this](event::npcdialog e) {
        if(!isshownpclog&&isshownpcdialog!=e.show)
            isshownpcdialog = e.show;
        }));
}

ui::~ui() {
    // 清理 ImGui
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

ui::ui(ID3D11Device* m_device) : m_device(m_device) {
    m_worldsrv = nullptr;
}

void ui::chooseworld() {
    if (m_worldsrv == nullptr) {
        if (m_device == nullptr) {
            MessageBoxA(nullptr, "D3D11 device is null when loading panorama texture", "Device error", MB_OK | MB_ICONERROR);
        }
        else {
            HR(CreateDDSTextureFromFile(
                m_device,
                L"C:\\Users\\33304\\Desktop\\Minecraft\\DDS\\DDS\\panorama_0.dds",
                nullptr,                           // 不需要 ID3D11Resource*
                m_worldsrv.GetAddressOf(),         // 输出 SRV
                0,                                 // maxsize (0 = 不限制)
                nullptr                            // alphaMode (忽略)
            ));// 加载选择界面贴图资源
        }
    }
    EventBus::Publish<event::worldphoto>(event::worldphoto(m_worldsrv.Get()));
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImVec2 windowSize(500, 400);// 设置窗口大小
    ImVec2 windowPos((displaySize.x - windowSize.x) * 0.5f, (displaySize.y - windowSize.y) * 0.5f);// 设置窗口位置，使其居中

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

    ImGui::Begin(u8"欢迎来到mc", nullptr,
        ImGuiWindowFlags_NoResize |        // 禁止调整大小
        ImGuiWindowFlags_NoMove |          // 禁止移动
        ImGuiWindowFlags_NoCollapse        // 禁止折叠
    );
    ImGui::Text(u8"选择已有世界：");
    ImGui::Separator();
    static bool isdelete = false;//是否正在删除世界
    static string deleteworldname;//正在删除的世界名称
    for (auto& pair : worldnames) {
        ImGui::Text("%s", pair.c_str());
        ImGui::SameLine(200);
        std::string playId = "play##" + pair; // keep label "play", but give each button a unique ID
        if (ImGui::Button(playId.c_str())) {
            // 处理按钮点击事件，加载对应的世界
            EventBus::Publish<event::worldname>(event::worldname(pair));
            worldname = pair;//设置当前世界名称
            m_showChooseWorld = FALSE;//关闭选择界面
            EventBus::Publish<event::playaudio> (event::playaudio(1));//播放游戏音乐
        }
        if(ImGui::Button(("delete##" + pair).c_str())) {
            // 处理删除按钮点击事件
            isdelete = true;
            deleteworldname = pair;
            ImGui::OpenPopup(u8"确认删除");
        }
    }
    if (isdelete) {
        // 弹出确认对话框
        if (ImGui::BeginPopupModal(u8"确认删除", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text(u8"确定要删除世界 \"%s\" 吗？此操作不可恢复。", deleteworldname.c_str());
            ImGui::Separator();

            if (ImGui::Button(u8"确定", ImVec2(120, 0))) {
                // 1. 删除存档文件
                std::string filePath = "./world/" + deleteworldname + ".cd";
                try {
                    if (fs::remove(filePath)) {
                        // 2. 从 vector 中移除
                        auto it = std::find(worldnames.begin(), worldnames.end(), deleteworldname);
                        if (it != worldnames.end()) {
                            worldnames.erase(it);
                        }

                        // 3. 重写 worldname.txt
                        std::ofstream worldfile("./world/worldname.txt", std::ios::trunc);
                        if (worldfile.is_open()) {
                            for (const auto& name : worldnames) {
                                worldfile << name << '\n';
                            }
                            worldfile.close();
                        }

                        // 可选：提示成功（调试用）
                        OutputDebugStringA(("World deleted: " + deleteworldname + "\n").c_str());
                    }
                    else {
                        MessageBoxA(nullptr, ("无法删除文件: " + filePath).c_str(), "错误", MB_OK | MB_ICONERROR);
                    }
                }
                catch (const fs::filesystem_error& e) {
                    MessageBoxA(nullptr, e.what(), "文件系统错误", MB_OK | MB_ICONERROR);
                }

                // 4. 关闭弹窗并重置状态
                ImGui::CloseCurrentPopup();
                isdelete = false;
                deleteworldname.clear();
                isdelete = false;
            }

            ImGui::SameLine();
            if (ImGui::Button(u8"取消", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
                isdelete = false;
                deleteworldname.clear();
                isdelete = false;
            }
            ImGui::EndPopup();
        }
    }
    ImGui::Separator();
    ImGui::Text(u8"创建新世界");
    ImGui::SameLine(100);
    static bool iscreat = false;
    if (ImGui::Button("create##newworld")) {
        iscreat = true;
    }
    if (iscreat) {
        // 处理创建新世界事件
        ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        ImVec2 windowSize(600, 400);
        ImVec2 windowPos((displaySize.x - windowSize.x) * 0.5f, (displaySize.y - windowSize.y) * 0.5f);
        ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
        ImGui::Begin(u8"世界创建", nullptr,
            ImGuiWindowFlags_NoResize |        // 禁止调整大小
            ImGuiWindowFlags_NoMove |          // 禁止移动
            ImGuiWindowFlags_NoCollapse        // 禁止折叠
        );
        ImGui::Text(u8"输入世界名称：");
        ImGui::Separator();
        static string worldname;
        ImGui::InputText(u8"(不要输入太长的名字哦)", &worldname);
        ImGui::Separator();
        if (ImGui::Button("OK")) {
            for (auto& pair : worldnames) {
                if (pair == worldname) {
                    MessageBox(nullptr, TEXT("世界已经存在"), TEXT("提示"), 0);
                    ImGui::End();
                    ImGui::End();
                    return;
                }
            }
            worldnames.push_back(worldname);// 将新世界名称添加到列表中
            ofstream worldfile("./world/worldname.txt", ios::app);
            worldfile << worldname << '\n';
            worldfile.close();// 将新世界名称添加到列表中
            creatworld(worldname);// 创建新世界文件
            iscreat = false;
        }
        ImGui::End();
    }
    ImGui::End();
}

void ui::creatworld(string s) {
    string path = "./world/" + s + ".cd";
    ofstream worldfile(path, ios::binary);// 创建新世界文件
    if (!worldfile.is_open()) {
        MessageBoxA(nullptr, ("Failed to create " + path).c_str(), "World creation failed", MB_OK | MB_ICONERROR);
        return;
    }
    int block[20][10][20] = { 0 };// 初始化世界数据（示例：全是空气）
    /*
    for (int x = 0; x < 20; x++) {
        for (int y = 0; y < 10; y++) {
            for (int z = 0; z < 20; z++) {
                if (y == 0 || y == 1) block[x][y][z] = 1;// 示例：底部两层是石头
                if (y == 2) block[x][y][z] = 2;// 示例：第三层是草地
            }
        }
    }*/
    worldfile.write(reinterpret_cast<const char*>(block), sizeof(block));// 将世界数据写入文件
    worldfile.close();
}

void ui::Render() {
    // 开始新帧
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    if (m_showChooseWorld) {
        chooseworld();
    }
    if (isshowsettings) {
        showsettings();
    }
    if (isshowbag) {
        showbag();
    }
    if (isshownpcdialog) {
        shownpcdialog();
        if (Input::IsKeyPressed('G')) {
            isshownpcdialog= false;
            isshownpclog = true;
        }
    }
    if (isshownpclog) {
        static int logcount = 0;
        if (Input::IsKeyPressed('F'))
            logcount++;
        shownpclog(logcount);
        if (logcount == 4) {
            logcount = 0;
        }
    }
     // 渲染 ImGui

}

void ui::showsettings() {

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImVec2 windowSize(600, 400);
    ImVec2 windowPos((displaySize.x - windowSize.x) * 0.5f, (displaySize.y - windowSize.y) * 0.5f);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
    ImGui::Begin(u8"设置", nullptr,
        ImGuiWindowFlags_NoResize |        // 禁止调整大小
        ImGuiWindowFlags_NoMove |          // 禁止移动
        ImGuiWindowFlags_NoCollapse        // 禁止折叠
    );
    if(ImGui::Button(u8"保存存档")){
        // 处理保存存档事件
        EventBus::Publish<event::updateworld>(event::updateworld(worldname));
    }
    ImGui::End();
}

void ui::init_bag_texture(ID3D11ShaderResourceView* grass, ID3D11ShaderResourceView* stone, ID3D11ShaderResourceView* shuijin, vector<bagobj>* bag){ 
    m_grasssrv = grass;
    m_stonesrv = stone;
    m_shuijinsrv = shuijin;
    bagobjs = bag;
}

void ui::showbag() {
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImVec2 windowSize(400, 250);
    ImVec2 windowPos((displaySize.x - windowSize.x) * 0.5f, (displaySize.y - windowSize.y) * 0.5f);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"背包", nullptr,
        ImGuiWindowFlags_NoResize |        // 禁止调整大小
        ImGuiWindowFlags_NoMove |          // 禁止移动
        ImGuiWindowFlags_NoCollapse        // 禁止折叠
    );

    if (!bagobjs || bagobjs->size() < 10) {
        ImGui::Text(u8"背包未初始化");
        ImGui::End();
        return;
    }

    static int selectedSlot = -1;
    if (selectedSlot >= (int)bagobjs->size()) {
        selectedSlot = -1;
    }

    const float buttonSize = 60.0f;
    const int columns = 5;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));

    for (int i = 0; i < 10; ++i) {
        int type = (*bagobjs)[i].type;
        int count = (*bagobjs)[i].count;

        ID3D11ShaderResourceView* tex = nullptr;
        if (type == 1) tex = m_stonesrv;
        else if (type == 2) tex = m_grasssrv;
        else if (type == 3) tex = m_shuijinsrv;

        ImGui::PushID(i);

        bool clicked = false;

        if (tex) {
            // 有物品：使用 ImageButton
            if (selectedSlot == i) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.7f, 1.0f, 0.9f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.5f, 0.9f, 1.0f));
            }

            clicked = ImGui::ImageButton((ImTextureID)tex, ImVec2(buttonSize, buttonSize));

            if (selectedSlot == i) {
                ImGui::PopStyleColor(3);
            }
        }
        else {
            // 空格子：使用 Button 带空格标签，或 InvisibleButton
            if (selectedSlot == i) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 1.0f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.7f, 1.0f, 0.9f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.5f, 0.9f, 1.0f));
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
            }

            clicked = ImGui::Button(" ", ImVec2(buttonSize, buttonSize));

            ImGui::PopStyleColor(selectedSlot == i ? 3 : 1);
        }

        // 处理点击
        if (clicked) {
            if (selectedSlot == -1) {
                selectedSlot = i;
            }
            else if (selectedSlot == i) {
                selectedSlot = -1;
            }
            else {
                std::swap((*bagobjs)[selectedSlot], (*bagobjs)[i]);
                selectedSlot = -1;
            }
        }

        // 显示数量
        if (count > 0) {
            ImVec2 pos = ImGui::GetItemRectMax();
            pos.x -= 20;
            pos.y -= 20;
            std::string countStr = std::to_string(count);
            ImGui::GetWindowDrawList()->AddText(pos, IM_COL32(255, 255, 255, 255), countStr.c_str());
        }

        ImGui::PopID();

        if ((i + 1) % columns != 0) {
            ImGui::SameLine();
        }
    }

    ImGui::PopStyleVar();
    ImGui::End();
}

void ui::shownpcdialog() {//显示提示框
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImVec2 windowSize(100, 40);
    ImVec2 windowPos((displaySize.x - windowSize.x) * 0.5f, (displaySize.y - windowSize.y) * 0.7f);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"按‘G’对话", nullptr,
        ImGuiWindowFlags_NoResize |        // 禁止调整大小
        ImGuiWindowFlags_NoMove |          // 禁止移动
        ImGuiWindowFlags_NoCollapse        // 禁止折叠
    );
    ImGui::End();
}

void ui::shownpclog(int count) {//显示对话内容
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    ImVec2 windowSize(250, 120);
    ImVec2 windowPos((displaySize.x - windowSize.x) * 0.5f, (displaySize.y - windowSize.y) * 0.7f);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"与神明的对话（按F继续）", nullptr,
        ImGuiWindowFlags_NoResize |        // 禁止调整大小
        ImGuiWindowFlags_NoMove |          // 禁止移动
        ImGuiWindowFlags_NoCollapse        // 禁止折叠
    );
    static bool found = false;
    if(count==0)
        ImGui::Text(u8"欢迎进入世界");
    else if(count==1)
        ImGui::Text(u8"你一定好奇为什么我没有血量");
    else if(count==2)
        ImGui::Text(u8"其实那些是我的分身");
    else if (count == 3) {
        ImGui::Text(u8"算了，给你些方块吧，去建筑世界");
        if (!found) {
            for (auto& pair : *bagobjs) {
                if (pair.type == 1 && pair.count < 90) {
                    pair.count += 10;
                    found = true;
                    break;
                }
            }
            if (!found) {
                for (auto& pair : *bagobjs) {
                    if (pair.type == 0) {
                        pair.type = 1;
                        pair.count = 10;
                        break;
                    }
                }
            }
        }
    }
    else
    {
        isshownpclog = false;
        found = false;
    }
    ImGui::End();
}
