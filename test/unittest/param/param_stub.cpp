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

#include "param_stub.h"
#include <dirent.h>
#include "beget_ext.h"
#include "init_param.h"
#include "param_manager.h"
#include "param_security.h"
#include "param_utils.h"
#include "init_group_manager.h"
#ifdef PARAM_LOAD_CFG_FROM_CODE
#include "param_cfg.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif
int LoadParamFromCmdLine(void);
static int g_testPermissionResult = DAC_RESULT_PERMISSION;
void SetTestPermissionResult(int result)
{
    g_testPermissionResult = result;
}

static const char *selinuxLabels[][2] = {
    {"test.permission.read", "test.persmission.read"},
    {"test.permission.write", "test.persmission.write"},
    {"test.permission.watch", "test.persmission.watch"}
};

static int TestGenHashCode(const char *buff)
{
    int code = 0;
    size_t buffLen = strlen(buff);
    for (size_t i = 0; i < buffLen; i++) {
        code += buff[i] - 'A';
    }
    return code;
}

static void TestSetSelinuxLogCallback(void) {}

static const char *forbitWriteParamName[] = {
    "ohos.servicectrl.",
    "test.permission.read",
    "test.persmission.watch"
};

static int TestSetParamCheck(const char *paraName, struct ucred *uc)
{
    // forbid to read ohos.servicectrl.
    for (size_t i = 0; i < ARRAY_LENGTH(forbitWriteParamName); i++) {
        if (strncmp(paraName, forbitWriteParamName[i], strlen(forbitWriteParamName[i])) == 0) {
            return 1;
        }
    }
    return g_testPermissionResult;
}

static const char *TestGetParamLabel(const char *paraName)
{
    BEGET_LOGI("TestGetParamLabel %s", paraName);
    for (size_t i = 0; i < ARRAY_LENGTH(selinuxLabels); i++) {
        if (strncmp(selinuxLabels[i][0], paraName, strlen(selinuxLabels[i][0])) == 0) {
            return selinuxLabels[i][1];
        }
    }
    int code = TestGenHashCode(paraName);
    code = code % (ARRAY_LENGTH(selinuxLabels));
    return selinuxLabels[code][1];
}

static const char *forbitReadParamName[] = {
    "ohos.servicectrl.",
    // "test.permission.write",
};
static int TestReadParamCheck(const char *paraName)
{
    // forbid to read ohos.servicectrl.
    for (size_t i = 0; i < ARRAY_LENGTH(forbitReadParamName); i++) {
        if (strncmp(paraName, forbitReadParamName[i], strlen(forbitReadParamName[i])) == 0) {
            return 1;
        }
    }
    return g_testPermissionResult;
}
static void TestDestroyParamList(ParamContextsList **list)
{
#ifdef PARAM_SUPPORT_SELINUX
    ParamContextsList *head = *list;
    while (head != nullptr) {
        ParamContextsList *next = head->next;
        free(head->info.paraName);
        free(head->info.paraContext);
        free(head);
        head = next;
    }
#endif
}
static ParamContextsList *TestGetParamList(void)
{
#ifdef PARAM_SUPPORT_SELINUX
    ParamContextsList *head = (ParamContextsList *)malloc(sizeof(ParamContextsList));
    BEGET_ERROR_CHECK(head != nullptr, return nullptr, "Failed to alloc ParamContextsList");
    head->info.paraName = strdup(selinuxLabels[0][0]);
    head->info.paraContext = strdup(selinuxLabels[0][1]);
    head->next = nullptr;
    for (size_t i = 1; i < ARRAY_LENGTH(selinuxLabels); i++) {
        ParamContextsList *node = (ParamContextsList *)malloc(sizeof(ParamContextsList));
        BEGET_ERROR_CHECK(node != nullptr, TestDestroyParamList(&head);
            return nullptr, "Failed to alloc ParamContextsList");
        node->info.paraName = strdup(selinuxLabels[i][0]);
        node->info.paraContext = strdup(selinuxLabels[i][1]);
        node->next = head->next;
        head->next = node;
    }
    return head;
#else
    return nullptr;
#endif
}

void TestSetSelinuxOps(void)
{
    SelinuxSpace space = {};
    space.setSelinuxLogCallback = TestSetSelinuxLogCallback;
    space.setParamCheck = TestSetParamCheck;
    space.getParamLabel = TestGetParamLabel;
    space.readParamCheck = TestReadParamCheck;
    space.getParamList = TestGetParamList;
    space.destroyParamList = TestDestroyParamList;
#ifdef PARAM_SUPPORT_SELINUX
    SetSelinuxOps(&space);
#endif
}
static void CreateTestFile(const char *fileName, const char *data)
{
    CheckAndCreateDir(fileName);
    PARAM_LOGV("PrepareParamTestData for %s", fileName);
    FILE *tmpFile = fopen(fileName, "wr");
    if (tmpFile != nullptr) {
        fprintf(tmpFile, "%s", data);
        (void)fflush(tmpFile);
        fclose(tmpFile);
    }
}
static void PrepareUeventdcfg(void)
{
    const char *ueventdcfg = "[device]\n"
        "/dev/test 0666 1000 1000\n"
        "[device]\n"
        "/dev/test1 0666 1000\n"
        "[device]\n"
        "/dev/test2 0666 1000 1000 1000 1000\n"
        "[sysfs]\n"
        "/dir/to/nothing attr_nowhere 0666 1000 1000\n"
        "[sysfs]\n"
        "  #/dir/to/nothing attr_nowhere 0666\n"
        "[sysfs\n"
        "/dir/to/nothing attr_nowhere 0666\n"
        "[firmware]\n"
        "/etc\n"
        "[device]\n"
        "/dev/testbinder 0666 1000 1000 const.dev.binder\n"
        "[device]\n"
        "/dev/testbinder1 0666 1000 1000 const.dev.binder\n"
        "[device]\n"
        "/dev/testbinder2 0666 1000 1000 const.dev.binder\n"
        "[device]\n"
        "/dev/testbinder3 0666 1000 1000 const.dev.binder\n";
    mkdir("/data/ueventd_ut", S_IRWXU | S_IRWXG | S_IRWXO);
    CreateTestFile("/data/ueventd_ut/valid.config", ueventdcfg);
}
static void PrepareModCfg(void)
{
    const char *modCfg = "testinsmod";
    CreateTestFile("/data/init_ut/test_insmod", modCfg);
}
static void PrepareInnerKitsCfg()
{
    const char *innerKitsCfg = "/dev/block/platform/soc/10100000.himci.eMMC/by-name/system /system "
        "ext4 ro,barrier=1 wait\n"
        "/dev/block/platform/soc/10100000.himci.eMMC/by-name/vendor /vendor "
        "ext4 ro,barrier=1 wait\n"
        "/dev/block/platform/soc/10100000.himci.eMMC/by-name/hos "
        "/hos ntfs nosuid,nodev,noatime,barrier=1,data=ordered wait\n"
        "/dev/block/platform/soc/10100000.himci.eMMC/by-name/userdata /data ext4 "
        "nosuid,nodev,noatime,barrier=1,data=ordered,noauto_da_alloc "
        "wait,reservedsize=104857600\n"
        "  aaaa\n"
        "aa aa\n"
        "aa aa aa\n"
        "aa aa aa aa\n";
    mkdir("/data/init_ut/mount_unitest/", S_IRWXU | S_IRWXG | S_IRWXO);
    CreateTestFile("/data/init_ut/mount_unitest/ReadFstabFromFile1.fstable", innerKitsCfg);
    CreateTestFile("/etc/fstab.required", "test");
}
static void PrepareGroupTestCfg()
{
    const char *data = "{"
	    "\"jobs\": [\"param:job1\", \"param:job2\", \"param:job4\"],"
	    "\"services\": [\"service:service1\", \"service:service3\", \"service:service2\"],"
	    "\"groups\": [\"subsystem.xxx1.group\", \"subsystem.xxx2.group\", \"subsystem.xxx4.group\"]"
    "}";
    const char *xxx1 = "{"
	    "\"groups\": [\"subsystem.xxx11.group\""
    "}";
    const char *xxx11 = "{"
	    "\"groups\": [\"subsystem.xxx12.group\""
    "}";
    const char *xxx12 = "{"
	    "\"groups\": [\"subsystem.xxx13.group\""
    "}";
    const char *xxx13 = "{"
	    "\"groups\": [\"subsystem.xxx14.group\""
    "}";
    const char *xxx14 = "{"
	    "\"groups\": [\"subsystem.xxx11.group\""
    "}";
    CreateTestFile(GROUP_DEFAULT_PATH "/device.boot.group.cfg", data);
    CreateTestFile(GROUP_DEFAULT_PATH "/subsystem.xxx1.group.cfg", xxx1);
    CreateTestFile(GROUP_DEFAULT_PATH "/subsystem.xxx11.group.cfg", xxx11);
    CreateTestFile(GROUP_DEFAULT_PATH "/subsystem.xxx12.group.cfg", xxx12);
    CreateTestFile(GROUP_DEFAULT_PATH "/subsystem.xxx13.group.cfg", xxx13);
    CreateTestFile(GROUP_DEFAULT_PATH "/subsystem.xxx14.group.cfg", xxx14);
}
static bool IsDir(const std::string &path)
{
    struct stat st {};
    if (stat(path.c_str(), &st) < 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}
static bool DeleteDir(const std::string &path)
{
    auto pDir = std::unique_ptr<DIR, decltype(&closedir)>(opendir(path.c_str()), closedir);
    if (pDir == nullptr) {
        return false;
    }

    struct dirent *dp = nullptr;
    while ((dp = readdir(pDir.get())) != nullptr) {
        std::string currentName(dp->d_name);
        if (currentName[0] != '.') {
            std::string tmpName(path);
            tmpName.append("/" + currentName);
            if (IsDir(tmpName)) {
                DeleteDir(tmpName);
            }
            remove(tmpName.c_str());
        }
    }
    if (remove(path.c_str()) != 0) {
        return false;
    }
    return true;
}
static void LoadParamFromCfg(void)
{
#ifdef PARAM_LOAD_CFG_FROM_CODE
    for (size_t i = 0; i < ARRAY_LENGTH(g_paramDefCfgNodes); i++) {
        PARAM_LOGI("InitParamClient name %s = %s", g_paramDefCfgNodes[i].name, g_paramDefCfgNodes[i].value);
        uint32_t dataIndex = 0;
        int ret = WriteParam(g_paramDefCfgNodes[i].name, g_paramDefCfgNodes[i].value, &dataIndex, 0);
        PARAM_CHECK(ret == 0, continue, "Failed to set param %d name %s %s",
            ret, g_paramDefCfgNodes[i].name, g_paramDefCfgNodes[i].value);
    }
#endif
}
#if !(defined __LITEOS_A__ || defined __LITEOS_M__)
static const char *g_triggerData = "{"
        "\"jobs\" : [{"
        "        \"name\" : \"early-init\","
        "        \"cmds\" : ["
        "            \"    write        '/proc/sys/kernel/sysrq 0'\","
        "            \"    load_persist_params \","
        "            \"    load_persist_params        \","
        "            \" #   load_persist_params \","
        "            \"   restorecon /postinstall\","
        "            \"mkdir /acct/uid\","
        "            \"chown root system /dev/memcg/memory.pressure_level\","
        "            \"chmod 0040 /dev/memcg/memory.pressure_level\","
        "            \"mkdir /dev/memcg/apps/ 0755 system system\","
        "           \"mkdir /dev/memcg/system 0550 system system\","
        "            \"start ueventd\","
        "            \"exec_start apexd-bootstrap\","
        "            \"setparam sys.usb.config ${persist.sys.usb.config}\""
        "        ]"
        "    },"
        "    {"
        "        \"name\" : \"param:trigger_test_1\","
        "        \"condition\" : \"test.sys.boot_from_charger_mode=5\","
        "        \"cmds\" : ["
        "            \"class_stop charger\","
        "            \"trigger late-init\""
        "        ]"
        "    },"
        "    {"
        "        \"name\" : \"param:trigger_test_2\","
        "        \"condition\" : \"test.sys.boot_from_charger_mode=1  "
        " || test.sys.boot_from_charger_mode=2   ||  test.sys.boot_from_charger_mode=3\","
        "        \"cmds\" : ["
        "            \"class_stop charger\","
        "            \"trigger late-init\""
        "        ]"
        "    },"
        "    {"
        "        \"name\" : \"load_persist_params_action\","
        "        \"cmds\" : ["
        "           \"load_persist_params\","
        "            \"start logd\","
        "            \"start logd-reinit\""
        "        ]"
        "    },"
        "    {"
        "        \"name\" : \"firmware_mounts_complete\","
        "        \"cmds\" : ["
        "            \"rm /dev/.booting\""
        "        ]"
        "    }"
        "]"
    "}";
#endif

void PrepareCmdLineHasSn()
{
    // for cmdline
    const char *cmdLineHasSnroot = "bootgroup=device.charge.group earlycon=uart8250,mmio32,0xfe660000 "
        "root=PARTUUID=614e0000-0000 rw rootwait rootfstype=ext4 console=ttyFIQ0 hardware=rk3568"
        " BOOT_IMAGE=/kernel ohos.boot.sn=/test init=/init ohos.required_mount.system="
        "/dev/block/platform/soc/10100000.himci.eMMC/by-name/misc@none@none@none@wait,required";
    CreateTestFile(BOOT_CMD_LINE, cmdLineHasSnroot);
    LoadParamFromCmdLine();
    const char *cmdLineHasntSn = "bootgroup=device.charge.group earlycon=uart8250,mmio32,0xfe660000 "
        "root=PARTUUID=614e0000-0000 rw rootwait rootfstype=ext4 console=ttyFIQ0 hardware=rk3568"
        " BOOT_IMAGE=/kernel init=/init ohos.required_mount.system="
        "/dev/block/platform/soc/10100000.himci.eMMC/by-name/misc@none@none@none@wait,required";
    CreateTestFile(BOOT_CMD_LINE, cmdLineHasntSn);
}

void PrepareInitUnitTestEnv(void)
{
    static int evnOk = 0;
    if (evnOk) {
        return;
    }
    PARAM_LOGI("PrepareInitUnitTestEnv");
    mkdir(STARTUP_INIT_UT_PATH, S_IRWXU | S_IRWXG | S_IRWXO);
    PrepareUeventdcfg();
    PrepareInnerKitsCfg();
    PrepareModCfg();
    PrepareGroupTestCfg();

#if !(defined __LITEOS_A__ || defined __LITEOS_M__)
    // for dac
    std::string dacData = "ohos.servicectrl.   = system:servicectrl:0775 \n";
    dacData += "test.permission.       = root:root:0770\n";
    dacData += "test.permission.read. =  root:root:0774\n";
    dacData += "test.permission.write.=  root:root:0772\n";
    dacData += "test.permission.watcher. = root:root:0771\n";
    CreateTestFile(STARTUP_INIT_UT_PATH "/system/etc/param/ohos.para.dac", dacData.c_str());
    CreateTestFile(STARTUP_INIT_UT_PATH"/trigger_test.cfg", g_triggerData);
#endif
    InitParamService();

#if !(defined __LITEOS_A__ || defined __LITEOS_M__)
    PrepareCmdLineHasSn();
    TestSetSelinuxOps();
    LoadSpecialParam();
#endif

    // read system parameters
    LoadDefaultParams("/system/etc/param/ohos_const", LOAD_PARAM_NORMAL);
    LoadDefaultParams("/vendor/etc/param", LOAD_PARAM_NORMAL);
    LoadDefaultParams("/system/etc/param", LOAD_PARAM_ONLY_ADD);
    // read ut parameters
    LoadDefaultParams(STARTUP_INIT_UT_PATH "/system/etc/param/ohos_const", LOAD_PARAM_NORMAL);
    LoadDefaultParams(STARTUP_INIT_UT_PATH "/vendor/etc/param", LOAD_PARAM_NORMAL);
    LoadDefaultParams(STARTUP_INIT_UT_PATH "/system/etc/param", LOAD_PARAM_ONLY_ADD);
    LoadParamFromCfg();
    evnOk = 1;
}

int TestCheckParamPermission(const ParamSecurityLabel *srcLabel, const char *name, uint32_t mode)
{
    // DAC_RESULT_FORBIDED
    return g_testPermissionResult;
}

int TestFreeLocalSecurityLabel(ParamSecurityLabel *srcLabel)
{
    return 0;
}

static __attribute__((constructor(101))) void ParamTestStubInit(void)
{
    EnableInitLog(INIT_DEBUG);
    PARAM_LOGI("ParamTestStubInit");
    PrepareInitUnitTestEnv();
}
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
