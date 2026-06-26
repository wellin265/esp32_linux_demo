#ifndef WAV_HANDLER_H
#define WAV_HANDLER_H

#include <stdint.h>
#include "esp_err.h"

#pragma pack(1)
typedef struct {
    // RIFF Header
    char riff_header[4];    // Contains "RIFF"
    uint32_t wav_size;      // Size of WAV file - 8
    char wave_header[4];    // Contains "WAVE"
    
    // Format Header
    char fmt_header[4];     // Contains "fmt "
    uint32_t fmt_chunk_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t sample_alignment;
    uint16_t bit_depth;
    
    // Data Header
    char data_header[4];    // Contains "data"
    uint32_t data_bytes;    // Number of bytes in data
} wav_header_t;
#pragma pack()

/**
 * @brief 初始化WAV文件头
 * @param header WAV头结构指针
 * @param sample_rate 采样率
 * @param bit_depth 位深度
 * @param channels 通道数
 */
void wav_create_header(wav_header_t* header, uint32_t sample_rate, 
                      uint16_t bit_depth, uint16_t channels);

/**
 * @brief 将音频数据保存为WAV文件
 * @param filename 文件名
 * @param audio_buffer 音频数据
 * @param audio_size 音频数据大小
 * @return esp_err_t
 */
esp_err_t wav_save_file(const char* filename, uint8_t* audio_buffer, 
                       size_t audio_size);

/**
 * @brief 从WAV文件加载音频数据
 * @param filename 文件名
 * @param audio_buffer 音频缓冲区
 * @param max_size 最大缓冲区大小
 * @param loaded_size 实际加载的大小
 * @return esp_err_t
 */
esp_err_t wav_load_file(const char* filename, uint8_t* audio_buffer,
                       size_t max_size, size_t* loaded_size);

#endif