/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <gmock/gmock.h>
#include <interfaces/ITextTrack.h>

using ::WPEFramework::Exchange::ITextTrackClosedCaptionsStyle;

class TextTrackClosedCaptionsStyleMock : public ITextTrackClosedCaptionsStyle {
public:
    TextTrackClosedCaptionsStyleMock() = default;
    virtual ~TextTrackClosedCaptionsStyleMock() = default;

    MOCK_METHOD(WPEFramework::Core::hresult, Register, (INotification* notification), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Unregister, (const INotification* notification), (override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetClosedCaptionsStyle, (const ITextTrackClosedCaptionsStyle::ClosedCaptionsStyle& style), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetClosedCaptionsStyle, (ITextTrackClosedCaptionsStyle::ClosedCaptionsStyle& style), (const, override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetFontFamily, (const ITextTrackClosedCaptionsStyle::FontFamily font), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetFontFamily, (ITextTrackClosedCaptionsStyle::FontFamily& font), (const, override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetFontSize, (const ITextTrackClosedCaptionsStyle::FontSize size), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetFontSize, (ITextTrackClosedCaptionsStyle::FontSize& size), (const, override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetFontColor, (const string& color), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetFontColor, (string& color), (const, override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetFontOpacity, (const int8_t opacity), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetFontOpacity, (int8_t& opacity), (const, override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetFontEdge, (const ITextTrackClosedCaptionsStyle::FontEdge edge), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetFontEdge, (ITextTrackClosedCaptionsStyle::FontEdge& edge), (const, override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetFontEdgeColor, (const string& color), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetFontEdgeColor, (string& color), (const, override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetBackgroundColor, (const string& color), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetBackgroundColor, (string& color), (const, override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetBackgroundOpacity, (const int8_t opacity), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetBackgroundOpacity, (int8_t& opacity), (const, override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetWindowColor, (const string& color), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetWindowColor, (string& color), (const, override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetWindowOpacity, (const int8_t opacity), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetWindowOpacity, (int8_t& opacity), (const, override));

    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void*, QueryInterface, (const uint32_t interfaceNumber), (override));
};

class TextTrackClosedCaptionsStyleNotificationMock : public ITextTrackClosedCaptionsStyle::INotification {
public:
    TextTrackClosedCaptionsStyleNotificationMock() = default;
    virtual ~TextTrackClosedCaptionsStyleNotificationMock() = default;

    MOCK_METHOD(void, OnClosedCaptionsStyleChanged, (const ITextTrackClosedCaptionsStyle::ClosedCaptionsStyle& style), (override));
    MOCK_METHOD(void, OnFontFamilyChanged, (const ITextTrackClosedCaptionsStyle::FontFamily font), (override));
    MOCK_METHOD(void, OnFontSizeChanged, (const ITextTrackClosedCaptionsStyle::FontSize size), (override));
    MOCK_METHOD(void, OnFontColorChanged, (const string& color), (override));
    MOCK_METHOD(void, OnFontOpacityChanged, (const int8_t opacity), (override));
    MOCK_METHOD(void, OnFontEdgeChanged, (const ITextTrackClosedCaptionsStyle::FontEdge edge), (override));
    MOCK_METHOD(void, OnFontEdgeColorChanged, (const string& color), (override));
    MOCK_METHOD(void, OnBackgroundColorChanged, (const string& color), (override));
    MOCK_METHOD(void, OnBackgroundOpacityChanged, (const int8_t opacity), (override));
    MOCK_METHOD(void, OnWindowColorChanged, (const string& color), (override));
    MOCK_METHOD(void, OnWindowOpacityChanged, (const int8_t opacity), (override));
};
