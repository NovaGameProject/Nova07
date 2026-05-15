// Nova Game Engine
// Copyright (C) 2026  brambora69123
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#pragma once
#include <string>
#include <variant>
#include <optional>
#include <cstdint>
#include "Common/MathTypes.hpp"

namespace Nova {
    // Wrapper types to distinguish Vector3 and Color3 in variant
    struct Color3Value {
        float r, g, b;
        Color3Value() : r(1), g(1), b(1) {}
        Color3Value(float r, float g, float b) : r(r), g(g), b(b) {}
        Color3Value(const Color3& c) : r(c.r), g(c.g), b(c.b) {}
        Color3 to_glm() const { return Color3(r, g, b); }
    };

    struct PropertyValue {
        enum class Kind { Nil, Bool, Int, Float, String, Vector3, CFrame, Color3 };

        using Storage = std::variant<
            std::nullopt_t,
            bool,
            int64_t,
            double,
            std::string,
            Vector3,
            CFrame,
            Color3Value
        >;

        Storage storage;
        Kind kind;

        PropertyValue() : storage(std::nullopt), kind(Kind::Nil) {}
        PropertyValue(bool v) : storage(v), kind(Kind::Bool) {}
        PropertyValue(int64_t v) : storage(v), kind(Kind::Int) {}
        PropertyValue(int v) : storage(static_cast<int64_t>(v)), kind(Kind::Int) {}
        PropertyValue(double v) : storage(v), kind(Kind::Float) {}
        PropertyValue(float v) : storage(static_cast<double>(v)), kind(Kind::Float) {}
        PropertyValue(const std::string& v) : storage(v), kind(Kind::String) {}
        PropertyValue(const char* v) : storage(std::string(v)), kind(Kind::String) {}
        PropertyValue(const Vector3& v) : storage(v), kind(Kind::Vector3) {}
        PropertyValue(const CFrame& v) : storage(v), kind(Kind::CFrame) {}
        // Color3 must be constructed via the static factory to avoid ambiguity with Vector3
        static PropertyValue FromColor3(const Color3& v) { return PropertyValue(Color3Value(v), Kind::Color3); }

        bool isNil() const { return kind == Kind::Nil; }
        bool isBool() const { return kind == Kind::Bool; }
        bool isInt() const { return kind == Kind::Int; }
        bool isFloat() const { return kind == Kind::Float; }
        bool isNumber() const { return kind == Kind::Int || kind == Kind::Float; }
        bool isString() const { return kind == Kind::String; }
        bool isVector3() const { return kind == Kind::Vector3; }
        bool isCFrame() const { return kind == Kind::CFrame; }
        bool isColor3() const { return kind == Kind::Color3; }

        bool toBool() const { return std::get<bool>(storage); }
        int64_t toInt() const { return std::get<int64_t>(storage); }
        double toFloat() const { return std::get<double>(storage); }
        double toNumber() const {
            if (kind == Kind::Int) return static_cast<double>(std::get<int64_t>(storage));
            return std::get<double>(storage);
        }
        const std::string& toString() const { return std::get<std::string>(storage); }
        const Vector3& toVector3() const { return std::get<Vector3>(storage); }
        const CFrame& toCFrame() const { return std::get<CFrame>(storage); }
        Color3 toColor3() const { return std::get<Color3Value>(storage).to_glm(); }

    private:
        PropertyValue(Color3Value v, Kind k) : storage(v), kind(k) {}
    };
}
