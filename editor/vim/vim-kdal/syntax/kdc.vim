" Vim syntax file
" Language:    KDAL Driver Code
" Maintainer:  Trong Phuc Nguyen <trongphuc.ng552003@gmail.com>
" URL:         https://github.com/NguyenTrongPhuc552003/kdal
" License:     GPL-3.0-or-later
" Last Change: 2026-04-12

if exists('b:current_syntax')
  finish
endif

" ── Comments ──────────────────────────────────────────────────────
syn match   kdcLineComment     "//.*$"              contains=@Spell
syn region  kdcBlockComment    start="/\*" end="\*/" contains=@Spell fold

" ── Strings ───────────────────────────────────────────────────────
syn region  kdcString          start='"' end='"'    contains=kdcEscape
syn match   kdcEscape          "\\."                contained

" ── Numbers ───────────────────────────────────────────────────────
syn match   kdcHexNumber       "\<0x[0-9a-fA-F]\+\>"
syn match   kdcDecNumber       "\<[0-9]\+\>"

" ── Version declaration ───────────────────────────────────────────
syn keyword kdcVersionKeyword  kdal_version

" ── Import / use ──────────────────────────────────────────────────
syn keyword kdcImport          import use as

" ── Top-level keywords ────────────────────────────────────────────
syn keyword kdcKeyword         driver for backend config

" ── Control flow ──────────────────────────────────────────────────
syn keyword kdcControl         if elif else while return let for in

" ── Handler / event keywords ──────────────────────────────────────
syn keyword kdcHandler         on
syn keyword kdcEvent           probe remove read write signal power timeout

" ── Power states ──────────────────────────────────────────────────
syn keyword kdcPowerState      on off suspend

" ── Type names ────────────────────────────────────────────────────
syn keyword kdcType            u8 u16 u32 u64 i8 i16 i32 i64 bool str buf

" ── Built-in functions ────────────────────────────────────────────
syn keyword kdcBuiltin         reg_read reg_write read write log
syn keyword kdcBuiltin         emit wait arm cancel

" ── Boolean / special ─────────────────────────────────────────────
syn keyword kdcBoolean         true false
syn keyword kdcSpecial         input

" ── Range operator ────────────────────────────────────────────────
syn match   kdcRange           "\.\."

" ── Operators ─────────────────────────────────────────────────────
syn match   kdcOperator        "[&|~^!<>=+\-*/%]\+"
syn match   kdcArrow           "->"

" ── Driver name (after "driver" keyword) ──────────────────────────
syn match   kdcDriverName      "\<\h\w*\>"          contained
syn match   kdcDriverDecl      "\<driver\s\+\h\w*\>" contains=kdcKeyword,kdcDriverName

" ── Handler name (after "on" keyword) ─────────────────────────────
syn match   kdcHandlerName     "\<\(probe\|remove\|read\|write\|signal\|power\|timeout\)\>" contained
syn match   kdcHandlerDecl     "\<on\s\+\(probe\|remove\|read\|write\|signal\|power\|timeout\)\>" contains=kdcHandler,kdcHandlerName

" ── Highlight links ───────────────────────────────────────────────
hi def link kdcLineComment     Comment
hi def link kdcBlockComment    Comment
hi def link kdcString          String
hi def link kdcEscape          SpecialChar
hi def link kdcHexNumber       Number
hi def link kdcDecNumber       Number
hi def link kdcVersionKeyword  PreProc
hi def link kdcImport          Include
hi def link kdcKeyword         Keyword
hi def link kdcControl         Conditional
hi def link kdcHandler         Statement
hi def link kdcEvent           Function
hi def link kdcPowerState      Constant
hi def link kdcType            Type
hi def link kdcBuiltin         Function
hi def link kdcBoolean         Boolean
hi def link kdcSpecial         Special
hi def link kdcRange           Operator
hi def link kdcOperator        Operator
hi def link kdcArrow           Operator
hi def link kdcDriverName      Identifier
hi def link kdcHandlerName     Function

let b:current_syntax = 'kdc'
