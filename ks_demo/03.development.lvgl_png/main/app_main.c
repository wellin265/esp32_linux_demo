/**
 * @file app_main.c
 * @brief LVGL v8.3 PNG 图片浏览器示例
 *
 * 本示例演示如何在 ESP32-S3 上使用 LVGL v8.3 显示存储在 SPIFFS 文件系统中的 PNG 图片。
 *
 * 功能：
 * - 初始化 SPIFFS 文件系统
 * - 扫描并列出所有图片文件
 * - 点击文件列表项显示对应图片
 * - 支持 PNG 格式图片解码显示
 *
 * @note 需要在 sdkconfig 中启用 LVGL 的 PNG 解码器支持
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lv_demos.h"

#include "esp_freertos_hooks.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
#include "esp_timer.h"
#endif
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_check.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "ksdiy_lvgl_port.h"
#include "lv_port_fs.h"

/** @brief 日志标签 */
static const char *TAG = "png";

/** @brief 带路径的文件名缓冲区 */
char file_name_with_path[128] = {0};

/**
 * @brief 列表按钮点击事件回调
 *
 * 当用户点击文件列表中的图片文件名时，加载并显示该图片。
 *
 * @param event 事件对象指针
 */
static void btn_event_cb(lv_event_t *event)
{
    lv_obj_t *img = (lv_obj_t *)event->user_data;
    const char *file_name = lv_list_get_btn_text(lv_obj_get_parent(event->target), event->target);
    ESP_LOGI(TAG, "btn_event_cb");

    /* 清空文件路径缓冲区 */
    memset(file_name_with_path, 0, sizeof(file_name_with_path));

    /* 构建完整文件路径（带挂载点和目录） */
    strcpy(file_name_with_path, "S:/spiffs/");
    strcat(file_name_with_path, file_name);

    /* 设置图片控件的源文件 */
    lv_img_set_src(img, file_name_with_path);

    /* 居中对齐图片 */
    lv_obj_align(img, LV_ALIGN_CENTER, 40, 0);

    /* 调试输出 */
    ESP_LOGI(TAG, "Display image file : %s", file_name_with_path);
}

/**
 * @brief 创建图片浏览器界面
 *
 * 创建一个包含文件列表和图片显示区域的界面：
 * - 左侧：可滚动的文件列表
 * - 右侧：图片显示区域
 *
 * 扫描 SPIFFS 分区中的所有文件并添加到列表中。
 */
static void image_display(void)
{
    /* 创建文件列表控件 */
    lv_obj_t *list = lv_list_create(lv_scr_act());
    lv_obj_set_size(list, 120, 240);
    lv_obj_set_style_border_width(list, 0, LV_STATE_DEFAULT);
    lv_obj_align(list, LV_ALIGN_LEFT_MID, 0, 0);

    /* 创建图片显示控件 */
    lv_obj_t *img = lv_img_create(lv_scr_act());

    /* 打开 SPIFFS 目录 */
    struct dirent *p_dirent = NULL;
    DIR *p_dir_stream = opendir("/spiffs");

    /* 扫描目录中的文件并添加到列表 */
    while (true)
    {
        p_dirent = readdir(p_dir_stream);
        if (NULL != p_dirent)
        {
            /* 为每个文件创建列表按钮 */
            lv_obj_t *btn = lv_list_add_btn(list, NULL, p_dirent->d_name);
            /* 注册点击事件回调，传递图片控件指针 */
            lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, (void *)img);
        }
        else
        {
            closedir(p_dir_stream);
            break;
        }
    }
}

/**
 * @brief 打印 SPIFFS 目录中的所有文件信息
 *
 * 用于调试，显示指定路径下所有文件的名称、inode 和类型。
 *
 * @param path 要扫描的目录路径
 */
static void SPIFFS_Directory(char *path)
{
    DIR *dir = opendir(path);
    assert(dir != NULL);
    while (true)
    {
        struct dirent *pe = readdir(dir);
        if (!pe)
            break;
        ESP_LOGI(__FUNCTION__, "d_name=%s d_ino=%d d_type=%x", pe->d_name, pe->d_ino, pe->d_type);
    }
    closedir(dir);
}

/**
 * @brief 应用程序主入口
 *
 * 初始化流程：
 * 1. 初始化 NVS 非易失性存储
 * 2. 挂载 SPIFFS 文件系统
 * 3. 初始化 LVGL 端口
 * 4. 初始化 LVGL 文件系统接口
 * 5. 创建图片浏览器界面
 */
void app_main(void)
{
    ESP_LOGI(TAG, "Compile time: %s %s", __DATE__, __TIME__);

    /* ===== 初始化 NVS ===== */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* ===== 初始化 SPIFFS 文件系统 ===== */
    ESP_LOGI(TAG, "Initializing SPIFFS");
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",         /* 挂载路径 */
        .partition_label = "storage",   /* 分区标签 */
        .max_files = 2,                 /* 最大同时打开文件数 */
        .format_if_mount_failed = true  /* 挂载失败时格式化 */
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        else if (ret == ESP_ERR_NOT_FOUND)
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        else
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        return;
    }

    /* 打印 SPIFFS 文件列表（调试用） */
    SPIFFS_Directory("/spiffs/");

    /* ===== 初始化 LVGL ===== */
    ksdiy_lvgl_port_init();   /* 初始化 LVGL 硬件端口 */
    lv_port_fs_init();        /* 初始化 LVGL 文件系统接口 */

    /* ===== 创建图片浏览器界面 ===== */
    if (ksdiy_lvgl_lock(0))
    {
        image_display();
        ksdiy_lvgl_unlock();
    }
}
