#pragma once

/*
 * Compile-time feature switches.
 * Set to 1 to build the feature, 0 to exclude it.
 */
#define APP_AI_ENABLE_HUMAN_FACE_DETECT 1
#define APP_AI_ENABLE_HUMAN_FACE_RECOGNITION 1
#define APP_AI_ENABLE_CAT_FACE_DETECT 1
#define APP_AI_ENABLE_COLOR_DETECT 1
#define APP_AI_ENABLE_MOTION_DETECT 1

typedef enum {
    APP_AI_FEATURE_HUMAN_FACE_DETECT = 1 << 0,
    APP_AI_FEATURE_HUMAN_FACE_RECOGNITION = 1 << 1,
    APP_AI_FEATURE_CAT_FACE_DETECT = 1 << 2,
    APP_AI_FEATURE_COLOR_DETECT = 1 << 3,
    APP_AI_FEATURE_MOTION_DETECT = 1 << 4,
} app_ai_feature_t;

/*
 * Select which AI features run at boot.
 *
 * These features can be combined in a pipeline:
 * - APP_AI_FEATURE_HUMAN_FACE_DETECT
 * - APP_AI_FEATURE_HUMAN_FACE_RECOGNITION
 * - APP_AI_FEATURE_CAT_FACE_DETECT
 * - APP_AI_FEATURE_COLOR_DETECT
 *
 * APP_AI_FEATURE_MOTION_DETECT currently runs as a standalone mode and should
 * not be combined with the others.
 *
 * Examples:
 *   #define APP_AI_ENABLED_FEATURES APP_AI_FEATURE_HUMAN_FACE_DETECT
 *   #define APP_AI_ENABLED_FEATURES (APP_AI_FEATURE_HUMAN_FACE_DETECT | APP_AI_FEATURE_COLOR_DETECT)
 */
#define APP_AI_ENABLED_FEATURES APP_AI_FEATURE_COLOR_DETECT

/*
 * Human face recognition database settings.
 * When auto-enroll is enabled and the database is empty, the first single face
 * seen by the camera will be enrolled as ID 1.
 */
#define APP_AI_RECOG_DB_PATH "/spiffs/face.db"
#define APP_AI_RECOG_AUTO_ENROLL_FIRST_FACE 1
