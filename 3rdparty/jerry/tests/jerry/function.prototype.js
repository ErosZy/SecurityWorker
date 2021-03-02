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

function f (arg1, arg2, arg3)
{
  this.string = arg1;
  this.number = arg2;
  this.boolean = arg3;
}

var this_arg = {};

f.call (this_arg, 's', 1, true, null);

assert (this_arg.string  === 's');
assert (this_arg.number  === 1);
assert (this_arg.boolean === true);
