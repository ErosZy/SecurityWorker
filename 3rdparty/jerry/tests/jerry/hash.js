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

a = {};
a['12345']=1;
a['13345']=3;
a['sss45']=4;
a['1'] = 2;

assert (a[12345] === 1);
assert (a[1] === 2);
assert (a[13345] === 3);
assert (a['sss45'] === 4);
