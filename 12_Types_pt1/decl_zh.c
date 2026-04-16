#include "defs.h"
#include "data.h"
#include "decl.h"

// 声明解析
// Copyright (c) 2019 Warren Toomey, GPL3


// 解析当前 token 并返回一个原始类型枚举值
int parse_type(int t) {
  if (t == T_CHAR)
    return (P_CHAR);
  if (t == T_INT)
    return (P_INT);
  if (t == T_VOID)
    return (P_VOID);
  fatald("Illegal type, token", t);
}

// variable_declaration: 'int' identifier ';'  ;
//
// 解析变量的声明
void var_declaration(void) {
  int id, type;

  // 获取变量的类型，然后是标识符
  type = parse_type(Token.token);
  scan(&Token);
  ident();
  // Text 现在有标识符的名称。
  // 将其添加为已知标识符
  // 并在汇编中生成其空间
  id = addglob(Text, type, S_VARIABLE);
  genglobsym(id);
  // 获取尾随的分号
  semi();
}

// 目前我们的函数定义语法非常简单
//
// function_declaration: 'void' identifier '(' ')' compound_statement   ;
//
// 解析一个简单函数的声明
struct ASTnode *function_declaration(void) {
  struct ASTnode *tree;
  int nameslot;

  // 找到 'void'、标识符和 '(' ')'。
  // 目前，对它们不做任何处理
  match(T_VOID, "void");
  ident();
  nameslot = addglob(Text, P_VOID, S_FUNCTION);
  lparen();
  rparen();

  // 获取复合语句的 AST 树
  tree = compound_statement();

  // 返回一个 A_FUNCTION 节点，它有函数的 nameslot
  // 和复合语句子树
  return (mkastunary(A_FUNCTION, P_VOID, tree, nameslot));
}