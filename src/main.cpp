#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "Config.hpp"
#include "Projection.hpp"
#include "OverlayRenderer.hpp"

using namespace geode::prelude;

// Hook 挂钩 PlayLayer
class $modify(MatryoshkaWindowLayer, PlayLayer) {
    struct Fields {
        HWND m_gameHwnd = nullptr;                  // 游戏主窗口句柄
        RECT m_origRect{ 0, 0, 0, 0 };              // 进入关卡前的初始窗口位置与大小
        bool m_initialized = false;                 // 初始化完成标志

        int m_screenWidth = 0;                      // 物理屏幕分辨率宽
        int m_screenHeight = 0;                     // 物理屏幕分辨率高
        int m_winWidth = 0;                         // 缩放后游戏客户区基准宽
        int m_winHeight = 0;                        // 缩放后游戏客户区基准高

        int m_borderLeft = 0;                       // 窗口左侧边框宽度
        int m_borderTop = 0;                        // 窗口顶部标题栏与边框高度
        int m_totalBorderW = 0;                     // 左右边框总厚度
        int m_totalBorderH = 0;                     // 上下边框+标题栏总厚度

        // 着地状态：0.0f = 地面触底锚定，1.0f = 天花板触顶锚定
        bool  m_p1TargetAnchorTop = false;          // P1 目标锚定方向 (false: 地面, true: 天花板)
        float m_p1AnchorRatio = 0.0f;               // P1 平滑插值比例 (0.0f ~ 1.0f)

        bool  m_p2TargetAnchorTop = true;           // P2 目标锚定方向 (默认反向天花板)
        float m_p2AnchorRatio = 1.0f;               // P2 平滑插值比例

        std::deque<cocos2d::CCPoint> m_waveTrail;   // P1 Wave 轨迹历史点队列
        std::deque<cocos2d::CCPoint> m_waveTrail2;  // P2 Wave 轨迹历史点队列

        // Wave 飞行方向与上一帧锚点
        int m_p1WaveDir = 0; // 1 = 向上, -1 = 向下, 0 = 平移/未初始化
        cocos2d::CCPoint m_p1LastPos{ 0.0f, 0.0f };

        int m_p2WaveDir = 0;
        cocos2d::CCPoint m_p2LastPos{ 0.0f, 0.0f };

        OverlayRenderer m_renderer;                 // 全屏 Direct2D 底层投影渲染器
    };

    // 恢复游戏窗口至进入关卡前的原始状态与层级
    void restoreWindow() {
        m_fields->m_renderer.cleanup();

        if (m_fields->m_initialized && m_fields->m_gameHwnd) {
            int origW = m_fields->m_origRect.right - m_fields->m_origRect.left;
            int origH = m_fields->m_origRect.bottom - m_fields->m_origRect.top;

            SetWindowPos(
                m_fields->m_gameHwnd,
                HWND_NOTOPMOST,
                m_fields->m_origRect.left,
                m_fields->m_origRect.top,
                origW,
                origH,
                SWP_NOACTIVATE | SWP_SHOWWINDOW
            );

            m_fields->m_waveTrail.clear();
            m_fields->m_waveTrail2.clear();
            m_fields->m_initialized = false;
        }
    }

    // PlayLayer 初始化：计算屏幕缩放、窗口边框并初始化外挂渲染层
    bool init(GJGameLevel * level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        loadOrInitConfig();

        // 1. 获取主游戏窗口句柄 (优先从 OpenGL 上下文获取)
        m_fields->m_gameHwnd = WindowFromDC(wglGetCurrentDC());
        if (!m_fields->m_gameHwnd) {
            m_fields->m_gameHwnd = FindWindowA(nullptr, "Geometry Dash");
        }

        if (m_fields->m_gameHwnd) {
            // 2. 记录原始尺寸并精确测量系统标题栏/边框边距
            GetWindowRect(m_fields->m_gameHwnd, &m_fields->m_origRect);

            POINT ptClientTopLeft = { 0, 0 };
            ClientToScreen(m_fields->m_gameHwnd, &ptClientTopLeft);
            m_fields->m_borderLeft = ptClientTopLeft.x - m_fields->m_origRect.left;
            m_fields->m_borderTop = ptClientTopLeft.y - m_fields->m_origRect.top;

            RECT clientRect;
            GetClientRect(m_fields->m_gameHwnd, &clientRect);
            m_fields->m_totalBorderW = (m_fields->m_origRect.right - m_fields->m_origRect.left) - clientRect.right;
            m_fields->m_totalBorderH = (m_fields->m_origRect.bottom - m_fields->m_origRect.top) - clientRect.bottom;

            m_fields->m_screenWidth = GetSystemMetrics(SM_CXSCREEN);
            m_fields->m_screenHeight = GetSystemMetrics(SM_CYSCREEN);

            // 3. 计算微型化窗口目标客户区大小
            float scale = std::clamp(g_config.windowScaleRatio, 0.01f, 1.0f);
            m_fields->m_winWidth = static_cast<int>(std::round(static_cast<float>(m_fields->m_screenWidth) * scale));
            m_fields->m_winHeight = static_cast<int>(std::round(static_cast<float>(m_fields->m_screenHeight) * scale));

            // 4. 将主窗口置顶并缩放居中
            SetWindowPos(
                m_fields->m_gameHwnd,
                HWND_TOPMOST,
                (m_fields->m_screenWidth - m_fields->m_winWidth) / 2 - m_fields->m_borderLeft,
                (m_fields->m_screenHeight - m_fields->m_winHeight) / 2 - m_fields->m_borderTop,
                m_fields->m_winWidth + m_fields->m_totalBorderW,
                m_fields->m_winHeight + m_fields->m_totalBorderH,
                SWP_NOACTIVATE
            );

            // 5. 初始化 DirectComposition + Direct2D 透明全屏 Overlay 与 2P 窗口
            m_fields->m_renderer.init(
                m_fields->m_gameHwnd,
                m_fields->m_screenWidth,
                m_fields->m_screenHeight,
                m_fields->m_winWidth,
                m_fields->m_winHeight,
                m_fields->m_totalBorderW,
                m_fields->m_totalBorderH
            );

            m_fields->m_initialized = true;
        }

        return true;
    }

    // 关卡重置（死亡复活/重新开始）：重置锚定状态、清空轨迹并隐藏 2P 窗口
    void resetLevel() {
        PlayLayer::resetLevel();
        if (m_fields->m_initialized) {
            m_fields->m_p1TargetAnchorTop = false;
            m_fields->m_p1AnchorRatio = 0.0f;
            m_fields->m_p2TargetAnchorTop = true;
            m_fields->m_p2AnchorRatio = 1.0f;

            m_fields->m_waveTrail.clear();
            m_fields->m_waveTrail2.clear();

            // 重置 Wave 状态
            m_fields->m_p1WaveDir = 0;
            m_fields->m_p1LastPos = cocos2d::CCPoint(0.0f, 0.0f);
            m_fields->m_p2WaveDir = 0;
            m_fields->m_p2LastPos = cocos2d::CCPoint(0.0f, 0.0f);

            if (m_fields->m_renderer.m_p2Hwnd) {
                ShowWindow(m_fields->m_renderer.m_p2Hwnd, SW_HIDE);
            }
        }
    }

    // 每帧物理更新后逻辑：同步窗口位置、追踪 Dual 状态、采集 Wave 轨迹并提交全屏投影
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        if (!m_fields->m_initialized || !m_fields->m_gameHwnd || !m_player1 || !m_fields->m_renderer.isReady()) {
            return;
        }

        auto director = cocos2d::CCDirector::sharedDirector();
        if (!director) return;
        cocos2d::CCSize winSize = director->getWinSize();
        if (winSize.width <= 0.0f || winSize.height <= 0.0f) return;

        // 1. 获取摄像机实时缩放
        float currentCameraScale = m_objectLayer ? std::abs(m_objectLayer->getScale()) : 1.0f;
        if (currentCameraScale <= 0.001f) currentCameraScale = 1.0f;

        // 辅助 Lambda：获取角色全部实时缩放
        auto getAccuratePlayerScale = [](PlayerObject* player) -> float {
            if (!player) return 1.0f;
            float sy = std::abs(player->getScaleY());
            if (sy > 0.001f) return sy;
            float sx = std::abs(player->getScaleX());
            if (sx > 0.001f) return sx;
            float s = std::abs(player->getScale());
            if (s > 0.001f) return s;
            if (player->m_vehicleSize > 0.001f) return player->m_vehicleSize;
            return 1.0f;
            };

        float pixelsPerPoint = static_cast<float>(m_fields->m_screenHeight) / winSize.height;

        // 2. 同步 Player 1 窗口
        if (m_player1->m_isOnGround) {
            // 仅在接触地面/天花板帧更新目标状态
            m_fields->m_p1TargetAnchorTop = m_player1->m_isUpsideDown;
        }

        //地面/天花板对准平滑过渡
        constexpr float kFastLerpSpeed = 90.0f;
        float targetRatio1 = m_fields->m_p1TargetAnchorTop ? 1.0f : 0.0f;
        m_fields->m_p1AnchorRatio += (targetRatio1 - m_fields->m_p1AnchorRatio) * std::clamp(kFastLerpSpeed * dt, 0.0f, 1.0f);
        if (std::abs(m_fields->m_p1AnchorRatio - targetRatio1) < 0.001f) {
            m_fields->m_p1AnchorRatio = targetRatio1;
        }

        float p1Scale = getAccuratePlayerScale(m_player1);
        float p1TotalScale = p1Scale * currentCameraScale;

        int curWinW = static_cast<int>(std::round(m_fields->m_winWidth * p1TotalScale));
        int curWinH = static_cast<int>(std::round(m_fields->m_winHeight * p1TotalScale));

        // 角色在屏幕上的物理半高像素投影（GD 标准半高为 15.0f）
        float p1HalfHeightDesk = 15.0f * p1Scale * pixelsPerPoint * currentCameraScale;

        cocos2d::CCPoint p1PhysPos = m_player1->m_position;
        cocos2d::CCPoint p1CenterWorld = m_objectLayer ? m_objectLayer->convertToWorldSpace(p1PhysPos) : p1PhysPos;
        POINT p1DeskPt = worldToScreen(p1CenterWorld, winSize, m_fields->m_screenWidth, m_fields->m_screenHeight);

        int clientX1 = p1DeskPt.x - (curWinW / 2);
        int winX1 = clientX1 - m_fields->m_borderLeft;

        // 地面触底：客户区底边贴地，标题栏向上伸展
        float winY_ground1 = static_cast<float>(p1DeskPt.y) + p1HalfHeightDesk - static_cast<float>(curWinH) - static_cast<float>(m_fields->m_borderTop);
        // 天花板触顶：窗口顶边（状态栏最上方）贴顶，整个窗口向下挂载
        float winY_ceiling1 = static_cast<float>(p1DeskPt.y) - p1HalfHeightDesk;

        float finalWinY1 = (1.0f - m_fields->m_p1AnchorRatio) * winY_ground1 + m_fields->m_p1AnchorRatio * winY_ceiling1;
        int winY1 = static_cast<int>(std::round(finalWinY1));

        // 极速无重绘窗口平移标志
        constexpr UINT kMotionFlags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOOWNERZORDER | SWP_NOREDRAW;

        SetWindowPos(
            m_fields->m_gameHwnd,
            nullptr,
            winX1,
            winY1,
            curWinW + m_fields->m_totalBorderW,
            curWinH + m_fields->m_totalBorderH,
            kMotionFlags
        );

        // 3. Dual 分裂检测与 Player 2 窗口同步
        bool isDual = (m_player2 != nullptr && m_player2->getParent() != nullptr && m_player2->isVisible());
        int curWinW2 = curWinW;
        int curWinH2 = curWinH;

        if (isDual && m_fields->m_renderer.m_p2Hwnd) {
            if (m_player2->m_isOnGround) {
                m_fields->m_p2TargetAnchorTop = m_player2->m_isUpsideDown;
            }

            float targetRatio2 = m_fields->m_p2TargetAnchorTop ? 1.0f : 0.0f;
            m_fields->m_p2AnchorRatio += (targetRatio2 - m_fields->m_p2AnchorRatio) * std::clamp(kFastLerpSpeed * dt, 0.0f, 1.0f);
            if (std::abs(m_fields->m_p2AnchorRatio - targetRatio2) < 0.001f) {
                m_fields->m_p2AnchorRatio = targetRatio2;
            }

            float p2Scale = getAccuratePlayerScale(m_player2);
            float p2TotalScale = p2Scale * currentCameraScale;

            curWinW2 = static_cast<int>(std::round(m_fields->m_winWidth * p2TotalScale));
            curWinH2 = static_cast<int>(std::round(m_fields->m_winHeight * p2TotalScale));

            float p2HalfHeightDesk = 15.0f * p2Scale * pixelsPerPoint * currentCameraScale;

            cocos2d::CCPoint p2PhysPos = m_player2->m_position;
            cocos2d::CCPoint p2CenterWorld = m_objectLayer ? m_objectLayer->convertToWorldSpace(p2PhysPos) : p2PhysPos;
            POINT p2DeskPt = worldToScreen(p2CenterWorld, winSize, m_fields->m_screenWidth, m_fields->m_screenHeight);

            int clientX2 = p2DeskPt.x - (curWinW2 / 2);
            int p2WinX = clientX2 - m_fields->m_borderLeft;

            float winY_ground2 = static_cast<float>(p2DeskPt.y) + p2HalfHeightDesk - static_cast<float>(curWinH2) - static_cast<float>(m_fields->m_borderTop);
            float winY_ceiling2 = static_cast<float>(p2DeskPt.y) - p2HalfHeightDesk;

            float finalWinY2 = (1.0f - m_fields->m_p2AnchorRatio) * winY_ground2 + m_fields->m_p2AnchorRatio * winY_ceiling2;
            int p2WinY = static_cast<int>(std::round(finalWinY2));

            // 显示或平移 2P 独立窗口
            if (!IsWindowVisible(m_fields->m_renderer.m_p2Hwnd)) {
                SetWindowPos(
                    m_fields->m_renderer.m_p2Hwnd,
                    HWND_TOPMOST,
                    p2WinX,
                    p2WinY,
                    curWinW2 + m_fields->m_totalBorderW,
                    curWinH2 + m_fields->m_totalBorderH,
                    SWP_NOACTIVATE | SWP_SHOWWINDOW
                );
            }
            else {
                SetWindowPos(
                    m_fields->m_renderer.m_p2Hwnd,
                    nullptr,
                    p2WinX,
                    p2WinY,
                    curWinW2 + m_fields->m_totalBorderW,
                    curWinH2 + m_fields->m_totalBorderH,
                    kMotionFlags
                );
            }

            // 同步主窗口图像并执行反色渲染至 2P 窗口
            m_fields->m_renderer.renderP2Inverted(m_fields->m_gameHwnd, curWinW, curWinH, curWinW2, curWinH2);
        }
        else if (m_fields->m_renderer.m_p2Hwnd && IsWindowVisible(m_fields->m_renderer.m_p2Hwnd)) {
            ShowWindow(m_fields->m_renderer.m_p2Hwnd, SW_HIDE);
        }

        // 4. Wave 轨迹记录与拐点裁剪优化
        auto updateWaveTrail = [](std::deque<cocos2d::CCPoint>& trail, int& currentDir, cocos2d::CCPoint& lastPos, const cocos2d::CCPoint& curPos, float maxDist) {
            float dx = curPos.x - lastPos.x;
            float dy = curPos.y - lastPos.y;

            // 当产生有效位移时更新
            if (std::abs(dx) > 0.001f || std::abs(dy) > 0.001f) {
                // 判断当前飞行方向：1 = 向上飞, -1 = 向下飞, 0 = 水平贴地面/天花板
                int newDir = (dy > 0.05f) ? 1 : ((dy < -0.05f) ? -1 : 0);

                if (trail.empty()) {
                    trail.push_back(curPos);
                    trail.push_back(curPos);
                    currentDir = newDir;
                }
                else {
                    // 变向瞬间（拐弯/触底触顶反弹）：将上一帧位置固化为折线拐点，开启新直线段
                    if (newDir != currentDir) {
                        trail.back() = lastPos;
                        trail.push_back(curPos);
                        currentDir = newDir;
                    }
                    else {
                        // 同方向直线延伸：只更新活动端点，绝不插入中间稠密散点
                        trail.back() = curPos;
                    }
                }
                lastPos = curPos;
            }

            // 平滑尾部裁剪（按像素距离对最老的线段进行插值截断）
            while (trail.size() >= 2) {
                if (curPos.x - trail[1].x > maxDist) {
                    trail.pop_front();
                }
                else if (curPos.x - trail[0].x > maxDist) {
                    float excess = (curPos.x - maxDist) - trail[0].x;
                    float segDx = trail[1].x - trail[0].x;
                    if (segDx > 0.001f) {
                        float t = std::clamp(excess / segDx, 0.0f, 1.0f);
                        trail[0].x = trail[0].x + t * (trail[1].x - trail[0].x);
                        trail[0].y = trail[0].y + t * (trail[1].y - trail[0].y);
                    }
                    break;
                }
                else {
                    break;
                }
            }
            };

        bool isWave = m_player1->m_isDart;
        if (isWave) {
            updateWaveTrail(m_fields->m_waveTrail, m_fields->m_p1WaveDir, m_fields->m_p1LastPos, p1PhysPos, g_config.waveTrailMaxDist);
        }
        else {
            if (!m_fields->m_waveTrail.empty()) {
                m_fields->m_waveTrail.clear();
                m_fields->m_p1WaveDir = 0;
            }
        }

        bool isWave2 = isDual && m_player2->m_isDart;
        if (isWave2) {
            updateWaveTrail(m_fields->m_waveTrail2, m_fields->m_p2WaveDir, m_fields->m_p2LastPos, m_player2->m_position, g_config.waveTrailMaxDist);
        }
        else {
            if (!m_fields->m_waveTrail2.empty()) {
                m_fields->m_waveTrail2.clear();
                m_fields->m_p2WaveDir = 0;
            }
        }

        // 5. 渲染底层投影与全屏提交
        m_fields->m_renderer.beginFrame(m_fields->m_screenWidth, m_fields->m_screenHeight);

        // 绘制地面与天花板
        m_fields->m_renderer.renderGroundAndCeiling(
            m_groundLayer, m_groundLayer2,
            m_objectLayer, winSize,
            m_fields->m_screenWidth, m_fields->m_screenHeight
        );

        // 视口剔除并渲染关卡物块 (方块/刺/Portal/Orbs等)
        m_fields->m_renderer.renderObjects(
            m_objects,
            m_objectLayer,
            winSize,
            m_fields->m_screenWidth,
            m_fields->m_screenHeight,
            currentCameraScale
        );

        // 绘制 Wave 模式平滑轨迹
        if (isWave || isWave2) {
            m_fields->m_renderer.updateWavePen(currentCameraScale, m_fields->m_screenHeight);
            if (isWave) m_fields->m_renderer.renderWaveTrail(m_fields->m_waveTrail, m_objectLayer, winSize, m_fields->m_screenWidth, m_fields->m_screenHeight);
            if (isWave2) m_fields->m_renderer.renderWaveTrail(m_fields->m_waveTrail2, m_objectLayer, winSize, m_fields->m_screenWidth, m_fields->m_screenHeight);
        }

        // 交换链 Present 呈现画面
        m_fields->m_renderer.present(m_fields->m_screenWidth, m_fields->m_screenHeight);
    }

    // 退出关卡 Hook：还原窗口大小与层级
    void onExit() {
        restoreWindow();
        PlayLayer::onExit();
    }
};