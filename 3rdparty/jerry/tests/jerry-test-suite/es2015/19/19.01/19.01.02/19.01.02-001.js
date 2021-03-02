/* Copyright JS Foundation and other contributors, http://js.foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

function test_set_prototype_of_error(o, proto, msg)
{
  var name = "";

  try
  {
    Object.setPrototypeOf(o, proto);
  }
  catch (e)
  {
    name = e.name;
  }

  assert(name === "TypeError");

  if (msg)
  {
    print(msg + " PASS (XFAIL)");
  }
}

(function test_incoercible_o(undefined)
{
  test_set_prototype_of_error(undefined, new Object(), "Object.setPrototypeOf(undefined, ...)");
  test_set_prototype_of_error(null, new Object(), "Object.setPrototypeOf(null, ...)");
})();
