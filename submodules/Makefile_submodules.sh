set -eo errtrace

# a copy function that you always wanted in bash
function copy() {
  local base_dir=$1
  local pattern=$2
  local dest_dir=$3
  find "$base_dir" -path "$base_dir/$pattern" | while read -r fname; do
    local dest_file=${fname/"$base_dir"/$dest_dir}
    mkdir -p "$(dirname "$dest_file")"
    cp "$fname" "$dest_file"
  done
}

function mkdir_cd() {
  local dir=$1
  mkdir -p "$dir" && cd "$dir"
}

# parse 'path' entries from .gitmodules
function submodule_list() {
  grep path .gitmodules | sed 's/.*= //'
}

function is_git_repo() {
  git status &>/dev/null
}

function git_clean() {
  if is_git_repo; then
    git clean -dfx
    git submodule foreach --quiet --recursive git clean -dfx
  fi
}

# a quick no-op if everything is in sync
function submodule_upd() {
  if ! is_git_repo; then
    echo "not a git repo"
    return 0
  fi
  echo "Updating submodules..."
  git submodule status | while read line; do
    if [ "${line:0:1}" == "+" ]; then
      line_words=("$line")
      path=${line_words[1]}
      echo "Resetting submodule ${path} because it's version is different from the committed version"
      (
        cd "${path}"
        git_clean
      )
    fi
  done
  git submodule update --jobs "$(scripts/cpu_count.sh)" --init --recursive
}
