" Vim syntax file
" Language:    KDAL Device Header
" Maintainer:  Trong Phuc Nguyen <trongphuc.ng552003@gmail.com>
" URL:         https://github.com/NguyenTrongPhuc552003/kdal
" License:     GPL-3.0-or-later
" Last Change: 2026-04-12

if exists('b:current_syntax')
  finish
endif

" ── Comments ──────────────────────────────────────────────────────
syn match   kdhLineComment     "//.*$"              contains=@Spell
syn region  kdhBlockComment    start="/\*" end="\*/" contains=@Spell fold

" ── Strings ───────────────────────────────────────────────────────
syn region  kdhString          start='"' end='"'    contains=kdhEscape
syn match   kdhEscape          "\\."                contained

" ── Numbers ───────────────────────────────────────────────────────
syn match   kdhHexNumber       "\<0x[0-9a-fA-F]\+\>"
syn match   kdhDecNumber       "\<[0-9]\+\>"

" ── Version declaration ───────────────────────────────────────────
syn keyword kdhVersionKeyword  kdal_version

" ── Import / as ───────────────────────────────────────────────────
syn keyword kdhImport          import as

" ── Top-level keywords ────────────────────────────────────────────
syn keyword kdhKeyword         device class compatible

" ── Section keywords ──────────────────────────────────────────────
syn keyword kdhSection         registers register_map signals capabilities
syn keyword kdhSection         power power_states config bitfield

" ── Type names ────────────────────────────────────────────────────
syn keyword kdhType            u8 u16 u32 u64

" ── Access qualifiers ─────────────────────────────────────────────
syn keyword kdhAccess          ro wo rw rc

" ── Signal direction ──────────────────────────────────────────────
syn keyword kdhDirection       in out inout

" ── Signal triggers ───────────────────────────────────────────────
syn keyword kdhTrigger         edge level completion timeout
syn keyword kdhEdgeLevel       rising falling any high low

" ── Power state keywords ──────────────────────────────────────────
syn keyword kdhPowerState      on off suspend
syn keyword kdhPowerSpec       allowed forbidden default

" ── Boolean literals ──────────────────────────────────────────────
syn keyword kdhBoolean         true false

" ── Bit range operator ────────────────────────────────────────────
syn match   kdhBitRange        "\.\."

" ── Device class name (after "class" keyword) ─────────────────────
syn match   kdhClassName       "\<\h\w*\>"          contained
syn match   kdhClassDecl       "\<class\s\+\h\w*\>" contains=kdhKeyword,kdhClassName

" ── Highlight links ───────────────────────────────────────────────
hi def link kdhLineComment     Comment
hi def link kdhBlockComment    Comment
hi def link kdhString          String
hi def link kdhEscape          SpecialChar
hi def link kdhHexNumber       Number
hi def link kdhDecNumber       Number
hi def link kdhVersionKeyword  PreProc
hi def link kdhImport          Include
hi def link kdhKeyword         Keyword
hi def link kdhSection         Structure
hi def link kdhType            Type
hi def link kdhAccess          StorageClass
hi def link kdhDirection       StorageClass
hi def link kdhTrigger         Function
hi def link kdhEdgeLevel       Constant
hi def link kdhPowerState      Constant
hi def link kdhPowerSpec       Constant
hi def link kdhBoolean         Boolean
hi def link kdhBitRange        Operator
hi def link kdhClassName       Identifier

let b:current_syntax = 'kdh'
