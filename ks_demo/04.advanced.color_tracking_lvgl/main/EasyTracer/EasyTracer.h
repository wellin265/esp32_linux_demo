/**
 * @file EasyTracer.h
 * @brief EasyTracer 颜色追踪算法头文件
 *
 * EasyTracer 是一个轻量级的颜色追踪算法，基于 HSL 颜色空间
 * 进行目标识别。算法通过腐蚀操作确定目标的边界和中心位置。
 *
 * 算法特点：
 * - 基于 HSL 颜色空间，对光照变化有一定鲁棒性
 * - 支持自定义颜色参数（色调、饱和度、亮度）
 * - 支持目标尺寸过滤
 * - 迭代腐蚀提高识别精度
 *
 * @author Kevincoooool
 * @date 2021-06-19
 */

#ifndef EASY_TRACER_H
#define EASY_TRACER_H

/* ===== 图像参数配置 ===== */
#define IMG_X 0     /**< 图像 X 起始坐标 */
#define IMG_Y 0     /**< 图像 Y 起始坐标 */
#define IMG_W 240   /**< 图像宽度 */
#define IMG_H 240   /**< 图像高度 */

/* ===== 算法参数配置 ===== */
/**
 * @brief 容错率参数
 *
 * 允许 1<<ALLOW_FAIL_PER 个像素点识别失败。
 * 数值越大识别越宽松，但可能增加误识别。
 */
#define ALLOW_FAIL_PER 3

/**
 * @brief 迭代次数
 *
 * 腐蚀操作的迭代次数。迭代次数越多识别越精确，
 * 但处理时间越长。
 */
#define ITERATE_NUM 8

/**
 * @brief 目标条件结构体
 *
 * 定义要识别的目标颜色的 HSL 范围和尺寸限制。
 */
typedef struct {
    unsigned char H_MIN;    /**< 目标最小色调 (0-240) */
    unsigned char H_MAX;    /**< 目标最大色调 (0-240) */

    unsigned char S_MIN;    /**< 目标最小饱和度 (0-240) */
    unsigned char S_MAX;    /**< 目标最大饱和度 (0-240) */

    unsigned char L_MIN;    /**< 目标最小亮度 (0-240) */
    unsigned char L_MAX;    /**< 目标最大亮度 (0-240) */

    unsigned int WIDTH_MIN; /**< 目标最小宽度（像素） */
    unsigned int HIGHT_MIN; /**< 目标最小高度（像素） */

    unsigned int WIDTH_MAX; /**< 目标最大宽度（像素） */
    unsigned int HIGHT_MAX; /**< 目标最大高度（像素） */
} TARGET_CONDI;

/**
 * @brief 识别结果结构体
 *
 * 存储颜色识别的结果，包括目标的位置和尺寸。
 */
typedef struct {
    unsigned int x;  /**< 目标中心 X 坐标 */
    unsigned int y;  /**< 目标中心 Y 坐标 */
    unsigned int w;  /**< 目标宽度 */
    unsigned int h;  /**< 目标高度 */
} RESULT;

/**
 * @brief 执行颜色追踪
 *
 * 唯一的 API 函数，用户将识别条件写入 Condition 结构体，
 * 函数返回目标的 x、y 坐标和尺寸。
 *
 * @param Condition 目标颜色条件（HSL 范围和尺寸限制）
 * @param Resu 识别结果（位置和尺寸）
 * @return 1 识别成功，0 识别失败
 *
 * @note 使用示例：
 * @code
 * TARGET_CONDI condition = {10, 120, 70, 250, 10, 180, 40, 40, 320, 240};
 * RESULT result;
 * if (Trace(&condition, &result)) {
 *     printf("Found target at (%d, %d), size: %dx%d\n",
 *            result.x, result.y, result.w, result.h);
 * }
 * @endcode
 */
int Trace(const TARGET_CONDI *Condition, RESULT *Resu);

#endif /* EASY_TRACER_H */
