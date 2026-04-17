#include "defs.h"
#include "data.h"
#include "decl.h"

// 类型与类型处理
// Copyright (c) 2019 Warren Toomey, GPL3

// 如果类型是任意大小的 int 类型则返回 true，否则返回 false
int inttype(int type) {
  if (type == P_CHAR || type == P_INT || type == P_LONG)
    return (1);
  return (0);
}

// 如果类型是指针类型则返回 true
int ptrtype(int type) {
  if (type == P_VOIDPTR || type == P_CHARPTR ||
      type == P_INTPTR || type == P_LONGPTR)
    return (1);
  return (0);
}

// 给定一个原始类型，返回指向它的指针类型
int pointer_to(int type) {
  int newtype;
  switch (type) {
    case P_VOID:
      newtype = P_VOIDPTR;
      break;
    case P_CHAR:
      newtype = P_CHARPTR;
      break;
    case P_INT:
      newtype = P_INTPTR;
      break;
    case P_LONG:
      newtype = P_LONGPTR;
      break;
    default:
      fatald("Unrecognised in pointer_to: type", type);
  }
  return (newtype);
}

// 给定一个原始指针类型，返回它所指向的类型
int value_at(int type) {
  int newtype;
  switch (type) {
    case P_VOIDPTR:
      newtype = P_VOID;
      break;
    case P_CHARPTR:
      newtype = P_CHAR;
      break;
    case P_INTPTR:
      newtype = P_INT;
      break;
    case P_LONGPTR:
      newtype = P_LONG;
      break;
    default:
      fatald("Unrecognised in value_at: type", type);
  }
  return (newtype);
}

// 给定一个抽象语法树和我们希望它成为的类型，
// 通过扩展或缩放来修改树，使其与该类型兼容。
// 如果没有发生更改则返回原始树，修改后返回修改过的树，
// 如果树与给定类型不兼容则返回 NULL。
// 如果这是二元运算的一部分，则抽象语法树操作码不为零。
struct ASTnode *modify_type(struct ASTnode *tree, int rtype, int op) {
  int ltype;
  int lsize, rsize;

  ltype = tree->type;

  // 比较标量 int 类型
  if (inttype(ltype) && inttype(rtype)) {

    // 两种类型相同，无需处理
    if (ltype == rtype)
      return (tree);

    // 获取每种类型的大小
    lsize = genprimsize(ltype);
    rsize = genprimsize(rtype);

    // 树的大小太大
    if (lsize > rsize)
      return (NULL);

    // 向右扩展
    if (rsize > lsize)
      return (mkastunary(A_WIDEN, rtype, tree, 0));
  }
  // 对于左侧的指针
  if (ptrtype(ltype)) {
    // 如果右侧类型相同且不是二元运算，则正常
    if (op == 0 && ltype == rtype)
      return (tree);
  }
  // 只能在 A_ADD 或 A_SUBTRACT 操作时缩放
  if (op == A_ADD || op == A_SUBTRACT) {

    // 左侧是 int 类型，右侧是指针类型，且原始类型的大小大于 1：缩放左侧
    if (inttype(ltype) && ptrtype(rtype)) {
      rsize = genprimsize(value_at(rtype));
      if (rsize > 1)
	return (mkastunary(A_SCALE, rtype, tree, rsize));
      else
	return (tree);		// 大小为 1，无需缩放
    }
  }
  // 如果执行到这里，则类型不兼容
  return (NULL);
}