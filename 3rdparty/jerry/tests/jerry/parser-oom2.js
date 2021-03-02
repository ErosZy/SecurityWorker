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

/* String which is 32 bytes long. */
var str = "'\\t' +'\\t' +'\\t'+'\\t'+'\\t'+'\\t'+";

for (var i = 0; i < 10; i++) {
  str = str + str;
}

str = "(function() { return " + str + "1 })";

/* Eat memory. */
var array = [];

try
{
  for (var i = 0; i < 90; i++)
  {
    array[i] = eval(str);
  }
  assert (false);
}
catch (err)
{
  array = null;
  assert (err === null);
}
