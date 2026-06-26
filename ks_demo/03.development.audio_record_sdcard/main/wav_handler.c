/**
 * @file wav_handler.c
 * @brief WAV 文件处理模块
 *
 * 本文件实现 WAV 音频文件的读写功能：
 * - 创建标准 WAV 文件头
 * - 保存音频数据到 WAV 文件
 * - 从 WAV 文件加载音频数据
 *
 * WAV 文件格式参考：http://soundfile.sapp.org/doc/WaveFormat/
 *
 * @copyright Copyright (c) 2024 酷世DIY
 */

#include "wav_handler.h"
#include "esp_log.h"
#include <string.h>

/** @brief 日志标签 */
static const char* TAG = "wav_handler";

/**
 * @brief 创建 WAV 文件头
 *
 * 根据音频参数创建标准 WAV 文件头结构。
 *
 * @param header      WAV 文件头结构体指针
 * @param sample_rate 采样率（如 16000）
 * @param bit_depth   位深度（如 16）
 * @param channels    声道数（1=单声道，2=立体声）
 */
void wav_create_header(wav_header_t* header, uint32_t sample_rate,
                       uint16_t bit_depth, uint16_t channels)
{
    /* RIFF 块标识 */
    memcpy(header->riff_header, "RIFF", 4);
    memcpy(header->wave_header, "WAVE", 4);

    /* fmt 子块标识 */
    memcpy(header->fmt_header, "fmt ", 4);

    /* PCM 格式参数 */
    header->fmt_chunk_size = 16;        /* PCM 格式固定为 16 */
    header->audio_format = 1;           /* PCM = 1 */
    header->num_channels = channels;
    header->sample_rate = sample_rate;
    header->bit_depth = bit_depth;

    /* 计算派生参数 */
    header->byte_rate = sample_rate * channels * (bit_depth / 8);
    header->sample_alignment = channels * (bit_depth / 8);

    /* 数据块大小将在保存时更新 */
    header->data_bytes = 0;
    header->wav_size = 0;
}

/**
 * @brief 保存音频数据到 WAV 文件
 *
 * 将音频缓冲区中的数据保存为标准 WAV 文件。
 *
 * @param filename     文件名（完整路径）
 * @param audio_buffer 音频数据缓冲区
 * @param audio_size   音频数据大小（字节）
 * @return esp_err_t
 *    - ESP_OK: 保存成功
 *    - ESP_FAIL: 保存失败
 */
esp_err_t wav_save_file(const char* filename, uint8_t* audio_buffer,
                        size_t audio_size)
{
    /* 以二进制写模式打开文件 */
    FILE* fp = fopen(filename, "wb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }

    /* 创建 WAV 文件头 */
    wav_header_t header;
    wav_create_header(&header, 16000, 16, 1);  /* 16kHz, 16-bit, mono */

    /* 更新数据大小 */
    header.data_bytes = audio_size;
    header.wav_size = audio_size + sizeof(wav_header_t) - 8;

    /* 写入文件头 */
    fwrite(&header, sizeof(wav_header_t), 1, fp);

    /* 写入音频数据 */
    fwrite(audio_buffer, 1, audio_size, fp);

    fclose(fp);
    return ESP_OK;
}

/**
 * @brief 从 WAV 文件加载音频数据
 *
 * 读取 WAV 文件并验证格式，提取音频数据到缓冲区。
 *
 * @param filename     文件名（完整路径）
 * @param audio_buffer 音频数据缓冲区
 * @param max_size     缓冲区最大大小
 * @param loaded_size  实际加载的数据大小（输出参数）
 * @return esp_err_t
 *    - ESP_OK: 加载成功
 *    - ESP_FAIL: 加载失败（文件不存在或格式错误）
 */
esp_err_t wav_load_file(const char* filename, uint8_t* audio_buffer,
                        size_t max_size, size_t* loaded_size)
{
    /* 以二进制读模式打开文件 */
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }

    /* 读取 WAV 文件头 */
    wav_header_t header;
    fread(&header, sizeof(wav_header_t), 1, fp);

    /* 验证 WAV 文件格式 */
    if (memcmp(header.riff_header, "RIFF", 4) != 0 ||
        memcmp(header.wave_header, "WAVE", 4) != 0) {
        ESP_LOGE(TAG, "Invalid WAV file format");
        fclose(fp);
        return ESP_FAIL;
    }

    /* 检查数据大小是否超过缓冲区 */
    size_t to_read = header.data_bytes;
    if (to_read > max_size) {
        ESP_LOGE(TAG, "Audio data too large for buffer");
        fclose(fp);
        return ESP_FAIL;
    }

    /* 读取音频数据 */
    *loaded_size = fread(audio_buffer, 1, to_read, fp);
    fclose(fp);

    return ESP_OK;
}
