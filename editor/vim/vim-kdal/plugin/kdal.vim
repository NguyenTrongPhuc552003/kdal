" vim-kdal - KDAL language support for Vim and Neovim.
"
" Provides:
"   - Syntax highlighting for .kdh and .kdc files
"   - Filetype detection via ftdetect/ and filetype.lua
"   - Comment/indent settings via ftplugin/
"   - Compiler plugins (kdality lint, kdalc compile)
"
" See: https://github.com/NguyenTrongPhuc552003/kdal

" Nothing to do here - all functionality is in:
"   ftdetect/kdal.vim   - filetype detection
"   syntax/kdh.vim      - .kdh syntax highlighting
"   syntax/kdc.vim      - .kdc syntax highlighting
"   ftplugin/kdh.vim    - .kdh settings
"   ftplugin/kdc.vim    - .kdc settings
"   compiler/kdality.vim - :make via kdality lint
"   compiler/kdalc.vim   - :make via kdality compile
"   filetype.lua        - Neovim >= 0.8 detection
