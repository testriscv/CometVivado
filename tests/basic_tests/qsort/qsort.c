#include <stdio.h>
#include <string.h>

int A[10] = {893, 40, 3233, 4267, 2669, 2541, 9073, 6023, 5681, 4622};

int partition(int low, int hi)
{
  int pivot = A[hi];
  int i     = low - 1, j;
  int temp;
  for (j = low; j < hi; j++) {
    if (A[j] < pivot) {
      i    = i + 1;
      temp = A[i];
      A[i] = A[j];
      A[j] = temp;
    }
  }
  if (A[hi] < A[i + 1]) {
    temp     = A[i + 1];
    A[i + 1] = A[hi];
    A[hi]    = temp;
  }
  return i + 1;
}

void qsort(int low, int hi)
{
  if (low < hi) {
    int p = partition(low, hi);
    qsort(low, p - 1);
    qsort(p + 1, hi);
  }
}

int main()
{
  int i;
  qsort(0, 10 - 1);
  for (i = 0; i < 10; ++i) {
    printf("%d\n", A[i]);
  }
}
