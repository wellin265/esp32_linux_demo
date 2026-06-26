/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "basic_freertos_smp_usage.h"
#include "esp_console.h"
#include "ksdiy_example_display.h"
#include "sdkconfig.h"

static void register_creating_task(void)
{
    const esp_console_cmd_t cmd = {
        .command = "create_task",
        .help = "Run the example that demonstrates how to create and run pinned and unpinned tasks",
        .hint = NULL,
        .func = &comp_creating_task_entry_func,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void register_queue(void)
{
    const esp_console_cmd_t cmd = {
        .command = "queue",
        .help = "Run the example that demonstrates how to use queue to communicate between tasks",
        .hint = NULL,
        .func = &comp_queue_entry_func,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void register_lock(void)
{
    const esp_console_cmd_t cmd = {
        .command = "lock",
        .help = "Run the example that demonstrates how to use mutex and spinlock to protect a shared resource",
        .hint = NULL,
        .func = &comp_lock_entry_func,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void register_task_notification(void)
{
    const esp_console_cmd_t cmd = {
        .command = "task_notification",
        .help = "Run the example that demonstrates how to use task notifications to synchronize tasks",
        .hint = NULL,
        .func = &comp_task_notification_entry_func,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void register_batch_proc_example(void)
{
    const esp_console_cmd_t cmd = {
        .command = "batch_processing",
        .help = "Run the example that combines queue, mutex, task notification together",
        .hint = NULL,
        .func = &comp_batch_proc_example_entry_func,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void config_console(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = 1024;

    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    esp_console_register_help_command();

    register_creating_task();
    register_queue();
    register_lock();
    register_task_notification();
    register_batch_proc_example();

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

void app_main(void)
{
    ksdiy_example_display_bootstrap("01.basic.freertos", "console command demos");
    ksdiy_example_display_set_lines("UART console ready", "help / create_task", "queue / lock / notify");
    config_console();
}
