#include "defs.h"
#include "data.h"
#include "decl.h"

// 类型与类型处理
// Copyright (c) 2019 Warren Toomey, GPL3

// 如果类型是整型则返回真，任何大小，否则返回假
int inttype(int type) {
  return ((type & 0xf) == 0);
}

// 如果类型是指针类型则返回真
int ptrtype(int type) {
  return ((type & 0xf) != 0);
}

// 给定一个基本类型，返回指向该类型的指针类型
int pointer_to(int type) {
  if ((type & 0xf) == 0xf)
    fatald("Unrecognised in pointer_to: type", type);
  return (type + 1);
}

// 给定一个基本指针类型，返回它所指向的类型
int value_at(int type) {
  if ((type & 0xf) == 0x0)
    fatald("Unrecognised in value_at: type", type);
  return (type - 1);
}

// 给定一个类型和一个复合类型指针，返回该类型以字节为单位的大小
int typesize(int type, struct symtable *ctype) {
  if (type == P_STRUCT || type == P_UNION)
    return (ctype->size);
  return (genprimsize(type));
}

// 给定一个AST树和一个我们希望它成为的类型，
// 可能通过加宽或缩放来修改树，以便它与此类型兼容。
// 如果未发生更改则返回原始树，如果修改了树则返回修改后的树，
// 如果树与给定类型不兼容则返回NULL。
// 如果这将成为二元运算的一部分，则AST op不为零。
struct ASTnode *modify_type(struct ASTnode *tree, int rtype, int op) {
  int ltype;
  int lsize, rsize;

  ltype = tree->type;

  // XXX 这些还未知
  if (ltype == P_STRUCT || ltype == P_UNION)
    fatal("Don't know how to do this yet");
  if (rtype == P_STRUCT || rtype == P_UNION)
    fatal("Don't know how to do this yet");

  // 比较标量整型类型
  if (inttype(ltype) && inttype(rtype)) {

    // 类型相同，无需处理
    if (ltype == rtype)
      return (tree);

    // 获取每种类型的大小
    lsize = typesize(ltype, NULL);	// XXX 尽快修复
    rsize = typesize(rtype, NULL);	// XXX 尽快修复

    // 树的大小太大
    if (lsize > rsize)
      return (NULL);

    // 向右加宽
    if (rsize > lsize)
      return (mkastunary(A_WIDEN, rtype, tree, NULL, 0));
  }
  // 对于左侧的指针
  if (ptrtype(ltype)) {
    // 如果右侧类型相同且不是二元操作，则OK
    if (op == 0 && ltype == rtype)
      return (tree);
  }
  // 仅在A_ADD或A_SUBTRACT操作中可缩放
  if (op == A_ADD || op == A_SUBTRACT) {

    // 左为整型，右为指针类型，且原始类型的大小大于1：缩放左操作数
    if (inttype(ltype) && ptrtype(rtype)) {
      rsize = genprimsize(value_at(rtype));
      if (rsize > 1)
 	return (mkastunary(A_SCALE, rtype, tree, NULL, rsize));
      else
	return (tree);		// 大小为1，无需缩放
    }
  }
  // 如果执行到这里，类型不兼容
  return (NULL);
}