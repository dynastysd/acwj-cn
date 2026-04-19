#include "defs.h"
#include "data.h"
#include "decl.h"

// 声明解析
// Copyright (c) 2019 Warren Toomey, GPL3

static struct symtable *composite_declaration(int type);
static int typedef_declaration(struct symtable **ctype);
static int type_of_typedef(char *name, struct symtable **ctype);
static void enum_declaration(void);

// 解析当前标记并返回原始类型枚举值，
// 指向任何组合类型的指针，并可能修改
// 类型的类别。
static int parse_type(struct symtable **ctype, int *class) {
  int type, exstatic = 1;

  // 查看类别是否已更改为 extern（稍后，static）
  while (exstatic) {
    switch (Token.token) {
      case T_EXTERN:
	*class = C_EXTERN;
	scan(&Token);
	break;
      default:
	exstatic = 0;
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
      // 有 ';' 则没有类型，所以返回 -1。
      // 示例：struct x {int y; int z};
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
      type = P_INT;		// 枚举实际上是 int
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
      fatals("Illegal type, token", Token.tokstr);
  }
  return (type);
}

// 给定由 parse_type() 解析的类型，扫描任何后续
// 的 '*' 标记并返回新类型
static int parse_stars(int type) {

  while (1) {
    if (Token.token != T_STAR)
      break;
    type = pointer_to(type);
    scan(&Token);
  }
  return (type);
}

static struct symtable *scalar_declaration(char *varname, int type,
					   struct symtable *ctype,
					   int class) {

  // 将其添加为已知的标量
  switch (class) {
    case C_EXTERN:
    case C_GLOBAL:
      return (addglob(varname, type, ctype, S_VARIABLE, class, 1));
      break;
    case C_LOCAL:
      return (addlocl(varname, type, ctype, S_VARIABLE, 1));
      break;
    case C_PARAM:
      return (addparm(varname, type, ctype, S_VARIABLE, 1));
      break;
    case C_MEMBER:
      return (addmemb(varname, type, ctype, S_VARIABLE, 1));
      break;
  }
  return (NULL);		// 保持 -Wall 愉快
}

static struct symtable *array_declaration(char *varname, int type,
					  struct symtable *ctype, int class) {
  struct symtable *sym;
  // 跳过 '['
  scan(&Token);

  // 检查我们是否有数组大小
  if (Token.token == T_INTLIT) {
    // 将其添加为已知的数组
    // 我们将数组视为指向其元素类型的指针
    switch (class) {
      case C_EXTERN:
      case C_GLOBAL:
	sym =
	  addglob(varname, pointer_to(type), ctype, S_ARRAY, class,
		  Token.intvalue);
	break;
      case C_LOCAL:
      case C_PARAM:
      case C_MEMBER:
	fatal("For now, declaration of non-global arrays is not implemented");
    }
  }
  // 确保我们有后续的 ']'
  scan(&Token);
  match(T_RBRACKET, "]");
  return (sym);
}


static int param_declaration_list(struct symtable *oldfuncsym,
				  struct symtable *newfuncsym) {
  int type, paramcnt = 0;
  struct symtable *ctype;
  struct symtable *protoptr = NULL;

  // 获取指向第一个原型参数的指针
  if (oldfuncsym != NULL)
    protoptr = oldfuncsym->member;

  // 循环获取任何参数
  while (Token.token != T_RPAREN) {
    // 获取下一个参数的类型
    type = declaration_list(&ctype, C_PARAM, T_COMMA, T_RPAREN);
    if (type == -1)
      fatal("Bad type in parameter list");

    // 确保此参数的类型与原型匹配
    if (protoptr != NULL) {
      if (type != protoptr->type)
	fatald("Type doesn't match prototype for parameter", paramcnt + 1);
      protoptr = protoptr->next;
    }
    paramcnt++;

    // 当遇到右括号时停止
    if (Token.token == T_RPAREN)
      break;
    // 我们需要一个逗号作为分隔符
    comma();
  }

  if (oldfuncsym != NULL && paramcnt != oldfuncsym->nelems)
    fatals("Parameter count mismatch for function", oldfuncsym->name);

  // 返回参数计数
  return (paramcnt);
}


//
// function_declaration: type identifier '(' parameter_list ')' ;
//      | type identifier '(' parameter_list ')' compound_statement   ;
//
// 解析函数的声明。
static struct symtable *function_declaration(char *funcname, int type,
					     struct symtable *ctype,
					     int class) {
  struct ASTnode *tree, *finalstmt;
  struct symtable *oldfuncsym, *newfuncsym = NULL;
  int endlabel, paramcnt;

  // Text 有标识符的名称。如果存在且是
  // 函数，则获取 id。否则，将 oldfuncsym 设置为 NULL。
  if ((oldfuncsym = findsymbol(funcname)) != NULL)
    if (oldfuncsym->stype != S_FUNCTION)
      oldfuncsym = NULL;

  // 如果这是一个新的函数声明，获取
  // 结束标签的标签 id，并将函数
  // 添加到符号表，
  if (oldfuncsym == NULL) {
    endlabel = genlabel();
    // 假设：函数只返回标量类型，所以下面的 NULL
    newfuncsym =
      addglob(funcname, type, NULL, S_FUNCTION, C_GLOBAL, endlabel);
  }
  // 扫描 '('、任何参数和 ')'。
  // 传入任何现有的函数原型指针
  lparen();
  paramcnt = param_declaration_list(oldfuncsym, newfuncsym);
  rparen();

  // 如果这是一个新的函数声明，
  // 使用参数数量更新函数符号条目。
  // 还要将参数列表复制到函数的节点。
  if (newfuncsym) {
    newfuncsym->nelems = paramcnt;
    newfuncsym->member = Parmhead;
    oldfuncsym = newfuncsym;
  }
  // 清空参数列表
  Parmhead = Parmtail = NULL;

  // 声明以分号结束，只是原型。
  if (Token.token == T_SEMI)
    return (oldfuncsym);

  // 这不仅仅是一个原型。
  // 将 Functionid 全局设置为函数的符号指针
  Functionid = oldfuncsym;

  // 获取复合语句的 AST 树并标记
  // 我们还没有解析循环或 switch
  Looplevel = 0;
  Switchlevel = 0;
  lbrace();
  tree = compound_statement(0);
  rbrace();

  // 如果函数类型不是 P_VOID..
  if (type != P_VOID) {

    // 如果函数中没有语句则报错
    if (tree == NULL)
      fatal("No statements in function with non-void type");

    // 检查复合语句中
    // 最后一个 AST 操作是否是 return 语句
    finalstmt = (tree->op == A_GLUE) ? tree->right : tree;
    if (finalstmt == NULL || finalstmt->op != A_RETURN)
      fatal("No return for function with non-void type");
  }
  // 构建 A_FUNCTION 节点，其中包含函数的符号指针
  // 和复合语句子树
  tree = mkastunary(A_FUNCTION, type, tree, oldfuncsym, endlabel);

  // 为其生成汇编代码
  if (O_dumpAST) {
    dumpAST(tree, NOLABEL, 0);
    fprintf(stdout, "\n\n");
  }
  genAST(tree, NOLABEL, NOLABEL, NOLABEL, 0);

  // 现在释放与
  // 此函数关联的符号
  freeloclsyms();
  return (oldfuncsym);
}

// 解析组合类型声明：struct 或 union。
// 要么找到现有的 struct/union 声明，要么构建
// 一个 struct/union 符号表条目并返回其指针。
static struct symtable *composite_declaration(int type) {
  struct symtable *ctype = NULL;
  struct symtable *m;
  int offset;
  int t;

  // 跳过 struct/union 关键字
  scan(&Token);

  // 查看是否有后续的 struct/union 名称
  if (Token.token == T_IDENT) {
    // 找到任何匹配的组合类型
    if (type == P_STRUCT)
      ctype = findstruct(Text);
    else
      ctype = findunion(Text);
    scan(&Token);
  }
  // 如果下一个标记不是 LBRACE，这是
  // 现有 struct/union 类型的用法。
  // 返回该类型的指针。
  if (Token.token != T_LBRACE) {
    if (ctype == NULL)
      fatals("unknown struct/union type", Text);
    return (ctype);
  }
  // 确保此 struct/union 类型尚未
  // 被定义
  if (ctype)
    fatals("previously defined struct/union", Text);

  // 构建组合类型并跳过左花括号
  if (type == P_STRUCT)
    ctype = addstruct(Text, P_STRUCT, NULL, 0, 0);
  else
    ctype = addunion(Text, P_UNION, NULL, 0, 0);
  scan(&Token);

  // 扫描成员列表
  while (1) {
    // 获取下一个成员。m 用作虚拟
    t= declaration_list(&m, C_MEMBER, T_SEMI, T_RBRACE);
    if (t== -1)
      fatal("Bad type in member list");
    if (Token.token == T_SEMI)
      scan(&Token);
    if (Token.token == T_RBRACE)
      break;
  }

  // 附加到 struct 类型的节点
  rbrace();
  if (Membhead==NULL)
    fatals("No members in struct", ctype->name);
  ctype->member = Membhead;
  Membhead = Membtail = NULL;

  // 设置初始成员的偏移量
  // 并找到它之后的第一个空闲字节
  m = ctype->member;
  m->posn = 0;
  offset = typesize(m->type, m->ctype);

  // 在组合类型中设置每个连续成员的位置
  // 联合很容易。对于 struct，对齐成员并找到下一个空闲字节
  for (m = m->next; m != NULL; m = m->next) {
    // 设置此成员的偏移量
    if (type == P_STRUCT)
      m->posn = genalign(m->type, offset, 1);
    else
      m->posn = 0;

    // 获取此成员之后的下一个空闲字节的偏移量
    offset += typesize(m->type, m->ctype);
  }

  // 设置组合类型的整体大小
  ctype->size = offset;
  return (ctype);
}

// 解析枚举声明
static void enum_declaration(void) {
  struct symtable *etype = NULL;
  char *name;
  int intval = 0;

  // 跳过 enum 关键字。
  scan(&Token);

  // 如果有后续的枚举类型名称，获取
  // 指向任何现有枚举类型节点的指针。
  if (Token.token == T_IDENT) {
    etype = findenumtype(Text);
    name = strdup(Text);	// 因为它很快就会被覆盖
    scan(&Token);
  }
  // 如果下一个标记不是 LBRACE，检查
  // 我们有一个枚举类型名称，然后返回
  if (Token.token != T_LBRACE) {
    if (etype == NULL)
      fatals("undeclared enum type:", name);
    return;
  }
  // 我们确实有 LBRACE。跳过它
  scan(&Token);

  // 如果我们有枚举类型名称，确保它
  // 之前没有被声明过。
  if (etype != NULL)
    fatals("enum type redeclared:", etype->name);
  else
    // 为此标识符构建一个枚举类型节点
    etype = addenum(name, C_ENUMTYPE, 0);

  // 循环获取所有枚举值
  while (1) {
    // 确保我们有一个标识符
    // 如果有整数字面量即将出现，则复制它
    ident();
    name = strdup(Text);

    // 确保此枚举值之前没有被声明过
    etype = findenumval(name);
    if (etype != NULL)
      fatals("enum value redeclared:", Text);

    // 如果下一个标记是 '='，跳过它并
    // 获取后续的整数字面量
    if (Token.token == T_ASSIGN) {
      scan(&Token);
      if (Token.token != T_INTLIT)
	fatal("Expected int literal after '='");
      intval = Token.intvalue;
      scan(&Token);
    }
    // 为此标识符构建一个枚举值节点。
    // 为下一个枚举标识符递增值。
    etype = addenum(name, C_ENUMVAL, intval++);

    // 如果是右花括号则退出，否则获取逗号
    if (Token.token == T_RBRACE)
      break;
    comma();
  }
  scan(&Token);			// 跳过右花括号
}

// 解析 typedef 声明并返回它
// 所表示的类型和 ctype
static int typedef_declaration(struct symtable **ctype) {
  int type, class = 0;

  // 跳过 typedef 关键字。
  scan(&Token);

  // 获取关键字后面的实际类型
  type = parse_type(ctype, &class);
  if (class != 0)
    fatal("Can't have extern in a typedef declaration");

  // 查看 typedef 标识符是否已存在
  if (findtypedef(Text) != NULL)
    fatals("redefinition of typedef", Text);

  // 获取任何后续的 '*' 标记
  type = parse_stars(type);

  // 它不存在，所以将其添加到 typedef 列表
  addtypedef(Text, type, *ctype, 0, 0);
  scan(&Token);
  return (type);
}

// 给定一个 typedef 名称，返回它表示的类型
static int type_of_typedef(char *name, struct symtable **ctype) {
  struct symtable *t;

  // 在列表中查找 typedef
  t = findtypedef(name);
  if (t == NULL)
    fatals("unknown type", name);
  scan(&Token);
  *ctype = t->ctype;
  return (t->type);
}

static void array_initialisation(struct symtable *sym, int type,
				 struct symtable *ctype, int class) {
  fatal("No array initialisation yet!");
}

// 解析变量或函数的声明。
// 类型和任何后续的 '*' 已被扫描，
// 我们有标识符在 Token 变量中。
// class 参数是变量的类别。
// 返回符号表中符号条目的指针
static struct symtable *symbol_declaration(int type, struct symtable *ctype,
					   int class) {
  struct symtable *sym = NULL;
  char *varname = strdup(Text);
  int stype = S_VARIABLE;
  // struct ASTnode *expr = NULL;

  // 假设它是一个标量变量。
  // 确保我们有一个标识符。
  // 我们在上面复制了它，所以我们可以扫描更多标记，例如
  // 用于局部变量赋值的表达式。
  ident();

  // 处理函数声明
  if (Token.token == T_LPAREN) {
    return (function_declaration(varname, type, ctype, class));
  }
  // 查看此数组或标量变量是否已被声明
  switch (class) {
    case C_EXTERN:
    case C_GLOBAL:
      if (findglob(varname) != NULL)
	fatals("Duplicate global variable declaration", varname);
    case C_LOCAL:
    case C_PARAM:
      if (findlocl(varname) != NULL)
	fatals("Duplicate local variable declaration", varname);
    case C_MEMBER:
      if (findmember(varname) != NULL)
	fatals("Duplicate struct/union member declaration", varname);
  }

  // 将数组或标量变量添加到符号表
  if (Token.token == T_LBRACKET) {
    sym = array_declaration(varname, type, ctype, class);
    stype = S_ARRAY;
  } else
    sym = scalar_declaration(varname, type, ctype, class);

  // 数组或标量变量正在被初始化
  if (Token.token == T_ASSIGN) {
    // 参数或成员不可能
    if (class == C_PARAM)
      fatals("Initialisation of a parameter not permitted", varname);
    if (class == C_MEMBER)
      fatals("Initialisation of a member not permitted", varname);
    scan(&Token);

    // 数组初始化
    if (stype == S_ARRAY)
      array_initialisation(sym, type, ctype, class);
    else {
      fatal("Scalar variable initialisation not done yet");
      // 变量初始化
      // if (class== C_LOCAL)
      // 局部变量，解析表达式
      // expr= binexpr(0);
      // 否则编写更多代码！
    }
  }
  // 生成数组或标量变量的存储空间。很快。
  // genstorage(sym, expr);
  return (sym);
}

// 解析有一个初始类型的符号列表。
// 返回符号的类型。et1 和 et2 是结束标记。
int declaration_list(struct symtable **ctype, int class, int et1, int et2) {
  int inittype, type;
  struct symtable *sym;

  // 获取初始类型。如果是 -1，则是
  // 组合类型定义，返回此类型
  if ((inittype = parse_type(ctype, &class)) == -1)
    return (inittype);

  // 现在解析符号列表
  while (1) {
    // 查看此符号是否是指针
    type = parse_stars(inittype);

    // 解析此符号
    sym = symbol_declaration(type, *ctype, class);

    // 我们解析了一个函数，没有列表所以离开
    if (sym->stype == S_FUNCTION) {
      if (class != C_GLOBAL)
        fatal("Function definition not at global level");
      return (type);
    }

    // 我们在列表的末尾，离开
    if (Token.token == et1 || Token.token == et2)
      return (type);

    // 否则，我们需要一个逗号作为分隔符
    comma();
  }
}

// 解析一个或多个全局声明，
// 变量、函数或 struct
void global_declarations(void) {
  struct symtable *ctype;
  while (Token.token != T_EOF) {
    declaration_list(&ctype, C_GLOBAL, T_SEMI, T_EOF);
    // 跳过任何分号和右花括号
    if (Token.token == T_SEMI)
      scan(&Token);
  }
}