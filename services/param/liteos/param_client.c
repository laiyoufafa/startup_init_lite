/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
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
#include "init_param.h"
#include "param_manager.h"
#ifdef PARAM_LOAD_CFG_FROM_CODE
#include "param_cfg.h"
#endif

#define MIN_SLEEP (100 * 1000)
static ClientWorkSpace g_clientSpace = {};
__attribute__((constructor)) static void ClientInit(void);
__attribute__((destructor)) static void ClientDeinit(void);

static int InitParamClient(void)
{
    if (PARAM_TEST_FLAG(g_clientSpace.flags, WORKSPACE_FLAGS_INIT)) {
        return 0;
    }
#ifdef __LITEOS_M__
    int ret = InitParamWorkSpace(0);
#else
    int ret = InitParamWorkSpace(1);
#endif
    PARAM_LOGV("InitParamClient");
    PARAM_CHECK(ret == 0, return -1, "Failed to init param workspace");
    PARAM_SET_FLAG(g_clientSpace.flags, WORKSPACE_FLAGS_INIT);
    // init persist to save
    InitPersistParamWorkSpace();
#ifdef PARAM_LOAD_CFG_FROM_CODE
    for (size_t i = 0; i < ARRAY_LENGTH(g_paramDefCfgNodes); i++) {
        PARAM_LOGI("InitParamClient name %s = %s", g_paramDefCfgNodes[i].name, g_paramDefCfgNodes[i].value);
        uint32_t dataIndex = 0;
        ret = WriteParam(g_paramDefCfgNodes[i].name, g_paramDefCfgNodes[i].value, &dataIndex, 0);
        PARAM_CHECK(ret == 0, continue, "Failed to set param %d name %s %s",
            ret, g_paramDefCfgNodes[i].name, g_paramDefCfgNodes[i].value);
    }
#endif
#ifdef __LITEOS_M__
    LoadParamFromBuild();
    // get persist param
    LoadPersistParams();
#endif
    return 0;
}

void ClientInit(void)
{
    PARAM_LOGV("ClientInit");
#ifndef STARTUP_INIT_TEST
    (void)InitParamClient();
#endif
}

void ClientDeinit(void)
{
    if (PARAM_TEST_FLAG(g_clientSpace.flags, WORKSPACE_FLAGS_INIT)) {
        CloseParamWorkSpace();
    }
    PARAM_SET_FLAG(g_clientSpace.flags, 0);
}

int SystemSetParameter(const char *name, const char *value)
{
    PARAM_CHECK(name != NULL && value != NULL, return -1, "Invalid name or value %s", name);
    InitParamClient();
    int ctrlService = 0;
    int ret = CheckParameterSet(name, value, GetParamSecurityLabel(), &ctrlService);
    PARAM_CHECK(ret == 0, return ret, "Forbit to set parameter %s", name);
    PARAM_LOGV("SystemSetParameter name %s value: %s ctrlService %d", name, value, ctrlService);
    if ((ctrlService & PARAM_CTRL_SERVICE) != PARAM_CTRL_SERVICE) { // ctrl param
        uint32_t dataIndex = 0;
        ret = WriteParam(name, value, &dataIndex, 0);
        PARAM_CHECK(ret == 0, return ret, "Failed to set param %d name %s %s", ret, name, value);
        ret = WritePersistParam(name, value);
        PARAM_CHECK(ret == 0, return ret, "Failed to set persist param name %s", name);
    } else {
        PARAM_LOGE("SystemSetParameter can not support service ctrl parameter name %s", name);
    }
    return ret;
}

int SystemWaitParameter(const char *name, const char *value, int32_t timeout)
{
    PARAM_CHECK(name != NULL && value != NULL, return -1, "Invalid name or value %s", name);
    long long globalCommit = 0;
    // first check permission
    ParamHandle handle = 0;
    int ret = ReadParamWithCheck(name, DAC_READ, &handle);
    if (ret != PARAM_CODE_NOT_FOUND && ret != 0 && ret != PARAM_CODE_NODE_EXIST) {
        PARAM_CHECK(ret == 0, return ret, "Forbid to wait parameters %s", name);
    }
    uint32_t diff = 0;
    time_t startTime;
    if (timeout <= 0) {
        timeout = DEFAULT_PARAM_WAIT_TIMEOUT;
    }
    (void)time(&startTime);
    do {
        long long commit = GetSystemCommitId();
        if (commit != globalCommit) {
            ParamNode *param = SystemCheckMatchParamWait(name, value);
            if (param != NULL) {
                ret = 0;
                break;
            }
        }
        globalCommit = commit;

        usleep(MIN_SLEEP);
        time_t finishTime;
        (void)time(&finishTime);
        diff = (uint32_t)difftime(finishTime, startTime);
        if (diff >= timeout) {
            ret = PARAM_CODE_TIMEOUT;
            break;
        }
    } while (1);
    PARAM_LOGI("SystemWaitParameter name %s value: %s diff %d timeout %d", name, value, diff, diff);
    return ret;
}

int WatchParamCheck(const char *keyprefix)
{
    return PARAM_CODE_NOT_SUPPORT;
}

int SystemCheckParamExist(const char *name)
{
    (void)InitParamClient();
    return SysCheckParamExist(name);
}

int SystemFindParameter(const char *name, ParamHandle *handle)
{
    (void)InitParamClient();
    PARAM_CHECK(name != NULL && handle != NULL, return -1, "The name or handle is null");
    int ret = ReadParamWithCheck(name, DAC_READ, handle);
    if (ret != PARAM_CODE_NOT_FOUND && ret != 0 && ret != PARAM_CODE_NODE_EXIST) {
        PARAM_CHECK(ret == 0, return ret, "Forbid to access parameter %s", name);
    }
    return ret;
}