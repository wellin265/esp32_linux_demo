/**
 * @file format_wav.h
 * @brief WAV 文件格式定义
 *
 * 本头文件定义了标准 WAV 文件格式的数据结构：
 * - RIFF 文件头
 * - fmt 子块（音频格式信息）
 * - data 子块（音频数据）
 *
 * WAV 文件格式参考：http://soundfile.sapp.org/doc/WaveFormat/
 *
 * @note 这些结构体仅适用于小端序架构（包括 Xtensa 和 RISC-V）
 *
 * @copyright SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 * @copyright SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WAV 文件头结构体
 *
 * WAV 文件由三个主要部分组成：
 * 1. RIFF 描述块（descriptor_chunk）
 * 2. 格式块（fmt_chunk）
 * 3. 数据块（data_chunk）
 *
 * @note 仅适用于单数据块的 WAV 文件
 */
typedef struct {
    /**
     * @brief RIFF 描述块
     *
     * WAV 文件以 RIFF 头开始，标识文件类型。
     */
    struct {
        char chunk_id[4];       /*!< 包含 "RIFF" ASCII 字符 */
        uint32_t chunk_size;    /*!< 文件大小 - 8 字节 */
        char chunk_format[4];   /*!< 包含 "WAVE" ASCII 字符 */
    } descriptor_chunk;

    /**
     * @brief fmt 格式块
     *
     * 描述音频数据的格式信息。
     */
    struct {
        char subchunk_id[4];        /*!< 包含 "fmt " ASCII 字符（注意末尾空格） */
        uint32_t subchunk_size;     /*!< 格式块大小（PCM 为 16） */
        uint16_t audio_format;      /*!< 音频格式：PCM = 1，其他值表示压缩格式 */
        uint16_t num_of_channels;   /*!< 声道数：单声道 = 1，立体声 = 2 */
        uint32_t sample_rate;       /*!< 采样率：8000、44100 等 */
        uint32_t byte_rate;         /*!< 字节率 = SampleRate * NumChannels * BitsPerSample / 8 */
        uint16_t block_align;       /*!< 块对齐 = NumChannels * BitsPerSample / 8 */
        uint16_t bits_per_sample;   /*!< 位深度：8 位 = 8，16 位 = 16 等 */
    } fmt_chunk;

    /**
     * @brief data 数据块
     *
     * 包含实际的音频数据。
     */
    struct {
        char subchunk_id[4];    /*!< 包含 "data" ASCII 字符 */
        uint32_t subchunk_size; /*!< 数据大小 = NumSamples * NumChannels * BitsPerSample / 8 */
        int16_t data[0];        /*!< 音频数据（柔性数组） */
    } data_chunk;
} wav_header_t;

/**
 * @brief 生成 PCM 格式 WAV 文件的默认头
 *
 * 此宏用于快速创建 WAV 文件头结构。
 *
 * @param wav_sample_size  音频数据大小（字节）
 * @param wav_sample_bits  位深度（如 16）
 * @param wav_sample_rate  采样率（如 16000）
 * @param wav_channel_num  声道数（1 或 2）
 *
 * @Example:
 * @code
 * wav_header_t header = WAV_HEADER_PCM_DEFAULT(data_size, 16, 16000, 1);
 * @endcode
 */
#define WAV_HEADER_PCM_DEFAULT(wav_sample_size, wav_sample_bits, wav_sample_rate, wav_channel_num) { \
    .descriptor_chunk = { \
        .chunk_id = {'R', 'I', 'F', 'F'}, \
        .chunk_size = (wav_sample_size) + sizeof(wav_header_t) - 8, \
        .chunk_format = {'W', 'A', 'V', 'E'}, \
    }, \
    .fmt_chunk = { \
        .subchunk_id = {'f', 'm', 't', ' '}, \
        .subchunk_size = 16, \
        .audio_format = 1, \
        .num_of_channels = (wav_channel_num), \
        .sample_rate = (wav_sample_rate), \
        .byte_rate = (wav_sample_rate) * (wav_channel_num) * (wav_sample_bits) / 8, \
        .block_align = (wav_channel_num) * (wav_sample_bits) / 8, \
        .bits_per_sample = (wav_sample_bits), \
    }, \
    .data_chunk = { \
        .subchunk_id = {'d', 'a', 't', 'a'}, \
        .subchunk_size = (wav_sample_size), \
    }, \
}

#ifdef __cplusplus
}
#endif
