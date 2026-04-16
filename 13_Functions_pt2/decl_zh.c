#include "defs.h"
#include "data.h"
#include "decl.h"

// 声明解析
// Copyright (c) 2019 Warren Toomey, GPL3


// 解析当前标记并返回基本类型枚举值
int parse_type(int t) {
  if (t == T_CHAR)
    return (P_CHAR);
  if (t == T_INT)
    return (P_INT);
  if (t == T_LONG)
    return (P_LONG);
  if (t == T_VOID)
    return (P_VOID);
  fatald("Illegal type, token", t);
}

// variable_declaration: type identifier ';'  ;
//
// 解析变量的声明
void var_declaration(void) {
  int id, type;

  // 获取变量的类型，然后是标识符
  type = parse_type(Token.token);
  scan(&Token);
  ident();
  // Text 现在有标识符的名称
  // 将其添加为已知标识符
  // 并在汇编中生成其空间
  id = addglob(Text, type, S_VARIABLE, 0);
  genglobsym(id);
  // 获取尾部分号
  semi();
}

//
// function_declaration: type identifier '(' ')' compound_statement   ;
//
// 解析简化函数的声明
struct ASTnode *function_declaration(void) {
  struct ASTnode *tree, *finalstmt;
  int nameslot, type, endlabel;

  // 获取变量的类型，然后是标识符
  type = parse_type(Token.token);
  scan(&Token);
  ident();

  // 获取结束标签的标签 ID，将函数添加到符号表，
  // 并将 Functionid 全局变量设置为函数的符号 ID
  endlabel = genlabel();
  nameslot = addglob(Text, type, S_FUNCTION, endlabel);
  Functionid = nameslot;

  // 扫描圆括号
  lparen();
  rparen();

  // 获取复合语句的 AST 树
  tree = compound_statement();

  // 如果函数类型不是 P_VOID，检查复合语句中
  // 最后的 AST 操作是否是 return 语句
  if (type != P_VOID) {
    finalstmt = (tree->op == A_GLUE) ? tree->right : tree;
    if (finalstmt == NULL || finalstmt->op != A_RETURN)
      fatal("No return for function with non-void type");
  }
  // 返回具有函数 nameslot 和复合语句子树的 A_FUNCTION 节点
  return (mkastunary(A_FUNCTION, type, tree, nameslot));
}
