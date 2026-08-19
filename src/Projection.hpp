#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <cocos2d.h>
#include <Geode/Geode.hpp>
#include <cmath>
#include <climits>

using namespace geode::prelude;

// 1. 整数坐标转换（保留给 SetWindowPos 等 Win32 API 使用）
inline POINT worldToScreen(const cocos2d::CCPoint& wp, const cocos2d::CCSize& winSize, int screenW, int screenH) {
    float pixelsPerPoint = static_cast<float>(screenH) / winSize.height;

    float cxCocos = winSize.width * 0.5f;
    float cyCocos = winSize.height * 0.5f;
    float cxScreen = static_cast<float>(screenW) * 0.5f;
    float cyScreen = static_cast<float>(screenH) * 0.5f;

    float sx = cxScreen + (wp.x - cxCocos) * pixelsPerPoint;
    float sy = cyScreen - (wp.y - cyCocos) * pixelsPerPoint;

    POINT pt;
    pt.x = static_cast<LONG>(std::lround(sx));
    pt.y = static_cast<LONG>(std::lround(sy));
    return pt;
}

// 2. Direct2D 亚像素高精度浮点转换
inline D2D1_POINT_2F worldToScreenF(const cocos2d::CCPoint& wp, const cocos2d::CCSize& winSize, int screenW, int screenH) {
    float pixelsPerPoint = static_cast<float>(screenH) / winSize.height;

    float cxCocos = winSize.width * 0.5f;
    float cyCocos = winSize.height * 0.5f;
    float cxScreen = static_cast<float>(screenW) * 0.5f;
    float cyScreen = static_cast<float>(screenH) * 0.5f;

    float sx = cxScreen + (wp.x - cxCocos) * pixelsPerPoint;
    float sy = cyScreen - (wp.y - cyCocos) * pixelsPerPoint;

    return D2D1::Point2F(sx, sy);
}
