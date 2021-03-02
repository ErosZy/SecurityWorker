// Copyright JS Foundation and other contributors, http://js.foundation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

function f()
{
  return 1;
}

assert(typeof(a) === "undefined");
assert(typeof(null) === "object");
assert(typeof(false) === "boolean");
assert(typeof(true) === "boolean");
assert(typeof(1) === "number");
assert(typeof(1.1) === "number");
assert(typeof('abcd') === "string");
assert(typeof("1.1") === "string");
assert(typeof(this) === "object");
assert(typeof(f) === "function");
