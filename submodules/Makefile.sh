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

# a quick no-op if everything is in sync
function submodule_upd() {
  if ! git status &>/dev/null; then
    echo "not a git repo"
    return 0
  fi
  echo "Updating submodules..."
  git submodule update --jobs "$(../scripts/cpu_count.sh)" --init --recursive
}
