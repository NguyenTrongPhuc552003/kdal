" Vim compiler plugin for KDAL (kdality lint)
" Usage:  :compiler kdality
"         :make          - runs kdality lint on current file
"         :copen         - view results in quickfix

if exists('current_compiler')
  finish
endif
let current_compiler = 'kdality'

CompilerSet makeprg=kdality\ lint\ %
" Error format: file.kdh:10: warning: message
"               file.kdc:5:12: error: message
CompilerSet errorformat=%f:%l:%c:\ %t%*[^:]:\ %m,%f:%l:\ %t%*[^:]:\ %m
