#! /bin/bash

#******************************************************************************
#
#   Copyright (c) 2019 Intel.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
#******************************************************************************/

ulimit -c unlimited
echo 1 > /proc/sys/kernel/core_uses_pid

./build/sample-app --usecasefile ./usecase/cat_b/mu1_100mhz/3301/usecase_ru.cfg --num_eth_vfs 8 \
--vf_addr_o_xu_a "0000:17:01.0,0000:17:09.0" \
--vf_addr_o_xu_b "0000:17:11.0,0000:17:19.0" \
--vf_addr_o_xu_c "0000:65:01.0,0000:65:09.0" \
--vf_addr_o_xu_d "0000:65:01.1,0000:65:09.1"
