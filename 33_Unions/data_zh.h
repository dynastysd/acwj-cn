#ifndef extern_
#define extern_ extern
#endif

// 全局变量
// Copyright (c) 2019 Warren Toomey, GPL3

extern_ int Line;		     	// 当前行号
extern_ int Putback;		     	// 扫描器放回的字符
extern_ struct symtable *Functionid; 	// 当前函数的符号指针
extern_ FILE *Infile;		     	// 输入和输出文件
extern_ FILE *Outfile;
extern_ char *Outfilename;		// 作为Outfile打开的文件名
extern_ struct token Token;		// 最近扫描的令牌
extern_ char Text[TEXTLEN + 1];		// 最近扫描的标识符

// 符号表列表
extern_ struct symtable *Globhead, *Globtail;	  // 全局变量和函数
extern_ struct symtable *Loclhead, *Locltail;	  // 局部变量
extern_ struct symtable *Parmhead, *Parmtail;	  // 局部参数
extern_ struct symtable *Membhead, *Membtail;	  // 结构体/联合体成员的临时列表
extern_ struct symtable *Structhead, *Structtail; // 结构体类型列表
extern_ struct symtable *Unionhead, *Uniontail;   // 结构体类型列表

// 命令行标志
extern_ int O_dumpAST;		// 为真时，转储AST树
extern_ int O_keepasm;		// 为真时，保留汇编文件
extern_ int O_assemble;		// 为真时，汇编汇编文件
extern_ int O_dolink;		// 为真时，链接目标文件
extern_ int O_verbose;		// 为真时，打印编译阶段信息
