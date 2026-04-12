" Vim compiler plugin for kdalc compiler
" Usage:  :compiler kdalc
"         :make          - compiles current .kdc file
"         :copen         - view results in quickfix

if exists('current_compiler')
  finish
endif
let current_compiler = 'kdalc'

CompilerSet makeprg=kdality\ compile\ %
" Error format: file.kdc:10: error: message
"               file.kdc:5:12: warning: message
CompilerSet errorformat=%f:%l:%c:\ %t%*[^:]:\ %m,%f:%l:\ %t%*[^:]:\ %m
