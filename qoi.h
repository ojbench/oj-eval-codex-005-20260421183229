#ifndef QOI_FORMAT_CODEC_QOI_H_
#define QOI_FORMAT_CODEC_QOI_H_

#include "utils.h"

constexpr uint8_t QOI_OP_INDEX_TAG = 0x00;
constexpr uint8_t QOI_OP_DIFF_TAG  = 0x40;
constexpr uint8_t QOI_OP_LUMA_TAG  = 0x80;
constexpr uint8_t QOI_OP_RUN_TAG   = 0xc0; 
constexpr uint8_t QOI_OP_RGB_TAG   = 0xfe;
constexpr uint8_t QOI_OP_RGBA_TAG  = 0xff;
constexpr uint8_t QOI_PADDING[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u};
constexpr uint8_t QOI_MASK_2 = 0xc0;

/**
 * @brief encode the raw pixel data of an image to qoi format.
 *
 * @param[in] width image width in pixels
 * @param[in] height image height in pixels
 * @param[in] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[in] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace = 0);

/**
 * @brief decode the qoi format of an image to raw pixel data
 *
 * @param[out] width image width in pixels
 * @param[out] height image height in pixels
 * @param[out] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[out] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace);


bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace) {

    // qoi-header part

    // write magic bytes "qoif"
    QoiWriteChar('q');
    QoiWriteChar('o');
    QoiWriteChar('i');
    QoiWriteChar('f');
    // write image width
    QoiWriteU32(width);
    // write image height
    QoiWriteU32(height);
    // write channel number
    QoiWriteU8(channels);
    // write color space specifier
    QoiWriteU8(colorspace);

    /* qoi-data part */
    int run = 0;
    int px_num = width * height;

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t r, g, b, a;
    r = 0u; g = 0u; b = 0u; a = 255u;
    uint8_t pre_r, pre_g, pre_b, pre_a;
    pre_r = 0u;
    pre_g = 0u;
    pre_b = 0u;
    pre_a = 255u;

    for (int i = 0; i < px_num; ++i) {
        r = QoiReadU8();
        g = QoiReadU8();
        b = QoiReadU8();
        if (channels == 4) a = QoiReadU8();

        // QOI_OP_RUN
        if (r == pre_r && g == pre_g && b == pre_b && a == pre_a) {
            run++;
            if (run == 62 || i == px_num - 1) {
                QoiWriteU8(QOI_OP_RUN_TAG | (run - 1));
                run = 0;
            }
            continue;
        }

        if (run > 0) {
            QoiWriteU8(QOI_OP_RUN_TAG | (run - 1));
            run = 0;
        }

        // QOI_OP_INDEX
        int idx = QoiColorHash(r, g, b, a);
        if (history[idx][0] == r && history[idx][1] == g && history[idx][2] == b && history[idx][3] == a) {
            QoiWriteU8(QOI_OP_INDEX_TAG | idx);
        } else {
            // QOI_OP_DIFF / LUMA / RGB(A)
            int dr = (int)r - (int)pre_r;
            int dg = (int)g - (int)pre_g;
            int db = (int)b - (int)pre_b;
            int da = (int)a - (int)pre_a;

            if (da == 0 && dr >= -2 && dr <= 1 && dg >= -2 && dg <= 1 && db >= -2 && db <= 1) {
                // QOI_OP_DIFF
                uint8_t b0 = QOI_OP_DIFF_TAG | ((dr + 2) << 4) | ((dg + 2) << 2) | (db + 2);
                QoiWriteU8(b0);
            } else if (da == 0) {
                int dg_l = dg;
                int dr_dg = dr - dg_l;
                int db_dg = db - dg_l;
                if (dg_l >= -32 && dg_l <= 31 && dr_dg >= -8 && dr_dg <= 7 && db_dg >= -8 && db_dg <= 7) {
                    // QOI_OP_LUMA
                    uint8_t b0 = QOI_OP_LUMA_TAG | (uint8_t)(dg_l + 32);
                    uint8_t b1 = ((dr_dg + 8) << 4) | (db_dg + 8);
                    QoiWriteU8(b0);
                    QoiWriteU8(b1);
                } else {
                    // QOI_OP_RGB
                    QoiWriteU8(QOI_OP_RGB_TAG);
                    QoiWriteU8(r);
                    QoiWriteU8(g);
                    QoiWriteU8(b);
                }
            } else {
                // QOI_OP_RGBA
                QoiWriteU8(QOI_OP_RGBA_TAG);
                QoiWriteU8(r);
                QoiWriteU8(g);
                QoiWriteU8(b);
                QoiWriteU8(a);
            }

            // update index
            history[idx][0] = r;
            history[idx][1] = g;
            history[idx][2] = b;
            history[idx][3] = a;
        }

        pre_r = r;
        pre_g = g;
        pre_b = b;
        pre_a = a;
    }

    // qoi-padding part
    for (int i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        QoiWriteU8(QOI_PADDING[i]);
    }

    return true;
}

bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace) {

    char c1 = QoiReadChar();
    char c2 = QoiReadChar();
    char c3 = QoiReadChar();
    char c4 = QoiReadChar();
    if (c1 != 'q' || c2 != 'o' || c3 != 'i' || c4 != 'f') {
        return false;
    }

    // read image width
    width = QoiReadU32();
    // read image height
    height = QoiReadU32();
    // read channel number
    channels = QoiReadU8();
    // read color space specifier
    colorspace = QoiReadU8();

    int run = 0;
    int px_num = width * height;

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    uint8_t r, g, b, a;
    a = 255u;

    for (int i = 0; i < px_num; ++i) {
        if (run > 0) {
            run--;
        } else {
            uint8_t b0 = QoiReadU8();
            if (b0 == QOI_OP_RGB_TAG) {
                r = QoiReadU8();
                g = QoiReadU8();
                b = QoiReadU8();
            } else if (b0 == QOI_OP_RGBA_TAG) {
                r = QoiReadU8();
                g = QoiReadU8();
                b = QoiReadU8();
                a = QoiReadU8();
            } else if ((b0 & QOI_MASK_2) == QOI_OP_INDEX_TAG) {
                int idx = b0 & 0x3f;
                r = history[idx][0];
                g = history[idx][1];
                b = history[idx][2];
                a = history[idx][3];
            } else if ((b0 & QOI_MASK_2) == QOI_OP_DIFF_TAG) {
                int dr = ((b0 >> 4) & 0x03) - 2;
                int dg = ((b0 >> 2) & 0x03) - 2;
                int db = (b0 & 0x03) - 2;
                r = (uint8_t)(r + dr);
                g = (uint8_t)(g + dg);
                b = (uint8_t)(b + db);
            } else if ((b0 & QOI_MASK_2) == QOI_OP_LUMA_TAG) {
                int dg = (b0 & 0x3f) - 32;
                uint8_t b1 = QoiReadU8();
                int dr_dg = ((b1 >> 4) & 0x0f) - 8;
                int db_dg = (b1 & 0x0f) - 8;
                r = (uint8_t)(r + dg + dr_dg);
                g = (uint8_t)(g + dg);
                b = (uint8_t)(b + dg + db_dg);
            } else if ((b0 & QOI_MASK_2) == QOI_OP_RUN_TAG) {
                run = (b0 & 0x3f);
            }
        }

        int idx = QoiColorHash(r, g, b, a);
        history[idx][0] = r;
        history[idx][1] = g;
        history[idx][2] = b;
        history[idx][3] = a;

        QoiWriteU8(r);
        QoiWriteU8(g);
        QoiWriteU8(b);
        if (channels == 4) QoiWriteU8(a);
    }

    bool valid = true;
    for (int i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        if (QoiReadU8() != QOI_PADDING[i]) valid = false;
    }

    return valid;
}

#endif // QOI_FORMAT_CODEC_QOI_H_
