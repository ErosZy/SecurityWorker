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

var sum = 0;
var i = 0, j = 0;
top:
        while (i++ < 10)
{
  j = 0;
  while (j++ < 20)
  {
    if (j > 9 && i % 2)
      continue top;

    sum += 1;
  }

  sum += 1;
}

assert(sum === 150);