/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

#include <gtest/gtest.h>

#include "AppGateway.h"

using namespace WPEFramework;
using namespace WPEFramework::Plugin;

namespace {
class TestAppGateway : public AppGateway {
public:
    void AddRef() const override {}
    uint32_t Release() const override { return Core::ERROR_NONE; }
};
} // namespace

TEST(AppGatewayTest, Information_DefaultIsEmpty)
{
    TestAppGateway plugin;
    EXPECT_EQ(std::string(), plugin.Information());
}

TEST(AppGatewayTest, ConstructAndDestroy_NoCrash)
{
    EXPECT_NO_THROW({
        TestAppGateway plugin;
    });
}
