function try_git_reset() {
  if [ ! -e .git ]; then
    return 0
  fi
  (
    git clean -dfx
    git reset --hard
    git submodule foreach --recursive "git clean -dfx; git reset --hard"
  ) &>/dev/null
}

function submodule_list() {
  git config --file .gitmodules --get-regexp path | awk '{ print $2 }'
}
