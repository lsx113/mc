# Minecraft · DirectX 11 渲染实现

用 **C++ / DirectX 11** 从零实现的 Minecraft 风格体素沙盒。渲染层全部手写：没有 Unity / Unreal，没有第三方渲染模块，只有裸 D3D11 调用与自写 HLSL。

本文档以**渲染**为主线，重点讲四块：粒子与血条（几何着色器扩面）、天空盒、光照模型、级联阴影。

![status](https://img.shields.io/badge/build-Debug%7Cx64-green)
![platform](https://img.shields.io/badge/platform-Windows%2010%2F11-blue)
![api](https://img.shields.io/badge/API-DirectX%2011-lightgrey)

---

## 0. 一览

| 维度     | 内容                                                           |
| ------ | ------------------------------------------------------------ |
| 图形 API | Direct3D 11（Feature Level 11.0 / 11.1），HLSL Shader Model 5.0 |
| 数学库    | DirectXMath、DirectXCollision                                 |
| 世界     | 20 × 10 × 20 体素，二进制存档                                        |
| 渲染模块   | 天空盒、体素地形、粒子（GS 公告牌）、血条（GS 扩面）、生物模型、小地图 RT、级联阴影               |
| 阴影     | 4 级 CSM，1024×1024 纹理数组，硬件 PCF + 级联混合                         |
| UI     | Dear ImGui（DX11 + Win32 后端）                                  |
| 模型     | assimp（PMX）                                                  |
| 音频     | DirectXTK AudioEngine                                        |
| 实测     | Debug 构建 1280×720，约 6.7 ms 帧时间（148 FPS）                      |

---

## 1. 渲染架构

### 1.1 每帧流程

    GameApp::DrawScene
    ├─ Clear RTV / DSV
    ├─ Shader::drawall
    │   ├─ update_shadow_matrices()      算 4 套光矩阵
    │   ├─ render_shadow_pass()          4 张阴影贴图（纯深度 pass）
    │   ├─ 天空盒                        z=w + 不写深度 + 不剔除
    │   ├─ drallblock()                  逐方块绘制 + 视锥剔除
    │   ├─ draw_particles()              GS 公告牌
    │   ├─ drawsw()                      生物模型 + GS 血条
    │   ├─ render_minimap()              渲染到 256² RT
    │   └─ draw_minimap()                全屏四边形贴到屏幕
    ├─ ImGui::Render                     UI
    └─ Present

### 1.2 常量缓冲槽位

固定分槽，每个着色器只认自己那一段，互不覆盖：

| 槽位  | 内容                  | 更新频率   |
| --- | ------------------- | ------ |
| b0  | 视图投影矩阵 / 当前级联光矩阵    | 每 pass |
| b1  | 每物体世界矩阵 + 逆转置       | 每物体    |
| b2  | 光源 + 材质 + 眼点        | 每帧     |
| b3  | 昼夜时间                | 每帧     |
| b4  | 阴影：4 级光矩阵 + 分割 + 参数 | 每帧     |

### 1.3 资源槽位

| 槽位    | 内容                       |
| ----- | ------------------------ |
| t0    | 主纹理（方块 / 模型 / 天空盒 / 小地图） |
| t1、t2 | 粒子的另外两张方块纹理              |
| t4    | 阴影纹理数组                   |
| s0    | 常规采样器（三线性 + WRAP）        |
| s1    | 比较采样器（阴影专用）              |

---

## 2. 粒子与血量条

两者共用同一套思路：**CPU 只上传一个点，面片由几何着色器生成**。

### 2.1 粒子：CPU 侧

方块被破坏时从破坏点喷出 5 个碎片，受重力下坠、逐渐透明消失（上限 30 个）。

    struct Particle {
        XMFLOAT3 Position;   // 世界坐标
        XMFLOAT3 Velocity;
        float    Life;       // 剩余寿命
        float    Size;
        int      BlockType;
    };

每帧更新后写进 DYNAMIC 顶点缓冲：

    Map(m_particleVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, ..., count * sizeof(ParticleVertex));
    Unmap(...);

### 2.2 几何着色器扩点成面

    [maxvertexcount(6)]
    void GS(point GS_IN input[1], inout TriangleStream<PS_IN> triStream)
    {
        float3 toEye = normalize(gEyePosW - center);
    
        float3 right = normalize(cross(float3(0,1,0), toEye));
        float3 newUp = normalize(cross(toEye, right));   // 二次正交化
    
        float3 offsets[4] = {
            -right*h - newUp*h,  right*h - newUp*h,
             right*h + newUp*h, -right*h + newUp*h
        };
        // 两个三角形 Append，中间 RestartStrip
    }

| 设计                                    | 原因                           |
| ------------------------------------- | ---------------------------- |
| 输入 point + POINTLIST                  | IA 只提交点，几何量在 GS 里生成          |
| right 后再算 newUp = cross(toEye, right) | 只用 up×toEye 得到的基不正交，面片会随视角倾斜 |
| maxvertexcount(6)                     | 写死让编译器分配寄存器                  |
| RestartStrip                          | 断开两个三角形，避免退化                 |

像素着色器按类型取三张纹理之一，颜色乘上寿命驱动的 alpha：

    if (input.type == 1)      texColor = gTexStone.Sample(gSampler, input.tex);
    else if (input.type == 2) texColor = gTexGrass.Sample(gSampler, input.tex);
    else                      texColor = gTexCrystal.Sample(gSampler, input.tex);
    return texColor * input.color;

### 2.3 混合与深度

    SrcBlend = SRC_ALPHA;  DestBlend = INV_SRC_ALPHA;  Op = ADD;
    DepthWriteMask = ZERO;  DepthFunc = LESS;

- 不写深度：半透明粒子写深度会挡住后面本该可见的透明像素；
- 保留深度测试：被方块挡住的部分正确剔除；
- 画完恢复 BlendState 与默认深度状态。

### 2.4 血量条：点扩矩形 + 程序化绘制

CPU 每帧每个生物只传一个点：

    struct BarVertex { XMFLOAT3 center; float blood; };

GS 扩成 0.8 × 0.15 的矩形（公告牌基同粒子，只是不是正方形）：

    [maxvertexcount(4)]
    void GS(point VertexOut input[1], inout TriangleStream<GeoOut> triStream)
    {
        float3 center = input[0].CenterW + float3(0, 2.2f, 0);   // 头顶偏移
        float3 toEye  = normalize(g_EyePosW - center);
        float3 right  = normalize(cross(float3(0,1,0), toEye));
        float3 newUp  = cross(toEye, right);
        // 4 个角 + UV
    }

像素着色器完全程序化，不用贴图：

    // 边框
    if (uv.x < 0.1 || uv.x > 0.9 || uv.y < 0.1 || uv.y > 0.9)
        return float4(0, 0, 0, 1);
    
    // 血量渐变：红 → 黄 → 绿
    float t = pin.Blood;
    float4 fg = (t <= 0.5f)
        ? lerp(float4(1,0,0,1), float4(1,1,0,1), t*2)
        : lerp(float4(1,1,0,1), float4(0,1,0,1), (t-0.5f)*2);
    
    return (uv.x <= pin.Blood) ? fg : float4(0.2, 0.2, 0.2, 1);

省一张纹理、省一次采样；纯色块程序化绘制也不会受贴图分辨率限制。

### 2.5 取舍与可改进

| 现状            | 问题             | 方向                         |
| ------------- | -------------- | -------------------------- |
| GS 扩面         | GS 是流水线串行瓶颈    | 实例化 + SV_VertexID 算四角      |
| 三张纹理 + if     | warp 内不一致会双发射  | Texture2DArray + 索引采样      |
| up 固定 (0,1,0) | 正俯视时 cross 退化  | dot 接近 ±1 时换参考轴            |
| 粒子不排序         | alpha 混合不满足交换律 | 按相机距离排序                    |
| 血条写深度         | 沿用方块状态，会遮挡后面物体 | 单独的 DepthWriteMask=ZERO 状态 |

---

## 3. 天空盒

### 3.1 三个技巧

| 技巧                      | 位置  | 解决什么              |
| ----------------------- | --- | ----------------- |
| 去掉 view 矩阵平移            | CPU | 立方体永远以相机为中心，不产生视差 |
| z = w                   | VS  | 透视除法后深度恒为 1，钉在远平面 |
| 不写深度 + LESS_EQUAL + 不剔除 | 状态  | 只在「没有几何体」的像素上着色   |

    // CPU
    XMMATRIX viewNoTranslate = m_viewMatrix;
    viewNoTranslate.r[3] = XMVectorSet(0, 0, 0, 1);
    
    // VS
    vout.PosH = mul(pos, g_ViewProj).xyww;    // 把 w 塞进 z
    vout.PosL = vin.PosL;                    // 直接当 cube 采样方向

立方体是 8 顶点 36 索引的单位盒，只带 POSITION——立方体贴图用方向采样，不需要 UV。

### 3.2 为什么必须 CULL_NONE

相机在凸立方体内部时，每条视线只和盒子相交一次，交在「出去的那个面」上；按外部绕序建的索引，从内侧看全是背面，开背面剔除就是黑屏。

这不是性能选择：凸体内部每像素只命中一个面，不存在双面 overdraw。

### 3.3 像素着色器

    float4 sky = g_SkyMap.Sample(g_Sampler, pin.PosL);
    float sunHeight = (cos(angle) + 1.0f) * 0.5f;
    float brightness = lerp(0.25f, 1.2f, sunHeight);
    finalColor = lerp(dayColor * nightTint, dayColor, sunHeight);

### 3.4 已知问题

| 问题              | 影响                              |
| --------------- | ------------------------------- |
| 画完没恢复光栅化状态      | CULL_NONE 生效到帧末，之后所有几何体双面渲染     |
| 第一个画            | 全屏像素先跑一遍天空盒 PS 再被覆盖，白花 overdraw |
| horizonGlow 可为负 | lerp 外插，日出前后过曝                  |
| 每像素算 cos/sin    | 输入只有常量，应在 CPU 算好                |

---

## 4. 光照模型

### 4.1 数据

    struct DirectionalLight {
        XMFLOAT4 ambient, diffuse, specular;
        XMFLOAT3 direction; float pad;      // pad 保证 16 字节对齐
    };
    struct Material {
        XMFLOAT4 ambient, diffuse, specular;   // specular.w = 高光指数
        XMFLOAT4 reflect;                       // 预留
    };

统一塞进 b2，每帧更新一次，所有物体共用。

### 4.2 公式：Lambert 漫反射 + Phong 高光

    float3 lightVec = -L.direction;              // 指向光源
    ambient = mat.ambient * L.ambient;
    
    float NdotL = dot(lightVec, normal);
    [flatten] if (NdotL > 0.0f) {
        float3 r = reflect(-lightVec, normal);   // Phong
        float  s = pow(max(dot(r, toEye), 0.0f), mat.specular.w);
        diffuse = NdotL * mat.diffuse * L.diffuse;
        spec    = s * mat.specular * L.specular;
    }

最终：

    litColor = albedo * (ambient + diffuse * shadow) + spec * shadow;

用 reflect 向量（Phong）而非半程向量（Blinn-Phong）；[flatten] 强制展开分支。

### 4.3 昼夜循环

    float angle  = (daytime / 240.0f) * XM_2PI;
    float sunY   = cosf(angle);        // 1 正午 → -1 午夜
    float sunX   = sinf(angle) * 0.8f;
    float sunZ   = 0.6f;
    
    m_lightDir       = -normalize(sunX, sunY, sunZ);   // 存「光传播方向」
    m_daylightFactor = max(sunY, 0.0f);

| 设计点                           | 说明                                                                      |
| ----------------------------- | ----------------------------------------------------------------------- |
| 常数 sunZ = 0.6                 | 太阳轨道被压低，最大仰角 59°，永远不过头顶——顶面和侧面都有明暗；竖分量不超过 0.86，阴影相机的 LookAt up 向量永不退化   |
| 存「传播方向」而非「太阳位置」               | 主 pass 的 lightVec = -direction 与阴影相机的 eye = center - direction·d 才能同时正确 |
| daylightFactor = max(sunY, 0) | 夜里方向光 diffuse 归零，只剩环境光                                                  |

### 4.4 雾

    float dist = length(PosW - eyePos);
    float fogFactor = saturate((dist - 10) / (30 - 10));
    litColor.rgb = lerp(litColor.rgb, lerp(nightFog, dayFog, sunHeight), fogFactor);

线性雾，10~30 米，颜色随昼夜变化。

### 4.5 已知问题

| 问题                        | 影响                                |
| ------------------------- | --------------------------------- |
| material.specular.rgb = 0 | 高光项恒为 0，实际只有 Lambert + 常量环境光，画面偏平 |
| 环境光常量 0.5                 | 与遮挡无关，坑里和开阔地一样亮（缺 AO 的观感来源）       |
| 全 UNORM 无 sRGB            | 光照不在线性空间计算，对比度偏低                  |

---

## 5. 级联阴影（CSM）

### 5.1 资源布局

    R32_TYPELESS 纹理（ArraySize = 4）
    ├─ 4 个 DSV（D32_FLOAT，每个只覆盖一个 slice）   ← 阴影 pass 写
    └─ 1 个 SRV（R32_FLOAT，TEXTURE2DARRAY）        ← 主 pass 读

| 选择              | 理由                                        |
| --------------- | ----------------------------------------- |
| 1 张纹理数组而非 4 张纹理 | 一次绑定，float3(uv, slice) 选级；混合时只改 slice     |
| TYPELESS        | DSV 用 D32_FLOAT、SRV 用 R32_FLOAT，资源本身不指定格式 |
| 1x 采样           | 主渲染开了 4x MSAA，MSAA 深度不能直接当阴影图             |

### 5.2 光矩阵计算（每级联七步）

1. 分割：0.1 / 10 / 100 / 500 / 1000

2. 从投影矩阵反推 fov 与 aspect，不额外存参数

3. 相机空间构造该段视锥的 8 个角点，一次乘 invView 转世界

4. 中心 = 8 点平均

5. 直径 = 8 点两两最大距离（28 次比较）

6. 光空间
   
       dist = diameter + 10;                       // 自适应，防止大级联 near 为负
       eye  = center - lightTravel * dist;
       lightView = LookAtLH(eye, center, up);
       lightProj = OrthographicLH(diameter, diameter, dist - d/2, dist + d/2);

7. texel 吸附
   
       origin = Transform(worldOrigin, lightView * lightProj) * (size / 2);
       offset = (round(origin) - origin) * (2 / size);
       shadowMat.r[3] += offset;

**为什么用直径而不是紧包围盒**：AABB 尺寸随相机旋转变化 → texel 密度抖 → 阴影边缘“呼吸”。直径只跟 fov 与近远平面有关，与朝向无关，代价是盒子大 1.4~1.8 倍。

**为什么吸附作用在矩阵上**：盒子中心在光空间恒为 (0,0,dist)，对它取整无效；要让世界原点落到纹素中心，只能修正投影矩阵的平移行。

### 5.3 阴影 pass

    PSSetShader(nullptr);                          // 纯深度，不设 PS
    OMSetRenderTargets(0, nullptr, m_shadowDSV[c]);
    ClearDepthStencilView(..., 1.0f, 0);
    RSSetState(m_shadowRS);                        // DepthBias=200, Slope=1.5
    OMSetDepthStencilState(m_pBlockDSS);           // 写深度 + LESS

绘制：方块（复用 24 顶点 / 36 索引的正方体 VB）+ 生物。粒子与血条不投影。

| 细节            | 原因                                  |
| ------------- | ----------------------------------- |
| 进 pass 前解绑 t4 | 独占槽位，上一帧主 pass 的绑定会留到这一帧，和 DSV 冲突   |
| 距离剔除乘 2       | 投射者可在接收者身后很远（59° 斜射时影子长 ≈ 高差 1.7 倍） |

### 5.4 采样与级联混合

    int SelectCascade(float viewDepth) {
        int c = 0;
        [unroll] for (int i = 0; i < 4; ++i) c += (viewDepth >= g_cascadeSplit[i]);
        return min(c, 3);
    }
    
    float SampleOneCascade(float3 posW, int c) {
        float4 sp = mul(float4(posW, 1), g_lightVP[c]);
        float2 uv = float2(sp.x*0.5f + 0.5f, 0.5f - sp.y*0.5f);
        if (outside) return 1.0f;
        return g_ShadowMap.SampleCmpLevelZero(g_ShadowSampler, float3(uv, c), sp.z);
    }

- **硬件 PCF**：SampleCmpLevelZero 一次调用 = 2×2 四个纹素比较 + 双线性，等效 4 次比较一条指令；
- **级联混合**：边界前后 10% 的过渡带里同时采两级做 lerp，消除硬切。

### 5.5 当前实际生效的只有级联 0

这是刻意的取舍，需要说清楚：

| 事实   | 说明                                    |
| ---- | ------------------------------------- |
| 世界尺寸 | 20 × 10 × 20 米                        |
| 分割   | 0.1 / 10 / 100 / 500 / 1000           |
| 结论   | 玩家站在地面时，视野内所有像素视深度都在 10 米内 → 全部落在级联 0 |

- 级联 0 的 texel = 23 mm（一个方块 43 个纹素），阴影形状干净；
- 级联 1~3 在当前场景没有像素会用到，作用是**为世界扩大预留接口**——世界尺寸或视距上去时只调分割，不改代码；
- 代价：far 拉满 1000 后，后三级盒子被视锥撑得极大（级联 3 边长 2355，texel 2.3 米），属当前场景的浪费，但不影响画面。

保留 4 级而不退化成 1 级，是因为目标是可扩展的渲染管线，不是当前场景的最优解；级联数量是常量，退化只需改循环次数。

### 5.6 局限

| 局限               | 影响                           | 路线                             |
| ---------------- | ---------------------------- | ------------------------------ |
| bias 不随级联缩放      | 级联 0（2 mm）和级联 3（2.3 m）共用同一偏移 | 把 texelWorld 按级联传进 cbuffer     |
| shader 侧无深度/法线偏移 | 只有光栅化 200/1.5 兜底             | 加回 normalBias = texelWorld × 2 |
| 视锥外投射者不参与        | 看不见但挡光的方块不在盒里                | 沿光线方向扩展正交盒                     |
| 没有软阴影            | 半影固定半个纹素                     | VSM/ESM                        |

---

## 6. 性能与验证

| 项          | 数值                               |
| ---------- | -------------------------------- |
| 分辨率        | 1280 × 720                       |
| 帧时间（Debug） | 6.7 ms / 148 FPS                 |
| 阴影 pass    | 4 次清屏 + 4 次全量方块绘制                |
| 已知瓶颈       | 方块逐块绘制（无实例化/合批），阴影 pass 把它放大 4 倍 |

验证手段：阴影 PS 临时输出灰度（看级联覆盖）或按级联着色（0 红 1 绿 2 蓝 3 黄，看边界）；光照用固定方向与昼夜循环对比；天空盒对比开关 z=w。

---

## 7. 构建与运行

### 环境

- Windows 10 / 11，Visual Studio 2022（MSVC v143 + Windows SDK）
- 显卡支持 Direct3D 11

### 依赖（仓库已内置）

| 依赖               | 位置                                                               | 说明     |
| ---------------- | ---------------------------------------------------------------- | ------ |
| Dear ImGui       | ImGui/                                                           | 源码参与编译 |
| assimp           | 1/install/include/、1/lib/Debug/assimp-vc143-mtd.lib              | 静态链接   |
| DirectXTK        | DirectXTK-main/.../Inc/、Bin/Desktop_2022/x64/Debug/DirectXTK.lib | 音频与工具  |
| DDSTextureLoader | DDS/DDSTextureLoader11.cpp                                       | 随工程编译  |

### 步骤

1. 用 VS2022 打开 Minecraft.sln
2. 配置选 **Debug | x64**（Release 未配头文件与库路径，链接会失败）
3. Ctrl + Shift + B 生成，F5 运行

命令行：

    msbuild Minecraft.vcxproj /p:Configuration=Debug /p:Platform=x64

### 运行时工作目录

所有资源路径（HLSL/、DDS/、world/、music/、model/）都相对项目根目录：

- VS 里 F5 调试时工作目录就是项目根目录，正常；
- 不要直接双击 x64\Debug\Minecraft.exe，那样工作目录是 exe 所在目录，资源全部加载失败；
- 要在资源管理器里跑，先把 HLSL/ DDS/ music/ model/ world/ 复制到 exe 旁边。

---

## 8. 操作

| 按键      | 功能                        |
| ------- | ------------------------- |
| W A S D | 前后左右移动（相对身体朝向）            |
| 空格      | 跳跃                        |
| 鼠标移动    | 转动视角                      |
| 鼠标左键    | 破坏准星指向的方块（15 格内），掉落可拾取物   |
| 鼠标右键    | 对着方块放置手持方块；对着生物则攻击        |
| B       | 打开 / 关闭背包（拖到 0 号格即切换手持方块） |
| ESC     | 打开 / 关闭设置界面（内含保存存档）       |

进入游戏后鼠标被锁定在窗口内并隐藏，按 ESC 或 B 释放。

---

## 9. 目录结构

    Minecraft/
    ├── Minecraft.sln                  # VS 解决方案
    ├── Minecraft.vcxproj              # 工程文件（Debug|x64 为唯一可用配置）
    ├── README.md
    ├── 源文件/                         # 全部自写代码
    │   ├── Main.cpp                   # WinMain 入口
    │   ├── GameApp.cpp / .h           # 游戏主类：初始化、每帧更新、绘制、鼠标锁定
    │   ├── d3dApp.cpp / .h            # D3D11 框架 + Input（键鼠状态、鼠标增量）
    │   ├── d3dUtil.cpp / .h           # CreateShaderFromFile：优先读 .cso，缺失则编译并缓存
    │   ├── DXTrace / CpuTimer / WinMin
    │   ├── game.cpp / .h              # Transform、Camera、Block、player、Drop、sw、Audio、Bag、Model
    │   ├── shader.cpp / .h            # 渲染层：阴影、天空盒、方块、粒子、血条、小地图
    │   └── ui.cpp / .h                # EventBus 事件总线 + ImGui 各界面
    ├── HLSL/                          # 着色器源文件（.cso 为运行时生成，已忽略）
    ├── ImGui/                         # Dear ImGui 源码
    ├── DDS/                           # 纹理加载器 + 方块/天空盒贴图
    ├── model/                         # 生物模型
    ├── music/                         # wav 音频
    ├── world/                         # 存档目录
    ├── 1/                             # assimp 头文件与静态库
    └── DirectXTK-main/                # DirectXTK 头文件与 Debug 库

关键设计：

- **事件总线**：EventBus::Subscribe 与 Publish 基于模板静态 unordered_map 做类型安全的发布订阅，各系统在自己的 init 里订阅，互不持有对方指针。
- **存档格式**：world/<世界名>.cd 是 int[20][10][20] 的小端裸 dump（16000 字节），按 [x][y][z] 索引，0 空气 / 1 石头 / 2 草 / 3 水晶。
