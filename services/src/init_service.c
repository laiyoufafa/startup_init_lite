/*
 * Copyright (c) 2020 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "init_service.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "init_adapter.h"
#include "init_cmds.h"
#include "init_log.h"
#include "init_perms.h"
#include "init_service_socket.h"

#define CAP_NUM 2

// 240 seconds, 4 minutes
static const int CRASH_TIME_LIMIT  = 240;
// maximum number of crashes within time CRASH_TIME_LIMIT for one service
static const int CRASH_COUNT_LIMIT = 4;

// 240 seconds, 4 minutes
static const int CRITICAL_CRASH_TIME_LIMIT  = 240;
// maximum number of crashes within time CRITICAL_CRASH_TIME_LIMIT for one service
static const int CRITICAL_CRASH_COUNT_LIMIT = 4;
static const int MAX_PID_STRING_LENGTH = 50;


static int SetAllAmbientCapability()
{
    for (int i = 0; i <= CAP_LAST_CAP; ++i) {
        if (SetAmbientCapability(i) != 0) {
            return SERVICE_FAILURE;
        }
    }
    return SERVICE_SUCCESS;
}

static int SetPerms(const Service *service)
{
    if (KeepCapability() != 0) {
        return SERVICE_FAILURE;
    }

    if (setgroups(service->servPerm.gIDCnt, service->servPerm.gIDArray) != 0) {
        INIT_LOGE("[Init] SetPerms, setgroups failed. errno = %d, gIDCnt=%d\n", errno, service->servPerm.gIDCnt);
        return SERVICE_FAILURE;
    }

    if (service->servPerm.uID != 0) {
        if (setuid(service->servPerm.uID) != 0) {
            printf("[Init] setuid of service: %s failed, uid = %d\n", service->name, service->servPerm.uID);
            return SERVICE_FAILURE;
        }
    }

    // umask call always succeeds and return the previous mask value which is not needed here
    (void)umask(DEFAULT_UMASK_INIT);

    struct __user_cap_header_struct capHeader;
    capHeader.version = _LINUX_CAPABILITY_VERSION_3;
    capHeader.pid = 0;

    struct __user_cap_data_struct capData[CAP_NUM] = {};
    for (unsigned int i = 0; i < service->servPerm.capsCnt; ++i) {
        if (service->servPerm.caps[i] == FULL_CAP) {
            for (int i = 0; i < CAP_NUM; ++i) {
                capData[i].effective = FULL_CAP;
                capData[i].permitted = FULL_CAP;
                capData[i].inheritable = FULL_CAP;
            }
            break;
        }
        capData[CAP_TO_INDEX(service->servPerm.caps[i])].effective |= CAP_TO_MASK(service->servPerm.caps[i]);
        capData[CAP_TO_INDEX(service->servPerm.caps[i])].permitted |= CAP_TO_MASK(service->servPerm.caps[i]);
        capData[CAP_TO_INDEX(service->servPerm.caps[i])].inheritable |= CAP_TO_MASK(service->servPerm.caps[i]);
    }

    if (capset(&capHeader, capData) != 0) {
        printf("[Init][Debug], capset faild for service: %s, error: %d\n", service->name, errno);
        return SERVICE_FAILURE;
    }
    for (unsigned int i = 0; i < service->servPerm.capsCnt; ++i) {
        if (service->servPerm.caps[i] == FULL_CAP) {
            return SetAllAmbientCapability();
        }
        if (SetAmbientCapability(service->servPerm.caps[i]) != 0) {
            printf("[Init][Debug], SetAmbientCapability faild for service: %s\n", service->name);
            return SERVICE_FAILURE;
        }
    }
    return SERVICE_SUCCESS;
}

int ServiceStart(Service *service)
{
    if (service == NULL) {
        printf("[Init] start service failed! null ptr.\n");
        return SERVICE_FAILURE;
    }

    if (service->attribute & SERVICE_ATTR_INVALID) {
        printf("[Init] start service %s invalid.\n", service->name);
        return SERVICE_FAILURE;
    }

    struct stat pathStat = {0};
    service->attribute &= (~(SERVICE_ATTR_NEED_RESTART | SERVICE_ATTR_NEED_STOP));
    if (stat(service->pathArgs[0], &pathStat) != 0) {
        service->attribute |= SERVICE_ATTR_INVALID;
        printf("[Init] start service %s invalid, please check %s.\n",\
            service->name, service->pathArgs[0]);
        return SERVICE_FAILURE;
    }
    int ret = 0;
    int pid = fork();
    if (pid == 0) {
        if (service->socketCfg != NULL) {    // start socket service
            printf("[init] Create socket \n");
            ret = DoCreateSocket(service->socketCfg);
            if (ret < 0) {
                return SERVICE_FAILURE;
            }
        }
        // permissions
        if (SetPerms(service) != SERVICE_SUCCESS) {
            INIT_LOGE("[Init] service %s exit! set perms failed! err %d.\n", service->name, errno);
            _exit(0x7f); // 0x7f: user specified
        }
        char pidString[MAX_PID_STRING_LENGTH];          // writepid
        pid_t childPid = getpid();
        if (snprintf(pidString, MAX_PID_STRING_LENGTH, "%d", childPid) <= 0) {
            INIT_LOGE("[Init] start service writepid sprintf failed.\n");
            return SERVICE_FAILURE;
        }
        for (int i = 0; i < MAX_WRITEPID_FILES; i++) {
            if (service->writepidFiles[i] == NULL) {
                continue;
            }
            FILE *fd = fopen(service->writepidFiles[i], "wb");
            if (fd == NULL) {
                INIT_LOGE("[Init] start service writepidFiles %s invalid.\n", service->writepidFiles[i]);
                continue;
            }
            if (fwrite(pidString, 1, strlen(pidString), fd) != strlen(pidString)) {
                 INIT_LOGE("[Init] start service writepid error.file:%s pid:%s\n", service->writepidFiles[i], pidString);
            }
            fclose(fd);
            printf("[Init] ServiceStart writepid filename=%s, childPid=%s, ok\n", service->writepidFiles[i], pidString);
        }

        printf("[init] service->name is %s \n", service->name);
#ifndef OHOS_LITE
         // L2 Can not be reset env
         if (execv(service->pathArgs[0], service->pathArgs) != 0) {
            printf("[Init] service %s execve failed! err %d.\n", service->name, errno);
         }
#else
        char* env[] = {"LD_LIBRARY_PATH=/storage/app/libs", NULL};
        if (execve(service->pathArgs[0], service->pathArgs, env) != 0) {
            printf("[Init] service %s execve failed! err %d.\n", service->name, errno);
        }
#endif

        _exit(0x7f); // 0x7f: user specified
    } else if (pid < 0) {
        printf("[Init] start service %s fork failed!\n", service->name);
        return SERVICE_FAILURE;
    }

    service->pid = pid;
    printf("[Init] start service %s succeed, pid %d.\n", service->name, service->pid);
    return SERVICE_SUCCESS;
}

int ServiceStop(Service *service)
{
    if (service == NULL) {
        printf("[Init] stop service failed! null ptr.\n");
        return SERVICE_FAILURE;
    }

    service->attribute &= ~SERVICE_ATTR_NEED_RESTART;
    service->attribute |= SERVICE_ATTR_NEED_STOP;
    if (service->pid <= 0) {
        return SERVICE_SUCCESS;
    }

    if (kill(service->pid, SIGKILL) != 0) {
        printf("[Init] stop service %s pid %d failed! err %d.\n", service->name, service->pid, errno);
        return SERVICE_FAILURE;
    }

    printf("[Init] stop service %s, pid %d.\n", service->name, service->pid);
    return SERVICE_SUCCESS;
}

// the service need to be restarted, if it crashed more than 4 times in 4 minutes
void CheckCritical(Service *service)
{
    if (service->attribute & SERVICE_ATTR_CRITICAL) {            // critical
        // crash time and count check
        time_t curTime = time(NULL);
        if (service->criticalCrashCnt == 0) {
            service->firstCriticalCrashTime = curTime;
            ++service->criticalCrashCnt;
        } else if (difftime(curTime, service->firstCriticalCrashTime) > CRITICAL_CRASH_TIME_LIMIT) {
            service->firstCriticalCrashTime = curTime;
            service->criticalCrashCnt = 1;
        } else {
            ++service->criticalCrashCnt;
            if (service->criticalCrashCnt > CRITICAL_CRASH_COUNT_LIMIT) {
                INIT_LOGE("[Init] reap critical service %s, crash too many times! Need reboot!\n", service->name);
                RebootSystem();
            }
        }
    }
}

static int ExecRestartCmd(const Service *service)
{
    printf("[init] ExecRestartCmd \n");
    if ((service == NULL) || (service->onRestart == NULL) || (service->onRestart->cmdLine == NULL)) {
        return SERVICE_FAILURE;
    }

    for (int i = 0; i < service->onRestart->cmdNum; i++) {
        printf("[init] SetOnRestart cmdLine->name %s  cmdLine->cmdContent %s \n", service->onRestart->cmdLine[i].name,
            service->onRestart->cmdLine[i].cmdContent);
        DoCmd(&service->onRestart->cmdLine[i]);
    }
    free(service->onRestart->cmdLine);
    free(service->onRestart);
    return SERVICE_SUCCESS;
}

void ServiceReap(Service *service)
{
    if (service == NULL) {
        printf("[Init] reap service failed! null ptr.\n");
        return;
    }

    service->pid = -1;
    // stopped by system-init itself, no need to restart even if it is not one-shot service
    if (service->attribute & SERVICE_ATTR_NEED_STOP) {
        service->attribute &= (~SERVICE_ATTR_NEED_STOP);
        service->crashCnt = 0;
        return;
    }

    // for one-shot service
    if (service->attribute & SERVICE_ATTR_ONCE) {
        // no need to restart
        if (!(service->attribute & SERVICE_ATTR_NEED_RESTART)) {
            service->attribute &= (~SERVICE_ATTR_NEED_STOP);
            return;
        }
        // the service could be restart even if it is one-shot service
    }

    // the service that does not need to be restarted restarts, indicating that it has crashed
    if (!(service->attribute & SERVICE_ATTR_NEED_RESTART)) {
        // crash time and count check
        time_t curTime = time(NULL);
        if (service->crashCnt == 0) {
            service->firstCrashTime = curTime;
            ++service->crashCnt;
        } else if (difftime(curTime, service->firstCrashTime) > CRASH_TIME_LIMIT) {
            service->firstCrashTime = curTime;
            service->crashCnt = 1;
        } else {
            ++service->crashCnt;
            if (service->crashCnt > CRASH_COUNT_LIMIT) {
                printf("[Init] reap service %s, crash too many times!\n", service->name);
                return;
            }
        }
    }

    CheckCritical(service);
    int ret = 0;
    if (service->onRestart != NULL) {
        ret = ExecRestartCmd(service);
        if (ret != SERVICE_SUCCESS) {
            printf("[Init] SetOnRestart fail \n");
        }
    }
    ret = ServiceStart(service);
    if (ret != SERVICE_SUCCESS) {
        INIT_LOGE("[Init] reap service %s start failed!\n", service->name);
    }

    service->attribute &= (~SERVICE_ATTR_NEED_RESTART);
}

