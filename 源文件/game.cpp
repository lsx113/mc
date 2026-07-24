#include"game.h"

#include <wincodec.h>
#include <memory>
std::vector<MeshData> Model::girlmodel;

int Block::block[20][10][20] = { 0 };
std::map<std::tuple<int, int, int>, DirectX::BoundingBox> Block::boundmap;

void Block::init() {
	m_subscriptions.push_back(EventBus::Subscribe<event::worldname>([this](event::worldname event) {
		string worldfile;
		worldfile = "world/" + event.name + ".cd";
        ifstream m_world(worldfile, ios::binary);
        if (!m_world.is_open()) {
            MessageBoxA(nullptr, ("Failed to open " + worldfile).c_str(), "World loading failed", MB_OK | MB_ICONERROR);
            return;
        }
        if (!m_world) {
            OutputDebugStringA("Read failed or incomplete");
        }
        else {
            OutputDebugStringA(("First block value: " + std::to_string(block[0][0][0])).c_str());
        }
		m_world.read(reinterpret_cast<char*>(block), sizeof(block));//读取世界
		m_world.close();
		//创建包围盒

		// 清空旧的包围盒映射
		boundmap.clear();
        // 遍历所有方块，为非空气方块创建包围盒
        const float blockSize = 1.0f;
        const float halfSize = blockSize * 0.5f;  // 0.5
        for (int x = 0; x < 20; ++x) {
            for (int y = 0; y < 10; ++y) {
                for (int z = 0; z < 20; ++z) {
                    if (block[x][y][z] != 0) {//创建包围盒
                        DirectX::XMFLOAT3 center(x + halfSize, y + halfSize, z + halfSize);
                        DirectX::XMFLOAT3 extents(halfSize, halfSize, halfSize);
                        DirectX::BoundingBox box;
                        box.Center = center;
                        box.Extents = extents;
                        boundmap[{x, y, z}] = box;
                    }
                }
            }
        }
		}));//订阅加载事件
    m_subscriptions.push_back(EventBus::Subscribe<event::updateworld>([this](event::updateworld event){
        string path = "./world/" + event.name + ".cd";
        ofstream worldfile(path, ios::binary);
        worldfile.write(reinterpret_cast<const char*>(block), sizeof(block));// 将世界数据写入文件
        worldfile.close();
        }));
}

Transform::Transform()
    : m_position(0, 0, 0)
    , m_scale(1, 1, 1)
    , m_rotation(0, 0, 0, 1)
    , m_matrixDirty(true)
{
    XMStoreFloat4x4(&m_worldMatrix, XMMatrixIdentity());
}

// 位置
void Transform::SetPosition(const XMFLOAT3& pos)
{
    m_position = pos;
    m_matrixDirty = true;
}

void Transform::SetPosition(float x, float y, float z)
{
    m_position = XMFLOAT3(x, y, z);
    m_matrixDirty = true;
}

XMFLOAT3 Transform::GetPosition() 
{
    return m_position;
}

// 缩放
void Transform::SetScale(const XMFLOAT3& scale)
{
    m_scale = scale;
    m_matrixDirty = true;
}

void Transform::SetScale(float x, float y, float z)
{
    m_scale = XMFLOAT3(x, y, z);
    m_matrixDirty = true;
}

void Transform::SetUniformScale(float scale)
{
    m_scale = XMFLOAT3(scale, scale, scale);
    m_matrixDirty = true;
}

XMFLOAT3 Transform::GetScale() 
{
    return m_scale;
}

// 旋转设置
void Transform::SetRotation(const XMFLOAT4& quat)
{
    m_rotation = quat;
    NormalizeRotation();
    m_matrixDirty = true;
}

void Transform::SetRotation(const XMFLOAT3& euler)
{
    XMVECTOR q = XMQuaternionRotationRollPitchYaw(euler.x, euler.y, euler.z);
    XMStoreFloat4(&m_rotation, q);
    NormalizeRotation();
    m_matrixDirty = true;
}

XMFLOAT4 Transform::GetRotationQuat() 
{
    return m_rotation;
}

// 归一化辅助
void Transform::NormalizeRotation()
{
    XMVECTOR q = XMLoadFloat4(&m_rotation);
    q = XMQuaternionNormalize(q);
    XMStoreFloat4(&m_rotation, q);
}

// 绕自身局部轴旋转
void Transform::RotateLocalX(float angle)
{
    XMVECTOR delta = XMQuaternionRotationAxis(XMVectorSet(1, 0, 0, 0), angle);
    XMVECTOR current = XMLoadFloat4(&m_rotation);
    // 局部旋转：先当前旋转，再局部旋转 → current * delta
    XMVECTOR newQuat = XMQuaternionMultiply(current, delta);
    XMStoreFloat4(&m_rotation, newQuat);
    NormalizeRotation();
    m_matrixDirty = true;
}

void Transform::RotateLocalY(float angle)
{
    XMVECTOR delta = XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), angle);
    XMVECTOR current = XMLoadFloat4(&m_rotation);
    XMVECTOR newQuat = XMQuaternionMultiply(current, delta);
    XMStoreFloat4(&m_rotation, newQuat);
    NormalizeRotation();
    m_matrixDirty = true;
}

void Transform::RotateLocalZ(float angle)
{
    XMVECTOR delta = XMQuaternionRotationAxis(XMVectorSet(0, 0, 1, 0), angle);
    XMVECTOR current = XMLoadFloat4(&m_rotation);
    XMVECTOR newQuat = XMQuaternionMultiply(current, delta);
    XMStoreFloat4(&m_rotation, newQuat);
    NormalizeRotation();
    m_matrixDirty = true;
}

void Transform::RotateLocalAxis(const XMFLOAT3& axis, float angle)
{
    XMVECTOR axisVec = XMLoadFloat3(&axis);
    XMVECTOR delta = XMQuaternionRotationAxis(axisVec, angle);
    XMVECTOR current = XMLoadFloat4(&m_rotation);
    XMVECTOR newQuat = XMQuaternionMultiply(current, delta);
    XMStoreFloat4(&m_rotation, newQuat);
    NormalizeRotation();
    m_matrixDirty = true;
}

// 绕世界轴旋转
void Transform::RotateWorldX(float angle)
{
    XMVECTOR delta = XMQuaternionRotationAxis(XMVectorSet(1, 0, 0, 0), angle);
    XMVECTOR current = XMLoadFloat4(&m_rotation);
    // 世界旋转：先应用世界旋转，再当前旋转 → delta * current
    XMVECTOR newQuat = XMQuaternionMultiply(delta, current);
    XMStoreFloat4(&m_rotation, newQuat);
    NormalizeRotation();
    m_matrixDirty = true;
}

void Transform::RotateWorldY(float angle)
{
    XMVECTOR delta = XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), angle);
    XMVECTOR current = XMLoadFloat4(&m_rotation);
    XMVECTOR newQuat = XMQuaternionMultiply(delta, current);
    XMStoreFloat4(&m_rotation, newQuat);
    NormalizeRotation();
    m_matrixDirty = true;
}

void Transform::RotateWorldZ(float angle)
{
    XMVECTOR delta = XMQuaternionRotationAxis(XMVectorSet(0, 0, 1, 0), angle);
    XMVECTOR current = XMLoadFloat4(&m_rotation);
    XMVECTOR newQuat = XMQuaternionMultiply(delta, current);
    XMStoreFloat4(&m_rotation, newQuat);
    NormalizeRotation();
    m_matrixDirty = true;
}

void Transform::RotateWorldAxis(const XMFLOAT3& axis, float angle)
{
    XMVECTOR axisVec = XMLoadFloat3(&axis);
    XMVECTOR delta = XMQuaternionRotationAxis(axisVec, angle);
    XMVECTOR current = XMLoadFloat4(&m_rotation);
    XMVECTOR newQuat = XMQuaternionMultiply(delta, current);
    XMStoreFloat4(&m_rotation, newQuat);
    NormalizeRotation();
    m_matrixDirty = true;
}

// 移动
void Transform::Translate(const XMFLOAT3& delta)
{
    m_position.x += delta.x;
    m_position.y += delta.y;
    m_position.z += delta.z;
    m_matrixDirty = true;
}

void Transform::Translate(float dx, float dy, float dz)
{
    m_position.x += dx;
    m_position.y += dy;
    m_position.z += dz;
    m_matrixDirty = true;
}

void Transform::MoveForward(float distance)
{
    XMVECTOR forward = XMVectorSet(0, 0, 1, 0);
    XMVECTOR quat = XMLoadFloat4(&m_rotation);
    XMVECTOR dir = XMVector3Rotate(forward, quat);
    XMVECTOR delta = dir * distance;
    XMFLOAT3 deltaF;
    XMStoreFloat3(&deltaF, delta);
    Translate(deltaF);
}

void Transform::MoveRight(float distance)
{
    XMVECTOR right = XMVectorSet(1, 0, 0, 0);
    XMVECTOR quat = XMLoadFloat4(&m_rotation);
    XMVECTOR dir = XMVector3Rotate(right, quat);
    XMVECTOR delta = dir * distance;
    XMFLOAT3 deltaF;
    XMStoreFloat3(&deltaF, delta);
    Translate(deltaF);
}

void Transform::MoveUp(float distance)
{
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);
    XMVECTOR quat = XMLoadFloat4(&m_rotation);
    XMVECTOR dir = XMVector3Rotate(up, quat);
    XMVECTOR delta = dir * distance;
    XMFLOAT3 deltaF;
    XMStoreFloat3(&deltaF, delta);
    Translate(deltaF);
}

// 世界矩阵
XMMATRIX Transform::GetWorldMatrix()
{
    if (m_matrixDirty)
        UpdateWorldMatrix();
    return XMLoadFloat4x4(&m_worldMatrix);
}

void Transform::UpdateWorldMatrix() 
{
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR scale = XMLoadFloat3(&m_scale);
    XMVECTOR rot = XMLoadFloat4(&m_rotation);
    rot = XMQuaternionNormalize(rot); // 额外保险

    XMMATRIX S = XMMatrixScalingFromVector(scale);
    XMMATRIX R = XMMatrixRotationQuaternion(rot);
    XMMATRIX T = XMMatrixTranslationFromVector(pos);

    XMMATRIX world = S * R * T;
    XMStoreFloat4x4(&m_worldMatrix, world);
    m_matrixDirty = false;
}

Camera::Camera(float width, float height )
    : m_yaw(0.0f)
    , m_pitch(0.0f)
    , m_roll(0.0f)
    , m_width(width)
    , m_height(height)
{
    SetPosition(2.0f, 14.4f, 2.0f);
    UpdateRotation();
    m_subscriptions.push_back(EventBus::Subscribe<event::setcamera>([this](event::setcamera event) {
        SetPosition(event.pos);
    }));
}

Camera::~Camera() {}

void Camera::SetPosition(const XMFLOAT3& pos)
{
    m_transform.SetPosition(pos);
}

void Camera::SetPosition(float x, float y, float z)
{
    m_transform.SetPosition(x, y, z);
}

XMFLOAT3 Camera::GetPosition() 
{
    return m_transform.GetPosition();
}

void Camera::Yaw(float angle)
{
    m_yaw += angle;
    UpdateRotation();
}

void Camera::Pitch(float angle)
{
    m_pitch += angle;
    ClampPitch();
    UpdateRotation();
}

void Camera::Roll(float angle)
{
    m_roll += angle;
    UpdateRotation();
}

void Camera::SetEulerAngles(float yaw, float pitch, float roll)
{
    m_yaw = yaw;
    m_pitch = pitch;
    m_roll = roll;
    ClampPitch();
    UpdateRotation();
}

void Camera::UpdateRotation()
{
    XMVECTOR quat = XMQuaternionRotationRollPitchYaw(m_pitch, m_yaw, m_roll);
    XMFLOAT4 quatFloat;
    XMStoreFloat4(&quatFloat, quat);
    m_transform.SetRotation(quatFloat);
}

void Camera::ClampPitch()
{
    if (m_pitch > MAX_PITCH)
        m_pitch = MAX_PITCH;
    if (m_pitch < -MAX_PITCH)
        m_pitch = -MAX_PITCH;
}

XMFLOAT3 Camera::GetForward()
{
    XMVECTOR forward = XMVectorSet(0, 0, 1, 0);
    XMFLOAT4 quat = m_transform.GetRotationQuat();  // 临时变量
    XMVECTOR rot = XMLoadFloat4(&quat);
    XMVECTOR worldDir = XMVector3Rotate(forward, rot);
    XMFLOAT3 result;
    XMStoreFloat3(&result, worldDir);
    return result;
}

XMFLOAT3 Camera::GetRight()
{
    XMVECTOR right = XMVectorSet(1, 0, 0, 0);
    XMFLOAT4 quat = m_transform.GetRotationQuat();
    XMVECTOR rot = XMLoadFloat4(&quat);
    XMVECTOR worldDir = XMVector3Rotate(right, rot);
    XMFLOAT3 result;
    XMStoreFloat3(&result, worldDir);
    return result;
}

XMFLOAT3 Camera::GetUp()
{
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);
    XMFLOAT4 quat = m_transform.GetRotationQuat();
    XMVECTOR rot = XMLoadFloat4(&quat);
    XMVECTOR worldDir = XMVector3Rotate(up, rot);
    XMFLOAT3 result;
    XMStoreFloat3(&result, worldDir);
    return result;
}
XMMATRIX Camera::GetViewMatrix() 
{
    XMMATRIX world = m_transform.GetWorldMatrix();
    return XMMatrixInverse(nullptr, world);
}

void Camera::Update(float dt)
{
    float sensitivity = 0.002f;  // 弧度/像素
    int dx = Input::GetMouseDeltaX();
    int dy = Input::GetMouseDeltaY();
    Yaw(dx * sensitivity);      // 左右移动鼠标 → 偏航
    Pitch(dy * sensitivity);    // 上下移动鼠标 → 俯仰
    updateblock();//更新破坏方块
}

void Camera::updateblock() {
    // 这里可以添加更新破坏方块的逻辑，例如检查玩家是否在破坏方块范围内，或者更新破坏方块的状态等。
       // 1. 计算 NDC 坐标
    float ndcX = (2.0f * Input::GetMouseX() / m_width) - 1.0f;
    float ndcY = 1.0f - (2.0f * Input::GetMouseY() / m_height); // 翻转Y轴
    // 2. 取近平面点 (z=0) 和远平面点 (z=1) 的 NDC
    XMVECTOR ndcNear = XMVectorSet(ndcX, ndcY, 0.0f, 1.0f);
    XMVECTOR ndcFar = XMVectorSet(ndcX, ndcY, 1.0f, 1.0f);
    // 3. 计算逆投影矩阵
    XMMATRIX invProj = XMMatrixInverse(nullptr, projectionMatrix);

    // 4. 变换到相机空间（齐次坐标）
    XMVECTOR camNear = XMVector4Transform(ndcNear, invProj);
    XMVECTOR camFar = XMVector4Transform(ndcFar, invProj);

    // 5. 除以 w 得到 3D 坐标
    camNear = XMVectorScale(camNear, 1.0f / XMVectorGetW(camNear));
    camFar = XMVectorScale(camFar, 1.0f / XMVectorGetW(camFar));
    // 6. 相机空间射线方向（从近平面指向远平面）
    XMVECTOR camDir = XMVectorSubtract(camFar, camNear);
    camDir = XMVector3Normalize(camDir);
    // 7. 计算逆视图矩阵
    XMMATRIX invView = XMMatrixInverse(nullptr, GetViewMatrix());

    // 8. 变换到世界空间
    //    射线起点：相机位置（相机空间原点）变换到世界
    XMVECTOR outOrigin = XMVector3TransformCoord(XMVectorZero(), invView);
    //    射线方向：相机空间方向向量变换到世界（忽略平移）
    XMVECTOR outDir = XMVector3TransformNormal(camDir, invView);
    outDir = XMVector3Normalize(outDir);
    if(Input::IsMouseButtonPressed(0)){//左键点击
        float distance=15.0f;
        float hitDist;
        std::tuple<int, int, int> coord;
        for (auto& pair : Block::boundmap) {
            if (pair.second.Intersects(outOrigin, outDir, hitDist)) {//射线与包围盒相交
                if (hitDist < distance) {
                    distance = hitDist;
                    coord = pair.first;
                }
            }
        }
        if(distance<15.0f){//找到一个可交点
            EventBus::Publish<event::leftmousedown>(event::leftmousedown(coord));//发布摧毁事件
        }
    }
    if (Input::IsMouseButtonPressed(1)) {
        float blockDist = 15.0f;
        float swDist = 15.0f;
        std::tuple<int, int, int> hitCoord;
        XMFLOAT3 hitNormal;
        bool hitBlock = false;
        swobj* hitSw = nullptr;

        // 1. 检测方块
        for (auto& pair : Block::boundmap) {
            const BoundingBox& box = pair.second;
            float t;
            if (box.Intersects(outOrigin, outDir, t) && t < blockDist) {
                blockDist = t;
                hitCoord = pair.first;

                // 计算放置面的法线
                XMVECTOR point = outOrigin + outDir * t;
                XMFLOAT3 hitPoint;
                XMStoreFloat3(&hitPoint, point);
                XMFLOAT3 center = box.Center;
                XMFLOAT3 delta = { hitPoint.x - center.x, hitPoint.y - center.y, hitPoint.z - center.z };
                float absX = fabs(delta.x);
                float absY = fabs(delta.y);
                float absZ = fabs(delta.z);
                if (absX > absY && absX > absZ)
                    hitNormal = { (delta.x > 0 ? 1.0f : -1.0f), 0, 0 };
                else if (absY > absX && absY > absZ)
                    hitNormal = { 0, (delta.y > 0 ? 1.0f : -1.0f), 0 };
                else
                    hitNormal = { 0, 0, (delta.z > 0 ? 1.0f : -1.0f) };
                hitBlock = true;
            }
        }

        // 2. 检测生物
        for (auto& s : sw::sws) {
            const BoundingBox& box = s.m_box;
            float t;
            if (box.Intersects(outOrigin, outDir, t) && t < swDist) {
                swDist = t;
                hitSw = &s;
            }
        }

        // 3. 根据距离判断交互类型
        if (hitSw && swDist <= blockDist) {
            // 攻击生物：发送生物包围盒的中心坐标
            EventBus::Publish<event::rightmousedown>(event::rightmousedown(0, hitSw->m_box.Center));
        }
        else if (hitBlock) {
            // 放置方块
            int x = std::get<0>(hitCoord);
            int y = std::get<1>(hitCoord);
            int z = std::get<2>(hitCoord);
            int nx = x + static_cast<int>(hitNormal.x);
            int ny = y + static_cast<int>(hitNormal.y);
            int nz = z + static_cast<int>(hitNormal.z);
            if (nx >= 0 && nx < 20 && ny >= 0 && ny < 10 && nz >= 0 && nz < 20) {
                if (Block::block[nx][ny][nz] == 0) {
                    XMFLOAT3 placePos(nx + 0.5f, ny + 0.5f, nz + 0.5f);
                    EventBus::Publish<event::rightmousedown>(event::rightmousedown(1, placePos));
                }
            }
        }
    }
}

void player::init() {
    m_transform_body.SetPosition(2.0f, 14.4f, 2.0f);
    m_transform_hand.SetPosition(2.4f, 14.4f, 2.0f);
    m_transform_foot_left.SetPosition(1.85f, 13.7f, 2.0f);
    m_transform_foot_right.SetPosition(2.15f, 13.7f, 2.0f);
    m_playerBox.Center = XMFLOAT3(2.0f, 14.0f, 2.0f);
    m_playerBox.Extents=XMFLOAT3(0.7f, 1.0f, 0.7f);
    // 订阅右键放置事件
    m_subscriptions.push_back(EventBus::Subscribe<event::rightmousedown>([this](const event::rightmousedown& e) {
        if (e.type == 1) {
            if(blocktype==0)return;//如果玩家当前选择的方块类型为0（空气），则不放置
            int x = static_cast<int>(floor(e.pos.x));
            int y = static_cast<int>(floor(e.pos.y));
            int z = static_cast<int>(floor(e.pos.z));
            if (x >= 0 && x < 20 && y >= 0 && y < 10 && z >= 0 && z < 20) {
                if (Block::block[x][y][z] == 0) {
                    // 计算新方块的包围盒
                    BoundingBox newBlockBox;
                    newBlockBox.Center = XMFLOAT3(x + 0.5f, y + 0.5f, z + 0.5f);
                    newBlockBox.Extents = XMFLOAT3(0.5f, 0.5f, 0.5f);
                    // 检查是否与玩家包围盒相交
                    if (!m_playerBox.Intersects(newBlockBox)) {
                        // 放置方块
                        Block::block[x][y][z] = blocktype;
                        Block::boundmap[{x, y, z}] = newBlockBox;
                        EventBus::Publish<event::playaudio>(event::playaudio(3));//播放放置方块音效
                        EventBus::Publish<event::putobj>(event::putobj(blocktype));//发布放置物品事件，参数为放置的方块类型
                    }
                }
            }
        }
        }));
}
float player::distance(XMFLOAT3 center) {
    XMFLOAT3 playerCenter = m_playerBox.Center;
    float dx = playerCenter.x - center.x;
    float dy = playerCenter.y - center.y;
    float dz = playerCenter.z - center.z;
    return dx * dx + dy * dy + dz * dz;
}

int player::blocktype = 1;//默认选择第一种方块

void player::Update(float dt) {
    const float speed = 5.0f;
    const float maxFootAngle = 0.5f;
    static float jumphight = 0;
    static bool isnablejump = false;
    static bool ismoveing = false;
    static float footAngle_left = 0.0f;
    static int FootDirection = 1;

    auto getNearbyBlockCoords = [this](const BoundingBox& box) -> std::vector<std::tuple<int, int, int>> {
        std::vector<std::tuple<int, int, int>> coords;
        const float epsilon = 0.01f;
        int minX = (int)std::floor(box.Center.x - box.Extents.x - epsilon);
        int maxX = (int)std::floor(box.Center.x + box.Extents.x);
        int minY = (int)std::floor(box.Center.y - box.Extents.y - epsilon);
        int maxY = (int)std::floor(box.Center.y + box.Extents.y);
        int minZ = (int)std::floor(box.Center.z - box.Extents.z - epsilon);
        int maxZ = (int)std::floor(box.Center.z + box.Extents.z);
        for (int x = minX; x <= maxX; ++x)
            for (int y = minY; y <= maxY; ++y)
                for (int z = minZ; z <= maxZ; ++z)
                    coords.emplace_back(x, y, z);
        return coords;
        };

    auto checkCollision = [this, &getNearbyBlockCoords](const XMFLOAT3& newPos) -> bool {
        XMFLOAT3 newCenter = XMFLOAT3(newPos.x, newPos.y - 0.4f, newPos.z);
        BoundingBox newBox(newCenter, m_playerBox.Extents);
        auto nearby = getNearbyBlockCoords(newBox);
        for (const auto& coord : nearby) {
            auto it = Block::boundmap.find(coord);
            if (it != Block::boundmap.end()) {
                if (newBox.Intersects(it->second)) {
                    return true;
                }
            }
        }
        return false;
        };

    // ========== WASD只移动身体，不处理手/脚位置 ==========

    // 前进 (W)
    if (Input::IsKeyDown('W')) {
        XMFLOAT3 oldPos = m_transform_body.GetPosition();
        m_transform_body.MoveForward(speed * dt);
        if (checkCollision(m_transform_body.GetPosition())) {
            m_transform_body.SetPosition(oldPos);
        }
        else {
            ismoveing = true;
        }
    }

    // 后退 (S)
    if (Input::IsKeyDown('S')) {
        XMFLOAT3 oldPos = m_transform_body.GetPosition();
        m_transform_body.MoveForward(-speed * dt);
        if (checkCollision(m_transform_body.GetPosition())) {
            m_transform_body.SetPosition(oldPos);
        }
        else {
            ismoveing = true;
        }
    }

    // 左移 (A)
    if (Input::IsKeyDown('A')) {
        XMFLOAT3 oldPos = m_transform_body.GetPosition();
        m_transform_body.MoveRight(-speed * dt);
        if (checkCollision(m_transform_body.GetPosition())) {
            m_transform_body.SetPosition(oldPos);
        }
    }

    // 右移 (D)
    if (Input::IsKeyDown('D')) {
        XMFLOAT3 oldPos = m_transform_body.GetPosition();
        m_transform_body.MoveRight(speed * dt);
        if (checkCollision(m_transform_body.GetPosition())) {
            m_transform_body.SetPosition(oldPos);
        }
    }

    // ========== 攻击动画 ==========
    static float attackTimer = 0.0f;
    static bool isAttacking = false;
    static const float ATTACK_DURATION = 0.3f;

    if (Input::IsMouseButtonPressed(0) && !isAttacking) {
        isAttacking = true;
        attackTimer = 0.0f;
    }

    if (isAttacking) {
        attackTimer += dt;
        if (attackTimer >= ATTACK_DURATION) {
            isAttacking = false;
            attackTimer = 0.0f;
        }
    }
    static bool isjump = false;
    // ========== 重力 ==========

    if (!isjump) {
        XMFLOAT3 oldPos = m_transform_body.GetPosition();
        m_transform_body.MoveUp(-speed * 0);
        if (checkCollision(m_transform_body.GetPosition())) {
            m_transform_body.SetPosition(oldPos);
            isnablejump = true;
        }
    }

    // ========== 跳跃 ==========
    if (Input::IsKeyPressed(VK_SPACE)) {
        if (isnablejump) {
            isjump = true;
            isnablejump = false;
        }
    }
    if (isjump) {
        XMFLOAT3 oldPos = m_transform_body.GetPosition();
        m_transform_body.MoveUp(speed * dt * 2);
        if (checkCollision(m_transform_body.GetPosition())) {
            m_transform_body.SetPosition(oldPos);
            isjump = false;
        }
        else {
            if (jumphight < 1.5f) {
                jumphight += speed * dt * 2;
            }
            else {
                jumphight = 0;
                isjump = false;
            }
        }
    }

    // ========== 脚部摆动（计算角度，不应用） ==========
    float leftFootSwing = 0.0f;
    float rightFootSwing = 0.0f;
    if (ismoveing) {
        if (FootDirection == 1) {
            footAngle_left += 1.0f * dt;
        }
        else {
            footAngle_left -= 1.0f * dt;
        }
        if (footAngle_left > maxFootAngle) {
            footAngle_left = maxFootAngle;
            FootDirection = -1;
        }
        else if (footAngle_left < -maxFootAngle) {
            footAngle_left = -maxFootAngle;
            FootDirection = 1;
        }
        leftFootSwing = footAngle_left;
        rightFootSwing = -footAngle_left;
    }
    else {
        footAngle_left = 0.0f;
    }

    // ========== 统一计算：旋转 + 所有部件位置和旋转 ==========
    float sensitivity = 0.002f;
    int dx = Input::GetMouseDeltaX();

    // 旋转身体
    m_transform_body.RotateWorldY(dx * sensitivity);

    // 获取身体位置和旋转
    XMFLOAT3 bodyPos = m_transform_body.GetPosition();
    XMFLOAT4 bodyRotQuat = m_transform_body.GetRotationQuat();
    XMVECTOR bodyPosVec = XMLoadFloat3(&bodyPos);
    XMVECTOR bodyRotVec = XMLoadFloat4(&bodyRotQuat);

    // 更新碰撞盒
    m_playerBox.Center = XMFLOAT3(bodyPos.x, bodyPos.y - 0.4f, bodyPos.z);

    // ========== 右手（关键修复） ==========
    {
        // 位置偏移：身体右侧 (0.4, 0, 0)
        XMVECTOR offset = XMVectorSet(0.4f, 0.0f, 0.0f, 0.0f);
        XMVECTOR rotatedOffset = XMVector3Rotate(offset, bodyRotVec);
        XMVECTOR worldPos = XMVectorAdd(bodyPosVec, rotatedOffset);
        XMFLOAT3 result;
        XMStoreFloat3(&result, worldPos);
        m_transform_hand.SetPosition(result);

        // 旋转：先应用身体旋转，再叠加攻击摆动
        if (isAttacking) {
            float t = attackTimer / ATTACK_DURATION;
            float swing = sinf(t * XM_PI) * 0.8f;

            // 获取身体的"右方向"作为摆动轴
            XMVECTOR bodyRight = XMVector3Rotate(XMVectorSet(1, 0, 0, 0), bodyRotVec);
            XMVECTOR swingRot = XMQuaternionRotationAxis(bodyRight, swing);

            // 身体旋转后叠加摆动：注意顺序！先身体旋转，再局部摆动
            XMVECTOR handRot = XMQuaternionMultiply(bodyRotVec, swingRot);
            XMFLOAT4 handRotFloat;
            XMStoreFloat4(&handRotFloat, handRot);
            m_transform_hand.SetRotation(handRotFloat);
        }
        else {
            // 不攻击时，手跟随身体旋转（保持与身体朝向一致）
            m_transform_hand.SetRotation(bodyRotQuat);
        }
    }

    // ========== 左脚 ==========
    {
        XMVECTOR offset = XMVectorSet(-0.15f, -0.7f, 0.0f, 0.0f);
        XMVECTOR rotatedOffset = XMVector3Rotate(offset, bodyRotVec);
        XMVECTOR worldPos = XMVectorAdd(bodyPosVec, rotatedOffset);
        XMFLOAT3 result;
        XMStoreFloat3(&result, worldPos);
        m_transform_foot_left.SetPosition(result);

        // 脚部摆动：绕身体的X轴（左右轴）摆动
        if (leftFootSwing != 0.0f) {
            // 获取身体的"右方向"作为摆动轴
            XMVECTOR bodyRight = XMVector3Rotate(XMVectorSet(1, 0, 0, 0), bodyRotVec);
            XMVECTOR swingRot = XMQuaternionRotationAxis(bodyRight, leftFootSwing);
            XMVECTOR footRot = XMQuaternionMultiply(bodyRotVec, swingRot);
            XMFLOAT4 footRotFloat;
            XMStoreFloat4(&footRotFloat, footRot);
            m_transform_foot_left.SetRotation(footRotFloat);
        }
        else {
            // 不摆动时，脚跟随身体旋转
            m_transform_foot_left.SetRotation(bodyRotQuat);
        }
    }

    // ========== 右脚 ==========
    {
        XMVECTOR offset = XMVectorSet(0.15f, -0.7f, 0.0f, 0.0f);
        XMVECTOR rotatedOffset = XMVector3Rotate(offset, bodyRotVec);
        XMVECTOR worldPos = XMVectorAdd(bodyPosVec, rotatedOffset);
        XMFLOAT3 result;
        XMStoreFloat3(&result, worldPos);
        m_transform_foot_right.SetPosition(result);

        // 脚部摆动：绕身体的X轴（左右轴）摆动
        if (rightFootSwing != 0.0f) {
            // 获取身体的"右方向"作为摆动轴
            XMVECTOR bodyRight = XMVector3Rotate(XMVectorSet(1, 0, 0, 0), bodyRotVec);
            XMVECTOR swingRot = XMQuaternionRotationAxis(bodyRight, rightFootSwing);
            XMVECTOR footRot = XMQuaternionMultiply(bodyRotVec, swingRot);
            XMFLOAT4 footRotFloat;
            XMStoreFloat4(&footRotFloat, footRot);
            m_transform_foot_right.SetRotation(footRotFloat);
        }
        else {
            // 不摆动时，脚跟随身体旋转
            m_transform_foot_right.SetRotation(bodyRotQuat);
        }
    }
    ismoveing = false;

    // 相机跟随
    XMVECTOR forward = XMVectorSet(0, 0, 1, 0);
    XMVECTOR worldForward = XMVector3Rotate(forward, bodyRotVec);
    XMFLOAT3 forwardDir;
    XMStoreFloat3(&forwardDir, worldForward);

    const float cameraOffset = 0.45f;
    XMFLOAT3 cameraPos;
    cameraPos.x = bodyPos.x + forwardDir.x * cameraOffset;
    cameraPos.y = bodyPos.y + forwardDir.y * cameraOffset;
    cameraPos.z = bodyPos.z + forwardDir.z * cameraOffset;
    EventBus::Publish<event::setcamera>(event::setcamera(cameraPos));
}

std::vector<dropobj> Drop::dropobjs;

void Drop::init() {
    m_subscriptions.push_back(EventBus::Subscribe<event::leftmousedown>([this](event::leftmousedown event) {
        int type = Block::block[std::get<0>(event.pos)][std::get<1>(event.pos)][std::get<2>(event.pos)];
        Block::block[std::get<0>(event.pos)][std::get<1>(event.pos)][std::get<2>(event.pos)] = 0;
        Block::boundmap.erase(event.pos);
        EventBus::Publish<event::playaudio>(event::playaudio(2));//播放破坏方块音效
        float x = std::get<0>(event.pos) + 0.5f;
        float y = std::get<1>(event.pos) + 0.1f;
        float z = std::get<2>(event.pos) + 0.5f;
        dropobj newdrop(XMFLOAT3(x, y, z), type);
        dropobjs.push_back(newdrop);
        }));
    m_subscriptions.push_back(EventBus::Subscribe<event::setcamera>([this](event::setcamera event) {
        const float pickUpDistSq = 1.5f * 1.5f; // 1.5 格内拾取
        XMFLOAT3 camPos = event.pos;
        for (auto it = dropobjs.begin(); it != dropobjs.end(); ) {
            XMFLOAT3 dropPos = it->pos;
            float dx = camPos.x - dropPos.x;
            float dy = camPos.y - dropPos.y;
            float dz = camPos.z - dropPos.z;
            float distSq = dx * dx + dy * dy + dz * dz;
            if (distSq < pickUpDistSq) {
                // 拾取掉落物，可增加背包逻辑，此处直接删除
                EventBus::Publish<event::getobj>(event::getobj(it->type));//发布拾取事件，参数为掉落物类型
                it = dropobjs.erase(it);

            }
            else {
                ++it;
            }
        }
        }));
}

void Drop::Update() {
    for (auto it = dropobjs.begin(); it != dropobjs.end(); ) {
        XMFLOAT3& pos = it->pos;
        int ix = static_cast<int>(floor(pos.x));
        int iz = static_cast<int>(floor(pos.z));

        // 边界检查：超出世界范围则删除
        if (ix < 0 || ix >= 20 || iz < 0 || iz >= 20) {
            it = dropobjs.erase(it);
            continue;
        }

        // 从当前所在的 Y 整数层开始向下查找第一个非空气方块
        int startY = static_cast<int>(floor(pos.y));
        float groundY = -1.0f;
        for (int y = startY; y >= 0; --y) {
            if (Block::block[ix][y][iz] != 0) {
                groundY = y + 1.0f;   // 方块顶部高度
                break;
            }
        }

        // 如果没有找到任何方块，删除掉落物
        if (groundY < 0.0f) {
            it = dropobjs.erase(it);
            continue;
        }

        // 将掉落物位置设置到方块顶部上方 0.1 格处
        pos.y = groundY + 0.1f;

        // 二次安全检测：如果仍然卡入方块内部（极少发生），强制上移
        int newY = static_cast<int>(floor(pos.y));
        if (newY >= 0 && newY < 10 && Block::block[ix][newY][iz] != 0) {
            pos.y = newY + 1.0f;
        }

        ++it;
    }
}


// ==================== 辅助函数：宽字符与UTF-8转换 ====================

// UTF-8 转宽字符 (UTF-16)
static std::wstring Utf8ToWstring(const std::string& utf8Str) {
    if (utf8Str.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, nullptr, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, &wstr[0], size);
    wstr.pop_back(); // 移除结尾的 \0
    return wstr;
}

// 宽字符转 UTF-8
static std::string WstringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8Str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8Str[0], size, nullptr, nullptr);
    utf8Str.pop_back();
    return utf8Str;
}

// ==================== Model::Init (完整替换) ====================

void Model::Init(ID3D11Device* device) {
    if (!girlmodel.empty()) return;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool comInit = SUCCEEDED(hr) || hr == S_FALSE;

    // ===== 关键修改：使用宽字符路径支持中文 =====
    const wchar_t* wFilePath = L"C:\\Users\\33304\\Desktop\\model\\妮露.pmx";

    // 转换为 UTF-8 供 Assimp 使用（Assimp 5.0+ 支持 UTF-8）
    std::string filePath = WstringToUtf8(wFilePath);

    // 获取目录（宽字符）
    std::wstring wFullPath(wFilePath);
    size_t pos = wFullPath.find_last_of(L"\\/");
    std::wstring wModelDir = (pos != std::wstring::npos) ? wFullPath.substr(0, pos + 1) : L"";
    std::string modelDir = WstringToUtf8(wModelDir);

    // 使用宽字符检查文件是否存在（更可靠）
    if (!std::filesystem::exists(wFilePath)) {
        OutputDebugStringA("ERROR: PMX file does not exist!\n");
        OutputDebugStringW((L"Expected path: " + std::wstring(wFilePath) + L"\n").c_str());

        // 尝试列出目录内容（调试用）
        if (std::filesystem::exists(wModelDir)) {
            OutputDebugStringA("Directory exists. Files found:\n");
            for (const auto& entry : std::filesystem::directory_iterator(wModelDir)) {
                OutputDebugStringW((L"  - " + entry.path().filename().wstring() + L"\n").c_str());
            }
        }
        if (comInit) CoUninitialize();
        return;
    }

    OutputDebugStringA(("Loading: " + filePath + "\n").c_str());

    // ===== 关键修改：使用内存加载完全绕过中文路径问题 =====
    std::ifstream file(wFilePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        OutputDebugStringA("Failed to open file\n");
        if (comInit) CoUninitialize();
        return;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        OutputDebugStringA("Failed to read file\n");
        if (comInit) CoUninitialize();
        return;
    }
    file.close();

    Assimp::Importer importer;
    unsigned int flags = aiProcess_Triangulate |
        // 不要 aiProcess_GenNormals！
        aiProcess_JoinIdenticalVertices |
        aiProcess_OptimizeMeshes |
        aiProcess_ConvertToLeftHanded |
        aiProcess_GlobalScale;

    importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, 1.0f);

    // 从内存加载，指定格式为 pmx
    const aiScene* scene = importer.ReadFileFromMemory(
        buffer.data(),
        static_cast<size_t>(size),
        flags,
        "pmx"
    );

    if (!scene || !scene->mRootNode) {
        OutputDebugStringA(("Assimp Error: " + std::string(importer.GetErrorString()) + "\n").c_str());
        if (comInit) CoUninitialize();
        return;
    }

    OutputDebugStringA(("Success! Meshes: " + std::to_string(scene->mNumMeshes) +
        ", Materials: " + std::to_string(scene->mNumMaterials) + "\n").c_str());

    ProcessNode(scene->mRootNode, DirectX::XMMatrixIdentity(), scene, device, modelDir, girlmodel);

    if (comInit) CoUninitialize();
}

// ==================== CreateSRVFromFile (完整替换，支持中文路径) ====================

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Model::CreateSRVFromFile(
    const std::string& filename, ID3D11Device* device) {

    // 将 UTF-8 路径转换为宽字符（支持中文）
    std::wstring wFilename = Utf8ToWstring(filename);

    OutputDebugStringW((L"[CreateSRVFromFile] Loading: " + wFilename + L"\n").c_str());

    // 检查文件是否存在（宽字符版本）
    if (!std::filesystem::exists(wFilename)) {
        OutputDebugStringW((L"  ERROR: File not found: " + wFilename + L"\n").c_str());
        return nullptr;
    }

    Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
    if (FAILED(hr)) return nullptr;

    // 使用宽字符路径加载
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    hr = wicFactory->CreateDecoderFromFilename(
        wFilename.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &decoder
    );

    if (FAILED(hr)) {
        OutputDebugStringA("  Failed to create WIC decoder\n");
        return nullptr;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return nullptr;

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    hr = wicFactory->CreateFormatConverter(&converter);
    if (FAILED(hr)) return nullptr;

    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) return nullptr;

    UINT width, height;
    converter->GetSize(&width, &height);
    std::vector<BYTE> pixels(width * height * 4);
    hr = converter->CopyPixels(nullptr, width * 4, (UINT)pixels.size(), pixels.data());
    if (FAILED(hr)) return nullptr;

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = { pixels.data(), width * 4 };
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    hr = device->CreateTexture2D(&texDesc, &initData, &texture);
    if (FAILED(hr)) return nullptr;

    ID3D11ShaderResourceView* srv = nullptr;
    device->CreateShaderResourceView(texture.Get(), nullptr, &srv);
    OutputDebugStringA("  Texture loaded successfully\n");
    return srv;
}

// ==================== LoadTexture (完整替换，支持PMX和中文路径) ====================

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Model::LoadTexture(
    aiMaterial* mat, aiTextureType type, const aiScene* scene, ID3D11Device* device,
    const std::string& modelDir) {

    aiString path;
    aiReturn result = AI_FAILURE;

    // PMX 特殊处理：尝试多种纹理类型
    if (type == aiTextureType_DIFFUSE) {
        result = mat->GetTexture(aiTextureType_DIFFUSE, 0, &path);
        if (result != AI_SUCCESS) result = mat->GetTexture(aiTextureType_BASE_COLOR, 0, &path);
        if (result != AI_SUCCESS) result = mat->GetTexture(aiTextureType_UNKNOWN, 0, &path);
        if (result != AI_SUCCESS) {
            // 遍历所有 UNKNOWN 纹理
            unsigned int count = mat->GetTextureCount(aiTextureType_UNKNOWN);
            for (unsigned int i = 0; i < count && result != AI_SUCCESS; i++) {
                result = mat->GetTexture(aiTextureType_UNKNOWN, i, &path);
            }
        }
    }
    else {
        result = mat->GetTexture(type, 0, &path);
    }

    if (result != AI_SUCCESS) {
        OutputDebugStringA("  [LoadTexture] No texture path found\n");
        return nullptr;
    }

    std::string pathStr = path.C_Str();
    OutputDebugStringA(("  [LoadTexture] Raw path: " + pathStr + "\n").c_str());

    // 处理嵌入纹理
    if (pathStr[0] == '*') {
        int texIndex = atoi(pathStr.c_str() + 1);
        if (texIndex >= 0 && texIndex < (int)scene->mNumTextures) {
            aiTexture* tex = scene->mTextures[texIndex];
            if (tex->mHeight == 0) {
                return CreateSRVFromMemory(tex->pcData, tex->mWidth, device);
            }
            else {
                // 原始 RGBA
                D3D11_TEXTURE2D_DESC texDesc = {};
                texDesc.Width = tex->mWidth;
                texDesc.Height = tex->mHeight;
                texDesc.MipLevels = 1;
                texDesc.ArraySize = 1;
                texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                texDesc.SampleDesc.Count = 1;
                texDesc.Usage = D3D11_USAGE_DEFAULT;
                texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                D3D11_SUBRESOURCE_DATA initData = { tex->pcData, tex->mWidth * 4 };
                Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
                if (SUCCEEDED(device->CreateTexture2D(&texDesc, &initData, &texture))) {
                    ID3D11ShaderResourceView* srv = nullptr;
                    device->CreateShaderResourceView(texture.Get(), nullptr, &srv);
                    return srv;
                }
            }
        }
        return nullptr;
    }

    // 组合完整路径（处理相对/绝对路径）
    std::string texPath;
    if (pathStr.find(":\\") != std::string::npos || pathStr.find("/") == 0) {
        texPath = pathStr; // 绝对路径
    }
    else {
        texPath = modelDir + pathStr; // 相对路径
    }

    // 尝试加载
    auto srv = CreateSRVFromFile(texPath, device);

    // 如果失败，尝试常见扩展名变体
    if (!srv) {
        std::string basePath = texPath.substr(0, texPath.find_last_of('.'));
        std::vector<std::string> exts = { ".png", ".bmp", ".jpg", ".jpeg", ".tga", ".dds" };
        for (const auto& ext : exts) {
            srv = CreateSRVFromFile(basePath + ext, device);
            if (srv) break;
        }
    }

    return srv;
}

// ==================== ProcessNode (完整替换，优化调试信息) ====================

void Model::ProcessNode(aiNode* node, const DirectX::XMMATRIX& parentTransform,
    const aiScene* scene, ID3D11Device* device, const std::string& modelDir,
    std::vector<MeshData>& outMeshes) {

    auto ConvertMatrix = [](const aiMatrix4x4& mat) -> DirectX::XMMATRIX {
        return DirectX::XMMATRIX(
            mat.a1, mat.b1, mat.c1, mat.d1,
            mat.a2, mat.b2, mat.c2, mat.d2,
            mat.a3, mat.b3, mat.c3, mat.d3,
            mat.a4, mat.b4, mat.c4, mat.d4
        );
        };

    DirectX::XMMATRIX localMat = ConvertMatrix(node->mTransformation);
    DirectX::XMMATRIX worldMat = localMat * parentTransform;

    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        MeshData meshData;

        aiString meshName = mesh->mName;
        OutputDebugStringA(("\n[Mesh] " + std::string(meshName.C_Str()) + "\n").c_str());
        OutputDebugStringA(("  Material Index: " + std::to_string(mesh->mMaterialIndex) + "\n").c_str());
        OutputDebugStringA(("  Vertices: " + std::to_string(mesh->mNumVertices) + "\n").c_str());

        // 顶点数据
        std::vector<ModelVertex> vertices;
        vertices.reserve(mesh->mNumVertices);
        for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
            ModelVertex vert;
            vert.Position = DirectX::XMFLOAT3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
            if (mesh->HasNormals()) {
                vert.Normal = DirectX::XMFLOAT3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);
            }
            else {
                vert.Normal = DirectX::XMFLOAT3(0, 1, 0);
            }
            if (mesh->HasTextureCoords(0)) {
                // PMX通常使用第0套UV
                float u = mesh->mTextureCoords[0][v].x;
                float v_coord = mesh->mTextureCoords[0][v].y;
                vert.TexCoord = DirectX::XMFLOAT2(u, v_coord);  // 必须赋值给顶点！
            }
            else {
                vert.TexCoord = DirectX::XMFLOAT2(0, 0);
            }
            vertices.push_back(vert);
        }
        // 打印前3个顶点的UV（调试用）
        if (mesh->HasTextureCoords(0) && !vertices.empty()) {
            OutputDebugStringA("  Sample UVs (first 3 vertices):\n");
            for (size_t v = 0; v < std::min(size_t(3), vertices.size()); ++v) {
                std::string uvInfo = "    [" + std::to_string(v) + "] U:" +
                    std::to_string(vertices[v].TexCoord.x) +
                    " V:" + std::to_string(vertices[v].TexCoord.y) + "\n";
                OutputDebugStringA(uvInfo.c_str());
            }
        }

        // 索引数据
        std::vector<UINT> indices;
        indices.reserve(mesh->mNumFaces * 3);
        for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
            aiFace& face = mesh->mFaces[f];
            for (unsigned int idx = 0; idx < face.mNumIndices; ++idx) {
                indices.push_back(face.mIndices[idx]);
            }
        }

        // 创建缓冲区
        D3D11_BUFFER_DESC vbd = {};
        vbd.Usage = D3D11_USAGE_DEFAULT;
        vbd.ByteWidth = sizeof(ModelVertex) * (UINT)vertices.size();
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA vInitData = { vertices.data() };
        device->CreateBuffer(&vbd, &vInitData, &meshData.VertexBuffer);

        D3D11_BUFFER_DESC ibd = {};
        ibd.Usage = D3D11_USAGE_DEFAULT;
        ibd.ByteWidth = sizeof(UINT) * (UINT)indices.size();
        ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA iInitData = { indices.data() };
        device->CreateBuffer(&ibd, &iInitData, &meshData.IndexBuffer);
        meshData.IndexCount = (UINT)indices.size();

        // 材质和纹理
        meshData.TextureSRV = nullptr;
        if (mesh->mMaterialIndex >= 0) {
            aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];

            aiString matName;
            mat->Get(AI_MATKEY_NAME, matName);
            OutputDebugStringA(("  [Material] " + std::string(matName.C_Str()) + "\n").c_str());

            // 尝试多种纹理类型
            meshData.TextureSRV = LoadTexture(mat, aiTextureType_DIFFUSE, scene, device, modelDir);
            if (!meshData.TextureSRV) {
                meshData.TextureSRV = LoadTexture(mat, aiTextureType_UNKNOWN, scene, device, modelDir);
            }
        }

        // 默认白色纹理兜底
        if (!meshData.TextureSRV) {
            OutputDebugStringA("  Using default white texture\n");
            meshData.TextureSRV = CreateDefaultWhiteTexture(device);
        }

        meshData.WorldMatrix = worldMat;
        outMeshes.push_back(std::move(meshData));
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        ProcessNode(node->mChildren[i], worldMat, scene, device, modelDir, outMeshes);
    }
}

// ==================== CreateDefaultWhiteTexture (新增) ====================

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Model::CreateDefaultWhiteTexture(ID3D11Device* device) {
    UINT whitePixels[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = 2;
    desc.Height = 2;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = { whitePixels, 2 * sizeof(UINT) };
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;

    if (SUCCEEDED(device->CreateTexture2D(&desc, &initData, &texture))) {
        ID3D11ShaderResourceView* srv = nullptr;
        if (SUCCEEDED(device->CreateShaderResourceView(texture.Get(), nullptr, &srv))) {
            return srv;
        }
    }
    return nullptr;
}

// ==================== CreateSRVFromMemory (保持不变，但确保存在) ====================

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Model::CreateSRVFromMemory(
    const void* data, size_t size, ID3D11Device* device) {

    Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
    if (FAILED(hr)) return nullptr;

    Microsoft::WRL::ComPtr<IWICStream> stream;
    hr = wicFactory->CreateStream(&stream);
    if (FAILED(hr)) return nullptr;

    hr = stream->InitializeFromMemory(const_cast<BYTE*>(static_cast<const BYTE*>(data)), static_cast<DWORD>(size));
    if (FAILED(hr)) return nullptr;

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    hr = wicFactory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) return nullptr;

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return nullptr;

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    hr = wicFactory->CreateFormatConverter(&converter);
    if (FAILED(hr)) return nullptr;

    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) return nullptr;

    UINT width, height;
    converter->GetSize(&width, &height);
    std::vector<BYTE> pixels(width * height * 4);
    hr = converter->CopyPixels(nullptr, width * 4, (UINT)pixels.size(), pixels.data());
    if (FAILED(hr)) return nullptr;

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = { pixels.data(), width * 4 };
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    hr = device->CreateTexture2D(&texDesc, &initData, &texture);
    if (FAILED(hr)) return nullptr;

    ID3D11ShaderResourceView* srv = nullptr;
    device->CreateShaderResourceView(texture.Get(), nullptr, &srv);
    return srv;
}

// ==================== sw 类实现 ====================
std::vector<swobj> sw::sws;

void sw::init() {
    //npc位置
    pos_npc = XMFLOAT3(0.0f, 1.0f, 20.0f);
    // 随机生成 2 个生物
    for (int i = 0; i < 2; ++i) {
        swobj obj;
        // 随机初始位置 (x:2~17, z:2~17, y:14 空中下落)
        float x = 2.0f + static_cast<float>(rand() % 16);
        float z = 2.0f + static_cast<float>(rand() % 16);
        float y = 14.0f;
        obj.m_transform.SetPosition(x, y, z);
        obj.m_transform.SetUniformScale(0.1f);          // 仅用于视觉渲染，不影响包围盒
        obj.blood = 10.0f;

        // 包围盒尺寸与玩家完全一致，不受缩放影响
        obj.m_box.Center = XMFLOAT3(x, y+1.0f, z);    
        obj.m_box.Extents = XMFLOAT3(0.5f, 1.0f, 0.5f); // 固定尺寸

        sws.push_back(obj);
    }

    // 订阅右键攻击事件 (type = 0)
    m_subscriptions.push_back(EventBus::Subscribe<event::rightmousedown>(
        [this](const event::rightmousedown& e) {
            if (e.type != 0) return;    // 只处理攻击事件

            // 寻找距离事件位置最近的生物（必须在攻击范围内）
            const float attackRangeSq = 2.5f * 2.5f;   // 攻击范围 2.5 格
            swobj* target = nullptr;
            float minDistSq = attackRangeSq;

            for (auto& s : sws) {
                XMFLOAT3 center = s.m_box.Center;
                float dx = center.x - e.pos.x;
                float dy = center.y - e.pos.y;
                float dz = center.z - e.pos.z;
                float distSq = dx * dx + dy * dy + dz * dz;
                if (distSq < minDistSq) {
                    minDistSq = distSq;
                    target = &s;
                }
            }

            if (target) {
                target->blood -= 1.0f;    // 扣血，无位移
                EventBus::Publish<event::playaudio>(event::playaudio(4)); // 播放攻击音效
            }
        }
    ));
    m_subscriptions.push_back(EventBus::Subscribe<event::setcamera>([this](const event::setcamera& e) {
            float distance_npc= powf(pos_npc.x - e.pos.x, 2) + powf(pos_npc.y - e.pos.y, 2) + powf(pos_npc.z - e.pos.z, 2);
            if (distance_npc < 16.0f) {
                 EventBus::Publish<event::npcdialog>(event::npcdialog(true));
            }
            else {
                 EventBus::Publish<event::npcdialog>(event::npcdialog(false));
            }
        }));
}

void sw::Update(float dt) {
    const float gravity = 9.8f;

    for (auto it = sws.begin(); it != sws.end(); ) {
        swobj& obj = *it;

        // 死亡移除
        if (obj.blood <= 0.0f) {
            it = sws.erase(it);
            continue;
        }

        // ---------- 重力模拟 ----------
        XMFLOAT3 oldPos = obj.m_transform.GetPosition();
        obj.m_transform.MoveUp(-gravity * dt);   // 下落

        // 构建移动后的包围盒（中心随 Transform 位置同步下移）
        XMFLOAT3 newPos = obj.m_transform.GetPosition();
        XMFLOAT3 newCenter = XMFLOAT3(newPos.x, newPos.y+1.0f , newPos.z);
        BoundingBox newBox(newCenter, obj.m_box.Extents);

        // 碰撞检测（与方块）
        bool collided = false;
        int minX = (int)floor(newBox.Center.x - newBox.Extents.x);
        int maxX = (int)floor(newBox.Center.x + newBox.Extents.x);
        int minY = (int)floor(newBox.Center.y - newBox.Extents.y);
        int maxY = (int)floor(newBox.Center.y + newBox.Extents.y);
        int minZ = (int)floor(newBox.Center.z - newBox.Extents.z);
        int maxZ = (int)floor(newBox.Center.z + newBox.Extents.z);

        for (int x = minX; x <= maxX && !collided; ++x) {
            for (int y = minY; y <= maxY && !collided; ++y) {
                for (int z = minZ; z <= maxZ && !collided; ++z) {
                    if (x < 0 || x >= 20 || y < 0 || y >= 10 || z < 0 || z >= 20)
                        continue;
                    auto itb = Block::boundmap.find({ x, y, z });
                    if (itb != Block::boundmap.end()) {
                        if (newBox.Intersects(itb->second)) {
                            collided = true;
                        }
                    }
                }
            }
        }

        if (collided) {
            obj.m_transform.SetPosition(oldPos);    // 回退位置
        }
        else {
            obj.m_box.Center = newCenter;           // 更新包围盒中心
        }

        ++it;
    }
}

void Audio::init() {
    g_audioEngine = std::make_unique<DirectX::AudioEngine>();// 创建音频引擎
    std::unique_ptr<DirectX::SoundEffect> m_jumpSound;
    m_jumpSound = std::make_unique<DirectX::SoundEffect>(
        g_audioEngine.get(),
        L"music/beforeplay.wav"
    );
    std::unique_ptr<DirectX::SoundEffectInstance> m_bgmInstance;
    m_bgmInstance = m_jumpSound->CreateInstance();
    m_soundInstances.push_back(std::move(m_bgmInstance));
    m_soundbgm.push_back(std::move(m_jumpSound));
    m_jumpSound = std::make_unique<DirectX::SoundEffect>(
        g_audioEngine.get(),
        L"music/gamebgm.wav"
    );
    m_bgmInstance = m_jumpSound->CreateInstance();
    m_soundInstances.push_back(std::move(m_bgmInstance));
    m_soundbgm.push_back(std::move(m_jumpSound));
    m_jumpSound = std::make_unique<DirectX::SoundEffect>(
        g_audioEngine.get(),
        L"music/destory.wav"
    );
    m_soundEffects.push_back(std::move(m_jumpSound));
    m_jumpSound = std::make_unique<DirectX::SoundEffect>(
        g_audioEngine.get(),
        L"music/put.wav"
    );
    m_soundEffects.push_back(std::move(m_jumpSound));
    m_jumpSound = std::make_unique<DirectX::SoundEffect>(
        g_audioEngine.get(),
        L"music/hit.wav"
    );
    m_soundEffects.push_back(std::move(m_jumpSound));//加载音效

    m_subscriptions.push_back(EventBus::Subscribe<event::playaudio>([this](const event::playaudio& e) {
        if(e.type==0){
            m_soundInstances[0]->Play(true);
        }
        else if(e.type==1){
            m_soundInstances[0]->Stop();
            m_soundInstances[1]->Play(true);
        }
        else
            m_soundEffects[e.type-2]->Play();
        }));
}

void Bag::init() {
    bagobjs.resize(10);
    for (int i = 0; i < 3; i++) {
        bagobjs[i].type = i + 1;
        bagobjs[i].count = 10;
    }//初始化背包，预设3种物品，每种10个
    m_subscriptions.push_back(EventBus::Subscribe<event::getobj>([this](const event::getobj& e) {
        bool found = false;
        for (auto& pair : bagobjs) {
            if (pair.type == e.type&& pair.count < 100) {
                pair.count++;
                found = true;
                break;
            }
        }
        if(!found){
            for (auto& pair : bagobjs) {
                if (pair.type == 0) {
                    pair.type = e.type;
                    pair.count = 1;
                    break;
                }
            }
        }
    }));//订阅拾取事件，增加背包中对应物品的数量
    m_subscriptions.push_back(EventBus::Subscribe<event::putobj>([this](const event::putobj& e) {
        bagobjs[0].count--;
        }));//订阅放置事件
}

void Bag::Update() {
    if (bagobjs[0].count == 0)
        bagobjs[0].type = 0;
    player::blocktype = bagobjs[0].type;
}