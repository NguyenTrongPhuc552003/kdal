---
title: Grammar Reference
description: Formal EBNF grammars for .kdh and .kdc files.
---

The KDAL language is defined by two formal grammars in
[ISO 14977 EBNF](https://en.wikipedia.org/wiki/Extended_Backus%E2%80%93Naur_form).
These are the authoritative source for parser implementation.

## Device Header Grammar (.kdh)

```ebnf
(* Top-level *)
kdh_file     = [ version_decl ] { import_decl } device_class_decl ;
version_decl = "kdal_version" ":" string_lit ";" ;
import_decl  = "import" string_lit [ "as" ident ] ";" ;

(* Device class declaration *)
device_class_decl =
    "device" "class" ident "{"
        [ registers_block ]
        [ signals_block ]
        [ capabilities_block ]
        [ power_block ]
        [ config_block ]
    "}" ;

(* Registers *)
registers_block = "registers" "{" { register_decl } "}" ;
register_decl   = ident ":" reg_type [ "@" hex_lit ] [ access_qualifier ]
                  [ "=" literal ] ";" ;
reg_type        = "u8" | "u16" | "u32" | "u64"
                | "bitfield" "{" { bit_field_member } "}" ;
bit_field_member = ident ":" bit_range [ "=" literal ] ";" ;
bit_range        = integer_lit ".." integer_lit | integer_lit ;
access_qualifier = "ro" | "wo" | "rw" | "rc" ;

(* Signals *)
signals_block    = "signals" "{" { signal_decl } "}" ;
signal_decl      = ident ":" signal_direction signal_trigger ";" ;
signal_direction = "in" | "out" | "inout" ;
signal_trigger   = "edge" "(" edge_type ")" | "level" "(" level_type ")"
                 | "completion" | "timeout" "(" expr ")" ;
edge_type  = "rising" | "falling" | "any" ;
level_type = "high" | "low" ;

(* Capabilities *)
capabilities_block = "capabilities" "{" { capability_decl } "}" ;
capability_decl    = ident [ "=" integer_lit ] ";" ;

(* Power *)
power_block      = "power" "{" { power_state_decl } "}" ;
power_state_decl = power_state_name ":" power_state_spec ";" ;
power_state_name = "on" | "off" | "suspend" | ident ;
power_state_spec = "allowed" | "forbidden" | "default" ;

(* Config *)
config_block     = "config" "{" { config_field_decl } "}" ;
config_field_decl = ident ":" type_name [ "=" literal ] [ "in" range_expr ] ";" ;
range_expr       = literal ".." literal ;
```

## Driver Implementation Grammar (.kdc)

```ebnf
(* Top-level *)
kdc_file     = version_decl { import_decl } [ backend_decl ] driver_block ;
version_decl = "kdal_version" ":" string_lit ";" ;
import_decl  = "import" string_lit [ "as" ident ] ";" ;

(* Backend annotation *)
backend_decl   = "backend" ident [ "{" { backend_option } "}" ] ;
backend_option = ident ":" literal ";" ;

(* Driver block *)
driver_block =
    "driver" ident [ "for" qualified_ident ] "{"
        [ config_bind_block ]
        probe_block
        [ remove_block ]
        { event_handler }
    "}" ;

(* Config bind *)
config_bind_block = "config" "{" { config_bind_stmt } "}" ;
config_bind_stmt  = ident "=" expr ";" ;

(* Probe / Remove *)
probe_block  = "probe"  "{" { statement } "}" ;
remove_block = "remove" "{" { statement } "}" ;

(* Event handlers *)
event_handler =
    "on" "read"    "(" reg_path ")"                 "{" { statement } "}"
  | "on" "write"   "(" reg_path "," ident ")"       "{" { statement } "}"
  | "on" "signal"  "(" ident ")"                    "{" { statement } "}"
  | "on" "power"   "(" power_transition ")"         "{" { statement } "}"
  | "on" "timeout" "(" ident ")"                    "{" { statement } "}" ;

power_transition = ("on" | "off" | "suspend" | ident)
                   [ "->" ("on" | "off" | "suspend" | ident) ] ;

(* Statements *)
statement =
    "let" ident [ ":" type_name ] "=" expr ";"
  | lvalue "=" expr ";"
  | "write" "(" reg_path "," expr ")" ";"
  | "emit" "(" ident [ "," expr ] ")" ";"
  | "wait" "(" ident "," expr ")" ";"
  | "arm" "(" ident "," expr ")" ";"
  | "cancel" "(" ident ")" ";"
  | "return" [ expr ] ";"
  | "log" "(" string_lit { "," expr } ")" ";"
  | if_stmt
  | for_stmt ;

if_stmt = "if" "(" expr ")" "{" { statement } "}"
          { "elif" "(" expr ")" "{" { statement } "}" }
          [ "else" "{" { statement } "}" ] ;

for_stmt = "for" ident "in" expr ".." expr "{" { statement } "}" ;

lvalue   = ident | reg_path ;
reg_path = ident "." ident { "." ident } ;
qualified_ident = ident { "." ident } ;

(* Expressions *)
expr = literal | ident | reg_path
     | "read" "(" reg_path ")"
     | expr binary_op expr
     | unary_op expr
     | "(" expr ")" ;

binary_op = "+" | "-" | "*" | "/" | "%" | "<<" | ">>"
          | "&" | "|" | "^" | "==" | "!=" | "<" | "<=" | ">" | ">="
          | "&&" | "||" ;
unary_op  = "-" | "~" | "!" ;

(* Types *)
type_name = "u8" | "u16" | "u32" | "u64"
          | "i8" | "i16" | "i32" | "i64"
          | "bool" | "str" | "timeout"
          | "buf" "[" type_name "]"
          | ident ;

(* Literals *)
literal     = integer_lit | hex_lit | bool_lit | string_lit ;
integer_lit = digit { digit } ;
hex_lit     = "0x" hex_digit { hex_digit } ;
bool_lit    = "true" | "false" ;
string_lit  = '"' { any_char_except_dquote } '"' ;

(* Identifiers *)
ident     = letter { letter | digit | "_" } ;
letter    = "a".."z" | "A".."Z" | "_" ;
digit     = "0".."9" ;
hex_digit = digit | "a".."f" | "A".."F" ;
```

Source files: [`lang/grammar/kdh.ebnf`](https://github.com/NguyenTrongPhuc552003/kdal/blob/main/lang/grammar/kdh.ebnf) and [`lang/grammar/kdc.ebnf`](https://github.com/NguyenTrongPhuc552003/kdal/blob/main/lang/grammar/kdc.ebnf).
