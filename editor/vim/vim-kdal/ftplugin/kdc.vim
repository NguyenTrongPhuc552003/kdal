" Vim ftplugin for KDAL Driver Code (.kdc)
" Sets comment strings, indentation, and folding.

if exists('b:did_ftplugin')
  finish
endif
let b:did_ftplugin = 1

" Comment format for vim-commentary and built-in comment toggling
setlocal commentstring=//\ %s
setlocal comments=sO:*\ -,mO:*\ \ ,exO:*/,s1:/*,mb:*,ex:*/,://

" Indentation
setlocal expandtab
setlocal shiftwidth=4
setlocal softtabstop=4
setlocal tabstop=4
setlocal smartindent

" Fold on braces
setlocal foldmethod=syntax
setlocal foldlevel=99

" Match pairs
setlocal matchpairs+=<:>

" File-local undo
let b:undo_ftplugin = 'setl commentstring< comments< expandtab< shiftwidth< softtabstop< tabstop< smartindent< foldmethod< foldlevel< matchpairs<'
