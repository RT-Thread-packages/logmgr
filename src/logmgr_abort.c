/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-01-13     ChenYong     first version
 */

#include <stdbool.h>

#include <rtthread.h>
#include <rtdevice.h>
#include <sys/time.h>

#include <dfs.h>
#include <dfs_posix.h>

#include <logmgr.h>

#define DBG_TAG "logmgr.abort"
#define DBG_LVL DBG_LOG
#include <ulog.h>

#ifdef LOGMGR_USING_CMBACKTRACE
#include <cm_backtrace.h>
#endif
#ifdef LOGMGR_USING_KDB
#include <kdb_sys.h>
#endif
#ifdef LOGMGR_USING_SYS_LOAD_MONITOR
#include <sys_load_monitor.h>
#endif
#define LOGMGR_CONSOLE_NAME            "logmgr"
#ifdef PKG_USING_FLASHDB
#include <flashdb.h>
#ifndef LOGMGR_FLASHDB_PART_NAME
#define LOGMGR_FLASHDB_PART_NAME       "logmgr"
#endif
#ifndef LOGMGR_FLASHDB_MAX_LEN
#define LOGMGR_FLASHDB_MAX_LEN         512
#endif
#define LOGMGR_FLASHDB_SECTOR_SIZE     4096
#ifndef LOGMGR_FLASHDB_MAX_SIZE
#define LOGMGR_FLASHDB_MAX_SIZE        65536
#endif
#endif /* PKG_USING_FLASHDB */
#ifndef LOGMGR_ABORT_FILE_PATH
#define LOGMGR_ABORT_FILE_PATH         "/abort.log"
#endif

#ifdef PKG_USING_FLASHDB
static struct fdb_tsdb g_tsdb;
#endif
static rt_bool_t is_init = RT_FALSE;
static struct rt_device g_console_dev;
static rt_device_t g_def_device = RT_NULL;

static void _logmgr_ipc_log(void);
static void _logmgr_memory_log(void);
static void _logmgr_console_switch(rt_device_t device);
void rt_hw_exception_install(rt_err_t (*exception_handle)(void *context));

static void printf_time_log(void)
{
    rt_kprintf("Logmgr Abort Start %s %s\n", __DATE__, __TIME__);
}

static void printf_second_header_log(const char *second_name)
{
    rt_kprintf("\n/******  %.*s  ******/\n", rt_strlen(second_name), second_name);
}

static void print_header_log(const char *log_name)
{
    rt_kprintf("\n");
    rt_kprintf("/**********************************************************/\n");
    rt_kprintf("/**************     %20s     **************/\n", log_name);
    rt_kprintf("/**********************************************************/\n");
    rt_kprintf("\n");
}

RT_WEAK rt_err_t logmgr_exception_hook(void *context)
{
    volatile uint8_t _continue = 1;

    /* set console device to new customize device */
    _logmgr_console_switch(&g_console_dev);

    /* show abort start time */
    printf_time_log();

#ifdef LOGMGR_USING_CMBACKTRACE
    print_header_log("CmBacktrace Log");
    /* cmbacktrace exception hook */
    extern rt_err_t exception_hook(void *context);
    exception_hook(context);
#endif
#ifdef LOGMGR_USING_IPC_LOG
    print_header_log("System IPC Log");
    _logmgr_ipc_log();
#endif
#ifdef LOGMGR_USING_KDB
    print_header_log("Kernel Running Log");
    kdb_sys_stop_dump();
#endif
#ifdef LOGMGR_USING_SYS_LOAD_MONITOR
    print_header_log("System Load Log");
    sys_load_monitor_dump();
#endif
#ifdef LOGMGR_USING_MEMORY_LOG
    print_header_log("System Memory Log");
    _logmgr_memory_log();
#endif

    while (_continue == 1);

    return RT_EOK;
}

RT_WEAK void logmgr_assert_hook(const char *ex, const char *func, rt_size_t line)
{
    volatile uint8_t _continue = 1;

    /* set console device to new customize device */
    _logmgr_console_switch(&g_console_dev);

    /* show abort start time */
    printf_time_log();

#ifdef PKG_USING_CMBACKTRACE
    print_header_log("CmBacktrace Log");
    /* cmbacktrace assert hook */
    extern void assert_hook(const char* ex, const char* func, rt_size_t line);
    assert_hook(ex, func, line);
#endif

    while (_continue == 1);
}

#ifdef LOGMGR_USING_MEMORY_LOG
static void _logmgr_memory_log(void)
{
#ifdef RT_USING_HEAP
    printf_second_header_log("sys memory log");
#ifdef RT_USING_MEMHEAP_AS_HEAP
    extern void list_memheap(void);
    list_memheap();
#else
    extern void list_mem(void);
    list_mem();
#endif
#endif /* RT_USING_HEAP */

#ifdef PKG_JMEM_STATS
    printf_second_header_log("js heap log");
    extern void jmem_heap(void);
    jmem_heap();
#endif
}
#endif /* LOGMGR_USING_MEMORY_LOG */

#ifdef LOGMGR_USING_IPC_LOG
/* system ipc log print */
static void _logmgr_ipc_log(void)
{
#ifdef RT_USING_SEMAPHORE
    printf_second_header_log("semaphore log");
    extern long list_sem(void);
    list_sem();
#endif
#ifdef RT_USING_EVENT
    printf_second_header_log("event log");
    extern long list_event(void);
    list_event();
#endif
#ifdef RT_USING_MUTEX
    printf_second_header_log("mutex log");
    extern long list_mutex(void);
    list_mutex();
#endif
#ifdef RT_USING_MAILBOX
    printf_second_header_log("mailbox log");
    extern long list_mailbox(void);
    list_mailbox();
#endif
#ifdef RT_USING_MESSAGEQUEUE
    printf_second_header_log("messagqueue log");
    long list_msgqueue(void);
    list_msgqueue();
#endif
}
#endif /* LOGMGR_USING_IPC_LOG */

#ifdef PKG_USING_FLASHDB
/* flashdb get time function */
static fdb_time_t _logmgr_get_time(void)
{
    static fdb_time_t count = 0;
    return count++;
}
#endif /* PKG_USING_FLASHDB */

/* flashdb iterator callback function */
static bool _logmgr_tsl_cb(fdb_tsl_t tsl, void *arg)
{
    int fd = *((int *) arg);
    struct fdb_blob blob;
    size_t data_len = 0;
    char data[LOGMGR_FLASHDB_MAX_LEN] = { 0 };

    /* get blob data by tsl data */
    fdb_blob_make(&blob, data, tsl->log_len);
    data_len = fdb_blob_read((fdb_db_t )&g_tsdb, fdb_tsl_to_blob(tsl, &blob));

    /* write falshdb historical data to file */
    write(fd, data, data_len);
    return false;
}
/* logmgr support packages initialized */
static int _logmgr_pkgs_init(void)
{
#ifdef PKG_USING_FLASHDB
    uint32_t sec_size = LOGMGR_FLASHDB_SECTOR_SIZE;
    uint32_t db_size  = LOGMGR_FLASHDB_MAX_SIZE;
    fdb_tsdb_t tsdb = &g_tsdb;

     /* initialize flashdb tsdb information */
    fdb_tsdb_control(tsdb, FDB_TSDB_CTRL_SET_SEC_SIZE, &sec_size);
    fdb_tsdb_control(tsdb, FDB_TSDB_CTRL_SET_MAX_SIZE,  &db_size);

    if (fdb_tsdb_init(tsdb, "logmgr",
            LOGMGR_FLASHDB_PART_NAME, _logmgr_get_time, LOGMGR_FLASHDB_MAX_LEN, RT_NULL) != FDB_NO_ERR)
    {
        LOG_E("logmgr flashdb tsdb initialized failed.");
        return -1;
    }

    /* write falsh tsdb data to file */
    if (fdb_tsl_query_count(tsdb, 0, 0x7FFFFFFF, FDB_TSL_WRITE) > 0)
    {
        int fd = -1;

        /* open the storge log file */
        fd = open(LOGMGR_ABORT_FILE_PATH, O_RDWR | O_CREAT | O_TRUNC);
        if (fd < 0)
        {
            LOG_E("open logmgr abort file failed.");
            return -1;
        }
        lseek(fd, 0, SEEK_END);

        /* set tsdb iterator callback */
        fdb_tsl_iter(tsdb, _logmgr_tsl_cb, (void *)&fd);
        fdb_tsl_clean(tsdb);
        close(fd);
    }
#endif /* PKG_USING_FLASHDB */
#ifdef LOGMGR_USING_CMBACKTRACE
    extern int rt_cm_backtrace_init(void);
    rt_cm_backtrace_init();
#endif
#ifdef PKG_USING_KDB
    kdb_sys_init();
    kdb_sys_start();
#endif
#ifdef PKG_USING_SYS_LOAD_MONITOR
    sys_load_monitor_init();
#endif
    return 0;
}

/* logmgr customize console write operation */
static rt_size_t _console_write(rt_device_t dev, rt_off_t pos, const void *buffer,
                        rt_size_t size)
{
#ifdef PKG_USING_FLASHDB
    struct fdb_blob blob;

    if (!is_init)
        return 0;

    RT_ASSERT(buffer);

    /* make log buffer blob */
    if (fdb_tsl_append(&g_tsdb, fdb_blob_make(&blob, buffer, size)) != FDB_NO_ERR)
    {
        return 0;
    }
#endif /* PKG_USING_FLASHDB */

    if (g_def_device)
    {
        rt_uint16_t old_flag = g_def_device->open_flag;

        /* write log data to original console device */
        g_def_device->open_flag |= RT_DEVICE_FLAG_STREAM;
        rt_device_write(g_def_device, 0, buffer, size);
        g_def_device->open_flag = old_flag;
    }

    return size;
}

#ifdef RT_USING_DEVICE_OPS
const static struct rt_device_ops _console_ops =
{
    RT_NULL,
    RT_NULL,
    RT_NULL,
    RT_NULL,
    _console_write,
    RT_NULL
};
#endif

/* logmgr customize console initialized */
static int _logmgr_console_init(void)
{
    rt_err_t ret = 0;
    rt_device_t device = &g_console_dev;

    device->type = RT_Device_Class_Char;
#ifdef RT_USING_DEVICE_OPS
    device->ops = &_console_ops;
#else
    device->init = RT_NULL;
    device->open = RT_NULL;
    device->close = RT_NULL;
    device->read = RT_NULL;
    device->write = _console_write;
    device->control = RT_NULL;
#endif
    device->user_data = RT_NULL;

    /* register virtual console device */
    ret = rt_device_register(device, LOGMGR_CONSOLE_NAME,
                             RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX);
    if (ret != RT_EOK)
    {
        return -1;
    }

    if (g_def_device == RT_NULL)
    {
        g_def_device = rt_device_find(RT_CONSOLE_DEVICE_NAME);
    }

    return 0;
}

/* logmgr customize console switch for log storage */
static void _logmgr_console_switch(rt_device_t device)
{
    rt_console_set_device(device->parent.name);
}

/* logmgr abort log print and storage initialize */
int logmgr_abort_init(void)
{
    if (is_init == RT_TRUE)
    {
        LOG_W("logmgr abort is already initialized.");
        return 0;
    }

    if (_logmgr_pkgs_init() < 0)
    {
        LOG_E("logmgr packages initialize failed.");
        return -1;
    }

    if (_logmgr_console_init() < 0)
    {
        LOG_E("logmgr console initialized failed.");
        return -1;
    }

    /* set system hardfault hook */
    rt_hw_exception_install(logmgr_exception_hook);
    /* set system assert hook */
    rt_assert_set_hook(logmgr_assert_hook);

    is_init = RT_TRUE;
    LOG_I("logmgr abort initialized success.");

    return 0;
}
