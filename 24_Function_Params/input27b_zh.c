// 用于测试函数调用和参数链接的 C 代码示例
// Copyright (c) 2019 Warren Toomey, GPL3

#include <stdio.h>
extern int param8(int a, int b, int c, int d, int e, int f, int g, int h);
extern int param5(int a, int b, int c, int d, int e);
extern int param2(int a, int b);
extern int param0();

int main() {
  param8(1,2,3,4,5,6,7,8); puts("--");
  param5(1,2,3,4,5); puts("--");
  param2(1,2); puts("--");
  param0();
  return(0);
}