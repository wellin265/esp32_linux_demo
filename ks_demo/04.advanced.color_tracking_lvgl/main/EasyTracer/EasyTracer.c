/**
 * @file EasyTracer.c
 * @brief EasyTracer 颜色追踪算法实现
 *
 * 本文件实现基于 HSL 颜色空间的轻量级颜色追踪算法。
 * 算法流程：
 * 1. 读取像素 RGB 值并转换为 HSL
 * 2. 在搜索区域内查找符合颜色条件的中心点
 * 3. 从中心点向四个方向腐蚀，确定目标边界
 * 4. 迭代优化识别结果
 *
 * @author Kevincoooool
 * @date 2021-06-19
 */

#include "EasyTracer.h"

/* ===== 辅助宏定义 ===== */
/** @brief 三个值中的最小值 */
#define min3v(v1, v2, v3) ((v1) > (v2) ? ((v2) > (v3) ? (v3) : (v2)) : ((v1) > (v3) ? (v3) : (v1)))

/** @brief 三个值中的最大值 */
#define max3v(v1, v2, v3) ((v1) < (v2) ? ((v2) < (v3) ? (v3) : (v2)) : ((v1) < (v3) ? (v3) : (v1)))

/**
 * @brief RGB 颜色结构体
 */
typedef struct
{
    unsigned char red;    /**< 红色分量 [0,255] */
    unsigned char green;  /**< 绿色分量 [0,255] */
    unsigned char blue;   /**< 蓝色分量 [0,255] */
} COLOR_RGB;

/**
 * @brief HSL 颜色结构体
 */
typedef struct
{
    unsigned char hue;         /**< 色调 [0,240] */
    unsigned char saturation;  /**< 饱和度 [0,240] */
    unsigned char luminance;   /**< 亮度 [0,240] */
} COLOR_HSL;

/**
 * @brief 搜索区域结构体
 */
typedef struct
{
    unsigned int X_Start;  /**< X 起始坐标 */
    unsigned int X_End;    /**< X 结束坐标 */
    unsigned int Y_Start;  /**< Y 起始坐标 */
    unsigned int Y_End;    /**< Y 结束坐标 */
} SEARCH_AREA;

/* ===== 外部函数声明 ===== */
/**
 * @brief 读取指定坐标的 RGB565 像素值
 *
 * 由外部模块实现，用于读取图像缓冲区中的像素。
 */
extern unsigned short RGB_ReadBit16Point(unsigned short x, unsigned short y);

/**
 * @brief 读取指定坐标的颜色值并转换为 RGB
 *
 * @param x X 坐标
 * @param y Y 坐标
 * @param Rgb 输出的 RGB 颜色值
 */
static void ReadColor(unsigned int x, unsigned int y, COLOR_RGB *Rgb)
{
    unsigned short C16;

    /* 读取 RGB565 像素值 */
    C16 = RGB_ReadBit16Point(x, y);

    /* 提取 RGB 分量（从 RGB565 转换为 RGB888） */
    Rgb->red = (unsigned char)((C16 & 0xf800) >> 8);
    Rgb->green = (unsigned char)((C16 & 0x07e0) >> 3);
    Rgb->blue = (unsigned char)((C16 & 0x001f) << 3);
}

/**
 * @brief RGB 转 HSL 颜色空间
 *
 * 将 RGB 颜色值转换为 HSL（色调、饱和度、亮度）值。
 * HSL 颜色空间对光照变化更鲁棒，适合颜色识别。
 *
 * @param Rgb 输入的 RGB 颜色值
 * @param Hsl 输出的 HSL 颜色值
 */
static void RGBtoHSL(const COLOR_RGB *Rgb, COLOR_HSL *Hsl)
{
    int h = 0, s = 0, l = 0, maxVal = 0, minVal = 0, difVal = 0;
    int r = Rgb->red;
    int g = Rgb->green;
    int b = Rgb->blue;

    /* 计算最大值和最小值 */
    maxVal = max3v(r, g, b);
    minVal = min3v(r, g, b);
    difVal = maxVal - minVal;

    /* 计算亮度 */
    l = (maxVal + minVal) * 240 / 255 / 2;

    if (maxVal == minVal)
    {
        /* 灰度图像（r=g=b） */
        h = 0;
        s = 0;
    }
    else
    {
        /* 计算色调 */
        if (maxVal == r)
        {
            if (g >= b)
                h = 40 * (g - b) / (difVal);
            else
                h = 40 * (g - b) / (difVal) + 240;
        }
        else if (maxVal == g)
            h = 40 * (b - r) / (difVal) + 80;
        else if (maxVal == b)
            h = 40 * (r - g) / (difVal) + 160;

        /* 计算饱和度 */
        if (l == 0)
            s = 0;
        else if (l <= 120)
            s = (difVal) * 240 / (maxVal + minVal);
        else
            s = (difVal) * 240 / (511 - (maxVal + minVal));
    }

    /* 限制输出范围 [0, 240] */
    Hsl->hue = (unsigned char)(((h > 240) ? 240 : ((h < 0) ? 0 : h)));
    Hsl->saturation = (unsigned char)(((s > 240) ? 240 : ((s < 0) ? 0 : s)));
    Hsl->luminance = (unsigned char)(((l > 240) ? 240 : ((l < 0) ? 0 : l)));
}

/**
 * @brief 颜色匹配
 *
 * 检查像素的 HSL 值是否在目标颜色范围内。
 *
 * @param Hsl 待检测的 HSL 值
 * @param Condition 目标颜色条件
 * @return 1 匹配成功，0 匹配失败
 */
static int ColorMatch(const COLOR_HSL *Hsl, const TARGET_CONDI *Condition)
{
    if (
        Hsl->hue > Condition->H_MIN &&
        Hsl->hue < Condition->H_MAX &&
        Hsl->saturation > Condition->S_MIN &&
        Hsl->saturation < Condition->S_MAX &&
        Hsl->luminance > Condition->L_MIN &&
        Hsl->luminance < Condition->L_MAX)
        return 1;
    else
        return 0;
}

/**
 * @brief 搜索目标中心点
 *
 * 在指定区域内使用粗搜索（腐蚀）算法查找符合颜色条件的中心点。
 *
 * @param x 输出的中心 X 坐标
 * @param y 输出的中心 Y 坐标
 * @param Condition 目标颜色条件
 * @param Area 搜索区域
 * @return 1 找到中心点，0 未找到
 */
static int SearchCentre(unsigned int *x, unsigned int *y, const TARGET_CONDI *Condition, const SEARCH_AREA *Area)
{
    unsigned int SpaceX, SpaceY, i, j, k, FailCount = 0;
    COLOR_RGB Rgb;
    COLOR_HSL Hsl;

    /* 计算搜索步长（根据目标最小尺寸） */
    SpaceX = Condition->WIDTH_MIN / 3;
    SpaceY = Condition->HIGHT_MIN / 3;

    /* 遍历搜索区域 */
    for (i = Area->Y_Start; i < Area->Y_End; i += SpaceY)
    {
        for (j = Area->X_Start; j < Area->X_End; j += SpaceX)
        {
            FailCount = 0;

            /* 在十字形区域内检测颜色 */
            for (k = 0; k < SpaceX + SpaceY; k++)
            {
                if (k < SpaceX)
                    ReadColor(j + k, i + SpaceY / 2, &Rgb);      /* 水平方向 */
                else
                    ReadColor(j + SpaceX / 2, i + (k - SpaceX), &Rgb);  /* 垂直方向 */

                RGBtoHSL(&Rgb, &Hsl);

                if (!ColorMatch(&Hsl, Condition))
                    FailCount++;

                /* 超过容错阈值则跳过此位置 */
                if (FailCount > ((SpaceX + SpaceY) >> ALLOW_FAIL_PER))
                    break;
            }

            /* 成功找到中心点 */
            if (k == SpaceX + SpaceY)
            {
                *x = j + SpaceX / 2;
                *y = i + SpaceY / 2;
                return 1;
            }
        }
    }
    return 0;
}

/**
 * @brief 腐蚀操作
 *
 * 从给定的中心点向四个方向扩展，确定目标的边界。
 * 通过腐蚀操作可以得到更精确的目标位置和尺寸。
 *
 * @param oldx 起始 X 坐标
 * @param oldy 起始 Y 坐标
 * @param Condition 目标颜色条件
 * @param Resu 输出的识别结果
 * @return 1 识别成功，0 识别失败
 */
static int Corrode(unsigned int oldx, unsigned int oldy, const TARGET_CONDI *Condition, RESULT *Resu)
{
    unsigned int Xmin, Xmax, Ymin, Ymax, i, FailCount = 0;
    COLOR_RGB Rgb;
    COLOR_HSL Hsl;

    /* 向左腐蚀，找左边界 */
    for (i = oldx; i > IMG_X; i--)
    {
        ReadColor(i, oldy, &Rgb);
        RGBtoHSL(&Rgb, &Hsl);
        if (!ColorMatch(&Hsl, Condition))
            FailCount++;
        if (FailCount > (((Condition->WIDTH_MIN + Condition->WIDTH_MAX) >> 2) >> ALLOW_FAIL_PER))
            break;
    }
    Xmin = i;
    FailCount = 0;

    /* 向右腐蚀，找右边界 */
    for (i = oldx; i < IMG_X + IMG_W; i++)
    {
        ReadColor(i, oldy, &Rgb);
        RGBtoHSL(&Rgb, &Hsl);
        if (!ColorMatch(&Hsl, Condition))
            FailCount++;
        if (FailCount > (((Condition->WIDTH_MIN + Condition->WIDTH_MAX) >> 2) >> ALLOW_FAIL_PER))
            break;
    }
    Xmax = i;
    FailCount = 0;

    /* 向上腐蚀，找上边界 */
    for (i = oldy; i > IMG_Y; i--)
    {
        ReadColor(oldx, i, &Rgb);
        RGBtoHSL(&Rgb, &Hsl);
        if (!ColorMatch(&Hsl, Condition))
            FailCount++;
        if (FailCount > (((Condition->HIGHT_MIN + Condition->HIGHT_MAX) >> 2) >> ALLOW_FAIL_PER))
            break;
    }
    Ymin = i;
    FailCount = 0;

    /* 向下腐蚀，找下边界 */
    for (i = oldy; i < IMG_Y + IMG_H; i++)
    {
        ReadColor(oldx, i, &Rgb);
        RGBtoHSL(&Rgb, &Hsl);
        if (!ColorMatch(&Hsl, Condition))
            FailCount++;
        if (FailCount > (((Condition->HIGHT_MIN + Condition->HIGHT_MAX) >> 2) >> ALLOW_FAIL_PER))
            break;
    }
    Ymax = i;

    /* 计算目标中心点和尺寸 */
    Resu->x = (Xmin + Xmax) / 2;
    Resu->y = (Ymin + Ymax) / 2;
    Resu->w = Xmax - Xmin;
    Resu->h = Ymax - Ymin;

    /* 验证目标尺寸是否在有效范围内 */
    if (((Xmax - Xmin) > (Condition->WIDTH_MIN)) && ((Ymax - Ymin) > (Condition->HIGHT_MIN)) &&
        ((Xmax - Xmin) < (Condition->WIDTH_MAX)) && ((Ymax - Ymin) < (Condition->HIGHT_MAX)))
        return 1;
    else
        return 0;
}

/**
 * @brief 执行颜色追踪（主 API）
 *
 * 唯一的 API 函数，用户将识别条件写入 Condition 结构体，
 * 函数返回目标的 x、y 坐标和尺寸。
 *
 * 算法流程：
 * 1. 在搜索区域内查找符合颜色条件的中心点
 * 2. 从中心点进行多次迭代腐蚀
 * 3. 验证识别结果的有效性
 * 4. 更新下次搜索的区域（缩小范围）
 *
 * @param Condition 目标颜色条件（HSL 范围和尺寸限制）
 * @param Resu 识别结果（位置和尺寸）
 * @return 1 识别成功，0 识别失败
 */
int Trace(const TARGET_CONDI *Condition, RESULT *Resu)
{
    unsigned int i;
    static unsigned int x0, y0, flag = 0;
    static SEARCH_AREA Area = {IMG_X, IMG_X + IMG_W, IMG_Y, IMG_Y + IMG_H};
    RESULT Result;

    if (flag == 0)
    {
        /* 首次搜索或丢失目标后全图搜索 */
        if (SearchCentre(&x0, &y0, Condition, &Area))
            flag = 1;
        else
        {
            /* 重置搜索区域为全图 */
            Area.X_Start = IMG_X;
            Area.X_End = IMG_X + IMG_W;
            Area.Y_Start = IMG_Y;
            Area.Y_End = IMG_Y + IMG_H;

            if (SearchCentre(&x0, &y0, Condition, &Area))
            {
                flag = 0;
                return 0;
            }
        }
    }
    Result.x = x0;
    Result.y = y0;

    /* 迭代腐蚀以提高精度 */
    for (i = 0; i < ITERATE_NUM; i++)
        Corrode(Result.x, Result.y, Condition, &Result);

    /* 最终腐蚀并验证结果 */
    if (Corrode(Result.x, Result.y, Condition, &Result))
    {
        /* 保存当前位置用于下次搜索 */
        x0 = Result.x;
        y0 = Result.y;
        Resu->x = Result.x;
        Resu->y = Result.y;
        Resu->w = Result.w;
        Resu->h = Result.h;
        flag = 1;

        /* 缩小下次搜索的区域（提高效率） */
        Area.X_Start = Result.x - ((Result.w) >> 1);
        Area.X_End = Result.x + ((Result.w) >> 1);
        Area.Y_Start = Result.y - ((Result.h) >> 1);
        Area.Y_End = Result.y + ((Result.h) >> 1);

        return 1;
    }
    else
    {
        /* 识别失败，重置标志以便下次全图搜索 */
        flag = 0;
        return 0;
    }
}
