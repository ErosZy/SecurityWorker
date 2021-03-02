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

#include "ecma-alloc.h"
#include "ecma-conversion.h"
#include "ecma-helpers.h"
#include "ecma-try-catch-macro.h"
#include "opcodes.h"

/** \addtogroup vm Virtual machine
 * @{
 *
 * \addtogroup vm_opcodes Opcodes
 * @{
 */

/**
 * Perform ECMA number logic operation.
 *
 * The algorithm of the operation is following:
 *   leftNum = ToNumber (leftValue);
 *   rightNum = ToNumber (rightValue);
 *   result = leftNum BitwiseLogicOp rightNum;
 *
 * @return ecma value
 *         Returned value must be freed with ecma_free_value
 */
ecma_value_t
do_number_bitwise_logic (number_bitwise_logic_op op, /**< number bitwise logic operation */
                         ecma_value_t left_value, /**< left value */
                         ecma_value_t right_value) /**< right value */
{
  JERRY_ASSERT (!ECMA_IS_VALUE_ERROR (left_value)
                && !ECMA_IS_VALUE_ERROR (right_value));

  ecma_value_t ret_value = ECMA_VALUE_EMPTY;

  ECMA_OP_TO_NUMBER_TRY_CATCH (num_left, left_value, ret_value);
  ECMA_OP_TO_NUMBER_TRY_CATCH (num_right, right_value, ret_value);

  ecma_number_t result = ECMA_NUMBER_ZERO;
  uint32_t right_uint32 = ecma_number_to_uint32 (num_right);

  switch (op)
  {
    case NUMBER_BITWISE_LOGIC_AND:
    {
      uint32_t left_uint32 = ecma_number_to_uint32 (num_left);
      result = (ecma_number_t) ((int32_t) (left_uint32 & right_uint32));
      break;
    }
    case NUMBER_BITWISE_LOGIC_OR:
    {
      uint32_t left_uint32 = ecma_number_to_uint32 (num_left);
      result = (ecma_number_t) ((int32_t) (left_uint32 | right_uint32));
      break;
    }
    case NUMBER_BITWISE_LOGIC_XOR:
    {
      uint32_t left_uint32 = ecma_number_to_uint32 (num_left);
      result = (ecma_number_t) ((int32_t) (left_uint32 ^ right_uint32));
      break;
    }
    case NUMBER_BITWISE_SHIFT_LEFT:
    {
      result = (ecma_number_t) (ecma_number_to_int32 (num_left) << (right_uint32 & 0x1F));
      break;
    }
    case NUMBER_BITWISE_SHIFT_RIGHT:
    {
      result = (ecma_number_t) (ecma_number_to_int32 (num_left) >> (right_uint32 & 0x1F));
      break;
    }
    case NUMBER_BITWISE_SHIFT_URIGHT:
    {
      uint32_t left_uint32 = ecma_number_to_uint32 (num_left);
      result = (ecma_number_t) (left_uint32 >> (right_uint32 & 0x1F));
      break;
    }
    case NUMBER_BITWISE_NOT:
    {
      result = (ecma_number_t) ((int32_t) ~right_uint32);
      break;
    }
  }

  ret_value = ecma_make_number_value (result);

  ECMA_OP_TO_NUMBER_FINALIZE (num_right);
  ECMA_OP_TO_NUMBER_FINALIZE (num_left);

  return ret_value;
} /* do_number_bitwise_logic */

/**
 * @}
 * @}
 */
