/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "systemgetparametervalue_fuzzer.h"
#include "init_param.h"
#include "fuzz_utils.h"

namespace OHOS {
    bool FuzzSystemGetParameterValue(const uint8_t* data, size_t size)
    {
        bool result = false;
        char buffer[PARAM_CONST_VALUE_LEN_MAX] = {0};
        uint32_t len = PARAM_CONST_VALUE_LEN_MAX;
        char *rest = nullptr;
        ParamHandle handle = (ParamHandle)strtoul(reinterpret_cast<char *>(const_cast<uint8_t *>(data)), &rest, BASE);
        if (!SystemGetParameterValue(handle, buffer, &len)) {
            result = true;
        }
        return result;
    }
}

/* Fuzzer entry point */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Run your code on data */
    OHOS::FuzzSystemGetParameterValue(data, size);
    return 0;
}
