/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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
**/

#pragma once

#include <array>
#include <cctype>
#include <core/JSON.h>
#include <string>
#include <syscall.h>
#include <vector>

enum LogLevel {FATAL_LEVEL = 0, ERROR_LEVEL, WARNING_LEVEL, INFO_LEVEL, DEBUG_LEVEL};

static int gDefaultLogLevel = DEBUG_LEVEL;

#ifdef __DEBUG__    // XXX: maybe use BUILD_TYPE
#define LOGTRACE(fmt, ...) do { fprintf(stderr, "[%d] TRACE [%s:%d] %s: " fmt "\n", (int)syscall(SYS_gettid), WPEFramework::Core::FileNameOnly(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__); fflush(stderr); } while (0)
#else
#define LOGTRACE(fmt, ...)
#endif
#define LOGDBG(fmt, ...) do { fprintf(stderr, "[%d] DEBUG [%s:%d] %s: " fmt "\n", (int)syscall(SYS_gettid), WPEFramework::Core::FileNameOnly(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__); fflush(stderr); } while (0)
#define LOGINFO(fmt, ...) do { fprintf(stderr, "[%d] INFO [%s:%d] %s: " fmt "\n", (int)syscall(SYS_gettid), WPEFramework::Core::FileNameOnly(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__); fflush(stderr); } while (0)
#define LOGWARN(fmt, ...) do { fprintf(stderr, "[%d] WARN [%s:%d] %s: " fmt "\n", (int)syscall(SYS_gettid), WPEFramework::Core::FileNameOnly(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__); fflush(stderr); } while (0)
#define LOGERR(fmt, ...) do { fprintf(stderr, "[%d] ERROR [%s:%d] %s: " fmt "\n", (int)syscall(SYS_gettid), WPEFramework::Core::FileNameOnly(__FILE__), __LINE__, __FUNCTION__, ##__VA_ARGS__); fflush(stderr); } while (0)

#define LOG_DEVICE_EXCEPTION0() LOGWARN("Exception caught: code=%d message=%s", err.getCode(), err.what());
#define LOG_DEVICE_EXCEPTION1(param1) LOGWARN("Exception caught" #param1 "=%s code=%d message=%s", param1.c_str(), err.getCode(), err.what());
#define LOG_DEVICE_EXCEPTION2(param1, param2) LOGWARN("Exception caught " #param1 "=%s " #param2 "=%s code=%d message=%s", param1.c_str(), param2.c_str(), err.getCode(), err.what());

namespace WPEFramework {
namespace LogSanitizer {

constexpr size_t kMaxLogPayloadLength = 200;
constexpr size_t kMaxJsonDepth = 32;

static const std::array<const char*, 9> kSensitiveJsonKeys = {
	"sat",
	"cdnaccesstoken",
	"advertising.vcid2",
	"license",
	"accesstoken",
	"access_token",
	"authorization",
	"token",
	"authtoken"
};

static const std::array<const char*, 23> kSensitiveNeedles = {
	"\"sat\"",
	"\\\"sat\\\"",
	"\"cdnaccesstoken\"",
	"\\\"cdnaccesstoken\\\"",
	"\"advertising.vcid2\"",
	"\\\"advertising.vcid2\\\"",
	"\"license\"",
	"\\\"license\\\"",
	"\"accesstoken\"",
	"\\\"accesstoken\\\"",
	"\"access_token\"",
	"\\\"access_token\\\"",
	"\"authorization\"",
	"\\\"authorization\\\"",
	"\"token\"",
	"\\\"token\\\"",
	"token=",
	"\"authtoken\"",
	"\\\"authtoken\\\"",
	"session=",
	"\"session\"",
	"\\\"session\\\"",
	"bearer "
};

struct JsonScanState {
	bool foundSensitive = false;
	bool foundOversizedString = false;
};

inline std::string ToLowerCopy(const std::string& input)
{
	std::string out = input;
	for (auto& c : out) {
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return out;
}

inline bool IsSensitiveJsonKey(const std::string& key)
{
	const std::string lowered = ToLowerCopy(key);
	for (const auto* k : kSensitiveJsonKeys) {
		if (lowered == k) {
			return true;
		}
	}

	return false;
}

inline bool ContainsSensitiveField(const std::string& input)
{
	const std::string lowered = ToLowerCopy(input);
	for (const auto* needle : kSensitiveNeedles) {
		if (lowered.find(needle) != std::string::npos) {
			return true;
		}
	}

	return false;
}

inline bool HasSensitiveDataInJson(const std::string& input)
{
	bool keySignatureFound = false;

	if (input.size() <= kMaxLogPayloadLength) {
		keySignatureFound = ContainsSensitiveField(input);

		if (!keySignatureFound) {
			return false;
		}
	}

	Core::JSON::VariantContainer root;
	if (!root.FromString(input)) {
		// Signature match in non-JSON payload should still be treated as sensitive.
		if (keySignatureFound) {
			return true;
		}
		return false;
	}

	JsonScanState state;
	struct WorkItem {
		const Core::JSON::Variant* value;
		const char* key;
		size_t depth;
	};
	std::vector<WorkItem> stack;
	stack.reserve(32);
	Core::JSON::VariantContainer::Iterator it = root.Variants();
	while (it.Next()) {
		stack.push_back(WorkItem{&(it.Current()), it.Label(), 0});
	}

	while (!stack.empty()) {
		const WorkItem current = stack.back();
		stack.pop_back();

		if (current.depth > kMaxJsonDepth || (state.foundSensitive && state.foundOversizedString)) {
			continue;
		}

		if (current.key != nullptr && !state.foundSensitive && IsSensitiveJsonKey(current.key)) {
			state.foundSensitive = true;
		}

		switch (current.value->Content()) {
		case Core::JSON::Variant::type::STRING: {
			if (!state.foundOversizedString) {
				const auto& jsonString = static_cast<const Core::JSON::String&>(*current.value);
				if (jsonString.Value().size() > kMaxLogPayloadLength) {
					state.foundOversizedString = true;
				}
			}
			break;
		}
		case Core::JSON::Variant::type::OBJECT: {
			const Core::JSON::VariantContainer& nested = current.value->Object();
			Core::JSON::VariantContainer::Iterator nestedIt = nested.Variants();
			while (nestedIt.Next()) {
				stack.push_back(WorkItem{&(nestedIt.Current()), nestedIt.Label(), current.depth + 1});
			}
			break;
		}
		case Core::JSON::Variant::type::ARRAY: {
			const Core::JSON::ArrayType<Core::JSON::Variant>& arr = current.value->Array();
			auto arrIt = arr.Elements();
			while (arrIt.Next()) {
				stack.push_back(WorkItem{&(arrIt.Current()), nullptr, current.depth + 1});
			}
			break;
		}
		default:
			break;
		}
	}

	return state.foundSensitive || state.foundOversizedString;
}

inline std::string RedactSensitiveForLog(const std::string& input)
{
	if (HasSensitiveDataInJson(input)) {
		return "[SENSITIVE_PAYLOAD_SUPPRESSED]";
	}

	return input;
}

} // namespace LogSanitizer
} // namespace WPEFramework
