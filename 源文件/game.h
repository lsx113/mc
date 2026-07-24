#ifndef GAME_H
#define GAME_H
#include"ui.h"
#include <map>
#include <tuple>
class Block {
public:
	static int block[20][10][20] ;//世界方块
	void init();
	std::vector<std::unique_ptr<EventBus::Subscription>> m_subscriptions;
	// 使用 std::map，键为坐标 tuple，值为包围盒
	static std::map<std::tuple<int, int, int>, DirectX::BoundingBox> boundmap;

};

class Transform
{
public:
    Transform();

    // 位置
    void SetPosition(const DirectX::XMFLOAT3& pos);
    void SetPosition(float x, float y, float z);
    DirectX::XMFLOAT3 GetPosition() ;

    // 缩放
    void SetScale(const DirectX::XMFLOAT3& scale);
    void SetScale(float x, float y, float z);
    void SetUniformScale(float scale);
    DirectX::XMFLOAT3 GetScale();

    // 旋转（四元数）
    void SetRotation(const DirectX::XMFLOAT4& quat);      // 四元数 (x,y,z,w)
    void SetRotation(const DirectX::XMFLOAT3& euler);     // 欧拉角 (弧度, 顺序 ZYX)
    DirectX::XMFLOAT4 GetRotationQuat() ;

    // 绕自身局部轴旋转（角度弧度）
    void RotateLocalX(float angle);
    void RotateLocalY(float angle);
    void RotateLocalZ(float angle);
    void RotateLocalAxis(const DirectX::XMFLOAT3& axis, float angle); // 绕局部任意轴

    // 绕世界轴旋转
    void RotateWorldX(float angle);
    void RotateWorldY(float angle);
    void RotateWorldZ(float angle);
    void RotateWorldAxis(const DirectX::XMFLOAT3& axis, float angle);

    // 移动（世界坐标系偏移）
    void Translate(const DirectX::XMFLOAT3& delta);
    void Translate(float dx, float dy, float dz);
    // 沿自身局部轴移动
    void MoveForward(float distance);   // 局部 +Z
    void MoveRight(float distance);     // 局部 +X
    void MoveUp(float distance);        // 局部 +Y

    // 获取世界矩阵（返回 XMMATRIX，使用时直接传值或 XMLoadFloat4x4 可省略）
    XMMATRIX GetWorldMatrix();

private:
    DirectX::XMFLOAT3 m_position;
    DirectX::XMFLOAT3 m_scale;
    DirectX::XMFLOAT4 m_rotation;          // 单位四元数 (x,y,z,w)
    DirectX::XMFLOAT4X4 m_worldMatrix; // 缓存的世界矩阵
    bool m_matrixDirty;

    void UpdateWorldMatrix() ;         // 更新缓存矩阵
    void NormalizeRotation();               // 归一化当前四元数
};

class Camera
{
public:
    Camera(float width, float height);
    ~Camera();

    // 位置操作（直接使用 Transform 的接口）
    void SetPosition(const DirectX::XMFLOAT3& pos);
    void SetPosition(float x, float y, float z);
    DirectX::XMFLOAT3 GetPosition() ;

    // 旋转操作（基于欧拉角，内部自动更新四元数）
    void Yaw(float angle);       // 绕世界/自身 Y 轴（偏航）
    void Pitch(float angle);     // 绕自身 X 轴（俯仰），自动限制角度
    void Roll(float angle);      // 绕自身 Z 轴（滚转）

    // 直接设置欧拉角（弧度）
    void SetEulerAngles(float yaw, float pitch, float roll);

    // 获取当前欧拉角（弧度）
    float GetYaw()   const { return m_yaw; }
    float GetPitch() const { return m_pitch; }
    float GetRoll()  const { return m_roll; }

    // 获取局部坐标轴在世界空间中的方向向量
    DirectX::XMFLOAT3 GetForward() ;
    DirectX::XMFLOAT3 GetRight()   ;
    DirectX::XMFLOAT3 GetUp()      ;

    // 获取视图矩阵（世界 → 相机空间）
    DirectX::XMMATRIX GetViewMatrix() ;

    // 每帧更新（处理输入等）
    void Update(float dt);
    void updateblock();//更新破坏方块
    XMMATRIX projectionMatrix;        // 投影矩阵（用于计算射线等）
private:
    Transform m_transform;        // 位置和旋转的实际存储

    float m_yaw;                  // 偏航角（弧度）
    float m_pitch;                // 俯仰角（弧度）
    float m_roll;                 // 滚转角（弧度）
    static constexpr float MAX_PITCH = DirectX::XM_PIDIV2 - 0.01f; // 限制在 ±89.9°

    // 将当前欧拉角应用到 Transform 的四元数
    void UpdateRotation();
    void ClampPitch();
    std::vector<std::unique_ptr<EventBus::Subscription>> m_subscriptions;//订阅事件用的vector
    float m_width;//屏幕宽度
    float m_height;//屏幕高度
};

class player {
public:
    Transform m_transform_hand;//玩家手部位置
    Transform m_transform_body;//玩家身体位置
    Transform m_transform_foot_left;//玩家左脚位置
    Transform m_transform_foot_right;//玩家右脚位置
    static int blocktype;//玩家当前选择的方块类型
    void init();
    void Update(float dt);
    BoundingBox m_playerBox; // 玩家包围盒
    std::vector<std::unique_ptr<EventBus::Subscription>> m_subscriptions;//订阅事件用的vector
    float distance(XMFLOAT3 center);
};


struct dropobj {
    XMFLOAT3 pos;
    int type;
    dropobj(XMFLOAT3 pos, int t) :pos(pos), type(t) {}
};
class Drop {//掉落物
public:
    std::vector<std::unique_ptr<EventBus::Subscription>> m_subscriptions;//订阅事件用的vector
    static std::vector<dropobj> dropobjs;//掉落物列表
     void init();
     void Update();
};

// 顶点结构（匹配你的着色器输入布局）
struct ModelVertex {
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 TexCoord;
};

// 单个网格的渲染数据
struct MeshData {
    Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer;
    UINT IndexCount = 0;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> TextureSRV;
    DirectX::XMMATRIX WorldMatrix = DirectX::XMMatrixIdentity();
};

class Model {
public:
    static std::vector<MeshData> girlmodel;
    static void Init(ID3D11Device* device);
private:
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateSRVFromFile(const std::string& filename, ID3D11Device* device);
    static void ProcessNode(aiNode* node, const DirectX::XMMATRIX& parentTransform, const aiScene* scene, ID3D11Device* device, const std::string& modelDir, std::vector<MeshData>& outMeshes);
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> LoadTexture(aiMaterial* mat, aiTextureType type, const aiScene* scene, ID3D11Device* device, const std::string& modelDir);
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateSRVFromMemory(const void* data, size_t size, ID3D11Device* device);
    static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateDefaultWhiteTexture(ID3D11Device* device); // 新增
};

struct swobj {
    Transform m_transform;
    float blood;//血量
    BoundingBox m_box;//包围盒
};

class sw {
public:
    static std::vector<swobj> sws;//生物列表
    void init();
    void Update(float dt);
    std::vector<std::unique_ptr<EventBus::Subscription>> m_subscriptions;
    XMFLOAT3 pos_npc;//npc位置
};
 

class Audio {
public:
    std::vector<std::unique_ptr<EventBus::Subscription>> m_subscriptions;
    void init();
    std::unique_ptr<DirectX::AudioEngine> g_audioEngine;
    vector<std::unique_ptr<DirectX::SoundEffect>> m_soundEffects;//音效列表
    vector<std::unique_ptr<DirectX::SoundEffectInstance>> m_soundInstances;//正在播放的音效实例列表
    vector<std::unique_ptr<DirectX::SoundEffect>> m_soundbgm;//音乐
};


class Bag {
public:
    std::vector<std::unique_ptr<EventBus::Subscription>> m_subscriptions;//订阅事件用的vector
    int bagcapacity = 10;//背包容量
    std::vector<bagobj> bagobjs;//背包物品列表
    void init();
    void Update();
    
};
#endif // !GAME_H
