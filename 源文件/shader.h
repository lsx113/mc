#ifndef SHADER_H
#define SHADER_H
#include"game.h"
struct VertexPosUV
{
	XMFLOAT3 pos;
	XMFLOAT2 uv;
	static const D3D11_INPUT_ELEMENT_DESC inputLayout[2];
};

struct VertexPos
{
	XMFLOAT3 pos;
	static const D3D11_INPUT_ELEMENT_DESC inputLayout[1];
};

struct VertexPosNormalTex
{
	XMFLOAT3 pos;
	XMFLOAT3 normal;
	XMFLOAT2 uv;
	static const D3D11_INPUT_ELEMENT_DESC inputLayout[3];
};

struct CBcamera
{
	XMMATRIX worldViewProj;
};

struct CBPerObject
{
	XMMATRIX world;
	XMMATRIX worldInvTranspose;
};

struct CBsw {
	XMMATRIX world;
	XMMATRIX worldInvTranspose;
	float blood;
	XMFLOAT3 eyepos;
};

struct DirectionalLight
{
	DirectX::XMFLOAT4 ambient;
	DirectX::XMFLOAT4 diffuse;
	DirectX::XMFLOAT4 specular;
	DirectX::XMFLOAT3 direction;
	float pad; // 最后用一个浮点数填充使得该结构体大小满足16的倍数，便于我们以后在HLSL设置数组
};

struct cbtime {
	float timer;
	float pad[3];
};

struct Material
{
	DirectX::XMFLOAT4 ambient;
	DirectX::XMFLOAT4 diffuse;
	DirectX::XMFLOAT4 specular; // w = 镜面反射强度
	DirectX::XMFLOAT4 reflect;
};

struct BarVertex {
	XMFLOAT3 center;
	float    blood;
};

// 血条常量缓冲区
struct CBswBar {
	XMFLOAT3 eyePosW;
	float    maxBlood;   // 最大血量，如 10.0f
};

struct CBPS
{
	DirectionalLight dirLight;
	Material material;
	XMFLOAT3 eyePos;
	float pad; // 填充使得结构体大小满足16的倍数
};

// 单个粒子（CPU侧）
struct Particle {
	XMFLOAT3 Position;      // 世界坐标
	XMFLOAT3 Velocity;      // 速度向量
	float Life;             // 剩余寿命（秒），≤0表示死亡
	float Size;             // 粒子大小
	int   BlockType;        // 方块类型（1石头,2草,3水晶），用于选择纹理
};

// GPU顶点结构（用于几何着色器输入）
struct ParticleVertex {
	XMFLOAT3 CenterW;       // 粒子中心世界坐标
	float    SizeW;         // 粒子大小
	XMFLOAT4 Color;         // 调制颜色（包含透明度）
	int      BlockType;     // 方块类型
	float    Padding[3];    // 填充到16字节对齐
};

class Shader {
public:
	static float daytime;
	ComPtr<ID3D11Buffer> m_timecbbuffer;//时间缓冲区
	void init(ID3D11Device* device, ID3D11DeviceContext* context,float as);// 初始化着色器
	ComPtr<ID3D11SamplerState> m_samplerState;//采样器状态
	std::vector<std::unique_ptr<EventBus::Subscription>> m_subscriptions;
	ID3D11Device* m_device;
	ID3D11DeviceContext* m_context;
	//封面*****//
	bool isshowcover = false;//是否显示封面
	ComPtr<ID3D11VertexShader> m_vertexShader_cover;//封面顶点着色器
	ComPtr<ID3D11PixelShader> m_pixelShader_cover;//封面像素着色器
	ComPtr<ID3D11InputLayout> m_inputLayout_cover;//封面输入布局
	ComPtr<ID3D11Buffer> m_Vertexbuffer_cover;//顶点缓冲区
	VertexPosUV vertex_cover[6] = {
		// 第一个三角形
		{ XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) }, // 左上
		{ XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }, // 右下
		{ XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) }, // 左下
		// 第二个三角形
		{ XMFLOAT3(-1.0f,  1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) }, // 左上
		{ XMFLOAT3(1.0f,  1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) }, // 右上
		{ XMFLOAT3(1.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }  // 右下
	};
	void init_cover();//初始化封面
	//******//
	//相机******//
	bool isfirstperson = true;//是否第一人称
	void update_view(XMMATRIX view,XMFLOAT3 pos);//更新视图矩阵
	XMMATRIX m_viewMatrix;//视图矩阵
	XMMATRIX m_projectionMatrix;//投影矩阵
	XMFLOAT3 eyePos;//相机位置
	ComPtr<ID3D11Buffer> m_viewProjBuffer;//视图投影常量缓冲区
	void init_view_proj_buffer();//初始化视图投影常量缓冲区
	//******//
	//天空盒******//
	ComPtr<ID3D11VertexShader> m_vertexShader_skybox;//天空盒顶点着色器
	ComPtr<ID3D11PixelShader> m_pixelShader_skybox;//天空盒像素着色器
	ComPtr<ID3D11InputLayout> m_inputLayout_skybox;//天空盒输入布局
	ComPtr<ID3D11ShaderResourceView> m_pSkyBoxSRV;//天空盒纹理资源视图
	ComPtr<ID3D11Buffer> m_Vertexbuffer_skybox;//天空盒顶点缓冲区
	ComPtr<ID3D11Buffer> m_Indexbuffer_skybox;//天空盒索引缓冲区
	ComPtr<ID3D11DepthStencilState> m_pSkyboxDSS;//天空盒深度模板状态
    ComPtr<ID3D11RasterizerState> m_rsNoCull; // 无剔除光栅化状态，用于天空盒
	void init_skybox();//初始化天空盒
	//******//
	//绘制方块******//
	ComPtr<ID3D11VertexShader> m_vertexShader_block;//方块顶点着色器
	ComPtr<ID3D11PixelShader> m_pixelShader_block;//方块像素着色器
	ComPtr<ID3D11InputLayout> m_inputLayout_block;//方块输入布局
	ComPtr<ID3D11DepthStencilState> m_pBlockDSS;//方块深度模板状态
	ComPtr<ID3D11SamplerState> m_blockSamplerState;//方块采样器状态
	ComPtr<ID3D11Buffer> m_Indexbuffer_block;//方块索引缓冲区
	ComPtr<ID3D11Buffer> m_Vertexbuffer_block;//方块顶点缓冲区
	ComPtr<ID3D11ShaderResourceView> m_grassSRV;//草地纹理资源视图
	ComPtr<ID3D11ShaderResourceView> m_stoneSRV;//石头纹理资源视图
	ComPtr<ID3D11ShaderResourceView> m_shuijintSRV;//水晶纹理资源视图
	ComPtr<ID3D11Buffer> m_cbPSBuffer;//像素着色器常量缓冲区
	ComPtr<ID3D11Buffer> m_cbPerObjectBuffer;//每个物体常量缓冲区
	void ExtractFrustumPlanes(XMVECTOR planes[6]);// 从视图投影矩阵中提取六个裁剪平面
	void init_block();//初始化方块
	void drallblock();//绘制方块
	const float daytimeCycle = 240.0f; // 一昼夜周期的持续时间（秒）
	//****//
	//绘制人物
	ComPtr<ID3D11Buffer> m_vertexBuffer_character_foot;//脚顶点缓冲区
	ComPtr<ID3D11Buffer> m_indexBuffer_character_foot;//脚索引缓冲区
	ComPtr<ID3D11Buffer> m_vertexBuffer_character_body;//身体顶点缓冲区
	ComPtr<ID3D11Buffer> m_indexBuffer_character_body;//身体索引缓冲区
	ComPtr<ID3D11Buffer> m_vertexBuffer_character_hand;//手顶点缓冲区
	ComPtr<ID3D11Buffer> m_indexBuffer_character_hand;//手索引缓冲区
	ComPtr<ID3D11ShaderResourceView> m_characterSRV;//人物纹理资源视图
	XMMATRIX m_characterWorldMatrix_hand;//人物手世界矩阵
	XMMATRIX m_characterWorldMatrix_body;//人物身体世界矩阵
	XMMATRIX m_characterWorldMatrix_footleft;//人物左脚世界矩阵
	XMMATRIX m_characterWorldMatrix_footright;//人物右脚世界矩阵
	void init_character();//初始化人物
	void drawcharacter();//绘制人物
	void update_character_world_matrix(XMMATRIX hand, XMMATRIX body, XMMATRIX footleft, XMMATRIX footright);//更新人物世界矩阵
	void drawdrop();
	//******//
	//生物//
// 生物模型着色器
	ComPtr<ID3D11VertexShader> m_vertexShader_swModel;
	ComPtr<ID3D11PixelShader>  m_pixelShader_swModel;
	ComPtr<ID3D11Buffer> m_cbSwBuffer;          // 生物常量缓冲区 (CBsw)
	// 血条着色器
	ComPtr<ID3D11VertexShader> m_vertexShader_swBar;
	ComPtr<ID3D11GeometryShader> m_geometryShader_swBar;
	ComPtr<ID3D11PixelShader>  m_pixelShader_swBar;
	// 血条顶点缓冲区（动态，每帧更新）
	ComPtr<ID3D11Buffer> m_vertexBuffer_swBar;
	ComPtr<ID3D11InputLayout> m_inputLayout_swBar;
	ComPtr<ID3D11Buffer> m_cbSwBarBuffer;
	void init_sw();          // 初始化所有生物相关着色器
	void drawsw();           //分别绘制模型和血条
	//******//
	// 小地图
	ComPtr<ID3D11Texture2D> m_minimapTex;            // 小地图纹理
	ComPtr<ID3D11RenderTargetView> m_minimapRTV;     // 小地图渲染目标视图
	ComPtr<ID3D11ShaderResourceView> m_minimapSRV;   // 小地图着色器资源视图
	ComPtr<ID3D11DepthStencilView> m_minimapDSV;     // 小地图深度视图
	ComPtr<ID3D11Buffer> m_minimapVertexBuffer;      // 小地图顶点缓冲区
	D3D11_VIEWPORT m_minimapViewport;                // 小地图视口
	void init_minimap();                             // 初始化小地图资源
	void render_minimap();                           // 渲染场景到小地图
	void draw_minimap();                             // 在右上角绘制小地图
	// ...
	//NPC
	void drawnpc();//绘制NPC
	//******//
		// 粒子系统相关
	void init_particles();                          // 初始化粒子系统资源
	void update_particles(float dt);                // 更新粒子逻辑
	void draw_particles();                          // 绘制粒子
	void emit_particles(const XMFLOAT3& center, int blockType); // 生成粒子

	static constexpr size_t MAX_PARTICLES = 30;   // 最大粒子数
	std::vector<Particle> m_particles;              // CPU粒子池
	size_t m_liveParticleCount;                     // 当前活跃粒子数

	// D3D资源
	ComPtr<ID3D11Buffer> m_particleVertexBuffer;    // 动态顶点缓冲区
	ComPtr<ID3D11VertexShader> m_vsParticle;        // 粒子顶点着色器
	ComPtr<ID3D11GeometryShader> m_gsParticle;      // 粒子几何着色器（公告牌）
	ComPtr<ID3D11PixelShader> m_psParticle;         // 粒子像素着色器
	ComPtr<ID3D11InputLayout> m_inputLayoutParticle;
	ComPtr<ID3D11BlendState> m_particleBlendState;  // 透明度混合状态
	ComPtr<ID3D11DepthStencilState> m_particleDSS;  // 禁用深度写入
	ComPtr<ID3D11Buffer> m_cbEyePosBuffer;  // 仅包含相机位置
	void drawall();
};
#endif // !SHADER_H
