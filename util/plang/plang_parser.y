// Copyright 2013, Beeri 15.  All rights reserved.
// Author: Roman Gershman (romange@gmail.com)
//
%filenames plang_parser
%scanner util/plang/plang_scanner.h
%lsp-needed
%namespace plang
// %parsefun-source plang_parser.cc
%baseclass-preinclude "util/plang/plang_parser_includes.h"
%polymorphic INT_T: IntLiteral*; EXPR_T: Expr*; ARGLIST_T : ArgList;
             IDENTIFIER_T: StringTerm*; STRING_T: std::string;

%token NUMBER
%token IDENTIFIER STRING DEF_TOK NOT_OP

%type  <EXPR_T> bool_expr scalar_expr comparison_predicate func_ref
%type <STRING_T> identifier
%type  <ARGLIST_T> arg_list
                                 // lowest precedence
%left AND_OP OR_OP LE_OP GE_OP NE_OP
%left   '='
                                // highest precedence

%%

input:
    // empty
  | bool_expr
  {
    res_val.reset($1);
  }
;

bool_expr:
    bool_expr AND_OP bool_expr
    {
      // std::cout << $1 << " AND " << $3 << '\n';
      $$ = new BinOp(BinOp::AND, $1, $3);
    }
|   bool_expr OR_OP bool_expr
    {
      // std::cout << $1 << " AND " << $3 << '\n';
      $$ = new BinOp(BinOp::OR, $1, $3);
    }
|   comparison_predicate
|  '(' bool_expr ')'
    {
      $$ = $2;
    }
| DEF_TOK '(' identifier ')'
  {
    $$ = new IsDefFun($3);
  }
| NOT_OP bool_expr
  {
     $$ = new BinOp(BinOp::NOT, $2, nullptr);
  }
;

comparison_predicate :
    scalar_expr '=' scalar_expr
    {
       // std::cout << $1 << " EQ " << $3 << '\n';
       $$ = new BinOp(BinOp::EQ, $1, $3);
    }
|
    scalar_expr NE_OP scalar_expr
    {
       $$ = new BinOp(BinOp::NOT, new BinOp(BinOp::EQ, $1, $3), nullptr);
    }
|  scalar_expr '<' scalar_expr
   {
     $$ = new BinOp(BinOp::LT, $1, $3);
   }
|  scalar_expr LE_OP scalar_expr
   {
     $$ = new BinOp(BinOp::LE, $1, $3);
   }
|  scalar_expr GE_OP scalar_expr
   {
     $$ = new BinOp(BinOp::LE, $3, $1);
   }
|  scalar_expr '>' scalar_expr
   {
     $$ = new BinOp(BinOp::LT, $3, $1);
   }
;


scalar_expr:
   identifier
   {
      $$ = new StringTerm($1, StringTerm::VARIABLE);
   }
 | func_ref
 | NUMBER
   {
      // std::cout << " number " << d_scanner.matched() << '\n';
      int64 tmp;
      if (safe_strto64(d_scanner.matched(), &tmp)) {
        $$ = new IntLiteral(IntLiteral::Signed(tmp));
      } else {
        uint64 tmp2;
        CHECK(safe_strtou64(d_scanner.matched(), &tmp2)) << d_scanner.matched();
        $$ = new IntLiteral(IntLiteral::Unsigned(tmp2));
      }
   }
 | STRING
   {
      // std::cout << " STRING " << d_scanner.matched() << '\n';
      $$ = new StringTerm(d_scanner.matched(), StringTerm::CONST);
   }
|  '(' scalar_expr ')'
   {
      $$ = $2;
   }
;

identifier : IDENTIFIER
 {
    // std::cout << " IDENTIFIER " << d_scanner.matched() << '\n';
    $$ = d_scanner.matched();
 }
 ;

func_ref:
   identifier '(' arg_list ')'
   {
     // std::cout << " function " << $1 << '\n';
     $$ = new FunctionTerm($1, std::move($3));
   }
;


arg_list : scalar_expr
    {
      $$ = ArgList{$1};
    }
|   arg_list ',' scalar_expr
    {
      ($1) .push_back($3);
      $$ = std::move($1);
    }
;