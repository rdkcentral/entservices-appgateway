###
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2024 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###

# Toolchain snippet used by CI to enable gcov instrumentation.
# We must set both compile and link flags; otherwise .gcno/.gcda may not be emitted
# (or lcov capture will be empty).
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g --coverage -fprofile-arcs -ftest-coverage")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g --coverage -fprofile-arcs -ftest-coverage")

# Ensure all binaries and shared objects link with gcov runtime.
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --coverage")
