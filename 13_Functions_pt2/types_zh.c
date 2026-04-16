#include "defs.h"
#include "data.h"
#include "decl.h"

// 类型和类型处理
// Copyright (c) 2019 Warren Toomey, GPL3

// 给定两个基本类型，
// 如果兼容则返回真，否则返回假。
// 同样，如果需要加宽以匹配另一个，
// 则返回零或 A_WIDEN 操作。
// 如果 onlyright 为真，则仅加宽左到右
int type_compatible(int *left, int *right, int onlyright) {
  int leftsize, rightsize;

  // 相同类型，它们是兼容的
  if (*left == *right) {
    *left = *right = 0;
    return (1);
  }
  // 获取每种类型的大小
  leftsize = genprimsize(*left);
  rightsize = genprimsize(*right);

  // 零大小的类型与任何类型都不兼容
  if ((leftsize == 0) || (rightsize == 0))
    return (0);

  // 根据需要加宽类型
  if (leftsize < rightsize) {
    *left = A_WIDEN;
    *right = 0;
    return (1);
  }
  if (rightsize < leftsize) {
    if (onlyright)
      return (0);
    *left = 0;
    *right = A_WIDEN;
    return (1);
  }
  // 剩余的任何大小相同因此兼容
  *left = *right = 0;
  return (1);
}
