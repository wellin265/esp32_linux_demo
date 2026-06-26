/**
 * @file fs_manager.c
 * @brief 文件系统管理模块
 *
 * 本文件实现统一的文件系统管理接口：
 * - SPIFFS 文件系统初始化（Flash 存储）
 * - SD 卡文件系统初始化（SDMMC 模式）
 * - 文件列表显示
 * - 文件系统卸载
 *
 * 支持的文件系统类型：
 * - SPIFFS：适用于小型文件存储
 * - SD 卡：适用于大容量存储和持久化
 *
 * @copyright Copyright (c) 2024 酷世DIY
 */

#include "fs_manager.h"
#include <dirent.h>

/** @brief 日志标签 */
static const char *TAG = "fs_manager";

/** @brief 当前文件系统类型 */
static fs_type_t current_fs_type = FS_TYPE_SPIFFS;

/** @brief SD 卡句柄 */
static sdmmc_card_t *sd_card = NULL;

/**
 * @brief 初始化 SPIFFS 文件系统
 *
 * 将 SPIFFS 分区挂载为虚拟文件系统。
 *
 * @param config SPIFFS 配置参数
 * @return esp_err_t 初始化结果
 */
static esp_err_t init_spiffs(fs_config_t *config)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = config->spiffs.base_path,
        .partition_label = config->spiffs.partition_label,
        .max_files = config->spiffs.max_files,
        .format_if_mount_failed = config->spiffs.format_if_mount_failed
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    return ESP_OK;
}

/**
 * @brief 初始化 SD 卡文件系统
 *
 * 使用 SDMMC 1-bit 模式挂载 SD 卡。
 *
 * @param config SD 卡配置参数
 * @return esp_err_t 初始化结果
 */
static esp_err_t init_sdcard(fs_config_t *config)
{
    esp_err_t ret = ESP_OK;

    /* SD 卡挂载配置 */
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = config->sd_card.format_if_mount_failed,
        .max_files = config->sd_card.max_files,
        .allocation_unit_size = 16 * 1024  /* 分配单元：16KB */
    };

    /* SD 卡主机配置（默认配置） */
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    /* SD 卡插槽配置 */
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;  /* 1-bit SD 模式 */
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;  /* 启用内部上拉 */

    /* 配置 GPIO 引脚（对于支持 GPIO 矩阵的芯片） */
#ifdef SOC_SDMMC_USE_GPIO_MATRIX
    slot_config.clk = config->sd_card.clk;
    slot_config.cmd = config->sd_card.cmd;
    slot_config.d0 = config->sd_card.d0;
#endif

    ESP_LOGI(TAG, "Mounting SD card to %s", config->sd_card.mount_point);

    /* 挂载 SD 卡 */
    ret = esp_vfs_fat_sdmmc_mount(config->sd_card.mount_point, &host, &slot_config,
                                  &mount_config, &sd_card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount SD card filesystem");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SD card (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    /* 打印 SD 卡信息 */
    sdmmc_card_print_info(stdout, sd_card);
    return ESP_OK;
}

/**
 * @brief 初始化文件系统
 *
 * 根据配置初始化指定的文件系统类型。
 *
 * @param config 文件系统配置参数
 * @return esp_err_t 初始化结果
 */
esp_err_t fs_manager_init(fs_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    current_fs_type = config->type;

    if (config->type == FS_TYPE_SPIFFS) {
        return init_spiffs(config);
    } else if (config->type == FS_TYPE_SD_CARD) {
        return init_sdcard(config);
    }

    return ESP_ERR_INVALID_ARG;
}

/**
 * @brief 列出目录中的所有文件
 *
 * 遍历指定目录并打印所有文件的信息。
 *
 * @param path 目录路径
 */
void fs_manager_list_files(const char* path)
{
    DIR *dir = opendir(path);
    if (dir == NULL) {
        ESP_LOGE(TAG, "Failed to open directory %s", path);
        return;
    }

    while (true) {
        struct dirent *pe = readdir(dir);
        if (!pe) break;

        ESP_LOGI(TAG, "d_name=%s d_ino=%d d_type=%x", pe->d_name, pe->d_ino, pe->d_type);
    }
    closedir(dir);
}

/**
 * @brief 卸载文件系统
 *
 * 卸载当前挂载的文件系统，释放资源。
 */
void fs_manager_deinit(void)
{
    if (current_fs_type == FS_TYPE_SPIFFS) {
        esp_vfs_spiffs_unregister(NULL);
    } else if (current_fs_type == FS_TYPE_SD_CARD && sd_card != NULL) {
        const char *mount_point = "/sdcard";  /* 使用默认挂载点 */
        esp_vfs_fat_sdcard_unmount(mount_point, sd_card);
        sd_card = NULL;
    }
}

/**
 * @brief 获取当前文件系统类型
 *
 * @return fs_type_t 当前文件系统类型
 */
fs_type_t fs_manager_get_type(void)
{
    return current_fs_type;
}
