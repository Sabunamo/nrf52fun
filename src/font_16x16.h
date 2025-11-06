#ifndef FONT_16X16_H
#define FONT_16X16_H

#include <stdint.h>

// 16x16 bitmap font definitions
// Each character is 16 rows x 16 columns
// Each row is represented by uint16_t (16 pixels)

// Digits (0-9)
extern const uint16_t font_16x16_0[16];
extern const uint16_t font_16x16_1[16];
extern const uint16_t font_16x16_2[16];
extern const uint16_t font_16x16_3[16];
extern const uint16_t font_16x16_4[16];
extern const uint16_t font_16x16_5[16];
extern const uint16_t font_16x16_6[16];
extern const uint16_t font_16x16_7[16];
extern const uint16_t font_16x16_8[16];
extern const uint16_t font_16x16_9[16];

// Special characters
extern const uint16_t font_16x16_colon[16];
extern const uint16_t font_16x16_degree[16];  // Degree symbol (°)

// Uppercase letters (needed for prayer names)
extern const uint16_t font_16x16_C[16];  // Capital C (for °C)
extern const uint16_t font_16x16_F[16];  // Fajr
extern const uint16_t font_16x16_S[16];  // Shuruq
extern const uint16_t font_16x16_D[16];  // Dhuhr
extern const uint16_t font_16x16_A[16];  // Asr
extern const uint16_t font_16x16_M[16];  // Maghrib
extern const uint16_t font_16x16_I[16];  // Isha

// Lowercase letters (needed for prayer names)
extern const uint16_t font_16x16_a[16];  // Fajr
extern const uint16_t font_16x16_j[16];  // Fajr
extern const uint16_t font_16x16_r[16];  // Fajr, Dhuhr, Shuruq
extern const uint16_t font_16x16_h[16];  // Shuruq, Dhuhr
extern const uint16_t font_16x16_u[16];  // Shuruq, Dhuhr
extern const uint16_t font_16x16_q[16];  // Shuruq
extern const uint16_t font_16x16_s[16];  // Asr
extern const uint16_t font_16x16_g[16];  // Maghrib
extern const uint16_t font_16x16_b[16];  // Maghrib
extern const uint16_t font_16x16_i[16];  // Maghrib, Isha
extern const uint16_t font_16x16_e[16];  // Shuruq
extern const uint16_t font_16x16_n[16];  // Shuruq

// Function to get 16x16 glyph for any character
const uint16_t* font_get_glyph_16x16(char c);

#endif // FONT_16X16_H
