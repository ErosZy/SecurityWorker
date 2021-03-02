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

var x = [0];
assert (x == x);
assert (x == !x);
assert (Array(3) == ",,");

assert ([] + [] == "");
assert ([] + {} == "[object Object]");
assert (eval ("{} + []") == 0);
assert (isNaN (eval ("{} + {}")));
assert ({} + [] == "[object Object]");
assert ({} + {} == "[object Object][object Object]");

assert ((! + [] + [] + ![]) === "truefalse");
