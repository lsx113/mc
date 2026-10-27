# Minecraft · DirectX 11 复刻

用 **C++ / DirectX 11** 从零实现的一个 Minecraft 风格沙盒游戏。世界、方块、玩家、生物、粒子和 UI 全部手写，没有使用任何游戏引擎。

框架沿用《DirectX11 3D 编程入门》（冯乐乐）中的 `d3dApp` 窗口与设备管理骨架，在此之上实现了完整的游戏循环。

![status](https://img.shields.io/badge/build-Debug%7Cx64-green)
![platform](https://img.shields.io/badge/platform-Windows%2010%2F11-blue)
![api](https://img.shields.io/badge/API-DirectX%2011-lightgrey)

---

## 功能

| 模块 | 实现内容 |
|------|----------|
| 世界 | 20×10×20 体素世界，二进制存档读写，ImGui 图形化的世界选择 / 创建 / 删除 |
| 地形渲染 | 逐方块绘制 + 视锥体剔除（6 平面提取）、方向光 + 昼夜循环（240 秒一轮）、距离雾 |
| 天空盒 | 立方体贴图 + 无深度写入 / 无剔除的专用渲染状态 |
| 玩家 | 第一人称视角、WASD 位移 + AABB 碰撞回退、重力与跳跃、身体 / 双手 / 双脚的分层渲染与走路摆腿、挥击动画 |
| 方块交互 | 屏幕中心射线与包围盒求交，左键破坏（15 格内）、右键按命中面法线放置 |
| 掉落物 | 破坏方块生成掉落物，自动下落到方块顶面，1.5 格内自动拾取 |
| 背包 | 10 格背包，0 号格为当前手持方块，拖拽交换整理 |
| 生物 | PMX 模型（assimp 解析，内存加载绕开中文路径）、重力下落、G-Buffer 血条、受伤扣血、死亡移除 |
| NPC | 固定位置 NPC，靠近提示对话，按键翻页推进对话，最后赠送方块 |
| 粒子 | 破坏方块时喷发，几何着色器扩点成面片 |
| 小地图 | 独立 RenderTarget + 正交俯视相机渲染，贴到屏幕右上角 |
| 音频 | DirectXTK AudioEngine，界面音乐 / 背景音乐 / 破坏 / 放置 / 攻击音效 |

---

## 技术栈

- **语言 / 标准**：C++17
- **图形 API**：Direct3D 11（`d3d11_1.h`），HLSL Shader Model 5.0
- **数学库**：DirectXMath、DirectXCollision
- **UI**：Dear ImGui（DX11 + Win32 后端）
- **模型**：assimp（PMX / MikuMikuDance 模型）
- **音频**：DirectXTK（`DirectX::AudioEngine`）

---

## 目录结构

```
Minecraft/
├── Minecraft.sln                  # VS 解决方案
├── Minecraft.vcxproj              # 工程文件（Debug|x64 为唯一可用配置）
├── README.md
├── 源文件/                         # 全部自写代码
│   ├── Main.cpp                   # WinMain 入口
│   ├── GameApp.cpp / .h           # 游戏主类：初始化、每帧更新、绘制、鼠标锁定
│   ├── d3dApp.cpp / .h            # D3D11 框架 + Input（键鼠状态、鼠标增量）
│   ├── d3dUtil.cpp / .h           # CreateShaderFromFile：优先读 .cso，缺失则编译 .hlsl 并缓存
│   ├── DXTrace / CpuTimer / WinMin
│   ├── game.cpp / .h              # Transform、Camera、Block、player、Drop、sw、Audio、Bag、Model
│   ├── shader.cpp / .h            # 渲染层：天空盒、方块、人物、生物、粒子、小地图、血条
│   └── ui.cpp / .h                # EventBus 事件总线 + ImGui 各界面
├── HLSL/                          # 着色器源文件（.cso 为运行时自动生成的产物，已忽略）
├── ImGui/                         # Dear ImGui 源码（参与编译）
├── DDS/
│   ├── DDSTextureLoader11.cpp/h   # 纹理加载器（参与编译）
│   └── DDS/*.dds                  # 方块 / 天空盒 / 封面贴图
├── model/                         # 妮露.pmx 及贴图
├── music/                         # wav 音频
├── world/                         # 存档目录
├── 1/                             # assimp 头文件与静态库
└── DirectXTK-main/                # DirectXTK 头文件与 Debug 库
```

### 代码里的几个关键设计

**事件总线（`ui.h`）**：`EventBus::Subscribe<T>` / `Publish<T>` 基于模板静态 `unordered_map` 实现类型安全的发布订阅。各系统在自己的 `init()` 里订阅事件，互不持有对方指针。现有事件：

```
worldname  世界加载     leftmousedown   破坏方块      rightmousedown  放置 / 攻击
setcamera  相机跟随     updateworld     保存存档      worldphoto     显示封面
playaudio  播放音效     getobj / putobj 拾取 / 放置   showbag / showsettings / npcdialog
```

**渲染管线（`shader.cpp`）**：常量缓冲区槽位固定为 `b0` 视图投影、`b1` 每物体世界矩阵、`b2` 光照材质、`b3` 时间。绘制顺序为天空盒 → 方块 → 粒子 → 人物 → 生物 → NPC → 小地图。

**存档格式**：`world/<世界名>.cd` 是 `int[20][10][20]` 的小端裸 dump，固定 **16000 字节**，按 `[x][y][z]` 索引，值为方块类型（0 空气 / 1 石头 / 2 草 / 3 水晶）。`world/worldname.txt` 每行一个世界名。

---

## 构建

### 环境要求

- Windows 10 / 11
- Visual Studio 2022，安装「使用 C++ 的桌面开发」工作负载（MSVC v143 + Windows SDK）
- 显卡支持 Direct3D 11

### 依赖

仓库中**已包含**编译所需的全部依赖，无需额外下载：

| 依赖 | 位置 | 说明 |
|------|------|------|
| Dear ImGui | `ImGui/` | 源码直接参与编译 |
| assimp | `1/install/include/`、`1/lib/Debug/assimp-vc143-mtd.lib` | 静态链接，无需 DLL |
| DirectXTK | `DirectXTK-main/DirectXTK-main/Inc/`、`Bin/Desktop_2022/x64/Debug/DirectXTK.lib` | 音频与部分工具 |
| DDSTextureLoader | `DDS/DDSTextureLoader11.cpp` | 随工程编译 |

> 之所以 `1/` 和 `DirectXTK-main/` 里只剩这些：完整的 assimp 源码/CMake 构建树和 DirectXTK 各平台二进制体积达 1.3 GB，已在 `.gitignore` 中排除，只保留工程编译实际用到的部分。

### 步骤

1. 双击 `Minecraft.sln`，用 VS2022 打开
2. 配置管理器里选择 **Debug | x64**（`Release` 配置未配置头文件与库路径，链接会失败）
3. `Ctrl + Shift + B` 生成，`F5` 运行

也可以命令行构建：

```bat
msbuild Minecraft.vcxproj /p:Configuration=Debug /p:Platform=x64
```

### ⚠️ 运行时工作目录

程序里所有资源路径（`HLSL/`、`DDS/`、`world/`、`music/`、`model/`）都是**相对项目根目录**的。

- 在 VS 里 `F5` 调试时，工作目录默认就是项目根目录，正常
- **不要**直接双击 `x64\Debug\Minecraft.exe`，那样工作目录是 exe 所在目录，所有资源都会加载失败
- 如果要在资源管理器里直接运行，请先把 `HLSL/ DDS/ music/ model/ world/` 复制到 exe 旁边

---

## 操作

| 按键 | 功能 |
|------|------|
| `W` `A` `S` `D` | 前后左右移动（相对身体朝向） |
| `空格` | 跳跃 |
| 鼠标移动 | 转动视角 |
| 鼠标左键 | 破坏准星指向的方块（15 格内），掉落可拾取物 |
| 鼠标右键 | 对着方块放置手持方块；对着生物则攻击 |
| `B` | 打开 / 关闭背包（把方块拖到 0 号格即切换手持方块） |
| `ESC` | 打开 / 关闭设置界面（内含「保存存档」） |
| `G` | 靠近 NPC 时开始对话 |
| `F` | 对话翻页 |

进入游戏后鼠标会被锁定在窗口内并隐藏，按 `ESC` 或 `B` 释放。

---

## 已知问题

以下是已知但尚未修复的问题，按影响排序：

1. **`Shader::drallblock()` 中 `texSRV` 未初始化** —— `switch` 只覆盖方块类型 1/2/3，缺少 `default` 分支，出现其它类型会把野指针绑到像素着色器（小地图里同名代码有兜底，两处不一致）
2. **天空盒渲染后未恢复光栅化状态** —— `m_rsNoCull`（无背面剔除）一直生效到帧结束，之后所有几何体都渲染双面，有额外开销
3. **方块绘制是逐块提交的** —— 每个方块每帧一次 `UpdateSubresource` 和一次 `XMMatrixInverse`；小地图又把整个世界重画一遍，等于世界一帧画两次。世界扩大前需要改成实例化 / 合并网格
4. **`EventBus::Publish` 按值拷贝监听表** —— `auto listeners = GetListenerMap<T>();` 每次发布都复制整张 `unordered_map<std::function>`，而 `setcamera` 每帧发布
5. **世界尺寸 20×10×20 硬编码**在方块数组、边界检查、小地图相机等多处，没有区块（chunk）概念
6. **存档名编码依赖系统 ANSI 代码页** —— `worldname.txt` 是 UTF-8，但读取时用窄字符路径交给系统按 GBK 转换，因此在中文 Windows 上文件名会变成 `娴嬭瘯4.cd` 这类形式（当前能对上，但换 UTF-8 locale 就读不到）；用 ImGui 新建世界同理
7. **窗口缩放后射线拾取偏移** —— `Camera` 的宽高在构造时固定为 1280×720，`OnResize` 未同步更新
8. **背包选中无 HUD 反馈** —— 选中只是把物品换到 0 号格，屏幕上没有快捷栏提示

> 已修复：封面与小地图封面、NPC 模型原先写死了 `C:\Users\...` 绝对路径，工程目录一移动就加载失败；现已改为相对项目根目录的路径。

---

## 编码约定

- `源文件/` 下的 `.cpp` / `.h` 是 **GBK** 编码、CRLF 换行（中文注释依赖该编码）。用 VS 编辑请保持 GBK；若用 UTF-8 编辑器另存，注释会乱码
- 资源路径一律相对**项目根目录**
- `.cso` 是运行产物：首次运行时 `CreateShaderFromFile` 会从对应 `.hlsl` 编译并写回，改完着色器可删掉 `.cso` 强制重编
