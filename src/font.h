#ifndef FONT_H
#define FONT_H

#include <stdint.h>

// 8x16 bitmap font definitions

// Lowercase letters (a-z)
extern const uint8_t font_a[16];
extern const uint8_t font_b[16];
extern const uint8_t font_c[16];
extern const uint8_t font_d[16];
extern const uint8_t font_e[16];
extern const uint8_t font_f[16];
extern const uint8_t font_g[16];
extern const uint8_t font_h[16];
extern const uint8_t font_i[16];
extern const uint8_t font_j[16];
extern const uint8_t font_k[16];
extern const uint8_t font_l[16];
extern const uint8_t font_m[16];
extern const uint8_t font_n[16];
extern const uint8_t font_o[16];
extern const uint8_t font_p[16];
extern const uint8_t font_q[16];
extern const uint8_t font_r[16];
extern const uint8_t font_s[16];
extern const uint8_t font_t[16];
extern const uint8_t font_u[16];
extern const uint8_t font_v[16];
extern const uint8_t font_w[16];
extern const uint8_t font_x[16];
extern const uint8_t font_y[16];
extern const uint8_t font_z[16];

// Uppercase letters (A-Z)
extern const uint8_t font_A[16];
extern const uint8_t font_B[16];
extern const uint8_t font_C[16];
extern const uint8_t font_D[16];
extern const uint8_t font_E[16];
extern const uint8_t font_F[16];
extern const uint8_t font_G[16];
extern const uint8_t font_H[16];
extern const uint8_t font_I[16];
extern const uint8_t font_J[16];
extern const uint8_t font_K[16];
extern const uint8_t font_L[16];
extern const uint8_t font_M[16];
extern const uint8_t font_N[16];
extern const uint8_t font_O[16];
extern const uint8_t font_P[16];
extern const uint8_t font_Q[16];
extern const uint8_t font_R[16];
extern const uint8_t font_S[16];
extern const uint8_t font_T[16];
extern const uint8_t font_U[16];
extern const uint8_t font_V[16];
extern const uint8_t font_W[16];
extern const uint8_t font_X[16];
extern const uint8_t font_Y[16];
extern const uint8_t font_Z[16];

// Digits (0-9)
extern const uint8_t font_0[16];
extern const uint8_t font_1[16];
extern const uint8_t font_2[16];
extern const uint8_t font_3[16];
extern const uint8_t font_4[16];
extern const uint8_t font_5[16];
extern const uint8_t font_6[16];
extern const uint8_t font_7[16];
extern const uint8_t font_8[16];
extern const uint8_t font_9[16];

// Special characters
extern const uint8_t font_space[16];
extern const uint8_t font_colon[16];
extern const uint8_t font_period[16];
extern const uint8_t font_slash[16];
extern const uint8_t font_dash[16];
extern const uint8_t font_percent[16];
extern const uint8_t font_degree[16];

// Function to get glyph for any character
const uint8_t* font_get_glyph(char c);

#endif // FONT_H