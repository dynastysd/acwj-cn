#include "defs.h"
#include "data.h"
#include "decl.h"

// 声明解析
// Copyright (c) 2019 Warren Toomey, GPL3


// variable_declaration: 'int' identifier ';'  ;
//
// 解析变量的声明
void var_declaration(void) {

  // 确保有一个 'int' 词法单元，后跟一个标识符
  // 和一个分号。Text 现在有标识符的名称。
  // 将其添加为已知标识符
  match(T_INT, "int");
  ident();
  addglob(Text);
  genglobsym(Text);
  semi();
}

// 目前我们有一个非常简化的函数定义语法
//
// function_declaration: 'void' identifier '(' ')' compound_statement   ;
//
// 解析简化函数的声明
struct ASTnode *function_declaration(void) {
  struct ASTnode *tree;
  int nameslot;

  // 找到 'void'、标识符和 '(' ')'。
  // 目前，对它们不做任何处理
  match(T_VOID, "void");
  ident();
  nameslot= addglob(Text);
  lparen();
  rparen();

  // 获取复合语句的 AST 树
  tree= compound_statement();

  // 返回一个 A_FUNCTION 节点，它有函数的名字槽
  // 和复合语句子树
  return(mkastunary(A_FUNCTION, tree, nameslot));
}