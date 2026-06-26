/**
 * @file format_wav.h
 * @brief WAV 文件格式定义
 *
 * 本头文件定义 WAV 音频文件格式的结构体和宏。
 * WAV 是 Microsoft 开发的音频文件格式，基于 RIFF 容器格式。
 *
 * WAV 文件结构：
 * 1. RIFF 描述块（descriptor_chunk）
 * 2. 格式块（fmt_chunk）
 * 3. 数据块（data_chunk）
 *
 * @note 参考: http://soundfile.sapp.org/doc/WaveFormat/
 *
 * @copyright Copyright (c) 2022 Espressif Systems (Shanghai) CO LTD
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WAV 文件头结构体（仅包含一个数据块）
 *
 * @note 此结构体仅适用于小端架构（包括 Xtensa 和 RISC-V）
 */
typedef struct {
    /**
     * @brief RIFF 描述块
     *
     * WAV 文件的起始部分，标识文件为 RIFF 格式。
     */
    struct {
        char chunk_id[4];       /**< 块标识，包含 "RIFF" ASCII 字符 */
        uint32_t chunk_size;    /**< 块大小（除前 8 字节外的文件大小） */
        char chunk_format[4];   /**< 格式标识，包含 "WAVE" */
    } descriptor_chunk;

    /**
     * @brief 格式块
     *
     * 描述音频数据的格式参数。
     */
    struct {
        char subchunk_id[4];        /**< 子块标识，包含 "fmt " */
        uint32_t subchunk_size;     /**< 子块大小（PCM 格式为 16） */
        uint16_t audio_format;      /**< 音频格式，PCM = 1，其他值表示压缩格式 */
        uint16_t num_of_channels;   /**< 声道数，单声道 = 1，立体声 = 2 */
        uint32_t sample_rate;       /**< 采样率，如 8000, 44100 等 */
        uint32_t byte_rate;         /**< 字节率 = 采样率 * 声道数 * 位深 / 8 */
        uint16_t block_align;       /**< 块对齐 = 声道数 * 位深 / 8 */
        uint16_t bits_per_sample;   /**< 位深，如 8, 16, 24 等 */
    } fmt_chunk;

    /**
     * @brief 数据块
     *
     * 包含实际的音频数据。
     */
    struct {
        char subchunk_id[4];    /**< 子块标识，包含 "data" */
        uint32_t subchunk_size; /**< 数据大小 = 样本数 * 声道数 * 位深 / 8 */
        int16_t data[0];        /**< 柔性数组，存储原始音频数据 */
    } data_chunk;
} wav_header_t;

/**
 * @brief PCM 格式 WAV 文件默认头
 *
 * 此宏用于生成 PCM 格式 WAV 文件的头部结构。
 *
 * @param wav_sample_size  音频数据大小（字节）
 * @param wav_sample_bits  位深（如 16）
 * @param wav_sample_rate  采样率（如 16000）
 * @param wav_channel_num  声道数（如 1 或 2）
 *
 * @note 使用示例：
 * @code
 * wav_header_t header = WAV_HEADER_PCM_DEFAULT(
 *     sample_size,    // 音频数据大小
 *     16,             // 16位
 *     16000,          // 16kHz
 *     1               // 单声道
 * );
 * @endcode
 */
#define WAV_HEADER_PCM_DEFAULT(wav_sample_size, wav_sample_bits, wav_sample_rate, wav_channel_num) { \
    .descriptor_chunk = { \
        .chunk_id = {'R', 'I', 'F', 'F'}, \
        .chunk_size = (wav_sample_size) + sizeof(wav_header_t) - 8, \
        .chunk_format = {'W', 'A', 'V', 'E'} \
    }, \
    .fmt_chunk = { \
        .subchunk_id = {'f', 'm', 't', ' '}, \
        .subchunk_size = 16, /* PCM 格式为 16 */ \
        .audio_format = 1,   /* PCM = 1 */ \
        .num_of_channels = (wav_channel_num), \
        .sample_rate = (wav_sample_rate), \
        .byte_rate = (wav_sample_bits) * (wav_sample_rate) * (wav_channel_num) / 8, \
        .block_align = (wav_sample_bits) * (wav_channel_num) / 8, \
        .bits_per_sample = (wav_sample_bits)\
    }, \
    .data_chunk = { \
        .subchunk_id = {'d', 'a', 't', 'a'}, \
        .subchunk_size = (wav_sample_size) \
    } \
}

#ifdef __cplusplus
}
#endif
