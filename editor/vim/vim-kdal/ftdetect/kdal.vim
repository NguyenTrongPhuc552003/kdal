" Detect KDAL file types
" .kdh  - KDAL Device Header
" .kdc  - KDAL Driver Code

autocmd BufNewFile,BufRead *.kdh setfiletype kdh
autocmd BufNewFile,BufRead *.kdc setfiletype kdc
