#include "defs.h"
#include "data.h"
#include "decl.h"

// 声明解析
// Copyright (c) 2019 Warren Toomey, GPL3

static struct symtable *composite_declaration(int type);
static void enum_declaration(void);
int typedef_declaration(struct symtable **ctype);
int type_of_typedef(char *name, struct symtable **ctype);


// 解析当前标记并返回
// 基本类型枚举值、指向任何复合类型的指针
// 并可能修改类型的类别
// 同时扫描下一个标记
int parse_type(struct symtable **ctype, int *class) {
  int type, exstatic=1;

  // 查看类别是否已更改为extern(后来是static)
  while (exstatic) {
    switch (Token.token) {
      case T_EXTERN: *class= C_EXTERN; scan(&Token); break;
      default: exstatic= 0;
    }
  }

  // 现在处理实际类型关键字
  switch (Token.token) {
    case T_VOID:
      type = P_VOID;
      scan(&Token);
      break;
    case T_CHAR:
      type = P_CHAR;
      scan(&Token);
      break;
    case T_INT:
      type = P_INT;
      scan(&Token);
      break;
    case T_LONG:
      type = P_LONG;
      scan(&Token);
      break;

      // 对于以下情况，如果在解析后
      // 有';'则没有类型，返回-1
    case T_STRUCT:
      type = P_STRUCT;
      *ctype = composite_declaration(P_STRUCT);
      if (Token.token == T_SEMI)
	type = -1;
      break;
    case T_UNION:
      type = P_UNION;
      *ctype = composite_declaration(P_UNION);
      if (Token.token == T_SEMI)
	type = -1;
      break;
    case T_ENUM:
      type = P_INT;		// 枚举实际上是int
      enum_declaration();
      if (Token.token == T_SEMI)
	type = -1;
      break;
    case T_TYPEDEF:
      type = typedef_declaration(ctype);
      if (Token.token == T_SEMI)
	type = -1;
      break;
    case T_IDENT:
      type = type_of_typedef(Text, ctype);
      break;
    default:
      fatald("Illegal type, token", Token.token);
  }

  // 扫描一个或多个额外的'*'标记
  // 并确定正确的指针类型
  while (1) {
    if (Token.token != T_STAR)
      break;
    type = pointer_to(type);
    scan(&Token);
  }

  // 我们离开时下一个标记已被扫描
  return (type);
}

// variable_declaration: type identifier ';'
//        | type identifier '[' INTLIT ']' ';'
//        ;
//
// 解析标量变量或具有给定大小的数组的声明
// 标识符已被扫描且我们有了类型
// class是变量的类别
// 返回符号表中变量条目的指针
struct symtable *var_declaration(int type, struct symtable *ctype, int class) {
  struct symtable *sym = NULL;

  // 查看这是否已被声明
  switch (class) {
    case C_EXTERN:
    case C_GLOBAL:
      if (findglob(Text) != NULL)
	fatals("Duplicate global variable declaration", Text);
    case C_LOCAL:
    case C_PARAM:
      if (findlocl(Text) != NULL)
	fatals("Duplicate local variable declaration", Text);
    case C_MEMBER:
      if (findmember(Text) != NULL)
	fatals("Duplicate struct/union member declaration", Text);
  }

  // Text现在有标识符的名称
  // 如果下一个标记是'['
  if (Token.token == T_LBRACKET) {
    // 跳过'['
    scan(&Token);

    // 检查我们是否有数组大小
    if (Token.token == T_INTLIT) {
      // 将其添加为已知数组并在汇编中生成其空间
      // 我们将数组视为指向其元素类型的指针
      switch (class) {
	case C_EXTERN:
	case C_GLOBAL:
	  sym =
	    addglob(Text, pointer_to(type), ctype, S_ARRAY, class, Token.intvalue);
	  break;
	case C_LOCAL:
	case C_PARAM:
	case C_MEMBER:
	  fatal
	    ("For now, declaration of non-global arrays is not implemented");
      }
    }
    // 确保有后续的']'
    scan(&Token);
    match(T_RBRACKET, "]");
  } else {
    // 将其添加为已知的标量
    // 并在汇编中生成其空间
    switch (class) {
      case C_EXTERN:
      case C_GLOBAL:
	sym = addglob(Text, type, ctype, S_VARIABLE, class, 1);
	break;
      case C_LOCAL:
	sym = addlocl(Text, type, ctype, S_VARIABLE, 1);
	break;
      case C_PARAM:
	sym = addparm(Text, type, ctype, S_VARIABLE, 1);
	break;
      case C_MEMBER:
	sym = addmemb(Text, type, ctype, S_VARIABLE, 1);
	break;
    }
  }
  return (sym);
}

// var_declaration_list: <null>
//           | variable_declaration
//           | variable_declaration separate_token var_declaration_list ;
//
// 当解析函数参数时，separate_token是','。
// 当解析struct/union成员时，separate_token是';'。
//
// 解析变量列表。将它们作为符号添加到
// 符号表列表之一，并返回变量数量。
// 如果funcsym不为NULL，则存在现有的函数
// 原型，因此比较每个变量的类型与此原型
static int var_declaration_list(struct symtable *funcsym, int class,
				int separate_token, int end_token) {
  int type;
  int paramcnt = 0;
  struct symtable *protoptr = NULL;
  struct symtable *ctype;

  // 如果有原型，获取指向
  // 第一个原型参数的指针
  if (funcsym != NULL)
    protoptr = funcsym->member;

  // 循环直到最终的结束标记
  while (Token.token != end_token) {
    // 获取类型和标识符
    type = parse_type(&ctype, &class);
    ident();

    // 检查此类型是否与原型匹配(如果有的话)
    if (protoptr != NULL) {
      if (type != protoptr->type)
	fatald("Type doesn't match prototype for parameter", paramcnt + 1);
      protoptr = protoptr->next;
    } else {
      // 将新参数添加到正确的符号表列表，基于class
      var_declaration(type, ctype, class);
    }
    paramcnt++;

    // 此时必须是separate_token或')'
    if ((Token.token != separate_token) && (Token.token != end_token))
      fatald("Unexpected token in parameter list", Token.token);
    if (Token.token == separate_token)
      scan(&Token);
  }

  // 检查此列表中的参数数量是否与
  // 任何现有原型匹配
  if ((funcsym != NULL) && (paramcnt != funcsym->nelems))
    fatals("Parameter count mismatch for function", funcsym->name);

  // 返回参数计数
  return (paramcnt);
}

//
// function_declaration: type identifier '(' parameter_list ')' ;
//      | type identifier '(' parameter_list ')' compound_statement   ;
//
// 解析函数声明。标识符已被扫描且我们有了类型
struct ASTnode *function_declaration(int type) {
  struct ASTnode *tree, *finalstmt;
  struct symtable *oldfuncsym, *newfuncsym = NULL;
  int endlabel, paramcnt;

  // Text有标识符名称。如果存在且是函数，获取其id。
  // 否则，将oldfuncsym设置为NULL
  if ((oldfuncsym = findsymbol(Text)) != NULL)
    if (oldfuncsym->stype != S_FUNCTION)
      oldfuncsym = NULL;

  // 如果这是新的函数声明，获取
  // 结束标签的标签ID，并将函数添加到符号表
  if (oldfuncsym == NULL) {
    endlabel = genlabel();
    // 假设：函数只返回标量类型，所以下面为NULL
    newfuncsym = addglob(Text, type, NULL, S_FUNCTION, C_GLOBAL, endlabel);
  }
  // 扫描'('、任何参数和')'
  // 传入任何现有的函数原型指针
  lparen();
  paramcnt = var_declaration_list(oldfuncsym, C_PARAM, T_COMMA, T_RPAREN);
  rparen();

  // 如果这是新的函数声明，用参数数量更新
  // 函数符号条目。同时将参数列表复制到函数的节点
  if (newfuncsym) {
    newfuncsym->nelems = paramcnt;
    newfuncsym->member = Parmhead;
    oldfuncsym = newfuncsym;
  }
  // 清空参数列表
  Parmhead = Parmtail = NULL;

  // 声明以分号结束，只是原型
  if (Token.token == T_SEMI) {
    scan(&Token);
    return (NULL);
  }
  // 这不只是原型。将Functionid全局设置为
  // 函数的符号指针
  Functionid = oldfuncsym;

  // 获取复合语句的AST树，并标记
  // 我们尚未解析任何循环或switch
  Looplevel= 0;
  Switchlevel= 0;
  tree = compound_statement();

  // 如果函数类型不是P_VOID..
  if (type != P_VOID) {

    // 如果函数中没有语句则报错
    if (tree == NULL)
      fatal("No statements in function with non-void type");

    // 检查复合语句中最后一个AST操作是否是return语句
    finalstmt = (tree->op == A_GLUE) ? tree->right : tree;
    if (finalstmt == NULL || finalstmt->op != A_RETURN)
      fatal("No return for function with non-void type");
  }
  // 返回一个A_FUNCTION节点，它有函数的符号指针
  // 和复合语句子树
  return (mkastunary(A_FUNCTION, type, tree, oldfuncsym, endlabel));
}

// 解析复合类型声明：struct或union。
// 要么找到现有的struct/union声明，要么
// 构建一个struct/union符号表条目并返回其指针
static struct symtable *composite_declaration(int type) {
  struct symtable *ctype = NULL;
  struct symtable *m;
  int offset;

  // 跳过struct/union关键字
  scan(&Token);

  // 查看是否有后续的struct/union名称
  if (Token.token == T_IDENT) {
    // 查找任何匹配的复合类型
    if (type == P_STRUCT)
      ctype = findstruct(Text);
    else
      ctype = findunion(Text);
    scan(&Token);
  }
  // 如果下一个标记不是LBRACE，这是
  // 现有struct/union类型的使用
  // 返回类型的指针
  if (Token.token != T_LBRACE) {
    if (ctype == NULL)
      fatals("unknown struct/union type", Text);
    return (ctype);
  }
  // 确保这个struct/union类型尚未被定义
  if (ctype)
    fatals("previously defined struct/union", Text);

  // 构建复合类型并跳过左花括号
  if (type == P_STRUCT)
    ctype = addstruct(Text, P_STRUCT, NULL, 0, 0);
  else
    ctype = addunion(Text, P_UNION, NULL, 0, 0);
  scan(&Token);

  // 扫描成员列表并附加到
  // struct类型的节点
  var_declaration_list(NULL, C_MEMBER, T_SEMI, T_RBRACE);
  rbrace();
  ctype->member = Membhead;
  Membhead = Membtail = NULL;

  // 设置初始成员的偏移量
  // 并找到其后的第一个空闲字节
  m = ctype->member;
  m->posn = 0;
  offset = typesize(m->type, m->ctype);

  // 在复合类型中设置每个后续成员的位置。
  // 联合很简单。对于struct，对齐成员并找到下一个空闲字节
  for (m = m->next; m != NULL; m = m->next) {
    // 为此成员设置偏移量
    if (type == P_STRUCT)
      m->posn = genalign(m->type, offset, 1);
    else
      m->posn = 0;

    // 获取此成员之后的下一个空闲字节的偏移量
    offset += typesize(m->type, m->ctype);
  }

  // 设置复合类型的整体大小
  ctype->size = offset;
  return (ctype);
}

// 解析枚举声明
static void enum_declaration(void) {
  struct symtable *etype = NULL;
  char *name;
  int intval = 0;

  // 跳过enum关键字
  scan(&Token);

  // 如果有后续的枚举类型名称，获取
  // 指向任何现有枚举类型节点的指针
  if (Token.token == T_IDENT) {
    etype = findenumtype(Text);
    name = strdup(Text);	// 因为它很快就会被覆盖
    scan(&Token);
  }
  // 如果下一个标记不是LBRACE，检查
  // 我们是否有枚举类型名称，然后返回
  if (Token.token != T_LBRACE) {
    if (etype == NULL)
      fatals("undeclared enum type:", name);
    return;
  }
  // 我们确实有LBRACE。跳过它
  scan(&Token);

  // 如果我们有枚举类型名称，确保它
  // 之前没有被声明过
  if (etype != NULL)
    fatals("enum type redeclared:", etype->name);
  else
    // 为此标识符构建一个枚举类型节点
    etype = addenum(name, C_ENUMTYPE, 0);

  // 循环获取所有枚举值
  while (1) {
    // 确保我们有标识符
    // 如果有int常量则复制它
    ident();
    name = strdup(Text);

    // 确保这个枚举值之前没有被声明过
    etype = findenumval(name);
    if (etype != NULL)
      fatals("enum value redeclared:", Text);

    // 如果下一个标记是'='，跳过它并
    // 获取后续的int常量
    if (Token.token == T_ASSIGN) {
      scan(&Token);
      if (Token.token != T_INTLIT)
	fatal("Expected int literal after '='");
      intval = Token.intvalue;
      scan(&Token);
    }
    // 为此标识符构建一个枚举值节点。
    // 为下一个枚举标识符增加值
    etype = addenum(name, C_ENUMVAL, intval++);

    // 如果是右花括号则退出，否则获取逗号
    if (Token.token == T_RBRACE)
      break;
    comma();
  }
  scan(&Token);			// 跳过右花括号
}

// 解析typedef声明并返回它表示的类型
// 和ctype
int typedef_declaration(struct symtable **ctype) {
  int type, class=0;

  // 跳过typedef关键字
  scan(&Token);

  // 获取关键字后的实际类型
  type = parse_type(ctype, &class);
  if (class != 0)
    fatal("Can't have extern in a typedef declaration");

  // 查看typedef标识符是否已存在
  if (findtypedef(Text) != NULL)
    fatals("redefinition of typedef", Text);

  // 它不存在，所以添加到typedef列表
  addtypedef(Text, type, *ctype, 0, 0);
  scan(&Token);
  return (type);
}

// 给定typedef名称，返回它表示的类型
int type_of_typedef(char *name, struct symtable **ctype) {
  struct symtable *t;

  // 在列表中查找typedef
  t = findtypedef(name);
  if (t == NULL)
    fatals("unknown type", name);
  scan(&Token);
  *ctype = t->ctype;
  return (t->type);
}

// 解析一个或多个全局声明，
// 可能是变量、函数或struct
void global_declarations(void) {
  struct ASTnode *tree;
  struct symtable *ctype;
  int type, class= C_GLOBAL;

  while (1) {
    // 当到达EOF时停止
    if (Token.token == T_EOF)
      break;

    // 获取类型
    type = parse_type(&ctype, &class);

    // 我们可能刚刚解析了一个没有关联变量的
    // struct、union或enum声明。
    // 下一个标记可能是';'。如果是则循环回去
    // XXX：我不满意这个，因为它允许
    // "struct fred;"作为接受的语句
    if (type == -1) {
      semi();
      continue;
    }
    // 我们必须读取标识符来查看是
    // 函数声明的'('还是变量声明的','或';'
    // Text由ident()调用填充
    ident();
    if (Token.token == T_LPAREN) {

      // 解析函数声明
      tree = function_declaration(type);

      // 只是函数原型，没有代码
      if (tree == NULL)
	continue;

      // 一个真正的函数，为其生成汇编代码
      if (O_dumpAST) {
	dumpAST(tree, NOLABEL, 0);
	fprintf(stdout, "\n\n");
      }
      genAST(tree, NOLABEL, NOLABEL, NOLABEL, 0);

      // 现在释放与
      // 此函数关联的符号
      freeloclsyms();
    } else {

      // 解析全局变量声明
      // 并跳过尾部分号
      var_declaration(type, ctype, class);
      semi();
    }
  }
}
