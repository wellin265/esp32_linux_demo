#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "ksdiy_example_display.h"
#include "sdmmc_cmd.h"

static const char *TAG = "example";
#define MOUNT_POINT "/sdcard"

static void update_sd_display(const char *line2, const char *line3)
{
    ksdiy_example_display_set_lines("SDMMC 1-bit", line2, line3);
}

void app_main(void)
{
    esp_err_t ret;
    ksdiy_example_display_bootstrap("02.beginner.sdmmc_sd", "SD card file demo");
    update_sd_display("init host", "");

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;

    ESP_LOGI(TAG, "Initializing SD card");
    ESP_LOGI(TAG, "Using SDMMC peripheral");
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;

#ifdef SOC_SDMMC_USE_GPIO_MATRIX
    slot_config.clk = GPIO_NUM_15;
    slot_config.cmd = GPIO_NUM_7;
    slot_config.d0 = GPIO_NUM_4;
#endif

    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(TAG, "Mounting filesystem");
    update_sd_display("mounting card", MOUNT_POINT);
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. If you want the card to be formatted, set the EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
            update_sd_display("mount failed", "filesystem error");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
            update_sd_display("card init failed", esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");
    update_sd_display("filesystem mounted", card->cid.name);

    sdmmc_card_print_info(stdout, card);

    const char *file_hello = MOUNT_POINT "/hello.txt";
    ESP_LOGI(TAG, "Opening file %s", file_hello);
    FILE *f = fopen(file_hello, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        update_sd_display("write open failed", file_hello);
        return;
    }

    fprintf(f, "Hello %s!\n", card->cid.name);
    fclose(f);
    ESP_LOGI(TAG, "File written");
    update_sd_display("wrote hello.txt", card->cid.name);

    const char *file_foo = MOUNT_POINT "/foo.txt";
    struct stat st;
    if (stat(file_foo, &st) == 0) {
        unlink(file_foo);
    }

    ESP_LOGI(TAG, "Renaming file %s to %s", file_hello, file_foo);
    if (rename(file_hello, file_foo) != 0) {
        ESP_LOGE(TAG, "Rename failed");
        update_sd_display("rename failed", "hello.txt -> foo.txt");
        return;
    }

    ESP_LOGI(TAG, "Reading file %s", file_foo);
    f = fopen(file_foo, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        update_sd_display("read open failed", file_foo);
        return;
    }

    char line[64];
    fgets(line, sizeof(line), f);
    fclose(f);

    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);
    update_sd_display("file check ok", line);

    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI(TAG, "Card unmounted");
    update_sd_display("card unmounted", "demo complete");
}
