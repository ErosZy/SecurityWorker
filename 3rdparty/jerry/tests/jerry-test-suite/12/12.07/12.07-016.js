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

var o = {p1: 1, p2: 2, p3: {p1: 100, p2: 200}, p4: 4, p5: 5}, sum = 0;
for (var p in o)
{
  if (p === "p2")
    continue;

  if (typeof (o[p]) === "object")
  {
    for (var pp in o[p])
    {
      if (pp === "p2")
        continue;

      sum += o[p][pp];
    }
  }
  else {
    sum += o[p];
  }
}

assert(sum === 110);
