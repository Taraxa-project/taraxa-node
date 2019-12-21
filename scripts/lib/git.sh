function try_git_reset() {
  if git status &>/dev/null; then
    git clean -dfx
    git reset --hard
    git submodule foreach --recursive "git clean -dfx; git reset --hard"
  fi
}
