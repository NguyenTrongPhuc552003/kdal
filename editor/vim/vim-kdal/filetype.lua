-- Neovim filetype detection for KDAL files.
-- This file is picked up automatically by Neovim >= 0.8 when the
-- plugin is in the runtimepath.

vim.filetype.add({
  extension = {
    kdh = "kdh",
    kdc = "kdc",
  },
})
