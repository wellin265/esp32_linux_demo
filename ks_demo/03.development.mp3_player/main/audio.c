/**
 * @file audio.c
 * @brief MP3 音频解码和播放控制模块
 *
 * 本文件实现 MP3 音频的解码和播放控制功能：
 * - 使用 helix_mp3 软件解码器解码 MP3 文件
 * - 通过 I2S 接口输出音频数据到 ES8311 DAC
 * - 支持播放/暂停、上一曲/下一曲控制
 * - 自动扫描指定目录下的 MP3 文件
 * - 支持多种采样率自动切换
 *
 * @copyright Copyright 2021 Espressif Systems (Shanghai) Co. Ltd.
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *               http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "audio.h"
#include <driver/i2s_std.h>
#include "app_speech.h"

/** @brief 日志标签 */
static const char *TAG = "audio";

/** @brief I2S 通道号 */
#define I2S_CHN I2S_NUM_0

/* ===== ID3 标签结构体定义 ===== */

/**
 * @brief ID3v1 标签结构体
 *
 * ID3v1 标签位于 MP3 文件末尾，固定 128 字节。
 * 包含歌曲标题、艺术家、专辑等信息。
 */
typedef struct
{
    char header[3];   /*!< 标识符，始终为 "TAG" */
    char title[30];   /*!< 歌曲标题 */
    char artist[30];  /*!< 艺术家名称 */
    char album[30];   /*!< 专辑名称 */
    char year[4];     /*!< 发行年份 */
    char comment[30]; /*!< 备注 */
    char genre;       /*!< 音乐类型 */
} __attribute__((packed)) mp3_id3_header_v1_t;

/**
 * @brief ID3v2 标签结构体
 *
 * ID3v2 标签位于 MP3 文件开头，变长。
 * 支持更丰富的元数据信息。
 */
typedef struct
{
    char header[3]; /*!< 标识符，始终为 "ID3" */
    char ver;       /*!< 版本号，ID3V2.3 为 3 */
    char revision;  /*!< 修订版本，通常为 0 */
    char flag;      /*!< 标志字节，仅使用 Bit[7..5] */
    char size[4];   /*!< 标签大小（同步安全整数） */
} __attribute__((packed)) mp3_id3_header_v2_t;

/* ===== 音频回调函数 ===== */

/** @brief 音频事件回调函数指针 */
static audio_cb_t s_audio_cb = NULL;

/** @brief 回调函数用户上下文 */
static void *audio_cb_usrt_ctx = NULL;

/**
 * @brief 注册音频事件回调函数
 *
 * @param call_back 回调函数指针
 * @param user_ctx 用户上下文指针
 * @return esp_err_t 注册结果
 */
esp_err_t audio_callback_register(audio_cb_t call_back, void *user_ctx)
{
    ESP_RETURN_ON_FALSE(esp_ptr_executable(call_back), ESP_ERR_INVALID_ARG,
                        TAG, "Not a valid call back");

    s_audio_cb = call_back;
    audio_cb_usrt_ctx = user_ctx;

    return ESP_OK;
}

/* ===== 音频文件扫描 ===== */

/** @brief 音频文件总数 */
static size_t audio_count = 0;

/** @brief 当前播放的音频索引 */
static size_t audio_index = 0;

/** @brief 音频文件名列表 */
static char **audio_list = NULL;

/**
 * @brief 扫描目录中的音频文件
 *
 * 遍历指定目录，获取所有音频文件名并存储到列表中。
 *
 * @param base_path 要扫描的目录路径
 * @return esp_err_t 扫描结果
 */
static esp_err_t audio_file_scan(char *base_path)
{
    audio_count = 0;
    struct dirent *p_dirent = NULL;
    DIR *p_dir_stream = opendir(base_path);

    /* 第一遍遍历：获取文件总数 */
    do
    {
        p_dirent = readdir(p_dir_stream);
        if (NULL != p_dirent)
        {
            audio_count++;
        }
        else
        {
            closedir(p_dir_stream);
            break;
        }
    } while (true);

    /* 分配文件列表内存 */
    audio_list = (char **)malloc(audio_count * sizeof(char *));
    ESP_RETURN_ON_FALSE(NULL != audio_list, ESP_ERR_NO_MEM,
                        TAG, "Failed allocate audio list buffer");

    /* 第二遍遍历：读取文件名 */
    p_dir_stream = opendir(base_path);
    for (size_t i = 0; i < audio_count; i++)
    {
        p_dirent = readdir(p_dir_stream);
        if (NULL != p_dirent)
        {
            audio_list[i] = malloc(sizeof(p_dirent->d_name));
            ESP_LOGI(TAG, "File : %s", strcpy(audio_list[i], p_dirent->d_name));
        }
        else
        {
            ESP_LOGE(TAG, "The file system may be corrupted");
            closedir(p_dir_stream);
            for (int j = i - 1; j >= 0; j--)
            {
                free(audio_list[i]);
            }
            free(audio_list);
            return ESP_ERR_INVALID_STATE;
        }
    }

    closedir(p_dir_stream);
    return ESP_OK;
}

/**
 * @brief 获取当前播放的音频索引
 *
 * @return size_t 当前音频索引
 */
size_t audio_get_index(void)
{
    return audio_index;
}

/**
 * @brief 根据索引获取音频文件名
 *
 * @param index 音频索引
 * @param base_path 基础路径（可选，用于拼接完整路径）
 * @return char* 文件名指针
 */
char *audio_get_name_from_index(size_t index, char *base_path)
{
    ESP_RETURN_ON_FALSE(index < audio_count, NULL,
                        TAG, "File index out of range");

    ESP_RETURN_ON_FALSE(NULL != audio_list, NULL,
                        TAG, "Audio file not found");

    ESP_RETURN_ON_FALSE(NULL != audio_list[index], NULL,
                        TAG, "Audio file not found");

    if (NULL != base_path)
    {
        strcat(strcat(base_path, "/"), audio_list[index]);
    }

    return audio_list[index];
}

/* ===== MP3 解码和播放 ===== */

/** @brief 音频事件队列句柄 */
static QueueHandle_t audio_event_queue = NULL;

/**
 * @brief 播放单个 MP3 文件
 *
 * 解码并播放指定的 MP3 文件：
 * 1. 解析 ID3 标签（跳过）
 * 2. 查找 MP3 同步字
 * 3. 解码 MP3 帧
 * 4. 根据采样率配置 I2S
 * 5. 输出音频数据到 DAC
 *
 * @param path MP3 文件路径
 * @return esp_err_t 播放结果
 */
static esp_err_t aplay_mp3(const char *path)
{
    ESP_LOGI(TAG, "start to decode %s", path);
    esp_err_t ret = ESP_OK;
    size_t bytes_write = 0;

    FILE *fp = NULL;
    int sample_rate = 0;
    uint8_t *output = NULL;
    uint8_t *read_buf = NULL;
    MP3FrameInfo frame_info;
    HMP3Decoder mp3_decoder = MP3InitDecoder();
    audio_event_t audio_event = AUDIO_EVENT_NONE;

    /* 创建 MP3 解码器 */
    ESP_RETURN_ON_FALSE(NULL != mp3_decoder, ESP_ERR_NO_MEM,
                        TAG, "Failed create MP3 decoder");

    /* 分配读取缓冲区 */
    read_buf = malloc(MAINBUF_SIZE);
    ESP_GOTO_ON_FALSE(NULL != read_buf, ESP_ERR_NO_MEM, clean_up,
                      TAG, "Failed allocate read buffer");

    /* 分配输出缓冲区（最大 1152 采样点 x 2 声道 x 2 字节） */
    output = malloc(1152 * sizeof(int16_t) * 2);
    ESP_GOTO_ON_FALSE(NULL != output, ESP_ERR_NO_MEM, clean_up,
                      TAG, "Failed allocate output buffer");

    /* 打开音频文件 */
    fp = fopen(path, "rb");
    ESP_GOTO_ON_FALSE(NULL != fp, ESP_ERR_NOT_FOUND, clean_up,
                      TAG, "File \"%s\" does not exist", path);

    /* 解析 ID3v2 标签 */
    mp3_id3_header_v2_t tag;
    if (sizeof(mp3_id3_header_v2_t) == fread(&tag, 1, sizeof(mp3_id3_header_v2_t), fp))
    {
        if (memcmp("ID3", (const void *)&tag, sizeof(tag.header)) == 0)
        {
            /* 计算标签长度（同步安全整数解码） */
            int tag_len =
                ((tag.size[0] & 0x7F) << 21) +
                ((tag.size[1] & 0x7F) << 14) +
                ((tag.size[2] & 0x7F) << 7) +
                ((tag.size[3] & 0x7F) << 0);
            /* 跳过 ID3v2 标签 */
            fseek(fp, tag_len - sizeof(mp3_id3_header_v2_t), SEEK_SET);
        }
        else
        {
            /* 没有 ID3v2 标签，从头开始读取 */
            fseek(fp, 0, SEEK_SET);
        }
    }

    /* 开始 MP3 解码循环 */
    int bytes_left = 0;
    unsigned char *read_ptr = read_buf;

    do
    {
        /* 处理来自其他任务的音频事件 */
        if (pdPASS == xQueueReceive(audio_event_queue, &audio_event, 0))
        {
            if (AUDIO_EVENT_PAUSE == audio_event)
            {
                /* 暂停：等待恢复事件 */
                xQueuePeek(audio_event_queue, &audio_event, portMAX_DELAY);
                continue;
            }

            if (AUDIO_EVENT_CHANGE == audio_event)
            {
                /* 切换歌曲：退出当前播放 */
                ret = ESP_FAIL;
                goto clean_up;
            }
        }

        /* 读取数据到缓冲区 */
        if (bytes_left < MAINBUF_SIZE)
        {
            memmove(read_buf, read_ptr, bytes_left);
            size_t bytes_read = fread(read_buf + bytes_left, 1, MAINBUF_SIZE - bytes_left, fp);

            ESP_GOTO_ON_FALSE(bytes_read > 0, ESP_OK, clean_up,
                              TAG, "No data read from strorage device");
            bytes_left = bytes_left + bytes_read;
            read_ptr = read_buf;
        }

        /* 查找 MP3 同步字 */
        int offset = MP3FindSyncWord(read_buf, MAINBUF_SIZE);

        if (offset >= 0)
        {
            read_ptr += offset;   /* 数据起始点 */
            bytes_left -= offset; /* 缓冲区剩余数据 */

            /* 解码 MP3 帧 */
            int mp3_dec_err = MP3Decode(mp3_decoder, &read_ptr, &bytes_left, (int16_t *)output, 0);
            ESP_GOTO_ON_FALSE(ERR_MP3_NONE == mp3_dec_err, ESP_FAIL, clean_up,
                              TAG, "Can't decode MP3 frame");

            /* 获取 MP3 帧信息并配置 I2S 时钟 */
            MP3GetLastFrameInfo(mp3_decoder, &frame_info);

            /* 采样率变化时重新配置音频设备 */
            if (sample_rate != frame_info.samprate)
            {
                ESP_LOGW(TAG, "MP3 nChans:%d samprate:%d bitsPerSample:%d",
                         frame_info.nChans, frame_info.samprate, frame_info.bitsPerSample);

                sample_rate = frame_info.samprate;

                /* 重新配置音频设备参数 */
                esp_codec_dev_close(output_dev_);
                esp_codec_dev_sample_info_t fs_out = {
                    .bits_per_sample = frame_info.bitsPerSample,
                    .channel = frame_info.nChans,
                    .channel_mask = 0,
                    .sample_rate = (uint32_t)frame_info.samprate,
                    .mclk_multiple = 0,
                };
                ESP_ERROR_CHECK(esp_codec_dev_open(output_dev_, &fs_out));
            }

            /* 将解码后的数据写入音频设备 */
            size_t i2s_bytes_written = 0;
            size_t output_size = frame_info.outputSamps * frame_info.nChans;
            esp_codec_dev_write(output_dev_, (void *)output, output_size);
        }
        else
        {
            /* 未找到同步字，尝试读取下一帧 */
            ESP_LOGE(TAG, "MP3 sync word not found");
            bytes_left = 0;
            continue;
        }
    } while (true);

/* 清理资源 */
clean_up:
    if (NULL != mp3_decoder)
        MP3FreeDecoder(mp3_decoder);
    if (NULL != fp)
        fclose(fp);
    if (NULL != read_buf)
        free(read_buf);
    if (NULL != output)
        free(output);

    return ret;
}

/**
 * @brief 音频播放任务
 *
 * 主要功能：
 * 1. 扫描音频文件
 * 2. 初始化音频编解码器
 * 3. 循环播放音频文件
 * 4. 处理播放控制事件
 *
 * @param pvParam 任务参数（音频文件目录路径）
 */
static void audio_task(void *pvParam)
{
    /* 扫描音频文件 */
    char *base_path = (char *)pvParam;
    if (NULL != base_path)
    {
        audio_file_scan(base_path);
        /* 通知 UI 文件扫描完成 */
        if (esp_ptr_executable(s_audio_cb))
        {
            audio_cb_ctx_t ctx = {
                .audio_event = AUDIO_EVENT_FILE_SCAN_DONE,
                .user_ctx = audio_cb_usrt_ctx,
            };
            s_audio_cb(&ctx);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Invalid base path");
        vTaskDelete(NULL);
    }

    /* 初始化音频编解码器 */
    Codec_I2S_init();

    /* 创建音频控制事件队列 */
    audio_event_queue = xQueueCreate(4, sizeof(audio_event_t));
    if (NULL == audio_event_queue)
    {
        vTaskDelete(NULL);
    }

    /* 获取第一首歌曲的文件名 */
    char full_name[256] = {[0 ... sizeof(full_name) - 1] = '\0'};
    strcpy(full_name, base_path);
    char *file_name = audio_get_name_from_index(audio_index, full_name);

    /* 开始播放循环 */
    while (vTaskDelay(1), NULL != file_name)
    {
        esp_err_t ret_val = aplay_mp3(full_name);

        /* 如果正常播放完成，自动切换到下一首 */
        if (ESP_OK == ret_val)
        {
            audio_index++;
            if (audio_index >= audio_count)
            {
                audio_index = 0;
            }
        }

        /* 通知 UI 音频索引变化 */
        if (esp_ptr_executable(s_audio_cb))
        {
            audio_cb_ctx_t ctx = {
                .audio_event = AUDIO_EVENT_CHANGE,
                .user_ctx = audio_cb_usrt_ctx,
            };
            s_audio_cb(&ctx);
        }

        /* 获取下一首歌曲的文件名 */
        file_name = audio_get_name_from_index(audio_index, strcpy(full_name, base_path));
    }

    /* 任务不应返回 */
    vTaskDelete(NULL);
}

/* ===== 音频播放控制函数 ===== */

/**
 * @brief 恢复播放
 *
 * @return esp_err_t 操作结果
 */
esp_err_t audio_play(void)
{
    ESP_RETURN_ON_FALSE(NULL != audio_event_queue, ESP_ERR_INVALID_STATE,
                        TAG, "Audio task not started yet");

    audio_event_t event = AUDIO_EVENT_PLAY;
    BaseType_t ret_val = xQueueSend(audio_event_queue, &event, 0);

    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_ERR_INVALID_STATE,
                        TAG, "The last event has not been processed yet");

    return ESP_OK;
}

/**
 * @brief 暂停播放
 *
 * @return esp_err_t 操作结果
 */
esp_err_t audio_pause(void)
{
    ESP_RETURN_ON_FALSE(NULL != audio_event_queue, ESP_ERR_INVALID_STATE,
                        TAG, "Audio task not started yet");

    audio_event_t event = AUDIO_EVENT_PAUSE;
    BaseType_t ret_val = xQueueSend(audio_event_queue, &event, 0);

    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_ERR_INVALID_STATE,
                        TAG, "The last event has not been processed yet");

    return ESP_OK;
}

/**
 * @brief 播放下一首
 *
 * @return esp_err_t 操作结果
 */
esp_err_t audio_play_next(void)
{
    ESP_RETURN_ON_FALSE(NULL != audio_event_queue, ESP_ERR_INVALID_STATE,
                        TAG, "Audio task not started yet");

    audio_index++;
    if (audio_index >= audio_count)
    {
        audio_index = 0;
    }
    audio_event_t event = AUDIO_EVENT_CHANGE;
    BaseType_t ret_val = xQueueSend(audio_event_queue, &event, 0);

    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_ERR_INVALID_STATE,
                        TAG, "The last event has not been processed yet");

    return ESP_OK;
}

/**
 * @brief 播放上一首
 *
 * @return esp_err_t 操作结果
 */
esp_err_t audio_play_prev(void)
{
    ESP_RETURN_ON_FALSE(NULL != audio_event_queue, ESP_ERR_INVALID_STATE,
                        TAG, "Audio task not started yet");

    if (audio_index == 0)
    {
        audio_index = audio_count;
    }
    audio_index--;
    audio_event_t event = AUDIO_EVENT_CHANGE;
    BaseType_t ret_val = xQueueSend(audio_event_queue, &event, 0);

    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_ERR_INVALID_STATE,
                        TAG, "The last event has not been processed yet");

    return ESP_OK;
}

/**
 * @brief 播放指定索引的歌曲
 *
 * @param index 歌曲索引
 * @return esp_err_t 操作结果
 */
esp_err_t audio_play_index(size_t index)
{
    ESP_RETURN_ON_FALSE(NULL != audio_event_queue, ESP_ERR_INVALID_STATE,
                        TAG, "Audio task not started yet");

    ESP_RETURN_ON_FALSE(index < audio_count, ESP_ERR_INVALID_ARG,
                        TAG, "File index out of range");

    audio_index = index;
    audio_event_t event = AUDIO_EVENT_CHANGE;
    BaseType_t ret_val = xQueueSend(audio_event_queue, &event, 0);

    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_ERR_INVALID_STATE,
                        TAG, "The last event has not been processed yet");

    return ESP_OK;
}

/**
 * @brief 启动 MP3 播放器
 *
 * 创建音频播放任务并开始播放。
 *
 * @param file_path 音频文件目录路径
 * @return esp_err_t 启动结果
 */
esp_err_t mp3_player_start(char *file_path)
{
    BaseType_t ret_val = xTaskCreatePinnedToCore(
        (TaskFunction_t)audio_task,
        (const char *const)"Audio Task",
        (const uint32_t)4 * 1024,
        (void *const)file_path,
        (UBaseType_t)configMAX_PRIORITIES - 1,
        (TaskHandle_t *const)NULL,
        (const BaseType_t)0);

    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_ERR_NO_MEM,
                        TAG, "Failed create audio task");

    return ESP_OK;
}
