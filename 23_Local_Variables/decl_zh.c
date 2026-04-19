#include "defs.h"
#include "data.h"
#include "decl.h"

// 声明的解析
// Copyright (c) 2019 Warren Toomey, GPL3


// 解析当前词法单元并返回
// 原始类型枚举值。同时
// 扫描下一个词法单元
int parse_type(void) {
  int type;
  switch (Token.token) {
    case T_VOID:
      type = P_VOID;
      break;
    case T_CHAR:
      type = P_CHAR;
      break;
    case T_INT:
      type = P_INT;
      break;
    case T_LONG:
      type = P_LONG;
      break;
    default:
      fatald("Illegal type, token", Token.token);
  }

  // 扫描一个或多个额外的 '*' 词法单元 
  // 并确定正确的指针类型
  while (1) {
    scan(&Token);
    if (Token.token != T_STAR)
      break;
    type = pointer_to(type);
  }

  // 离开时下一个词法单元已被扫描
  return (type);
}

// variable_declaration: type identifier ';'
//        | type identifier '[' INTLIT ']' ';'
//        ;
//
// 解析标量变量或具有给定大小的数组
// 的声明。标识符已被扫描且我们有类型
void var_declaration(int type, int islocal) {

  // Text现在有标识符的名称。
  // 如果下一个词法单元是 '['
  if (Token.token == T_LBRACKET) {
    // 跳过 '['
    scan(&Token);

    // 检查我们是否有数组大小
    if (Token.token == T_INTLIT) {
      // 将其添加为已知的数组并在汇编中生成其空间。
      // 我们将数组视为指向其元素类型的指针
      if (islocal) {
	addlocl(Text, pointer_to(type), S_ARRAY, 0, Token.intvalue);
      } else {
	addglob(Text, pointer_to(type), S_ARRAY, 0, Token.intvalue);
      }
    }
    // 确保有随后的 ']'
    scan(&Token);
    match(T_RBRACKET, "]");
  } else {
    // 将其添加为已知的标量
    // 并在汇编中生成其空间
    if (islocal) {
      addlocl(Text, type, S_VARIABLE, 0, 1);
    } else {
      addglob(Text, type, S_VARIABLE, 0, 1);
    }
  }

  // 获取尾随的分号
  semi();
}

//
// function_declaration: type identifier '(' ')' compound_statement   ;
//
// 解析一个简单函数的声明。
// 标识符已被扫描且我们有类型
struct ASTnode *function_declaration(int type) {
  struct ASTnode *tree, *finalstmt;
  int nameslot, endlabel;

  // Text现在有标识符的名称。
  // 获取结束标签的标签ID，将函数添加到
  // 符号表，并将Functionid全局变量设置为
  // 函数的符号ID
  endlabel = genlabel();
  nameslot = addglob(Text, type, S_FUNCTION, endlabel, 0);
  Functionid = nameslot;

  genresetlocals();		// 重置新局部变量的位置

  // 扫描括号
  lparen();
  rparen();

  // 获取复合语句的抽象语法树
  tree = compound_statement();

  // 如果函数类型不是P_VOID ..
  if (type != P_VOID) {

    // 如果函数中没有语句则报错
    if (tree == NULL)
      fatal("No statements in function with non-void type");

    // 检查复合语句中的最后一个
    // 抽象语法树操作是否是return语句
    finalstmt = (tree->op == A_GLUE) ? tree->right : tree;
    if (finalstmt == NULL || finalstmt->op != A_RETURN)
      fatal("No return for function with non-void type");
  }
  // 返回具有函数nameslot
  // 和复合语句子树的A_FUNCTION节点
  return (mkastunary(A_FUNCTION, type, tree, nameslot));
}


// 解析一个或多个全局声明，可以是
// 变量或函数
void global_declarations(void) {
  struct ASTnode *tree;
  int type;

  while (1) {

    // 我们必须读取类型和标识符
    // 来查看是函数声明的 '('
    // 还是变量声明的 ',' 或 ';'。
    // Text由ident()调用填充
    type = parse_type();
    ident();
    if (Token.token == T_LPAREN) {

      // 解析函数声明并
      // 为其生成汇编代码
      tree = function_declaration(type);
      if (O_dumpAST) {
	dumpAST(tree, NOLABEL, 0);
	fprintf(stdout, "\n\n");
      }
      genAST(tree, NOLABEL, 0);
    } else {

      // 解析全局变量声明
      var_declaration(type, 0);
    }

    // 当我们到达EOF时停止
    if (Token.token == T_EOF)
      break;
  }
}