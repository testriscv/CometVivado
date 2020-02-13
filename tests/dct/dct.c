#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define ABS(a) ((a) < 0 ? -(a) : (a))
#define MULFIXED(a, b) (((a) * (b)) >> 15)

#define VAL00 (0.5 / 4.0)
#define VAL0x (0.7071067 / 4.0)
#define VALx0 (0.7071067 / 4.0)
#define VALxx (1.0 / 4.0)

#define C0 (2 * (0.7071068))
#define C1 (2 * (0.49039))
#define C2 (2 * (0.46194))
#define C3 (2 * (0.41573))
#define C4 (2 * (0.35355))
#define C5 (2 * (0.27779))
#define C6 (2 * (0.19134))
#define C7 (2 * (0.09755))

#define FXP_C0 46341
#define FXP_C1 32138
#define FXP_C2 30274
#define FXP_C3 27245
#define FXP_C4 23170
#define FXP_C5 18205
#define FXP_C6 12540
#define FXP_C7 6393

void fast_fixed_dct8x8(short pixel[8][8], short data[8][8]);
void fast_fixed_dct8(int in[8], int out[8]);

void fast_fixed_dct8x8(short pixel[8][8], short data[8][8])
{
  int ping[8][8];
  int pong[8][8];
  int u, v;

  for (u = 0; u < 8; u++)
    for (v = 0; v < 8; v++) {
      ping[u][v] = (int)pixel[u][v];
      //		printf("%f ", ping[u][v]);
    }
  for (u = 0; u < 8; u++)
    fast_fixed_dct8(&ping[u][0], &pong[u][0]);
  for (u = 0; u < 8; u++)
    for (v = 0; v < 8; v++)
      ping[u][v] = pong[v][u];
  for (u = 0; u < 8; u++)
    fast_fixed_dct8(&ping[u][0], &pong[u][0]);
  for (u = 0; u < 8; u++)
    for (v = 0; v < 8; v++) {
      data[u][v] = (short)pong[v][u];
    }
}

void fast_fixed_dct8(int in[8], int out[8])
{
  int k, n, i;
  int tmp[8];
  int tmp2[8];

  for (i = 0; i < 4; i++)
    tmp[i] = in[i] + in[7 - i];
  for (i = 4; i < 8; i++)
    tmp[i] = -in[i] + in[7 - i];

  tmp2[0] = tmp[0] + tmp[3];
  tmp2[1] = tmp[1] + tmp[2];
  tmp2[2] = tmp[1] - tmp[2];
  tmp2[3] = tmp[0] - tmp[3];
  tmp2[4] = tmp[4];
  tmp2[5] = MULFIXED(FXP_C4, tmp[6] - tmp[5]);
  tmp2[6] = MULFIXED(FXP_C4, tmp[5] + tmp[5]);
  tmp2[7] = tmp[7];

  tmp[0] = MULFIXED(FXP_C4, tmp2[0] + tmp2[1]);
  tmp[1] = MULFIXED(FXP_C4, tmp2[0] - tmp2[1]);
  tmp[2] = MULFIXED(FXP_C6, tmp2[2]) + MULFIXED(FXP_C2, tmp2[3]);
  tmp[3] = MULFIXED(FXP_C6, tmp2[3]) - MULFIXED(FXP_C2, tmp2[2]);
  tmp[4] = tmp2[4] + tmp2[5];
  tmp[5] = tmp2[4] - tmp2[5];
  tmp[6] = tmp2[7] - tmp2[6];
  tmp[7] = tmp2[6] + tmp2[7];

  out[0] = tmp[0] >> 1;
  out[4] = tmp[1] >> 1;
  out[2] = tmp[2] >> 1;
  out[6] = tmp[3] >> 1;
  out[1] = (MULFIXED(FXP_C7, tmp[4]) + MULFIXED(FXP_C1, tmp[7])) >> 1;
  out[5] = (MULFIXED(FXP_C3, tmp[5]) + MULFIXED(FXP_C5, tmp[6])) >> 1;
  out[3] = (MULFIXED(FXP_C3, tmp[6]) - MULFIXED(FXP_C5, tmp[5])) >> 1;
  out[7] = (MULFIXED(FXP_C7, tmp[7]) - MULFIXED(FXP_C1, tmp[4])) >> 1;
}

void print_block(short in[8][8])
{
  int i, j;
  for (i = 0; i < 8; i++) {
    for (j = 0; j < 8; j++) {
      printf("%d    ", in[i][j]);
    }
    printf("\n");
  }
}

int main()
{
  short insample[8][8] = {{139, 144, 149, 153, 155, 155, 155, 155}, {144, 151, 153, 156, 159, 156, 156, 156},
                          {150, 155, 160, 163, 158, 156, 156, 156}, {159, 161, 162, 160, 160, 159, 159, 159},
                          {159, 160, 161, 162, 162, 155, 155, 155}, {161, 161, 161, 161, 160, 157, 157, 157},
                          {162, 162, 161, 163, 162, 157, 157, 157}, {162, 162, 161, 161, 163, 158, 158, 158}};

  short refsample[8][8] = {{1260, -1, -12, -5, 2, -2, -3, 1}, {-23, -17, -6, -3, -3, 0, 0, -1},
                           {-11, -9, -2, 2, 0, -1, -1, 0},    {-7, -2, 0, 1, 1, 0, 0, 0},
                           {-1, -1, 1, 2, 0, -1, 1, 1},       {2, 0, 2, 0, -1, 1, 1, -1},
                           {-1, 0, 0, -1, 0, 2, 1, -1},       {-3, 2, -4, -2, 2, 1, -1, 0}};

  short fast_fixed_output[8][8];

  for (int oneIteration = 0; oneIteration < 512; oneIteration++) {
    fast_fixed_dct8x8(insample, fast_fixed_output);
    fast_fixed_dct8x8(fast_fixed_output, insample);
  }

  print_block(insample);
}
