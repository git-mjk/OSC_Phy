/******************************************************************************
*
*   Copyright (c) 2021 Intel.
*
*   Licensed under the Apache License, Version 2.0 (the "License");
*   you may not use this file except in compliance with the License.
*   You may obtain a copy of the License at
*
*       http://www.apache.org/licenses/LICENSE-2.0
*
*   Unless required by applicable law or agreed to in writing, software
*   distributed under the License is distributed on an "AS IS" BASIS,
*   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*   See the License for the specific language governing permissions and
*   limitations under the License.
*
*******************************************************************************/
#include "nr5g_fapi_config_loader.h"
#include "nr5g_fapi_dpdk.h"
#include "sched.h"

#define NR5G_FAPI_MAX_SHMSIZE (2126512128)
char *fgets_s(
    char *string,
    size_t len,
    FILE * fp);

void nr5g_fapi_get_worker_info(
    struct rte_cfgfile *cfg_file,
    unsigned int num_cpus,
    nr5g_fapi_thread_params_t * thread_params,
    const char* worker_name)
{
    const char *entry;

    entry = rte_cfgfile_get_entry(cfg_file, worker_name, "core_id");
    if (entry) {
        thread_params->thread_worker.core_id = (uint8_t) atoi(entry);
        if (thread_params->thread_worker.core_id >= (uint8_t) num_cpus) {
            printf("Core Id is not in the range 0 to %d: configured: %d\n",
                num_cpus, thread_params->thread_worker.core_id);
            exit(-1);
        }
    }

    entry =
        rte_cfgfile_get_entry(cfg_file, worker_name,
        "thread_sched_policy");
    if (entry) {
        thread_params->thread_worker.thread_sched_policy = (uint8_t) atoi(entry);
        if (thread_params->thread_worker.thread_sched_policy != SCHED_FIFO &&
            thread_params->thread_worker.thread_sched_policy != SCHED_RR) {
            printf("Thread Policy valid range is Schedule Policy [1: SCHED_FIFO"
                " 2: SCHED_RR]: configured: %d\n",
                thread_params->thread_worker.thread_sched_policy);
            exit(-1);
        }
    }

    int min_prio =
        sched_get_priority_min(thread_params->thread_worker.thread_sched_policy);
    int max_prio =
        sched_get_priority_max(thread_params->thread_worker.thread_sched_policy);
    entry =
        rte_cfgfile_get_entry(cfg_file, worker_name, "thread_priority");
    if (entry) {
        thread_params->thread_worker.thread_priority = (uint8_t) atoi(entry);
        if (thread_params->thread_worker.thread_priority < min_prio &&
            thread_params->thread_worker.thread_priority > max_prio) {
            printf("Thread priority valid range is %d to %d: configured: %d\n",
                min_prio, max_prio, thread_params->thread_worker.thread_priority);
            exit(-1);
        }
    }
}

p_nr5g_fapi_cfg_t nr5g_fapi_config_loader(
    char *prgname,
    const char *cfg_fname)
{
    struct rte_cfgfile *cfg_file;
    p_nr5g_fapi_cfg_t cfg;
    const char *entry;
    size_t dev_name_len, mem_zone_name_len;
    unsigned int num_cpus = 0;
    char check_core_count[255], *max_core;
    FILE *fp = NULL;

    if (cfg_fname == NULL) {
        printf("Error: Configuration filename was not passed.");
        return NULL;
    }

    cfg_file = rte_cfgfile_load(cfg_fname, 0);
    if (cfg_file == NULL) {
        printf("Error: Cofiguration file loading failed.");
        return NULL;
    }

    cfg = (p_nr5g_fapi_cfg_t) calloc(1, sizeof(nr5g_fapi_cfg_t));
    if (!cfg) {
        printf("Error: not enough memory in the system");
        return NULL;
    }
    cfg->prgname = prgname;
    fp = popen("cat /proc/cpuinfo | grep processor | wc -l", "r");
    if (NULL == fp) {
        printf("Error: In getting maximum number of lcores in the system.");
        free(cfg);
        return NULL;
    }
    max_core = fgets_s(check_core_count, 255, fp);
    if (NULL == max_core) {
        printf("Error: In getting maximum number of lcores in the system.");
        free(cfg);
        pclose(fp);
        return NULL;
    }
    pclose(fp);
    num_cpus = atoi(max_core);

    cfg->is_urllc_enabled = TRUE;
    entry = rte_cfgfile_get_entry(cfg_file, "URLLC", "is_enabled");
    if (entry)
    {
        cfg->is_urllc_enabled = (bool)atoi(entry);
        if (!cfg->is_urllc_enabled)
            printf("URLLC disabled\n");
    }

    nr5g_fapi_get_worker_info(cfg_file, num_cpus, &cfg->mac2phy_thread_params, "MAC2PHY_WORKER");
    nr5g_fapi_get_worker_info(cfg_file, num_cpus, &cfg->phy2mac_thread_params, "PHY2MAC_WORKER");
    nr5g_fapi_get_worker_info(cfg_file, num_cpus, &cfg->urllc_mac2phy_thread_params, "MAC2PHY_URLLC_WORKER");
    nr5g_fapi_get_worker_info(cfg_file, num_cpus, &cfg->urllc_phy2mac_thread_params, "PHY2MAC_URLLC_WORKER");

    entry = rte_cfgfile_get_entry(cfg_file, "WLS_CFG", "device_name");
    if (entry) {
        dev_name_len = (strlen(entry) > (NR5G_FAPI_DEVICE_NAME_LEN)) ?
            (NR5G_FAPI_DEVICE_NAME_LEN) : strlen(entry);
        rte_strlcpy(cfg->wls.device_name, entry, dev_name_len + 1);
    }

    entry = rte_cfgfile_get_entry(cfg_file, "WLS_CFG", "shmem_size");
    if (entry) {
        cfg->wls.shmem_size = (uint64_t) atoll(entry);
        if (cfg->wls.shmem_size > NR5G_FAPI_MAX_SHMSIZE) {
            printf("The memory range cannot exceed %d", NR5G_FAPI_MAX_SHMSIZE);
            exit(-1);
        }
    }

    entry = rte_cfgfile_get_entry(cfg_file, "LOGGER", "level");
    if (entry) {
        if (!strcmp(entry, "info"))
            cfg->logger.level = INFO_LOG;
        else if (!strcmp(entry, "debug"))
            cfg->logger.level = DEBUG_LOG;
        else if (!strcmp(entry, "error"))
            cfg->logger.level = ERROR_LOG;
        else if (!strcmp(entry, "trace"))
            cfg->logger.level = TRACE_LOG;
        else
            cfg->logger.level = NONE_LOG;
    }

    entry = rte_cfgfile_get_entry(cfg_file, "DPDK", "dpdk_iova_mode");
    if (entry) {
        cfg->dpdk.iova_mode = (uint8_t) atoi(entry);
        if (cfg->dpdk.iova_mode >= DPDK_IOVA_MAX_MODE) {
            printf("The mode is out of range: %d", cfg->dpdk.iova_mode);
            exit(-1);
        }
    }

    entry = rte_cfgfile_get_entry(cfg_file, "DPDK", "dpdk_memory_zone");
    if (entry) {
        mem_zone_name_len = (strlen(entry) > (NR5G_FAPI_MEMORY_ZONE_NAME_LEN)) ?
            (NR5G_FAPI_MEMORY_ZONE_NAME_LEN) : strlen(entry);
        rte_strlcpy(cfg->dpdk.memory_zone, entry, mem_zone_name_len + 1);
    }

    return cfg;
}

char *fgets_s(
    char *string,
    size_t len,
    FILE * fp)
{
    if (fgets(string, len, fp) != 0) {
        string[strcspn(string, "\n")] = '\0';
        return string;
    }
    return 0;
}
