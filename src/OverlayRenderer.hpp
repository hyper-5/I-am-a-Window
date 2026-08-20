#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d2d1_1.h>
#include <d2d1helper.h>
#include <dcomp.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dcomp.lib")

#include "Config.hpp"
#include "Projection.hpp"
#include <deque>
#include <vector>
#include <algorithm>

// COM 接口安全释放辅助模板
template<typename T>
inline void SafeRelease(T*& p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

// 将 Win32 COLORREF 转换为 Direct2D ColorF
inline D2D1::ColorF ColorRefToD2D(COLORREF c, float alpha = 1.0f) {
    return D2D1::ColorF(
        GetRValue(c) / 255.0f,
        GetGValue(c) / 255.0f,
        GetBValue(c) / 255.0f,
        alpha
    );
}

// 快速物件分类枚举（用于单分支 O(1) 快速分发）
enum class FastObjectClass : uint8_t {
    Skip = 0,       // 忽略/不渲染
    Solid,          // 普通实体方块
    Slope,          // 斜坡
    Breakable,      // 可破坏物体
    Hazard,         // 尖刺 / 齿轮
    SpeedPortal,    // 变速门
    Portal,         // Portal
    Pad,            // Pads
    OrbOrDash,      // Orbs / Dash
    Collectible     // 收集品 (金币/钥匙/拾取物)
};

class OverlayRenderer {
public:
    HWND m_overlayHwnd = nullptr;   // 顶层透明全屏绘制窗口句柄
    HWND m_p2Hwnd = nullptr;        // 2P 独立反色镜像窗口句柄

    // 常驻硬件 DC 缓存（避免每帧 GetDC / ReleaseDC 开销）
    HDC  m_hdcGame = nullptr;
    HDC  m_hdcP2 = nullptr;

    // D3D11 / DXGI 设备与交换链
    ID3D11Device* m_d3dDevice = nullptr;
    ID3D11DeviceContext* m_d3dContext = nullptr;
    IDXGISwapChain1* m_swapChain = nullptr;

    // DirectComposition 合成器组件（用于穿透透明与超低延迟硬件呈现）
    IDCompositionDevice* m_dcompDevice = nullptr;
    IDCompositionTarget* m_dcompTarget = nullptr;
    IDCompositionVisual* m_dcompVisual = nullptr;

    // Direct2D 渲染上下文与目标位图
    ID2D1Factory1* m_d2dFactory = nullptr;
    ID2D1DeviceContext* m_d2dContext = nullptr;
    ID2D1Bitmap1* m_targetBitmap = nullptr;
    ID2D1StrokeStyle* m_waveStrokeStyle = nullptr;

    // 预烘焙静态 Geometry 几何缓存（显存常驻，基于 [-0.5, 0.5] 归一化局部坐标）
    ID2D1PathGeometry* m_geoSpike = nullptr;          // 尖刺
    ID2D1PathGeometry* m_geoSlope = nullptr;          // 斜坡
    ID2D1PathGeometry* m_geoSaw = nullptr;            // 齿轮外圈
    ID2D1PathGeometry* m_geoSawHub = nullptr;         // 齿轮中心轴
    ID2D1PathGeometry* m_geoPad = nullptr;            // 跳垫
    ID2D1PathGeometry* m_geoPortalFrame = nullptr;    // 传送门外框
    ID2D1PathGeometry* m_geoPortalBeam = nullptr;     // 传送门核心光束
    ID2D1PathGeometry* m_geoDualArrowFrame = nullptr; // 双人/单人门外箭头
    ID2D1PathGeometry* m_geoDualArrowBeam = nullptr;  // 双人/单人门内箭头
    ID2D1PathGeometry* m_geoSpeedArrow = nullptr;     // 变速箭头
    ID2D1PathGeometry* m_geoDashOuter = nullptr;      // Dash外三角
    ID2D1PathGeometry* m_geoDashInner = nullptr;      // Dash内三角

    // 基础画刷缓存
    ID2D1SolidColorBrush* m_brushSolid = nullptr;
    ID2D1SolidColorBrush* m_brushHazard = nullptr;
    ID2D1SolidColorBrush* m_brushHazardDark = nullptr;
    ID2D1SolidColorBrush* m_brushOutline = nullptr;
    ID2D1SolidColorBrush* m_brushWaveTrail = nullptr;

    // 预置调色板画刷（门 / 环 / 垫色彩映射）
    ID2D1SolidColorBrush* m_colYellow = nullptr;    ID2D1SolidColorBrush* m_colYellowCore = nullptr;
    ID2D1SolidColorBrush* m_colPink = nullptr;      ID2D1SolidColorBrush* m_colPinkCore = nullptr;
    ID2D1SolidColorBrush* m_colOrange = nullptr;    ID2D1SolidColorBrush* m_colOrangeCore = nullptr;
    ID2D1SolidColorBrush* m_colBlue = nullptr;      ID2D1SolidColorBrush* m_colBlueCore = nullptr;
    ID2D1SolidColorBrush* m_colGreen = nullptr;     ID2D1SolidColorBrush* m_colGreenCore = nullptr;
    ID2D1SolidColorBrush* m_colBlack = nullptr;     ID2D1SolidColorBrush* m_colBlackCore = nullptr;
    ID2D1SolidColorBrush* m_colPurple = nullptr;    ID2D1SolidColorBrush* m_colPurpleCore = nullptr;
    ID2D1SolidColorBrush* m_colWhite = nullptr;     ID2D1SolidColorBrush* m_colWhiteCore = nullptr;
    ID2D1SolidColorBrush* m_colGrey = nullptr;      ID2D1SolidColorBrush* m_colGreyCore = nullptr;
    ID2D1SolidColorBrush* m_colCyan = nullptr;      ID2D1SolidColorBrush* m_colCyanCore = nullptr;
    ID2D1SolidColorBrush* m_colAmber = nullptr;     ID2D1SolidColorBrush* m_colAmberCore = nullptr;
    ID2D1SolidColorBrush* m_colMagenta = nullptr;   ID2D1SolidColorBrush* m_colMagentaCore = nullptr;
    ID2D1SolidColorBrush* m_colRed = nullptr;       ID2D1SolidColorBrush* m_colRedCore = nullptr;

    float m_currentPenWidth = 0.0f; // 当前 Wave 轨迹线宽

    // 检查渲染器是否就绪
    bool isReady() const {
        return m_d2dContext != nullptr;
    }

    // 根据多边形顶点构建闭合 Unit PathGeometry
    ID2D1PathGeometry* createUnitGeometry(const D2D1_POINT_2F* pts, int count) {
        ID2D1PathGeometry* path = nullptr;
        if (SUCCEEDED(m_d2dFactory->CreatePathGeometry(&path))) {
            ID2D1GeometrySink* sink = nullptr;
            if (SUCCEEDED(path->Open(&sink))) {
                sink->BeginFigure(pts[0], D2D1_FIGURE_BEGIN_FILLED);
                sink->AddLines(pts + 1, count - 1);
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                sink->Close();
                sink->Release();
            }
        }
        return path;
    }

    // 预烘焙静态图元形状（尖刺、斜坡、锯齿、跳垫、传送门框架与箭头）
    void initStaticGeometries() {
        // 尖刺 (3 顶点)
        D2D1_POINT_2F spikePts[3] = { {-0.5f, -0.5f}, {0.0f, 0.5f}, {0.5f, -0.5f} };
        m_geoSpike = createUnitGeometry(spikePts, 3);

        // 标准 1x1 直角斜坡
        D2D1_POINT_2F slopePts[3] = { {-0.5f, -0.5f}, {0.5f, 0.5f}, {0.5f, -0.5f} };
        m_geoSlope = createUnitGeometry(slopePts, 3);

        // 16 齿圆锯齿轮外框
        D2D1_POINT_2F sawPts[16];
        for (int i = 0; i < 16; ++i) {
            float angle = static_cast<float>(i) * (3.14159265f / 8.0f);
            float rad = (i % 2 == 0) ? 0.48f : 0.35f;
            sawPts[i] = { rad * std::cos(angle), rad * std::sin(angle) };
        }
        m_geoSaw = createUnitGeometry(sawPts, 16);

        // 齿轮中心八边形轴心
        D2D1_POINT_2F hubPts[8];
        for (int i = 0; i < 8; ++i) {
            float angle = static_cast<float>(i) * (3.14159265f / 4.0f);
            hubPts[i] = { 0.15f * std::cos(angle), 0.15f * std::sin(angle) };
        }
        m_geoSawHub = createUnitGeometry(hubPts, 8);

        // Pads梯形外框
        D2D1_POINT_2F padPts[4] = { {-0.45f, -0.5f}, {0.45f, -0.5f}, {0.28f, -0.1f}, {-0.28f, -0.1f} };
        m_geoPad = createUnitGeometry(padPts, 4);

        // 常规传送门外框与光束
        D2D1_POINT_2F pFrame[6] = { {-0.35f, -0.35f}, {-0.35f, 0.35f}, {0.0f, 0.5f}, {0.35f, 0.35f}, {0.35f, -0.35f}, {0.0f, -0.5f} };
        m_geoPortalFrame = createUnitGeometry(pFrame, 6);

        D2D1_POINT_2F pBeam[4] = { {-0.12f, -0.3f}, {0.12f, -0.3f}, {0.12f, 0.3f}, {-0.12f, 0.3f} };
        m_geoPortalBeam = createUnitGeometry(pBeam, 4);

        // 双人/单人模式箭头传送门
        D2D1_POINT_2F dFrame[6] = { {-0.35f, 0.45f}, {0.35f, 0.0f}, {-0.35f, -0.45f}, {-0.15f, -0.45f}, {0.55f, 0.0f}, {-0.15f, 0.45f} };
        m_geoDualArrowFrame = createUnitGeometry(dFrame, 6);

        D2D1_POINT_2F dBeam[6] = { {-0.15f, 0.30f}, {0.25f, 0.0f}, {-0.15f, -0.30f}, {-0.02f, -0.30f}, {0.38f, 0.0f}, {-0.02f, 0.30f} };
        m_geoDualArrowBeam = createUnitGeometry(dBeam, 6);

        // 变速门单箭头
        D2D1_POINT_2F spArrow[6] = {
            { -0.50f,  0.46f },
            { -0.05f,  0.46f },
            {  0.50f,  0.00f },
            { -0.05f, -0.46f },
            { -0.50f, -0.46f },
            {  0.05f,  0.00f }
        };
        m_geoSpeedArrow = createUnitGeometry(spArrow, 6);

        // Dash内外箭头
        D2D1_POINT_2F dOuter[3] = { {-0.35f, 0.4f}, {0.45f, 0.0f}, {-0.35f, -0.4f} };
        m_geoDashOuter = createUnitGeometry(dOuter, 3);

        D2D1_POINT_2F dInner[3] = { {-0.18f, 0.22f}, {0.28f, 0.0f}, {-0.18f, -0.22f} };
        m_geoDashInner = createUnitGeometry(dInner, 3);
    }

    // 初始化渲染窗口、D3D11、DirectComposition、D2D管线与资源
    bool init(HWND gameHwnd, int screenW, int screenH, int winW, int winH, int totalBorderW, int totalBorderH) {
        // 1. 注册并创建全屏透明穿透 Overlay 窗口
        WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
            switch (msg) {
            case WM_NCHITTEST: return HTTRANSPARENT; // 鼠标穿透
            case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
            }
            return DefWindowProc(hwnd, msg, wParam, lParam);
            };
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = "GD_Matryoshka_Overlay_Class";
        RegisterClassEx(&wc);

        m_overlayHwnd = CreateWindowEx(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP,
            "GD_Matryoshka_Overlay_Class", "Matryoshka Overlay", WS_POPUP,
            0, 0, screenW, screenH, nullptr, nullptr, GetModuleHandle(nullptr), nullptr
        );
        if (!m_overlayHwnd) return false;
        ShowWindow(m_overlayHwnd, SW_SHOWNOACTIVATE);

        // 2. 注册并创建 Dual 模式 2P 独立反色渲染窗口 (CS_OWNDC 持久化 DC)
        WNDCLASSEX wcP2 = { sizeof(WNDCLASSEX) };
        wcP2.style = CS_OWNDC;
        wcP2.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
            if (msg == WM_ERASEBKGND) return 1;
            return DefWindowProc(hwnd, msg, wParam, lParam);
            };
        wcP2.hInstance = GetModuleHandle(nullptr);
        wcP2.lpszClassName = "GD_Dual_P2_Inverted_Window_Class";
        wcP2.hbrBackground = nullptr;
        RegisterClassEx(&wcP2);

        DWORD mainStyle = static_cast<DWORD>(GetWindowLong(gameHwnd, GWL_STYLE));
        DWORD mainExStyle = static_cast<DWORD>(GetWindowLong(gameHwnd, GWL_EXSTYLE));
        DWORD p2Style = (mainStyle | WS_CAPTION | WS_BORDER | WS_SYSMENU) & ~WS_VISIBLE;
        DWORD p2ExStyle = (mainExStyle | WS_EX_TOPMOST | WS_EX_NOACTIVATE);

        m_p2Hwnd = CreateWindowEx(
            p2ExStyle, "GD_Dual_P2_Inverted_Window_Class", "Geometry Dash (2P)", p2Style,
            0, 0, winW + totalBorderW, winH + totalBorderH, nullptr, nullptr, GetModuleHandle(nullptr), nullptr
        );

        if (m_p2Hwnd) {
            HICON hIcon = (HICON)SendMessage(gameHwnd, WM_GETICON, ICON_SMALL, 0);
            if (!hIcon) hIcon = (HICON)GetClassLongPtr(gameHwnd, GCLP_HICONSM);
            if (hIcon) SendMessage(m_p2Hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

            // 缓存主游戏与 2P 窗口的 DC 句柄
            m_hdcGame = GetDC(gameHwnd);
            m_hdcP2 = GetDC(m_p2Hwnd);
            if (m_hdcP2) {
                SetStretchBltMode(m_hdcP2, COLORONCOLOR);
            }
            ShowWindow(m_p2Hwnd, SW_HIDE);
        }

        // 3. 初始化 D3D11 设备与 DirectComposition 交换链
        UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
        D3D_FEATURE_LEVEL featureLevel;
        HRESULT hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, creationFlags,
            featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
            &m_d3dDevice, &featureLevel, &m_d3dContext
        );
        if (FAILED(hr)) return false;

        IDXGIDevice* dxgiDevice = nullptr;
        m_d3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
        IDXGIAdapter* dxgiAdapter = nullptr;
        dxgiDevice->GetAdapter(&dxgiAdapter);
        IDXGIFactory2* dxgiFactory = nullptr;
        dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&dxgiFactory);

        DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
        swapDesc.Width = screenW;
        swapDesc.Height = screenH;
        swapDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        swapDesc.SampleDesc.Count = 1;
        swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapDesc.BufferCount = 2;
        swapDesc.Scaling = DXGI_SCALING_STRETCH;
        swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        swapDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

        hr = dxgiFactory->CreateSwapChainForComposition(m_d3dDevice, &swapDesc, nullptr, &m_swapChain);
        SafeRelease(dxgiFactory);
        SafeRelease(dxgiAdapter);
        if (FAILED(hr)) {
            SafeRelease(dxgiDevice);
            return false;
        }

        // 4. 初始化 Direct2D 渲染上下文
        D2D1_FACTORY_OPTIONS options = {};
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &options, (void**)&m_d2dFactory);

        ID2D1Device* d2dDevice = nullptr;
        m_d2dFactory->CreateDevice(dxgiDevice, &d2dDevice);
        d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2dContext);
        SafeRelease(d2dDevice);
        SafeRelease(dxgiDevice);

        IDXGISurface* dxgiBackBuffer = nullptr;
        m_swapChain->GetBuffer(0, __uuidof(IDXGISurface), (void**)&dxgiBackBuffer);
        D2D1_BITMAP_PROPERTIES1 bitmapProps = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
        );
        m_d2dContext->CreateBitmapFromDxgiSurface(dxgiBackBuffer, &bitmapProps, &m_targetBitmap);
        SafeRelease(dxgiBackBuffer);
        m_d2dContext->SetTarget(m_targetBitmap);

        // 5. 绑定 DirectComposition 可视化树到 Overlay 窗口
        DCompositionCreateDevice(nullptr, __uuidof(IDCompositionDevice), (void**)&m_dcompDevice);
        m_dcompDevice->CreateTargetForHwnd(m_overlayHwnd, true, &m_dcompTarget);
        m_dcompDevice->CreateVisual(&m_dcompVisual);
        m_dcompVisual->SetContent(m_swapChain);
        m_dcompTarget->SetRoot(m_dcompVisual);
        m_dcompDevice->Commit();

        // 6. 创建 Wave 轨迹线描边样式 (Round Cap / Join)
        D2D1_STROKE_STYLE_PROPERTIES strokeProps = D2D1::StrokeStyleProperties(
            D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
            D2D1_LINE_JOIN_ROUND, 10.0f, D2D1_DASH_STYLE_SOLID, 0.0f
        );
        m_d2dFactory->CreateStrokeStyle(strokeProps, nullptr, 0, &m_waveStrokeStyle);

        // 7. 创建并缓存所有常用实体画刷
        auto createBrush = [this](COLORREF c) -> ID2D1SolidColorBrush* {
            ID2D1SolidColorBrush* brush = nullptr;
            m_d2dContext->CreateSolidColorBrush(ColorRefToD2D(c), &brush);
            return brush;
            };

        m_brushSolid = createBrush(g_config.colorSolidBlock);
        m_brushHazard = createBrush(g_config.colorHazardMain);
        m_brushHazardDark = createBrush(g_config.colorHazardHub);
        m_brushOutline = createBrush(RGB(20, 20, 20));
        m_brushWaveTrail = createBrush(g_config.colorWaveTrail);

        m_colYellow = createBrush(RGB(255, 220, 25));    m_colYellowCore = createBrush(RGB(255, 255, 180));
        m_colPink = createBrush(RGB(255, 85, 220));      m_colPinkCore = createBrush(RGB(255, 205, 245));
        m_colOrange = createBrush(RGB(255, 125, 20));    m_colOrangeCore = createBrush(RGB(255, 215, 140));
        m_colBlue = createBrush(RGB(35, 160, 255));      m_colBlueCore = createBrush(RGB(190, 235, 255));
        m_colGreen = createBrush(RGB(40, 230, 75));      m_colGreenCore = createBrush(RGB(180, 255, 190));
        m_colBlack = createBrush(RGB(25, 25, 25));       m_colBlackCore = createBrush(RGB(170, 170, 170));
        m_colPurple = createBrush(RGB(185, 45, 250));    m_colPurpleCore = createBrush(RGB(235, 175, 255));
        m_colWhite = createBrush(RGB(255, 255, 255));    m_colWhiteCore = createBrush(RGB(255, 255, 255));
        m_colGrey = createBrush(RGB(220, 220, 220));     m_colGreyCore = createBrush(RGB(110, 110, 110));
        m_colCyan = createBrush(RGB(30, 220, 255));      m_colCyanCore = createBrush(RGB(180, 250, 255));
        m_colAmber = createBrush(RGB(255, 175, 20));     m_colAmberCore = createBrush(RGB(255, 230, 160));
        m_colMagenta = createBrush(RGB(215, 60, 255));   m_colMagentaCore = createBrush(RGB(245, 190, 255));
        m_colRed = createBrush(RGB(255, 45, 45));        m_colRedCore = createBrush(RGB(255, 180, 180));

        initStaticGeometries();

        SetForegroundWindow(gameHwnd);
        SetFocus(gameHwnd);

        return true;
    }

    // 零分配高效 GDI 硬件反色像素拷贝（单帧同步 2P 画面）
    void renderP2Inverted(HWND gameHwnd, int w1, int h1, int w2, int h2) {
        if (!m_p2Hwnd || !IsWindowVisible(m_p2Hwnd) || !m_hdcP2) return;
        if (!m_hdcGame) {
            m_hdcGame = GetDC(gameHwnd);
            if (!m_hdcGame) return;
        }

        // 使用 NOTSRCCOPY 算子实现低开销全屏反色复制
        StretchBlt(m_hdcP2, 0, 0, w2, h2, m_hdcGame, 0, 0, w1, h1, NOTSRCCOPY);
    }

    // 动态计算 Wave 轨迹线宽（随分辨率与摄像机缩放自适应）
    void updateWavePen(float zoomScale, int screenH) {
        int baseW = std::max(g_config.waveLineMinWidth, static_cast<int>(screenH * g_config.waveLineWidthRatio));
        m_currentPenWidth = std::max(2.0f, static_cast<float>(std::round(baseW * zoomScale)));
    }

    // 帧开始：清空透明画布
    void beginFrame(int screenW, int screenH) {
        if (!m_d2dContext) return;
        m_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());
        m_d2dContext->BeginDraw();
        m_d2dContext->Clear(D2D1::ColorF(0, 0, 0, 0.0f));
    }

    // 渲染地面与天花板边界
    void renderGroundAndCeiling(cocos2d::CCNode* ground1, cocos2d::CCNode* ground2,
        cocos2d::CCNode* objectLayer, const cocos2d::CCSize& winSize,
        int screenW, int screenH) {
        if (!objectLayer || !m_d2dContext) return;
        m_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());

        // 渲染主地面 (y=90)
        if (ground1 && ground1->isVisible()) {
            float groundY = 90.0f;
            cocos2d::CCPoint groundTop = objectLayer->convertToWorldSpace(cocos2d::CCPoint(0, groundY));
            D2D1_POINT_2F scrTop = worldToScreenF(groundTop, winSize, screenW, screenH);

            D2D1_RECT_F gRect = D2D1::RectF(0.0f, scrTop.y, static_cast<float>(screenW), static_cast<float>(screenH) + 50.0f);
            m_d2dContext->FillRectangle(gRect, m_brushSolid);
            m_d2dContext->DrawLine(D2D1::Point2F(0.0f, scrTop.y), D2D1::Point2F(static_cast<float>(screenW), scrTop.y), m_brushOutline, 1.5f);
        }

        // 渲染天花板 (针对重力颠倒场景)
        if (ground2 && ground2->isVisible()) {
            cocos2d::CCPoint worldCeil = ground2->convertToWorldSpace(cocos2d::CCPoint(0, 0));
            D2D1_POINT_2F scrCeil = worldToScreenF(worldCeil, winSize, screenW, screenH);

            D2D1_RECT_F cRect = D2D1::RectF(0.0f, -50.0f, static_cast<float>(screenW), scrCeil.y);
            m_d2dContext->FillRectangle(cRect, m_brushSolid);
            m_d2dContext->DrawLine(D2D1::Point2F(0.0f, scrCeil.y), D2D1::Point2F(static_cast<float>(screenW), scrCeil.y), m_brushOutline, 1.5f);
        }
    }

    // 根据 GameObject 原生类型与 ID 精确分类
    inline FastObjectClass classifyGameObject(GameObject* obj) {
        if (!obj) return FastObjectClass::Skip;
        auto type = obj->m_objectType;
        int id = obj->m_objectID;

        // 1. 原生可破坏物体判定（直接识别所有碎石、裂纹块、Dash可撞碎物体）
        if (type == GameObjectType::Breakable) {
            return FastObjectClass::Breakable;
        }

        // 2. 原生收集品类型通配（涵盖全部金币、银币、钥匙、拾取物等）
        if (type == GameObjectType::Collectible ||
            type == GameObjectType::SecretCoin ||
            type == GameObjectType::UserCoin) {
            return FastObjectClass::Collectible;
        }

        // 3. 特殊 ID 备用补漏（经典金币、银币、骷髅币、拾取物等）
        switch (id) {
        case 22: case 1329: case 1322: // Secret Coins & User Coins
        case 1611:                     // Key
        case 1585:                     // Skull Coin
        case 1816:                     // Pickup Item
            return FastObjectClass::Collectible;
        }

        if (type == GameObjectType::Solid) return FastObjectClass::Solid;
        if (type == GameObjectType::Slope) return FastObjectClass::Slope;
        if (type == GameObjectType::Hazard || type == GameObjectType::AnimatedHazard) return FastObjectClass::Hazard;

        // 变速门、Portal、Pads、Orbs/Dash ID 匹配
        switch (id) {
        case 200: case 201: case 202: case 203: case 1334:
            return FastObjectClass::SpeedPortal;
        case 10: case 11: case 12: case 13: case 45: case 46: case 47:
        case 99: case 101: case 111: case 286: case 287: case 660:
        case 745: case 747: case 1331: case 1933: case 2064: case 2902: case 2926:
            return FastObjectClass::Portal;
        case 35: case 67: case 140: case 1332: case 3005:
            return FastObjectClass::Pad;
        case 36: case 84: case 141: case 1022: case 1330: case 1333:
        case 1594: case 1704: case 1751: case 3004: case 3027:
            return FastObjectClass::OrbOrDash;
        default:
            break;
        }

        // 过滤非物理/装饰物
        if (type == GameObjectType::Modifier || type == GameObjectType::Decoration || type == GameObjectType::CollisionObject) {
            return FastObjectClass::Skip;
        }
        return FastObjectClass::Solid;
    }

    // 核心：全图元视口剔除与 Direct2D 批量硬件加速绘制
    void renderObjects(cocos2d::CCArray* objects, cocos2d::CCNode* objectLayer,
        const cocos2d::CCSize& winSize, int screenW, int screenH, float cameraScale) {
        if (!objects || !m_d2dContext || objects->data->num == 0) return;

        // 屏幕空间与 Cocos 世界空间比例换算
        float pixelsPerPoint = static_cast<float>(screenH) / winSize.height;
        float invPixelsPerPoint = 1.0f / pixelsPerPoint;
        float halfWinW = winSize.width * 0.5f;
        float halfWinH = winSize.height * 0.5f;
        float halfScreenW = static_cast<float>(screenW) * 0.5f;
        float halfScreenH = static_cast<float>(screenH) * 0.5f;

        float layerScale = objectLayer ? std::abs(objectLayer->getScale()) : 1.0f;
        if (layerScale <= 0.001f) layerScale = 1.0f;
        float layerRotation = objectLayer ? objectLayer->getRotation() : 0.0f;

        // 逆向视口 AABB 裁剪盒（瞬时剔除 99% 的屏幕外物体）
        float marginPx = 250.0f * cameraScale;
        D2D1_POINT_2F screenCorners[4] = {
            { -marginPx, -marginPx },
            { static_cast<float>(screenW) + marginPx, -marginPx },
            { static_cast<float>(screenW) + marginPx, static_cast<float>(screenH) + marginPx },
            { -marginPx, static_cast<float>(screenH) + marginPx }
        };

        float minLocalX = 1e9f, maxLocalX = -1e9f;
        float minLocalY = 1e9f, maxLocalY = -1e9f;

        for (int i = 0; i < 4; ++i) {
            float wx = halfWinW + (screenCorners[i].x - halfScreenW) * invPixelsPerPoint;
            float wy = halfWinH - (screenCorners[i].y - halfScreenH) * invPixelsPerPoint;
            cocos2d::CCPoint localPt = objectLayer ? objectLayer->convertToNodeSpace(cocos2d::CCPoint(wx, wy)) : cocos2d::CCPoint(wx, wy);

            minLocalX = std::min(minLocalX, localPt.x);
            maxLocalX = std::max(maxLocalX, localPt.x);
            minLocalY = std::min(minLocalY, localPt.y);
            maxLocalY = std::max(maxLocalY, localPt.y);
        }

        GameObject** rawObjs = reinterpret_cast<GameObject**>(objects->data->arr);
        int totalObjs = static_cast<int>(objects->data->num);

        // 遍历所有物体并执行绘制
        for (int i = 0; i < totalObjs; ++i) {
            GameObject* obj = rawObjs[i];
            if (!obj || obj->m_isDisabled) continue;
            if (!obj->isVisible() && !obj->m_isHide) continue;

            const cocos2d::CCPoint& objPos = obj->getPosition();
            if (objPos.x < minLocalX || objPos.x > maxLocalX || objPos.y < minLocalY || objPos.y > maxLocalY) {
                continue;
            }

            FastObjectClass objClass = classifyGameObject(obj);
            if (objClass == FastObjectClass::Skip) continue;

            // 转换至当前屏幕像素坐标
            cocos2d::CCPoint worldPos = objectLayer ? objectLayer->convertToWorldSpace(objPos) : objPos;
            float sx = halfScreenW + (worldPos.x - halfWinW) * pixelsPerPoint;
            float sy = halfScreenH - (worldPos.y - halfWinH) * pixelsPerPoint;

            cocos2d::CCSize sz = obj->getContentSize();
            float w = (sz.width > 0.5f) ? sz.width : 30.0f;
            float h = (sz.height > 0.5f) ? sz.height : 30.0f;
            if (w > 500.0f || h > 500.0f) continue;

            float drawW = std::max(w, 2.0f);
            float drawH = std::max(h, 2.0f);
            float effScaleX = drawW * obj->getScaleX() * pixelsPerPoint * layerScale;
            float effScaleY = drawH * obj->getScaleY() * pixelsPerPoint * layerScale;
            float strokeW = 1.0f / std::max(0.01f, std::abs(effScaleX));

            // 构建物体局部到屏幕矩阵变换 (缩放 + 翻转 + 旋转 + 平移)
            D2D1::Matrix3x2F mat = D2D1::Matrix3x2F::Scale(effScaleX, -effScaleY)
                * D2D1::Matrix3x2F::Rotation(obj->getRotation() + layerRotation)
                * D2D1::Matrix3x2F::Translation(sx, sy);
            m_d2dContext->SetTransform(mat);

            int objID = obj->m_objectID;

            // 依类别分发图元渲染
            switch (objClass) {
            case FastObjectClass::Breakable: {
                // 可破坏方块：绿色填充 + 黑色描边
                D2D1_RECT_F blockRect = D2D1::RectF(-0.5f, -0.5f, 0.5f, 0.5f);
                m_d2dContext->FillRectangle(blockRect, m_colGreen);
                m_d2dContext->DrawRectangle(blockRect, m_brushOutline, strokeW);
                break;
            }
            case FastObjectClass::Collectible: {
                // 收集品：带黑色描边的绿色方块
                D2D1_RECT_F colRect = D2D1::RectF(-0.45f, -0.45f, 0.45f, 0.45f);
                m_d2dContext->FillRectangle(colRect, m_colGreen);
                m_d2dContext->DrawRectangle(colRect, m_brushOutline, strokeW);
                break;
            }
            case FastObjectClass::Hazard: {
                // 危险物：锯齿 / 尖刺
                bool isSaw = (obj->m_objectType == GameObjectType::AnimatedHazard || (drawW >= 32.0f && std::abs(drawW - drawH) < 8.0f));
                if (isSaw) {
                    m_d2dContext->FillGeometry(m_geoSaw, m_brushHazard);
                    m_d2dContext->DrawGeometry(m_geoSaw, m_brushOutline, strokeW);
                    m_d2dContext->FillGeometry(m_geoSawHub, m_brushHazardDark);
                    m_d2dContext->DrawGeometry(m_geoSawHub, m_brushOutline, strokeW);
                }
                else {
                    m_d2dContext->FillGeometry(m_geoSpike, m_brushHazard);
                    m_d2dContext->DrawGeometry(m_geoSpike, m_brushOutline, strokeW);
                }
                break;
            }
            case FastObjectClass::SpeedPortal: {
                // 变速门：根据倍速配置颜色与箭头数量 (0.5x=黄1, 1x=蓝1, 2x=绿2, 3x=粉3, 4x=红4)
                ID2D1SolidColorBrush* spBrush = m_colYellow;
                int arrowCount = 1;
                float dir = 1.0f;
                if (objID == 200) { spBrush = m_colYellow; arrowCount = 1; dir = -1.0f; }
                else if (objID == 201) { spBrush = m_colBlue; arrowCount = 1; }
                else if (objID == 202) { spBrush = m_colGreen; arrowCount = 2; }
                else if (objID == 203) { spBrush = m_colPink; arrowCount = 3; }
                else if (objID == 1334) { spBrush = m_colRed; arrowCount = 4; }

                float step = 1.0f / static_cast<float>(arrowCount + 1);
                float chevronScaleX = 1.15f * step;
                for (int a = 0; a < arrowCount; ++a) {
                    float offsetX = -0.5f + (a + 1) * step;
                    D2D1::Matrix3x2F arrowMat = D2D1::Matrix3x2F::Scale(dir * chevronScaleX, 0.95f)
                        * D2D1::Matrix3x2F::Translation(offsetX, 0.0f)
                        * mat;
                    m_d2dContext->SetTransform(arrowMat);
                    m_d2dContext->FillGeometry(m_geoSpeedArrow, spBrush);
                    m_d2dContext->DrawGeometry(m_geoSpeedArrow, m_brushOutline, strokeW / chevronScaleX);
                }
                break;
            }
            case FastObjectClass::Portal: {
                // 传送门色彩映射 (重力/尺寸/双人/载具形态等)
                ID2D1SolidColorBrush* pFrameBrush = m_colPurple;
                ID2D1SolidColorBrush* pBeamBrush = m_colPurpleCore;

                switch (objID) {
                case 10: case 287: case 747: case 46: // 正常重力 / 解除Dual / 传送门 / 镜像解除 (蓝)
                    pFrameBrush = m_colBlue; pBeamBrush = m_colBlueCore; break;
                case 11: case 1933:                   // 反向重力 / Swing (黄)
                    pFrameBrush = m_colYellow; pBeamBrush = m_colYellowCore; break;
                case 2926: case 12: case 99:          // 切换重力 / Cube / Normal (绿)
                    pFrameBrush = m_colGreen; pBeamBrush = m_colGreenCore; break;
                case 13:                              // Ship (粉)
                    pFrameBrush = m_colPink; pBeamBrush = m_colPinkCore; break;
                case 47: case 286: case 2902: case 2064: case 45: // Ball / Dual / 传送入口 / 传送出口 / 镜像进入 (橙)
                    pFrameBrush = m_colOrange; pBeamBrush = m_colOrangeCore; break;
                case 111:                             // UFO (琥珀橙)
                    pFrameBrush = m_colAmber; pBeamBrush = m_colAmberCore; break;
                case 660:                             // Wave (青蓝)
                    pFrameBrush = m_colCyan; pBeamBrush = m_colCyanCore; break;
                case 745:                             // Robot (白)
                    pFrameBrush = m_colWhite; pBeamBrush = m_colWhiteCore; break;
                case 101:                             // Mini (品红)
                    pFrameBrush = m_colMagenta; pBeamBrush = m_colMagentaCore; break;
                default:                              // Spider / 默认 (紫)
                    break;
                }

                // 双人/单人门绘制箭头框，常规门绘制矩形光束框
                if (objID == 286 || objID == 287) {
                    m_d2dContext->FillGeometry(m_geoDualArrowFrame, pFrameBrush);
                    m_d2dContext->DrawGeometry(m_geoDualArrowFrame, m_brushOutline, strokeW);
                    m_d2dContext->FillGeometry(m_geoDualArrowBeam, pBeamBrush);
                    m_d2dContext->DrawGeometry(m_geoDualArrowBeam, m_brushOutline, strokeW);
                }
                else {
                    m_d2dContext->FillGeometry(m_geoPortalFrame, pFrameBrush);
                    m_d2dContext->DrawGeometry(m_geoPortalFrame, m_brushOutline, strokeW);
                    m_d2dContext->FillGeometry(m_geoPortalBeam, pBeamBrush);
                    m_d2dContext->DrawGeometry(m_geoPortalBeam, m_brushOutline, strokeW);
                }
                break;
            }
            case FastObjectClass::Pad: {
                // Pads色彩映射 (粉 140, 橙 1332, 蓝 67, 紫 3005, 默认黄 35)
                ID2D1SolidColorBrush* padBrush = m_colYellow;
                if (objID == 140) padBrush = m_colPink;
                else if (objID == 1332) padBrush = m_colOrange;
                else if (objID == 67) padBrush = m_colBlue;
                else if (objID == 3005) padBrush = m_colPurple;

                m_d2dContext->FillGeometry(m_geoPad, padBrush);
                m_d2dContext->DrawGeometry(m_geoPad, m_brushOutline, strokeW);
                break;
            }
            case FastObjectClass::OrbOrDash: {
                // 跳环 Orbs 与 Dash 冲刺环色彩映射
                ID2D1SolidColorBrush* outerBrush = m_colYellow;
                ID2D1SolidColorBrush* innerBrush = m_colYellowCore;
                bool isDash = (objID == 1704 || objID == 1751); // 1704: 绿 Dash, 1751: 粉 Dash

                if (objID == 141 || objID == 1751) {             // 141: 粉 Orb / 1751: 粉 Dash
                    outerBrush = m_colPink; innerBrush = m_colPinkCore;
                }
                else if (objID == 1333) {                        // 1333: 橙Orb
                    outerBrush = m_colOrange; innerBrush = m_colOrangeCore;
                }
                else if (objID == 84) {                          // 84: 蓝Orb
                    outerBrush = m_colBlue; innerBrush = m_colBlueCore;
                }
                else if (objID == 1022 || objID == 1704) {       // 1022: 绿Orb / 1704: 绿 Dash
                    outerBrush = m_colGreen; innerBrush = m_colGreenCore;
                }
                else if (objID == 1330) {                        // 1330: 黑 Orb
                    outerBrush = m_colBlack; innerBrush = m_colBlackCore;
                }
                else if (objID == 3004) {                        // 3004: Spider Orb
                    outerBrush = m_colPurple; innerBrush = m_colPurpleCore;
                }
                else if (objID == 3027) {                        // 3027: Teleport Orb
                    outerBrush = m_colWhite; innerBrush = m_colWhiteCore;
                }
                else if (objID == 1594) {                        // 1594: Toggle Orb
                    outerBrush = m_colGrey; innerBrush = m_colGreyCore;
                }

                if (isDash) {
                    // Dash：绘制双层多边形Dash三角
                    m_d2dContext->FillGeometry(m_geoDashOuter, outerBrush);
                    m_d2dContext->DrawGeometry(m_geoDashOuter, m_brushOutline, strokeW);
                    m_d2dContext->FillGeometry(m_geoDashInner, innerBrush);
                    m_d2dContext->DrawGeometry(m_geoDashInner, m_brushOutline, strokeW);
                }
                else {
                    /// 默认: 黄Orb
                    D2D1_ELLIPSE outerEllipse = D2D1::Ellipse(D2D1::Point2F(0, 0), 0.44f, 0.44f);
                    m_d2dContext->FillEllipse(outerEllipse, outerBrush);
                    m_d2dContext->DrawEllipse(outerEllipse, m_brushOutline, strokeW);

                    D2D1_ELLIPSE innerEllipse = D2D1::Ellipse(D2D1::Point2F(0, 0), 0.20f, 0.20f);
                    m_d2dContext->FillEllipse(innerEllipse, innerBrush);
                    m_d2dContext->DrawEllipse(innerEllipse, m_brushOutline, strokeW);
                }
                break;
            }
            case FastObjectClass::Slope: {
                // 斜坡
                m_d2dContext->FillGeometry(m_geoSlope, m_brushSolid);
                m_d2dContext->DrawGeometry(m_geoSlope, m_brushSolid, strokeW);
                break;
            }
            case FastObjectClass::Solid:
            default: {
                // 标准实体矩形方块
                D2D1_RECT_F blockRect = D2D1::RectF(-0.5f, -0.5f, 0.5f, 0.5f);
                m_d2dContext->FillRectangle(blockRect, m_brushSolid);
                m_d2dContext->DrawRectangle(blockRect, m_brushSolid, strokeW);
                break;
            }
            }
        }

        m_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());
    }

    // 绘制 Wave 模式轨迹折线
    void renderWaveTrail(const std::deque<cocos2d::CCPoint>& trail, cocos2d::CCNode* objectLayer,
        const cocos2d::CCSize& winSize, int screenW, int screenH) {
        if (trail.size() < 2 || !m_d2dContext || !m_d2dFactory) return;
        m_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());

        // 投影轨迹历史点集至屏幕空间
        std::vector<D2D1_POINT_2F> pts;
        pts.reserve(trail.size());
        for (const auto& pt : trail) {
            cocos2d::CCPoint wp = objectLayer ? objectLayer->convertToWorldSpace(pt) : pt;
            pts.push_back(worldToScreenF(wp, winSize, screenW, screenH));
        }

        // 构造 PathGeometry 一次性硬件绘制
        ID2D1PathGeometry* path = nullptr;
        if (SUCCEEDED(m_d2dFactory->CreatePathGeometry(&path))) {
            ID2D1GeometrySink* sink = nullptr;
            if (SUCCEEDED(path->Open(&sink))) {
                sink->BeginFigure(pts[0], D2D1_FIGURE_BEGIN_HOLLOW);
                sink->AddLines(pts.data() + 1, static_cast<UINT32>(pts.size() - 1));
                sink->EndFigure(D2D1_FIGURE_END_OPEN);
                sink->Close();
                sink->Release();

                m_d2dContext->DrawGeometry(path, m_brushWaveTrail, m_currentPenWidth, m_waveStrokeStyle);
            }
            path->Release();
        }
    }

    // 帧呈现：结束绘制并提交交换链 (V-Sync 0 极速模式)
    void present(int screenW, int screenH) {
        if (!m_d2dContext || !m_swapChain) return;
        m_d2dContext->EndDraw();
        m_swapChain->Present(0, 0);
    }

    // 释放所有 D3D11/D2D/DComp COM 资源与窗口/DC 句柄
    void cleanup() {
        SafeRelease(m_geoSpike);
        SafeRelease(m_geoSlope);
        SafeRelease(m_geoSaw);
        SafeRelease(m_geoSawHub);
        SafeRelease(m_geoPad);
        SafeRelease(m_geoPortalFrame);
        SafeRelease(m_geoPortalBeam);
        SafeRelease(m_geoDualArrowFrame);
        SafeRelease(m_geoDualArrowBeam);
        SafeRelease(m_geoSpeedArrow);
        SafeRelease(m_geoDashOuter);
        SafeRelease(m_geoDashInner);

        SafeRelease(m_brushSolid);
        SafeRelease(m_brushHazard);
        SafeRelease(m_brushHazardDark);
        SafeRelease(m_brushOutline);
        SafeRelease(m_brushWaveTrail);

        SafeRelease(m_colYellow);  SafeRelease(m_colYellowCore);
        SafeRelease(m_colPink);    SafeRelease(m_colPinkCore);
        SafeRelease(m_colOrange);  SafeRelease(m_colOrangeCore);
        SafeRelease(m_colBlue);    SafeRelease(m_colBlueCore);
        SafeRelease(m_colGreen);   SafeRelease(m_colGreenCore);
        SafeRelease(m_colBlack);   SafeRelease(m_colBlackCore);
        SafeRelease(m_colPurple);  SafeRelease(m_colPurpleCore);
        SafeRelease(m_colWhite);   SafeRelease(m_colWhiteCore);
        SafeRelease(m_colGrey);    SafeRelease(m_colGreyCore);
        SafeRelease(m_colCyan);    SafeRelease(m_colCyanCore);
        SafeRelease(m_colAmber);   SafeRelease(m_colAmberCore);
        SafeRelease(m_colMagenta); SafeRelease(m_colMagentaCore);
        SafeRelease(m_colRed);     SafeRelease(m_colRedCore);

        SafeRelease(m_waveStrokeStyle);
        SafeRelease(m_targetBitmap);
        SafeRelease(m_d2dContext);
        SafeRelease(m_d2dFactory);

        SafeRelease(m_dcompVisual);
        SafeRelease(m_dcompTarget);
        SafeRelease(m_dcompDevice);

        SafeRelease(m_swapChain);
        SafeRelease(m_d3dContext);
        SafeRelease(m_d3dDevice);

        if (m_p2Hwnd) {
            if (m_hdcP2) {
                ReleaseDC(m_p2Hwnd, m_hdcP2);
                m_hdcP2 = nullptr;
            }
            DestroyWindow(m_p2Hwnd);
            m_p2Hwnd = nullptr;
        }

        if (m_hdcGame) {
            m_hdcGame = nullptr;
        }

        if (m_overlayHwnd) {
            DestroyWindow(m_overlayHwnd);
            m_overlayHwnd = nullptr;
        }
    }
};