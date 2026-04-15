#include "defs.h"
#include "data.h"
#include "decl.h"

// 声明的解析
// Copyright (c) 2019 Warren Toomey, GPL3


// declaration: 'int' identifier ';'  ;
//
// 解析一个变量的声明
void var_declaration(void) {

  // 确保有一个 'int' 词法单元，后跟一个标识符
  // 和一个分号。Text 现在有该标识符的名称。
  // 将其添加为已知标识符
  match(T_INT, "int");
  ident();
  addglob(Text);
  genglobsym(Text);
  semi();
}