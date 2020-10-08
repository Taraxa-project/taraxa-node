function is_git_repo() {
  git status &>/dev/null
}

function try_git_reset() {
  if is_git_repo; then
    git clean -dfx
    git reset --hard
    git submodule foreach --recursive "git clean -dfx; git reset --hard"
  fi
}
