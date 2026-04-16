#include "defs.h"
#include "data.h"
#include "decl.h"

// 类型与类型处理
// Copyright (c) 2019 Warren Toomey, GPL3

// 给定两个原始类型，
// 如果它们兼容则返回 true，否则返回 false。
// 如果需要扩展以匹配另一个，还返回
// 零或 A_WIDEN 操作。
// 如果 onlyright 为 true，则仅从左向右扩展。
int type_compatible(int *left, int *right, int onlyright) {

  // Void 与任何类型都不兼容
  if ((*left == P_VOID) || (*right == P_VOID))
    return (0);

  // 相同类型，它们是兼容的
  if (*left == *right) {
    *left = *right = 0;
    return (1);
  }

  // 根据需要将 P_CHAR 扩展为 P_INT
  if ((*left == P_CHAR) && (*right == P_INT)) {
    *left = A_WIDEN;
    *right = 0;
    return (1);
  }
  if ((*left == P_INT) && (*right == P_CHAR)) {
    if (onlyright)
      return (0);
    *left = 0;
    *right = A_WIDEN;
    return (1);
  }
  // 任何剩余的类型对都是兼容的
  *left = *right = 0;
  return (1);
}