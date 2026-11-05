#include"shader.h"
const D3D11_INPUT_ELEMENT_DESC VertexPosUV::inputLayout[2] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

const D3D11_INPUT_ELEMENT_DESC VertexPos::inputLayout[1] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

const D3D11_INPUT_ELEMENT_DESC VertexPosNormalTex::inputLayout[3] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

float Shader::daytime = 0;

void Shader::init(ID3D11Device* device, ID3D11DeviceContext* context,float aspectRatio) {
    m_context = context;
    m_device = device;
    float fovAngleY = XM_PI / 3.0f;   // 60度
    float nearZ = 0.1f;
    float farZ = 1000.0f;
    m_projectionMatrix = XMMatrixPerspectiveFovLH(fovAngleY, aspectRatio, nearZ, farZ);
    m_viewMatrix = XMMatrixIdentity();
    init_view_proj_buffer();
    init_skybox();
    init_block();
    init_sw();
    init_minimap();
    init_particles();
    init_shadow();
    //创建时间缓冲区
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(cbtime);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    HR(m_device->CreateBuffer(&bd, nullptr, m_timecbbuffer.GetAddressOf()));
    m_context->PSSetConstantBuffers(3, 1, m_timecbbuffer.GetAddressOf());
    // 设置图元类型为三角形列表
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;  // 三线性滤波
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;     // U方向重复
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;     // V方向重复
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;     // W方向重复
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    HR(m_device->CreateSamplerState(&sampDesc, m_samplerState.GetAddressOf()));//设置采样器
    m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());
    init_cover();
    m_subscriptions.push_back(EventBus::Subscribe<event::worldphoto>([this](event::worldphoto event) {
        isshowcover = true;
        m_context->IASetInputLayout(m_inputLayout_cover.Get());
        m_context->VSSetShader(m_vertexShader_cover.Get(), nullptr, 0);
        m_context->PSSetShaderResources(0, 1, &event.srv);
        m_context->PSSetShader(m_pixelShader_cover.Get(), nullptr, 0);
        UINT stride = sizeof(VertexPosUV);
        UINT offset = 0;
        m_context->IASetVertexBuffers(0, 1, m_Vertexbuffer_cover.GetAddressOf(), &stride, &offset);
        }));//订阅封面加载事件
    m_subscriptions.push_back(EventBus::Subscribe<event::worldname>([this](event::worldname event) {
        isshowcover = false;
        }));//订阅封面关闭事件
}

void Shader::init_cover() {
    ComPtr<ID3DBlob> blob;
    // 创建顶点着色器
    HR(CreateShaderFromFile(L"HLSL\\vs_cover.cso", L"HLSL\\vs_cover.hlsl", "VS", "vs_5_0", blob.ReleaseAndGetAddressOf()));
    HR(m_device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_vertexShader_cover.GetAddressOf()));
    // 创建并绑定顶点布局
    HR(m_device->CreateInputLayout(VertexPosUV::inputLayout, ARRAYSIZE(VertexPosUV::inputLayout),
        blob->GetBufferPointer(), blob->GetBufferSize(), m_inputLayout_cover.GetAddressOf()));

    // 创建像素着色器
    HR(CreateShaderFromFile(L"HLSL\\ps_cover.cso", L"HLSL\\ps_cover.hlsl", "PS", "ps_5_0", blob.ReleaseAndGetAddressOf()));
    HR(m_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_pixelShader_cover.GetAddressOf()));

    // 填充缓冲区描述
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;          // GPU 读写
    bd.ByteWidth = sizeof(vertex_cover);         // 总字节数
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; // 指定为顶点缓冲区
    bd.CPUAccessFlags = 0;

    // 初始化数据
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertex_cover;
    HR(m_device->CreateBuffer(&bd, &initData, m_Vertexbuffer_cover.GetAddressOf()));
}

void Shader::update_view(XMMATRIX view, XMFLOAT3 pos) {
    m_viewMatrix = view;
    eyePos = pos;
    cbtime t;
    t.timer = daytime;
    m_context->UpdateSubresource(m_timecbbuffer.Get(), 0, nullptr, &t, 0, 0);
    if (daytime >daytimeCycle)
        daytime = 0.0f;
}

void Shader::update_light(float dt) {
	daytime += dt;
	if (daytime > daytimeCycle)
		daytime = 0.0f;

	// 根据 daytime 计算光照方向
	const float dayLength = daytimeCycle;// 一天周期（秒）
	float angle = (daytime / dayLength) * XM_2PI;// 0 ~ 2π

	// 光源绕 XZ 平面旋转，Y 轴代表高度。夜晚时 sunY < 0 表示光源在地平线下
	float sunY = cosf(angle);// 1 (正午) → -1 (午夜)
	float sunX = sinf(angle) * 0.8f;// 水平旋转
	float sunZ = 0.6f;

	//XMVECTOR lightDir = XMVectorSet(sunX, sunY, sunZ, 0.0f);
    XMVECTOR lightDir = XMVectorSet(0.8f, 0.8f, 0.6f, 0.0f);
	lightDir = XMVector3Normalize(lightDir);
	// 存的是「光从太阳射向场景」的方向：正午 y 为负，与固定光照那版一致
	XMStoreFloat3(&m_lightDir, XMVectorNegate(lightDir));

	// 判断是否为白天（光源方向 Y 分量 > 0）
	bool isDay = (sunY > 0.0f);
     m_daylightFactor = 0.8f;
	//m_daylightFactor = isDay ? sunY : 0.0f;// 随太阳高度变化
}

void Shader::init_shadow() {
    // 1. 一张纹理数组，4 个 slice
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = kShadowSize;
    td.Height = kShadowSize;
    td.MipLevels = 1;
    td.ArraySize = CSMcount;
    td.Format = DXGI_FORMAT_R32_TYPELESS;
    td.SampleDesc.Count = 1;
    td.SampleDesc.Quality = 0;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = 0;
    td.MiscFlags = 0;
    HR(m_device->CreateTexture2D(&td, nullptr, m_shadowtexture.GetAddressOf()));

    // 2. 每级联一个 DSV
    for (UINT i = 0; i < CSMcount; ++i) {
        D3D11_DEPTH_STENCIL_VIEW_DESC dd = {};
        dd.Format = DXGI_FORMAT_D32_FLOAT;
        dd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        dd.Flags = 0;
        dd.Texture2DArray.MipSlice = 0;
        dd.Texture2DArray.FirstArraySlice = i;
        dd.Texture2DArray.ArraySize = 1;
        HR(m_device->CreateDepthStencilView(m_shadowtexture.Get(), &dd, m_shadowDSV[i].GetAddressOf()));
    }

    // 3. 一个 SRV 看全部 slice
    D3D11_SHADER_RESOURCE_VIEW_DESC sd = {};
    sd.Format = DXGI_FORMAT_R32_FLOAT;
    sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    sd.Texture2DArray.MostDetailedMip = 0;
    sd.Texture2DArray.MipLevels = 1;
    sd.Texture2DArray.FirstArraySlice = 0;
    sd.Texture2DArray.ArraySize = CSMcount;
    HR(m_device->CreateShaderResourceView(m_shadowtexture.Get(), &sd, m_shadowSRV.GetAddressOf()));

    // 4. 比较采样器：硬件 2x2 比较 + 双线性
    D3D11_SAMPLER_DESC samp = {};
    samp.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    samp.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    samp.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    samp.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    samp.BorderColor[0] = 1.0f;
    samp.BorderColor[1] = 1.0f;
    samp.BorderColor[2] = 1.0f;
    samp.BorderColor[3] = 1.0f;
    samp.MinLOD = 0;
    samp.MaxLOD = D3D11_FLOAT32_MAX;
    samp.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    HR(m_device->CreateSamplerState(&samp, m_shadowSampler.GetAddressOf()));

    // 5. 阴影 pass 光栅化状态：带深度偏移
    D3D11_RASTERIZER_DESC rs = {};
    rs.FillMode = D3D11_FILL_SOLID;
    rs.CullMode = D3D11_CULL_BACK;
    rs.FrontCounterClockwise = FALSE;
    rs.DepthBias = 200;
    rs.DepthBiasClamp = 0.0f;
    rs.SlopeScaledDepthBias = 1.5f;
    rs.DepthClipEnable = TRUE;
    rs.ScissorEnable = FALSE;
    rs.MultisampleEnable = FALSE;
    rs.AntialiasedLineEnable = FALSE;
    HR(m_device->CreateRasterizerState(&rs, m_shadowRS.GetAddressOf()));

    // 6. 常量缓冲区 b4
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(CBShadow);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = 0;
    HR(m_device->CreateBuffer(&cbd, nullptr, m_cbshadowVP.GetAddressOf()));

    // 7. 阴影顶点着色器 + 输入布局
    ComPtr<ID3DBlob> blob;
    HR(CreateShaderFromFile(L"HLSL\\vs_shadow.cso", L"HLSL\\vs_shadow.hlsl", "VS", "vs_5_0", blob.GetAddressOf()));
    HR(m_device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_shadowVS.GetAddressOf()));
    HR(m_device->CreateInputLayout(VertexPos::inputLayout, ARRAYSIZE(VertexPos::inputLayout),
        blob->GetBufferPointer(), blob->GetBufferSize(), m_shadowLayout.GetAddressOf()));
}

void Shader::update_shadow_matrices() {
    // 级联分割：从相机到阴影最远距离，单位是世界单位
    // 级联 0 覆盖 0.1~10，其余三级一直到相机 far=1000
    const float splits[CSMcount + 1] = { 0.1f, 10.0f, 100.0f, 500.0f, 1000.0f };
    for (int i = 0; i <= CSMcount; ++i)
        m_CascadePartitionsPercentage[i] = splits[i];

    XMMATRIX invView = XMMatrixInverse(nullptr, m_viewMatrix);
    XMVECTOR lightTravel = XMVector3Normalize(XMLoadFloat3(&m_lightDir));

    // 从投影矩阵反推 fov 与宽高比
    float yScale = XMVectorGetY(m_projectionMatrix.r[1]);
    float xScale = XMVectorGetX(m_projectionMatrix.r[0]);
    float tanHalf = 1.0f / yScale;
    float aspect = yScale / xScale;

    for (UINT c = 0; c < CSMcount; ++c) {
        float n = splits[c];
        float f = splits[c + 1];

        float hNear = n * tanHalf, wNear = hNear * aspect;
        float hFar = f * tanHalf, wFar = hFar * aspect;

        // 这一段视锥的 8 个角点（相机空间），再变到世界
        XMVECTOR cs[8] = {
            XMVectorSet(-wNear, -hNear, n, 1.0f), XMVectorSet(wNear, -hNear, n, 1.0f),
            XMVectorSet(wNear,  hNear,  n, 1.0f), XMVectorSet(-wNear, hNear, n, 1.0f),
            XMVectorSet(-wFar, -hFar,  f, 1.0f),  XMVectorSet(wFar, -hFar, f, 1.0f),
            XMVectorSet(wFar,  hFar,   f, 1.0f),  XMVectorSet(-wFar, hFar, f, 1.0f),
        };
        for (int k = 0; k < 8; ++k)
            cs[k] = XMVector3TransformCoord(cs[k], invView);

        // 中心 = 8 点平均
        XMVECTOR center = XMVectorZero();
        for (int k = 0; k < 8; ++k)
            center = XMVectorAdd(center, cs[k]);
        center = XMVectorScale(center, 1.0f / 8.0f);

        // 直径 = 8 点两两最大距离，与相机朝向无关
        float diameter = 0.0f;
        for (int a = 0; a < 8; ++a)
            for (int b = a + 1; b < 8; ++b) {
                float d = XMVectorGetX(XMVector3Length(XMVectorSubtract(cs[a], cs[b])));
                if (d > diameter) diameter = d;
            }

        // 光空间：眼睛沿光线退到盒子外，距离随盒子边长自适应
        // （固定 100 时，边长超过 200 的级联会算出负的 near，矩阵非法）
        const float dist = diameter + 10.0f;
        XMVECTOR up = fabsf(XMVectorGetY(lightTravel)) > 0.99f
            ? XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)
            : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        XMVECTOR eye = XMVectorSubtract(center, XMVectorScale(lightTravel, dist));
        XMMATRIX lightView = XMMatrixLookAtLH(eye, center, up);

        // 正交投影：边长 = 直径，深度覆盖整个球
        XMMATRIX lightProj = XMMatrixOrthographicLH(diameter, diameter,
            dist - diameter * 0.5f, dist + diameter * 0.5f);

        XMMATRIX shadowMat = lightView * lightProj;

        // texel 吸附：把世界原点对齐到纹素中心，否则纹素网格会随相机滑动
        // 此时 center 在光空间恒为 (0,0,dist)，吸它没有意义，要吸的是「世界原点」
        XMVECTOR origin = XMVector4Transform(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), shadowMat);
        origin = XMVectorScale(origin, (float)kShadowSize * 0.5f);          // NDC → 纹理空间
        XMVECTOR offset = XMVectorSubtract(XMVectorRound(origin), origin);  // 差几个纹素
        offset = XMVectorScale(offset, 2.0f / (float)kShadowSize);          // 纹理空间 → NDC
        offset = XMVectorSetZ(offset, 0.0f);
        offset = XMVectorSetW(offset, 0.0f);
        shadowMat.r[3] = XMVectorAdd(shadowMat.r[3], offset);

        m_lightVP[c] = XMMatrixTranspose(shadowMat);
    }
}

void Shader::render_shadow_pass() {
    if (Block::boundmap.empty() && sw::sws.empty())
        return;

    // 1. 保存主 pass 的渲染目标、深度、视口、光栅化状态
    ID3D11RenderTargetView* oldRTV = nullptr;
    ID3D11DepthStencilView* oldDSV = nullptr;
    m_context->OMGetRenderTargets(1, &oldRTV, &oldDSV);

    D3D11_VIEWPORT oldVP;
    UINT numVP = 1;
    m_context->RSGetViewports(&numVP, &oldVP);

    ID3D11RasterizerState* oldRS = nullptr;
    m_context->RSGetState(&oldRS);

    // 2. 解绑阴影 SRV，避免和 DSV 冲突
    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    m_context->PSSetShaderResources(4, 1, nullSRV);

    // 3. 纯深度 pass
    m_context->PSSetShader(nullptr, nullptr, 0);
    m_context->VSSetShader(m_shadowVS.Get(), nullptr, 0);
    m_context->IASetInputLayout(m_shadowLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->RSSetState(m_shadowRS.Get());
    m_context->OMSetDepthStencilState(m_pBlockDSS.Get(), 0);

    D3D11_VIEWPORT svp = { 0.0f, 0.0f, (float)kShadowSize, (float)kShadowSize, 0.0f, 1.0f };
    m_context->RSSetViewports(1, &svp);

    CBShadow cb = {};
    for (UINT i = 0; i < CSMcount; ++i)
        cb.lightVP[i] = m_lightVP[i];
    cb.cascadeSplit = XMFLOAT4(m_CascadePartitionsPercentage[1], m_CascadePartitionsPercentage[2],
                               m_CascadePartitionsPercentage[3], m_CascadePartitionsPercentage[4]);
    cb.shadowParams = XMFLOAT4(1.0f / (float)kShadowSize, 0.0f, 0.0f, 0.0f);

    UINT strideB = sizeof(VertexPosNormalTex), offsetB = 0;
    UINT strideM = sizeof(ModelVertex), offsetM = 0;
    XMVECTOR camPos = XMLoadFloat3(&eyePos);

    for (UINT c = 0; c < CSMcount; ++c) {
        m_context->OMSetRenderTargets(0, nullptr, m_shadowDSV[c].Get());
        m_context->ClearDepthStencilView(m_shadowDSV[c].Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

        cb.cascadeIndex = c;
        m_context->UpdateSubresource(m_cbshadowVP.Get(), 0, nullptr, &cb, 0, 0);
        m_context->VSSetConstantBuffers(4, 1, m_cbshadowVP.GetAddressOf());

        // --- 方块 ---
        m_context->IASetVertexBuffers(0, 1, m_Vertexbuffer_block.GetAddressOf(), &strideB, &offsetB);
        m_context->IASetIndexBuffer(m_Indexbuffer_block.Get(), DXGI_FORMAT_R32_UINT, 0);

        for (auto& entry : Block::boundmap) {
            int bx = std::get<0>(entry.first);
            int by = std::get<1>(entry.first);
            int bz = std::get<2>(entry.first);
            if (Block::block[bx][by][bz] == 0)
                continue;

            // 只在当前级联覆盖的范围内画
            // 投射者可能远在接收者之后（斜射时影子长度可达高差的 1.7 倍），
            // 所以剔除阈值放宽到该级远平面的 2 倍，否则影子边缘会成片消失
            const XMFLOAT3& bc = entry.second.Center;
            float d = XMVectorGetX(XMVector3Length(XMLoadFloat3(&bc) - camPos));
            if (d > m_CascadePartitionsPercentage[c + 1] * 2.0f)
                continue;

            XMMATRIX world = XMMatrixTranslation(bc.x, bc.y, bc.z);
            CBPerObject cbObj;
            cbObj.world = XMMatrixTranspose(world);
            cbObj.worldInvTranspose = cbObj.world;
            m_context->UpdateSubresource(m_cbPerObjectBuffer.Get(), 0, nullptr, &cbObj, 0, 0);
            m_context->VSSetConstantBuffers(1, 1, m_cbPerObjectBuffer.GetAddressOf());
            m_context->DrawIndexed(36, 0, 0);
        }

        // --- 生物 ---
        if (!Model::girlmodel.empty()) {
            for (UINT si = 0; si < (UINT)sw::sws.size(); ++si) {
                XMMATRIX world = sw::sws[si].m_transform.GetWorldMatrix();
                CBPerObject cbObj;
                cbObj.world = XMMatrixTranspose(world);
                cbObj.worldInvTranspose = cbObj.world;
                m_context->UpdateSubresource(m_cbPerObjectBuffer.Get(), 0, nullptr, &cbObj, 0, 0);
                m_context->VSSetConstantBuffers(1, 1, m_cbPerObjectBuffer.GetAddressOf());
                for (auto& mesh : Model::girlmodel) {
                    m_context->IASetVertexBuffers(0, 1, mesh.VertexBuffer.GetAddressOf(), &strideM, &offsetM);
                    m_context->IASetIndexBuffer(mesh.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
                    m_context->DrawIndexed(mesh.IndexCount, 0, 0);
                }
            }

        }
    }

    // 4. 恢复主 pass 状态
    m_context->OMSetRenderTargets(1, &oldRTV, oldDSV);
    m_context->RSSetViewports(1, &oldVP);
    if (oldRS) m_context->RSSetState(oldRS);
    if (oldRTV) oldRTV->Release();
    if (oldDSV) oldDSV->Release();
    if (oldRS) oldRS->Release();
}

void Shader::drawall() {
    if (isshowcover) {
        m_context->Draw(6, 0);
    }
    else {
        //更新视图投影矩阵
        CBcamera cb;
        // 阴影：先算 4 个级联的光矩阵，再渲染阴影贴图
        update_shadow_matrices();
        render_shadow_pass();

        XMMATRIX viewNoTranslate = m_viewMatrix;
        viewNoTranslate.r[3] = XMVectorSet(0, 0, 0, 1);  // 清除平移
        XMMATRIX vp = viewNoTranslate * m_projectionMatrix;
        cb.worldViewProj = XMMatrixTranspose(vp);
        m_context->UpdateSubresource(m_viewProjBuffer.Get(), 0, nullptr, &cb, 0, 0);
        m_context->VSSetConstantBuffers(0, 1, m_viewProjBuffer.GetAddressOf());
        // 绘制天空盒
        // 设置深度状态
        m_context->OMSetDepthStencilState(m_pSkyboxDSS.Get(), 0);
        // 使用无剔除光栅化状态，确保立方体内部面可见
        m_context->RSSetState(m_rsNoCull.Get());
        m_context->IASetInputLayout(m_inputLayout_skybox.Get());
        m_context->VSSetShader(m_vertexShader_skybox.Get(), nullptr, 0);
        m_context->PSSetShaderResources(0, 1, m_pSkyBoxSRV.GetAddressOf());
        m_context->PSSetShader(m_pixelShader_skybox.Get(), nullptr, 0);
        UINT stride = sizeof(VertexPos);
        UINT offset = 0;
        m_context->IASetVertexBuffers(0, 1, m_Vertexbuffer_skybox.GetAddressOf(), &stride, &offset);
        m_context->IASetIndexBuffer(m_Indexbuffer_skybox.Get(), DXGI_FORMAT_R32_UINT, 0);
        m_context->DrawIndexed(36, 0, 0);
        // 恢复默认光栅化状态
        //m_context->RSSetState(nullptr);
        drallblock();//绘制方块
        draw_particles();//绘制粒子
        drawsw();
        render_minimap(); // 渲染小地图到纹理
        draw_minimap(); // 绘制小地图

    }
}

void Shader::init_view_proj_buffer() {
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(CBcamera);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    HR(m_device->CreateBuffer(&bd, nullptr, m_viewProjBuffer.GetAddressOf()));
}

void Shader::init_skybox() {
    // ------------------------------
    // 1. 创建立方体顶点和索引数据
    // ------------------------------
    VertexPos vertices[8] = {
        // 前面
        { XMFLOAT3(-1.0f, -1.0f, -1.0f) }, { XMFLOAT3(-1.0f,  1.0f, -1.0f) },
        { XMFLOAT3(1.0f,  1.0f, -1.0f) }, { XMFLOAT3(1.0f, -1.0f, -1.0f) },
        // 后面
        { XMFLOAT3(-1.0f, -1.0f,  1.0f) }, { XMFLOAT3(1.0f, -1.0f,  1.0f) },
        { XMFLOAT3(1.0f,  1.0f,  1.0f) }, { XMFLOAT3(-1.0f,  1.0f,  1.0f) }
    };
    static const DWORD indices[36] = {
        // 前面 (z = -1)
        0, 1, 2,  0, 2, 3,
        // 后面 (z = +1)
        5, 6, 7,  5, 7, 4,
        // 左面 (x = -1)
        4, 7, 1,  4, 1, 0,
        // 右面 (x = +1)
        3, 2, 6,  3, 6, 5,
        // 顶面 (y = +1)
        1, 7, 6,  1, 6, 2,
        // 底面 (y = -1)
        4, 0, 3,  4, 3, 5
    };
    // 创建顶点缓冲区
    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = sizeof(vertices);
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA vInitData = { vertices };
    HR(m_device->CreateBuffer(&vbd, &vInitData, m_Vertexbuffer_skybox.GetAddressOf()));

    // 创建索引缓冲区
    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = sizeof(indices);
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA iInitData = { indices };
    HR(m_device->CreateBuffer(&ibd, &iInitData, m_Indexbuffer_skybox.GetAddressOf()));
    // 2. 加载天空盒立方体贴图
// ------------------------------
    HR(CreateDDSTextureFromFile(m_device, L"DDS/DDS/sky.dds", nullptr, m_pSkyBoxSRV.GetAddressOf()));
    ComPtr<ID3DBlob> vsBlob, psBlob;
    HR(CreateShaderFromFile(L"HLSL\\vs_sky.cso", L"HLSL\\vs_sky.hlsl", "VS", "vs_5_0", vsBlob.GetAddressOf()));
    HR(m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_vertexShader_skybox.GetAddressOf()));

    HR(CreateShaderFromFile(L"HLSL\\ps_sky.cso", L"HLSL\\ps_sky.hlsl", "PS", "ps_5_0", psBlob.GetAddressOf()));
    HR(m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_pixelShader_skybox.GetAddressOf()));

    // 创建输入布局（只有位置）
    HR(m_device->CreateInputLayout(VertexPos::inputLayout, ARRAYSIZE(VertexPos::inputLayout),
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), m_inputLayout_skybox.GetAddressOf()));
    // 创建天空盒专用深度状态（禁用深度写入，深度测试为 LESS_EQUAL）
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = true;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    dsDesc.StencilEnable = false;
    HR(m_device->CreateDepthStencilState(&dsDesc, m_pSkyboxDSS.GetAddressOf()));
    // 创建无剔除光栅化状态，方便在立方体内部查看天空盒
    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_NONE;
    rsDesc.FrontCounterClockwise = FALSE;
    rsDesc.DepthClipEnable = TRUE;
    HR(m_device->CreateRasterizerState(&rsDesc, m_rsNoCull.GetAddressOf()));
    // 创建方块深度状态（启用深度写入，比较函数为 LESS）
    D3D11_DEPTH_STENCIL_DESC dsDesc1 = {};
    dsDesc1.DepthEnable = true;
    dsDesc1.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc1.DepthFunc = D3D11_COMPARISON_LESS;
    dsDesc1.StencilEnable = false;
    HR(m_device->CreateDepthStencilState(&dsDesc1, m_pBlockDSS.GetAddressOf()));

    // 创建方块采样器（线性过滤，Wrap 寻址）
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    HR(m_device->CreateSamplerState(&sampDesc, m_blockSamplerState.GetAddressOf()));
}

void Shader::ExtractFrustumPlanes( XMVECTOR planes[6])
{
    XMMATRIX m = m_viewMatrix * m_projectionMatrix;

    // 左平面: (M03 + M00, M13 + M10, M23 + M20, M33 + M30)
    planes[0] = XMPlaneNormalize(XMVectorSet(
        m.r[0].m128_f32[3] + m.r[0].m128_f32[0],
        m.r[1].m128_f32[3] + m.r[1].m128_f32[0],
        m.r[2].m128_f32[3] + m.r[2].m128_f32[0],
        m.r[3].m128_f32[3] + m.r[3].m128_f32[0]));

    // 右平面: (M03 - M00, M13 - M10, M23 - M20, M33 - M30)
    planes[1] = XMPlaneNormalize(XMVectorSet(
        m.r[0].m128_f32[3] - m.r[0].m128_f32[0],
        m.r[1].m128_f32[3] - m.r[1].m128_f32[0],
        m.r[2].m128_f32[3] - m.r[2].m128_f32[0],
        m.r[3].m128_f32[3] - m.r[3].m128_f32[0]));

    // 底平面: (M03 + M01, M13 + M11, M23 + M21, M33 + M31)
    planes[2] = XMPlaneNormalize(XMVectorSet(
        m.r[0].m128_f32[3] + m.r[0].m128_f32[1],
        m.r[1].m128_f32[3] + m.r[1].m128_f32[1],
        m.r[2].m128_f32[3] + m.r[2].m128_f32[1],
        m.r[3].m128_f32[3] + m.r[3].m128_f32[1]));

    // 顶平面: (M03 - M01, M13 - M11, M23 - M21, M33 - M31)
    planes[3] = XMPlaneNormalize(XMVectorSet(
        m.r[0].m128_f32[3] - m.r[0].m128_f32[1],
        m.r[1].m128_f32[3] - m.r[1].m128_f32[1],
        m.r[2].m128_f32[3] - m.r[2].m128_f32[1],
        m.r[3].m128_f32[3] - m.r[3].m128_f32[1]));

    // 近平面: (M03 + M02, M13 + M12, M23 + M22, M33 + M32)
    planes[4] = XMPlaneNormalize(XMVectorSet(
        m.r[0].m128_f32[3] + m.r[0].m128_f32[2],
        m.r[1].m128_f32[3] + m.r[1].m128_f32[2],
        m.r[2].m128_f32[3] + m.r[2].m128_f32[2],
        m.r[3].m128_f32[3] + m.r[3].m128_f32[2]));

    // 远平面: (M03 - M02, M13 - M12, M23 - M22, M33 - M32)
    planes[5] = XMPlaneNormalize(XMVectorSet(
        m.r[0].m128_f32[3] - m.r[0].m128_f32[2],
        m.r[1].m128_f32[3] - m.r[1].m128_f32[2],
        m.r[2].m128_f32[3] - m.r[2].m128_f32[2],
        m.r[3].m128_f32[3] - m.r[3].m128_f32[2]));
}

void Shader::drallblock() {
    // ========== 重新计算并绑定完整的视图投影矩阵（用于方块） ==========
    CBcamera cb;
    XMMATRIX vp = m_viewMatrix * m_projectionMatrix;   // 使用完整视图矩阵（包含平移）
    cb.worldViewProj = XMMatrixTranspose(vp);
    m_context->UpdateSubresource(m_viewProjBuffer.Get(), 0, nullptr, &cb, 0, 0);
    m_context->VSSetConstantBuffers(0, 1, m_viewProjBuffer.GetAddressOf());
    // 设置深度状态（启用深度写入）
    m_context->OMSetDepthStencilState(m_pBlockDSS.Get(), 0);
    // 设置采样器到槽位 0
    m_context->PSSetSamplers(0, 1, m_blockSamplerState.GetAddressOf());
    // 绑定顶点缓冲区
    UINT stride = sizeof(VertexPosNormalTex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_Vertexbuffer_block.GetAddressOf(), &stride, &offset);
    // 绑定索引缓冲区
    m_context->IASetIndexBuffer(m_Indexbuffer_block.Get(), DXGI_FORMAT_R32_UINT, 0);
    // 输入布局
    m_context->IASetInputLayout(m_inputLayout_block.Get());
    // 着色器
    m_context->VSSetShader(m_vertexShader_block.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader_block.Get(), nullptr, 0);
    // 绑定阴影资源：t4 贴图数组、s1 比较采样器、b4 光矩阵
    m_context->PSSetShaderResources(4, 1, m_shadowSRV.GetAddressOf());
    m_context->PSSetSamplers(1, 1, m_shadowSampler.GetAddressOf());
    m_context->PSSetConstantBuffers(4, 1, m_cbshadowVP.GetAddressOf());

    // ========== 2. 设置像素着色器常量缓冲区（槽位2） ==========
    CBPS cbPS;
    memset(&cbPS, 0, sizeof(cbPS));

    // 光照方向与白天强度由 update_light() 每帧算好
    cbPS.dirLight.direction = m_lightDir;
    float daylightFactor = m_daylightFactor;

    // 环境光
    cbPS.dirLight.ambient = XMFLOAT4(
        0.5f ,
        0.5f ,
        0.5f ,
        1.0f
    );

    // 漫反射：白天有光，夜晚为0
    cbPS.dirLight.diffuse = XMFLOAT4(
        1.0f * daylightFactor,
        1.0f * daylightFactor,
        1.0f * daylightFactor,
        1.0f
    );

    // 镜面反射：同样受白天影响
    cbPS.dirLight.specular = XMFLOAT4(
        0.5f * daylightFactor,
        0.5f * daylightFactor,
        0.5f * daylightFactor,
        1.0f
    );
    cbPS.material.ambient = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
    cbPS.material.diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    cbPS.material.specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    cbPS.material.reflect = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    // 眼睛位置
    cbPS.eyePos = eyePos;
    cbPS.pad = 0.0f;
    m_context->UpdateSubresource(m_cbPSBuffer.Get(), 0, nullptr, &cbPS, 0, 0);
    m_context->PSSetConstantBuffers(2, 1, m_cbPSBuffer.GetAddressOf());   // 槽位2

    // ========== 3. 获取视锥体平面 ==========
    XMVECTOR planes[6];
    ExtractFrustumPlanes(planes);

    for (auto& entry : Block::boundmap) {
        int x = std::get<0>(entry.first);
        int y = std::get<1>(entry.first);
        int z = std::get<2>(entry.first);
        const BoundingBox& box = entry.second;

        // 正确的视锥体剔除：测试是否完全在任一平面的外部
        bool inside = true;
        for (int i = 0; i < 6; ++i) {
            // 计算包围盒中心到平面的有符号距离
            XMVECTOR center = XMLoadFloat3(&box.Center);
            float dist = XMVectorGetX(XMPlaneDotCoord(planes[i], center));

            // 计算包围盒在平面法向量方向上的最大投影半径
            // 公式：r = e_x * |n_x| + e_y * |n_y| + e_z * |n_z|
            float nx = fabs(XMVectorGetX(planes[i]));
            float ny = fabs(XMVectorGetY(planes[i]));
            float nz = fabs(XMVectorGetZ(planes[i]));
            float r = box.Extents.x * nx + box.Extents.y * ny + box.Extents.z * nz;

            // 如果距离 < -r，说明包围盒完全在平面负半空间（视锥体外部）
            if (dist < -r) {
                inside = false;
                break;
            }
        }

        if (!inside) continue;  // 剔除


        // 获取方块类型
        int blockType = Block::block[x][y][z]; // 或者从 block 数组获取
        if (blockType != 0) {
            // 世界矩阵
            XMFLOAT3 center = box.Center; // 中心已经计算好
            XMMATRIX world = XMMatrixTranslation(center.x, center.y, center.z);
            CBPerObject cbObj;
            cbObj.world = XMMatrixTranspose(world);
            cbObj.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, world));
            m_context->UpdateSubresource(m_cbPerObjectBuffer.Get(), 0, nullptr, &cbObj, 0, 0);
            m_context->VSSetConstantBuffers(1, 1, m_cbPerObjectBuffer.GetAddressOf());

            // 纹理
            ID3D11ShaderResourceView* texSRV;
            switch (blockType) {
            case 1: // 石头
                texSRV = m_stoneSRV.Get();
                break;
            case 2: // 草
                texSRV = m_grassSRV.Get();
                break;
            case 3://水晶
                texSRV = m_shuijintSRV.Get();
            }

            m_context->PSSetShaderResources(0, 1, &texSRV);

            m_context->DrawIndexed(36, 0, 0);
        }
    }
 
    drawdrop();
}

void Shader::init_block() {
    OutputDebugStringW((L"boundmap size: " + std::to_wstring(Block::boundmap.size())).c_str());
    //1.创建顶点数据
    VertexPosNormalTex vertices[24] = {
        // 前面 (Z+)
        { XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT3(0,0,1), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT3(0,0,1), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(0.5f,  0.5f,  0.5f), XMFLOAT3(0,0,1), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT3(0,0,1), XMFLOAT2(1.0f, 1.0f) },

        // 后面 (Z-)
        { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0,0,-1), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT3(0,0,-1), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(0.5f,  0.5f, -0.5f), XMFLOAT3(0,0,-1), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT3(0,0,-1), XMFLOAT2(1.0f, 1.0f) },

        // 左面 (X-)
        { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(-1,0,0), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT3(-1,0,0), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT3(-1,0,0), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT3(-1,0,0), XMFLOAT2(1.0f, 1.0f) },

        // 右面 (X+)
        { XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT3(1,0,0), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(0.5f,  0.5f, -0.5f), XMFLOAT3(1,0,0), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(0.5f,  0.5f,  0.5f), XMFLOAT3(1,0,0), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT3(1,0,0), XMFLOAT2(1.0f, 1.0f) },

        // 上面 (Y+)
        { XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT3(0,1,0), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT3(0,1,0), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(0.5f,  0.5f,  0.5f), XMFLOAT3(0,1,0), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(0.5f,  0.5f, -0.5f), XMFLOAT3(0,1,0), XMFLOAT2(1.0f, 1.0f) },

        // 下面 (Y-)
        { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0,-1,0), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT3(0,-1,0), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT3(0,-1,0), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT3(0,-1,0), XMFLOAT2(1.0f, 1.0f) }
    };
    static const DWORD indices[36] = {
        // 前面
        0,1,2, 0,2,3,
        // 后面
        4,6,5, 4,7,6,
        // 左面
        8,9,10, 8,10,11,
        // 右面
        12,14,13, 12,15,14,
        // 上面
        16,17,18, 16,18,19,
        // 下面
        20,22,21, 20,23,22
    };
    // -------------------------
    // 2. 加载纹理
    // ------------------------------
    HR(CreateDDSTextureFromFile(m_device, L"DDS/DDS/grass.dds", nullptr, m_grassSRV.GetAddressOf()));
    HR(CreateDDSTextureFromFile(m_device, L"DDS/DDS/stone.dds", nullptr, m_stoneSRV.GetAddressOf()));
    HR(CreateDDSTextureFromFile(m_device, L"DDS/DDS/shuijin.dds", nullptr, m_shuijintSRV.GetAddressOf()));
    // ------------------------------
    // 3. 编译着色器
    // ------------------------------
    ComPtr<ID3DBlob> vsBlob, psBlob;
    HR(CreateShaderFromFile(L"HLSL\\vs_block.cso", L"HLSL\\vs_block.hlsl", "VS", "vs_5_0", vsBlob.GetAddressOf()));
    HR(m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_vertexShader_block.GetAddressOf()));
    HR(m_device->CreateInputLayout(VertexPosNormalTex::inputLayout, ARRAYSIZE(VertexPosNormalTex::inputLayout),
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), m_inputLayout_block.GetAddressOf()));
    HR(CreateShaderFromFile(L"HLSL\\ps_block.cso", L"HLSL\\ps_block.hlsl", "PS", "ps_5_0", psBlob.GetAddressOf()));
    HR(m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_pixelShader_block.GetAddressOf()));


    // ------------------------------
    // 5. 创建顶点缓冲区
    // ------------------------------
    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = sizeof(vertices);
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA vInitData = { vertices };
    HR(m_device->CreateBuffer(&vbd, &vInitData, m_Vertexbuffer_block.GetAddressOf()));

    // ------------------------------
    // 6. 创建索引缓冲区
    // ------------------------------
    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = sizeof(indices);
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA iInitData = { indices };
    HR(m_device->CreateBuffer(&ibd, &iInitData, m_Indexbuffer_block.GetAddressOf()));

    // ------------------------------
    // 7. 创建常量缓冲区
    // ------------------------------
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(CBPerObject);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    HR(m_device->CreateBuffer(&cbd, nullptr, m_cbPerObjectBuffer.GetAddressOf()));

    cbd.ByteWidth = sizeof(CBPS);
    HR(m_device->CreateBuffer(&cbd, nullptr, m_cbPSBuffer.GetAddressOf()));
}



void Shader::drawdrop() {
    if (Drop::dropobjs.empty()) return;
    CBPerObject cbObj;
    for (const auto& drop : Drop::dropobjs) {
        // 世界矩阵：缩放 0.1 倍并平移到掉落物位置
        XMMATRIX scaleMat = XMMatrixScaling(0.1f, 0.1f, 0.1f);
        XMMATRIX transMat = XMMatrixTranslation(drop.pos.x, drop.pos.y, drop.pos.z);
        XMMATRIX world = scaleMat * transMat;
        cbObj.world = XMMatrixTranspose(world);
        cbObj.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, world));
        m_context->UpdateSubresource(m_cbPerObjectBuffer.Get(), 0, nullptr, &cbObj, 0, 0);
        m_context->VSSetConstantBuffers(1, 1, m_cbPerObjectBuffer.GetAddressOf());

        // 根据掉落物类型选择纹理（1=石头，2=草，3=水晶，其他默认石头）
        ID3D11ShaderResourceView* texSRV = m_stoneSRV.Get();
        if (drop.type == 2) texSRV = m_grassSRV.Get();
        else if (drop.type == 3) texSRV = m_shuijintSRV.Get();
        m_context->PSSetShaderResources(0, 1, &texSRV);

        // 绘制一个立方体（36 个索引）
        m_context->DrawIndexed(36, 0, 0);
    }
}

void Shader::init_sw() {
    ComPtr<ID3DBlob> blob;

    // ========== 1. 生物模型着色器 ==========
    // 顶点着色器
    HR(CreateShaderFromFile(L"HLSL\\vs_sw_model.cso", L"HLSL\\vs_sw_model.hlsl", "VS", "vs_5_0", blob.ReleaseAndGetAddressOf()));
    HR(m_device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_vertexShader_swModel.GetAddressOf()));
    // 像素着色器
    HR(CreateShaderFromFile(L"HLSL\\ps_sw_model.cso", L"HLSL\\ps_sw_model.hlsl", "PS", "ps_5_0", blob.ReleaseAndGetAddressOf()));
    HR(m_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_pixelShader_swModel.GetAddressOf()));

    // 模型常量缓冲区 (CBsw) - 若之前未创建则创建
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(CBsw);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    HR(m_device->CreateBuffer(&cbd, nullptr, m_cbSwBuffer.GetAddressOf()));

    // ========== 2. 血条着色器 ==========
    // 血条顶点着色器（注意保留 blob 用于输入布局）
    HR(CreateShaderFromFile(L"HLSL\\vs_sw_bar.cso", L"HLSL\\vs_sw_bar.hlsl", "VS", "vs_5_0", blob.ReleaseAndGetAddressOf()));
    HR(m_device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_vertexShader_swBar.GetAddressOf()));

    // 创建血条输入布局（使用 vs_sw_bar 的 blob）
    D3D11_INPUT_ELEMENT_DESC barLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLOOD",    0, DXGI_FORMAT_R32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    HR(m_device->CreateInputLayout(barLayout, ARRAYSIZE(barLayout),
        blob->GetBufferPointer(), blob->GetBufferSize(),
        m_inputLayout_swBar.GetAddressOf()));

    // 几何着色器
    HR(CreateShaderFromFile(L"HLSL\\gs_sw_bar.cso", L"HLSL\\gs_sw_bar.hlsl", "GS", "gs_5_0", blob.ReleaseAndGetAddressOf()));
    HR(m_device->CreateGeometryShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_geometryShader_swBar.GetAddressOf()));

    // 像素着色器
    HR(CreateShaderFromFile(L"HLSL\\ps_sw_bar.cso", L"HLSL\\ps_sw_bar.hlsl", "PS", "ps_5_0", blob.ReleaseAndGetAddressOf()));
    HR(m_device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, m_pixelShader_swBar.GetAddressOf()));

    // 血条常量缓冲区
    cbd.ByteWidth = sizeof(CBswBar);
    HR(m_device->CreateBuffer(&cbd, nullptr, m_cbSwBarBuffer.GetAddressOf()));

    // 血条动态顶点缓冲区
    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DYNAMIC;
    vbd.ByteWidth = sizeof(BarVertex) * 100;   // 预留100个生物
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    HR(m_device->CreateBuffer(&vbd, nullptr, m_vertexBuffer_swBar.GetAddressOf()));
}

void Shader::drawsw() {
    if (sw::sws.empty() || Model::girlmodel.empty()) return;

    // ============================================================
    // 保存当前管线状态（仅保存会被我们修改的部分）
    // ============================================================
    ID3D11VertexShader* oldVS = nullptr;
    ID3D11PixelShader* oldPS = nullptr;
    ID3D11GeometryShader* oldGS = nullptr;
    ID3D11InputLayout* oldLayout = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY oldTopology;
    ID3D11Buffer* oldVSConstantBuffers[2] = { nullptr, nullptr };
    ID3D11Buffer* oldPSConstantBuffers[2] = { nullptr, nullptr };
    ID3D11Buffer* oldGSConstantBuffers[2] = { nullptr, nullptr };

    m_context->VSGetShader(&oldVS, nullptr, nullptr);
    m_context->PSGetShader(&oldPS, nullptr, nullptr);
    m_context->GSGetShader(&oldGS, nullptr, nullptr);
    m_context->IAGetInputLayout(&oldLayout);
    m_context->IAGetPrimitiveTopology(&oldTopology);
    m_context->VSGetConstantBuffers(0, 2, oldVSConstantBuffers);
    m_context->PSGetConstantBuffers(0, 2, oldPSConstantBuffers);
    m_context->GSGetConstantBuffers(0, 2, oldGSConstantBuffers);

    // ============================================================
    // 第一步：绘制生物模型
    // ============================================================
    m_context->VSSetShader(m_vertexShader_swModel.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader_swModel.Get(), nullptr, 0);
    m_context->GSSetShader(nullptr, nullptr, 0);

    m_context->IASetInputLayout(m_inputLayout_block.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 绑定每帧常量缓冲区 (b0, b2) —— 与方块/掉落物相同
    m_context->VSSetConstantBuffers(0, 1, m_viewProjBuffer.GetAddressOf());
    m_context->PSSetConstantBuffers(2, 1, m_cbPSBuffer.GetAddressOf());

    // 采样器
    m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

    // 绑定阴影资源：t4 贴图数组、s1 比较采样器、b4 光矩阵
    m_context->PSSetShaderResources(4, 1, m_shadowSRV.GetAddressOf());
    m_context->PSSetSamplers(1, 1, m_shadowSampler.GetAddressOf());
    m_context->PSSetConstantBuffers(4, 1, m_cbshadowVP.GetAddressOf());

    for (auto& obj : sw::sws) {
        XMMATRIX world = obj.m_transform.GetWorldMatrix();

        CBsw cbSw;
        cbSw.world = XMMatrixTranspose(world);
        cbSw.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, world));
        cbSw.blood = obj.blood;
        // eyepos 在模型着色器中未使用，但仍需填充以保持结构体大小
        cbSw.eyepos = XMFLOAT3(0, 0, 0);

        m_context->UpdateSubresource(m_cbSwBuffer.Get(), 0, nullptr, &cbSw, 0, 0);
        m_context->VSSetConstantBuffers(1, 1, m_cbSwBuffer.GetAddressOf());  // b1

        // 绘制所有网格（girlmodel）
        for (const auto& mesh : Model::girlmodel) {
            UINT stride = sizeof(ModelVertex);
            UINT offset = 0;
            m_context->IASetVertexBuffers(0, 1, mesh.VertexBuffer.GetAddressOf(), &stride, &offset);
            m_context->IASetIndexBuffer(mesh.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
            m_context->PSSetShaderResources(0, 1, mesh.TextureSRV.GetAddressOf());
            m_context->DrawIndexed(mesh.IndexCount, 0, 0);
        }
    }

    // ============================================================
    // 第二步：绘制血条（点列表 + 几何着色器）
    // ============================================================
    m_context->VSSetShader(m_vertexShader_swBar.Get(), nullptr, 0);
    m_context->GSSetShader(m_geometryShader_swBar.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader_swBar.Get(), nullptr, 0);
    m_context->IASetInputLayout(m_inputLayout_swBar.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

    // 绑定常量缓冲区
    m_context->VSSetConstantBuffers(0, 1, m_viewProjBuffer.GetAddressOf());  // b0 (GS也会用)
    m_context->GSSetConstantBuffers(0, 1, m_viewProjBuffer.GetAddressOf());
    // b1 给 GS/PS 用（相机位置、最大血量）
    CBswBar cbBar;
    cbBar.eyePosW = eyePos;
    cbBar.maxBlood = 10.0f;
    m_context->UpdateSubresource(m_cbSwBarBuffer.Get(), 0, nullptr, &cbBar, 0, 0);
    m_context->GSSetConstantBuffers(1, 1, m_cbSwBarBuffer.GetAddressOf());
    m_context->PSSetConstantBuffers(1, 1, m_cbSwBarBuffer.GetAddressOf());

    // 构建动态顶点数据
    std::vector<BarVertex> barVertices;
    barVertices.reserve(sw::sws.size());
    for (const auto& obj : sw::sws) {
        BarVertex v;
        v.center = obj.m_box.Center;  // 生物包围盒中心
        v.blood = obj.blood;
        barVertices.push_back(v);
    }

    // 更新动态缓冲区
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(m_context->Map(m_vertexBuffer_swBar.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, barVertices.data(), sizeof(BarVertex) * barVertices.size());
        m_context->Unmap(m_vertexBuffer_swBar.Get(), 0);
    }

    UINT stride = sizeof(BarVertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_vertexBuffer_swBar.GetAddressOf(), &stride, &offset);
    m_context->Draw((UINT)barVertices.size(), 0);

    // ============================================================
    // 第三步：恢复所有被修改的管线状态到进入函数前的状态
    // ============================================================
    m_context->VSSetShader(oldVS, nullptr, 0);
    m_context->PSSetShader(oldPS, nullptr, 0);
    m_context->GSSetShader(oldGS, nullptr, 0);
    m_context->IASetInputLayout(oldLayout);
    m_context->IASetPrimitiveTopology(oldTopology);

    // 恢复常量缓冲区绑定（注意：这里恢复的是槽位0~1的绑定，因为我们的绘制只改了这些槽位）
    m_context->VSSetConstantBuffers(0, 2, oldVSConstantBuffers);
    m_context->PSSetConstantBuffers(0, 2, oldPSConstantBuffers);
    m_context->GSSetConstantBuffers(0, 2, oldGSConstantBuffers);

    // 释放 COM 引用
    SAFE_RELEASE(oldVS);
    SAFE_RELEASE(oldPS);
    SAFE_RELEASE(oldGS);
    SAFE_RELEASE(oldLayout);
    for (int i = 0; i < 2; ++i) {
        SAFE_RELEASE(oldVSConstantBuffers[i]);
        SAFE_RELEASE(oldPSConstantBuffers[i]);
        SAFE_RELEASE(oldGSConstantBuffers[i]);
    }

    // 不需要恢复顶点/索引缓冲区、纹理等，因为后续绘制会自己重新绑定。
    // 但为了绝对安全，可以额外恢复一下常用的采样器状态。
    m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());
}

void Shader::init_minimap() {
    // 小地图尺寸（比如256x256）
    const UINT mapWidth = 256;
    const UINT mapHeight = 256;

    // 创建纹理
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = mapWidth;
    texDesc.Height = mapHeight;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    HR(m_device->CreateTexture2D(&texDesc, nullptr, m_minimapTex.GetAddressOf()));

    // 创建RTV
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = texDesc.Format;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    HR(m_device->CreateRenderTargetView(m_minimapTex.Get(), &rtvDesc, m_minimapRTV.GetAddressOf()));

    // 创建SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    HR(m_device->CreateShaderResourceView(m_minimapTex.Get(), &srvDesc, m_minimapSRV.GetAddressOf()));

    // 创建深度缓冲区（小地图也需要深度测试）
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = mapWidth;
    depthDesc.Height = mapHeight;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> depthTex;
    HR(m_device->CreateTexture2D(&depthDesc, nullptr, depthTex.GetAddressOf()));
    HR(m_device->CreateDepthStencilView(depthTex.Get(), nullptr, m_minimapDSV.GetAddressOf()));

    // 视口
    m_minimapViewport.TopLeftX = 0.0f;
    m_minimapViewport.TopLeftY = 0.0f;
    m_minimapViewport.Width = (float)mapWidth;
    m_minimapViewport.Height = (float)mapHeight;
    m_minimapViewport.MinDepth = 0.0f;
    m_minimapViewport.MaxDepth = 1.0f;

    // 创建屏幕四边形顶点缓冲区（用于显示小地图）
    VertexPosUV vertices[6] = {
        // 第一个三角形
        { XMFLOAT3(0.6f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) }, // 左上
        { XMFLOAT3(1.0f, 0.6f, 0.0f), XMFLOAT2(1.0f, 1.0f) }, // 右下
        { XMFLOAT3(0.6f, 0.6f, 0.0f), XMFLOAT2(0.0f, 1.0f) }, // 左下
        // 第二个三角形
        { XMFLOAT3(0.6f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) }, // 左上
        { XMFLOAT3(1.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) }, // 右上
        { XMFLOAT3(1.0f, 0.6f, 0.0f), XMFLOAT2(1.0f, 1.0f) }  // 右下
    };
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(vertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA initData = { vertices };
    HR(m_device->CreateBuffer(&bd, &initData, m_minimapVertexBuffer.GetAddressOf()));
}

void Shader::render_minimap() {
    // 保存当前渲染目标和深度模板视图
    ID3D11RenderTargetView* oldRTV = nullptr;
    ID3D11DepthStencilView* oldDSV = nullptr;
    m_context->OMGetRenderTargets(1, &oldRTV, &oldDSV);
    D3D11_VIEWPORT oldVP;
    UINT numVP = 1;
    m_context->RSGetViewports(&numVP, &oldVP);

    // 设置小地图渲染目标
    m_context->OMSetRenderTargets(1, m_minimapRTV.GetAddressOf(), m_minimapDSV.Get());
    m_context->RSSetViewports(1, &m_minimapViewport);
    // 清除
    float clearColor[4] = { 0.5f, 0.7f, 1.0f, 1.0f }; // 淡蓝色背景
    m_context->ClearRenderTargetView(m_minimapRTV.Get(), clearColor);
    m_context->ClearDepthStencilView(m_minimapDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    // 使用俯视相机渲染场景
    // 1. 设置视图矩阵（俯视角度）
    XMMATRIX view = XMMatrixLookAtLH(
        XMVectorSet(10.0f, 20.0f, 10.0f, 1.0f),   // 眼睛位置
        XMVectorSet(10.0f, 0.0f, 10.0f, 1.0f),    // 看向地图中心
        XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)       // 上方向：改为世界Z轴正向
    );
    // 2. 设置投影矩阵（正交投影更适合小地图）
    XMMATRIX proj = XMMatrixOrthographicLH(30.0f, 30.0f, 1.0f, 100.0f);
    // 3. 更新常量缓冲区
    CBcamera cb;
    cb.worldViewProj = XMMatrixTranspose(view * proj);
    m_context->UpdateSubresource(m_viewProjBuffer.Get(), 0, nullptr, &cb, 0, 0);
    m_context->VSSetConstantBuffers(0, 1, m_viewProjBuffer.GetAddressOf());

    // 4. 渲染方块（简化，只绘制方块，不绘制细节）
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetInputLayout(m_inputLayout_block.Get());
    m_context->VSSetShader(m_vertexShader_block.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader_block.Get(), nullptr, 0);
    UINT stride = sizeof(VertexPosNormalTex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_Vertexbuffer_block.GetAddressOf(), &stride, &offset);
    m_context->IASetIndexBuffer(m_Indexbuffer_block.Get(), DXGI_FORMAT_R32_UINT, 0);

    // 绘制所有方块
    for (auto& entry : Block::boundmap) {
        int x = std::get<0>(entry.first);
        int y = std::get<1>(entry.first);
        int z = std::get<2>(entry.first);
        int blockType = Block::block[x][y][z];
        if (blockType != 0) {
            XMFLOAT3 center = entry.second.Center;
            XMMATRIX world = XMMatrixTranslation(center.x, center.y, center.z);
            CBPerObject cbObj;
            cbObj.world = XMMatrixTranspose(world);
            cbObj.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, world));
            m_context->UpdateSubresource(m_cbPerObjectBuffer.Get(), 0, nullptr, &cbObj, 0, 0);
            m_context->VSSetConstantBuffers(1, 1, m_cbPerObjectBuffer.GetAddressOf());

            ID3D11ShaderResourceView* texSRV;
            if (blockType == 1) texSRV = m_stoneSRV.Get();
            else if (blockType == 2) texSRV = m_grassSRV.Get();
            else if (blockType == 3) texSRV = m_shuijintSRV.Get();
            else texSRV = m_stoneSRV.Get(); // 默认石头纹理
            m_context->PSSetShaderResources(0, 1, &texSRV);
            m_context->DrawIndexed(36, 0, 0);
        }
    }

    // 恢复原始渲染目标、深度和视口
    m_context->OMSetRenderTargets(1, &oldRTV, oldDSV);
    if (oldRTV) oldRTV->Release();
    if (oldDSV) oldDSV->Release();
    m_context->RSSetViewports(1, &oldVP);
}

void Shader::draw_minimap() {
    // 使用封面着色器绘制小地图纹理
    m_context->IASetInputLayout(m_inputLayout_cover.Get());
    m_context->VSSetShader(m_vertexShader_cover.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader_cover.Get(), nullptr, 0);
    m_context->PSSetShaderResources(0, 1, m_minimapSRV.GetAddressOf());

    UINT stride = sizeof(VertexPosUV);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_minimapVertexBuffer.GetAddressOf(), &stride, &offset);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->Draw(6, 0);
}


void Shader::init_particles() {
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(XMFLOAT4); 
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    HR(m_device->CreateBuffer(&bd, nullptr, m_cbEyePosBuffer.GetAddressOf()));
    m_liveParticleCount = 0;
    m_particles.reserve(MAX_PARTICLES);

    // 创建动态顶点缓冲区
    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DYNAMIC;
    vbd.ByteWidth = sizeof(ParticleVertex) * MAX_PARTICLES;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    HR(m_device->CreateBuffer(&vbd, nullptr, m_particleVertexBuffer.GetAddressOf()));

    // 编译着色器
    ComPtr<ID3DBlob> vsBlob, gsBlob, psBlob, errorBlob;
    // 顶点着色器
    HR(CreateShaderFromFile(L"HLSL\\vs_particle.cso", L"HLSL\\vs_particle.hlsl", "VS", "vs_5_0", vsBlob.GetAddressOf()));
    HR(m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_vsParticle.GetAddressOf()));

    // 几何着色器
    HR(CreateShaderFromFile(L"HLSL\\gs_particle.cso", L"HLSL\\gs_particle.hlsl", "GS", "gs_5_0", gsBlob.GetAddressOf()));
    HR(m_device->CreateGeometryShader(gsBlob->GetBufferPointer(), gsBlob->GetBufferSize(), nullptr, m_gsParticle.GetAddressOf()));

    // 像素着色器
    HR(CreateShaderFromFile(L"HLSL\\ps_particle.cso", L"HLSL\\ps_particle.hlsl", "PS", "ps_5_0", psBlob.GetAddressOf()));
    HR(m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_psParticle.GetAddressOf()));

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "SIZE",     0, DXGI_FORMAT_R32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TYPE",     0, DXGI_FORMAT_R32_SINT,        0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    HR(m_device->CreateInputLayout(layout, ARRAYSIZE(layout), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), m_inputLayoutParticle.GetAddressOf()));

    // 混合状态（Alpha混合）
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    HR(m_device->CreateBlendState(&blendDesc, m_particleBlendState.GetAddressOf()));

    // 深度模板状态（禁用写入）
    D3D11_DEPTH_STENCIL_DESC dssDesc = {};
    dssDesc.DepthEnable = TRUE;
    dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // 不写入深度
    dssDesc.DepthFunc = D3D11_COMPARISON_LESS;
    HR(m_device->CreateDepthStencilState(&dssDesc, m_particleDSS.GetAddressOf()));

    // 订阅破坏方块事件
    m_subscriptions.push_back(EventBus::Subscribe<event::leftmousedown>([this](const event::leftmousedown& e) {
        int x = std::get<0>(e.pos);
        int y = std::get<1>(e.pos);
        int z = std::get<2>(e.pos);
        int blockType = Block::block[x][y][z];
        if (blockType != 0) {
            XMFLOAT3 center(x + 0.5f, y + 0.5f, z + 0.5f);
            emit_particles(center, blockType);
        }
        }));
}

void Shader::emit_particles(const XMFLOAT3& center, int blockType) {
    const int count = 5; // 每次破坏生成5个粒子
    for (int i = 0; i < count; ++i) {
        if (m_liveParticleCount >= MAX_PARTICLES) break;

        Particle* slot = nullptr;
        if (!slot) {
            if (m_particles.size() < MAX_PARTICLES) {
                m_particles.emplace_back();
                slot = &m_particles.back();
            }
            else {
                break;
            }
        }

        // 随机方向（向上半球为主）
        float theta = XM_2PI * (rand() / (float)RAND_MAX);
        float phi = XM_PIDIV2 * (rand() / (float)RAND_MAX);
        XMFLOAT3 dir = {
            sinf(phi) * cosf(theta),
            sinf(phi) * sinf(theta) + 0.3f,
            cosf(phi)
        };
        float speed = 2.0f + (rand() / (float)RAND_MAX) * 3.0f;
        slot->Velocity = { dir.x * speed, dir.y * speed, dir.z * speed };
        slot->Position = center;
        slot->Life = 1.0f + (rand() / (float)RAND_MAX) * 0.5f;
        slot->Size = 0.1f + (rand() / (float)RAND_MAX) * 0.15f;
        slot->BlockType = blockType;

        ++m_liveParticleCount;
    }
}

void Shader::update_particles(float dt) {
    if(m_particles.empty()) return;
    const float GRAVITY = -5.0f;
    m_liveParticleCount = 0;

    // 从后向前遍历，方便删除
    for (size_t i = 0; i < m_particles.size(); ) {
        Particle& p = m_particles[i];
        if (p.Life <= 0.0f) {
            // 与最后一个元素交换并弹出（高效删除）
            p = m_particles.back();
            m_particles.pop_back();
            // 不递增 i，因为交换过来的新元素还没处理
            continue;
        }

        // 重力影响速度
        p.Velocity.y += GRAVITY * dt;
        // 更新位置
        p.Position.x += p.Velocity.x * dt;
        p.Position.y += p.Velocity.y * dt;
        p.Position.z += p.Velocity.z * dt;
        // 寿命减少
        p.Life -= dt;
        // 尺寸略微缩小
        p.Size *= (1.0f - dt * 0.5f);

        ++m_liveParticleCount;
        ++i; // 只有未删除时才递增索引
    }

    // 更新相机位置缓冲区
    struct EyePosBuffer { XMFLOAT3 eyePos; float pad; };
    EyePosBuffer eyeBuf = { eyePos, 0.0f };
    m_context->UpdateSubresource(m_cbEyePosBuffer.Get(), 0, nullptr, &eyeBuf, 0, 0);
}

void Shader::draw_particles() {
    if (m_liveParticleCount == 0) return;

    // 填充动态顶点缓冲区
    D3D11_MAPPED_SUBRESOURCE mapped;
    HR(m_context->Map(m_particleVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
    ParticleVertex* vtx = (ParticleVertex*)mapped.pData;
    size_t count = 0;
    for (const auto& p : m_particles) {
        if (p.Life <= 0.0f) continue;
        vtx[count].CenterW = p.Position;
        vtx[count].SizeW = p.Size;
        // 根据寿命计算透明度
        float alpha = min(p.Life * 1.5f, 1.0f);
        vtx[count].Color = XMFLOAT4(1.0f, 1.0f, 1.0f, alpha);
        vtx[count].BlockType = p.BlockType;
        ++count;
    }
    m_context->Unmap(m_particleVertexBuffer.Get(), 0);

    // 设置管线状态
    m_context->IASetInputLayout(m_inputLayoutParticle.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    UINT stride = sizeof(ParticleVertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_particleVertexBuffer.GetAddressOf(), &stride, &offset);

    m_context->VSSetShader(m_vsParticle.Get(), nullptr, 0);
    m_context->GSSetShader(m_gsParticle.Get(), nullptr, 0);
    m_context->PSSetShader(m_psParticle.Get(), nullptr, 0);

    // 绑定常量缓冲区（需要视图投影矩阵和相机位置）
    m_context->VSSetConstantBuffers(0, 1, m_viewProjBuffer.GetAddressOf());
    m_context->GSSetConstantBuffers(0, 1, m_viewProjBuffer.GetAddressOf());
    // 相机位置传递给GS用
    m_context->GSSetConstantBuffers(1, 1, m_cbEyePosBuffer.GetAddressOf());

    // 绑定纹理数组（3张方块纹理）
    ID3D11ShaderResourceView* srvs[3] = { m_stoneSRV.Get(), m_grassSRV.Get(), m_shuijintSRV.Get() };
    m_context->PSSetShaderResources(0, 3, srvs);
    m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

    // 设置混合和深度状态
    float blendFactor[4] = { 1,1,1,1 };
    m_context->OMSetBlendState(m_particleBlendState.Get(), blendFactor, 0xFFFFFFFF);
    m_context->OMSetDepthStencilState(m_particleDSS.Get(), 0);

    m_context->Draw((UINT)count, 0);

    // 恢复默认状态
    m_context->OMSetBlendState(nullptr, blendFactor, 0xFFFFFFFF);
    m_context->OMSetDepthStencilState(nullptr, 0);
    m_context->GSSetShader(nullptr, nullptr, 0);
}