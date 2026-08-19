#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <Geode/Geode.hpp>
#include <Geode/utils/file.hpp>
#include <matjson.hpp>
#include <vector>
#include <filesystem>
#include <algorithm>

using namespace geode::prelude;

struct ModConfig {
    float windowScaleRatio = 0.1f;                   //窗口相对于物理屏幕分辨率的缩放基准比例
    float waveLineWidthRatio = 0.032f;                   //Wave轨迹线条粗细相对于物理屏幕高度的动态比例
    int   waveLineMinWidth = 28;                         //Wave轨迹最小保底像素宽度
    int   waveTrailMaxPoints = 800;                      //Wave轨迹队列最大保留点数
    float waveTrailMaxDist = 1200.0f;                    //Wave轨迹超出视口裁剪物理距离

    COLORREF colorSolidBlock = RGB(45, 125, 245);        //Solid主要颜色
    COLORREF colorHazardMain = RGB(245, 45, 45);         //Hazard主要颜色
    COLORREF colorHazardHub = RGB(140, 20, 20);          //Hazard内部填充颜色
    COLORREF colorWaveTrail = RGB(255, 255, 255);        //Wave轨迹颜色
};

inline ModConfig g_config;

inline void loadOrInitConfig() {
    auto configDir = Mod::get()->getConfigDir();
    std::error_code ec;
    std::filesystem::create_directories(configDir, ec);
    auto configPath = configDir / "config.json";

    if (!std::filesystem::exists(configPath)) {
        matjson::Value json;
        json["window_scale_ratio"] = g_config.windowScaleRatio;
        json["wave_line_width_ratio"] = g_config.waveLineWidthRatio;
        json["wave_line_min_width"] = g_config.waveLineMinWidth;
        json["wave_trail_max_points"] = g_config.waveTrailMaxPoints;
        json["wave_trail_max_dist"] = g_config.waveTrailMaxDist;

        matjson::Value colors;
        colors["solid_block"] = std::vector<int>{ 45, 125, 245 };
        colors["hazard_main"] = std::vector<int>{ 245, 45, 45 };
        colors["hazard_hub"] = std::vector<int>{ 140, 20, 20 };
        colors["wave_trail"] = std::vector<int>{ 255, 255, 255 };
        json["colors"] = colors;

        (void)file::writeString(configPath, json.dump());
        log::info("Generated default config.json");
    }
    else {
        auto res = file::readString(configPath);
        if (!res) return;
        auto parseRes = matjson::parse(res.unwrap());
        if (!parseRes) return;

        auto val = parseRes.unwrap();
        if (val.contains("window_scale_ratio")) {
            if (auto n = val["window_scale_ratio"].as<float>()) g_config.windowScaleRatio = n.unwrap();
        }
        if (val.contains("wave_line_width_ratio")) {
            if (auto n = val["wave_line_width_ratio"].as<float>()) g_config.waveLineWidthRatio = n.unwrap();
        }
        if (val.contains("wave_line_min_width")) {
            if (auto n = val["wave_line_min_width"].as<int>()) g_config.waveLineMinWidth = n.unwrap();
        }
        if (val.contains("wave_trail_max_points")) {
            if (auto n = val["wave_trail_max_points"].as<int>()) g_config.waveTrailMaxPoints = n.unwrap();
        }
        if (val.contains("wave_trail_max_dist")) {
            if (auto n = val["wave_trail_max_dist"].as<float>()) g_config.waveTrailMaxDist = n.unwrap();
        }

        auto parseColor = [](matjson::Value& parent, const std::string& key, COLORREF fallback) -> COLORREF {
            if (parent.contains(key)) {
                if (auto vec = parent[key].as<std::vector<int>>()) {
                    auto arr = vec.unwrap();
                    if (arr.size() >= 3) {
                        return RGB(std::clamp(arr[0], 0, 255), std::clamp(arr[1], 0, 255), std::clamp(arr[2], 0, 255));
                    }
                }
            }
            return fallback;
            };

        if (val.contains("colors")) {
            auto cols = val["colors"];
            g_config.colorSolidBlock = parseColor(cols, "solid_block", g_config.colorSolidBlock);
            g_config.colorHazardMain = parseColor(cols, "hazard_main", g_config.colorHazardMain);
            g_config.colorHazardHub = parseColor(cols, "hazard_hub", g_config.colorHazardHub);
            g_config.colorWaveTrail = parseColor(cols, "wave_trail", g_config.colorWaveTrail);
        }
    }
}