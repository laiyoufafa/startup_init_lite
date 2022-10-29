/*
* Copyright (c) 2022 Huawei Device Co., Ltd.
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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "begetctl.h"
#include "init_utils.h"
#include "init_param.h"
#include "securec.h"
#include "shell_utils.h"
#include "parameter.h"

#define BEGETCTRL_INIT_CMD "ohos.servicectrl.cmd"

/**
 * @brief 通用的命令通道，注册函数完成参数校验，调用HandleCmd进行处理
 * init进程使用 initCmd 按命令字进行处理
 */
static int HandleCmd(BShellHandle shell, const char *cmdName, int argc, char **argv)
{
    BSH_CHECK(shell != NULL, return BSH_INVALID_PARAM, "Invalid shell env");
    BSH_LOGI("initCmd %s argc %d", cmdName, argc);
    // format args
    if (argc == 0) {
        return SystemSetParameter(BEGETCTRL_INIT_CMD, cmdName);
    }
    char value[PARAM_VALUE_LEN_MAX] = { 0 };
    int ret = sprintf_s(value, sizeof(value) - 1, "%s ", cmdName);
    BSH_CHECK(ret > 0, return BSH_INVALID_PARAM, "Failed to format cmdName");
    for (int i = 0; i < argc; i++) {
        ret = strcat_s(value, sizeof(value), argv[i]);
        BSH_CHECK(ret == 0, return BSH_INVALID_PARAM, "Failed to format name");
        ret = strcat_s(value, sizeof(value), " ");
        BSH_CHECK(ret == 0, return BSH_INVALID_PARAM, "Failed to format name");
    }
    return SystemSetParameter(BEGETCTRL_INIT_CMD, value);
}

static int SetInitLogLevelFromParam(BShellHandle shell, int argc, char **argv)
{
    if (argc != 2) { // 2 is set log level parameter number
        char *helpArgs[] = {"set", NULL};
        BShellCmdHelp(shell, 1, helpArgs);
        return 0;
    }
    const char *LOG_LEVEL_STR[] = { "DEBUG", "INFO", "WARNING", "ERROR", "FATAL" };
    errno = 0;
    unsigned int level = strtoul(argv[1], 0, 10); // 10 is decimal
    if (errno != 0) {
        printf("Failed to transform %s to unsigned int. \n", argv[1]);
        return -1;
    }
    if ((level >= INIT_DEBUG) && (level <= INIT_FATAL)) {
        int ret = HandleCmd(shell, "setloglevel", argc - 1, &argv[1]);
        if (ret != 0) {
            printf("Failed to set log level %s. \n", LOG_LEVEL_STR[level]);
        } else {
            printf("Success to set log level %s. \n", LOG_LEVEL_STR[level]);
        }
    } else {
        printf("%s is invalid. \n", argv[1]);
    }
    return 0;
}

static int32_t GetInitLogLevelFromParam(BShellHandle shell, int argc, char **argv)
{
    const char *LOG_LEVEL_STR[] = { "DEBUG", "INFO", "WARNING", "ERROR", "FATAL" };
    int level = GetIntParameter(INIT_DEBUG_LEVEL, (int)INIT_ERROR);
    if ((level >= INIT_DEBUG) && (level <= INIT_FATAL)) {
        printf("Init log level: %s \n", LOG_LEVEL_STR[level]);
    }
    return 0;
}

MODULE_CONSTRUCTOR(void)
{
    const CmdInfo infos[] = {
        {"set", SetInitLogLevelFromParam,
            "set init log level 0:debug, 1:info, 2:warning, 3:err, 4:fatal", "set log level", "set log level"},
        {"get", GetInitLogLevelFromParam, "get init log level", "get log level", "get log level"},
    };
    for (size_t i = 0; i < sizeof(infos) / sizeof(infos[0]); i++) {
        BShellEnvRegisterCmd(GetShellHandle(), &infos[i]);
    }
}