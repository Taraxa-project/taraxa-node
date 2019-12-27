function copy() {
  local base_dir=$1
  local pattern=$2
  local dest_dir=$3
  find "$base_dir" -path "$base_dir/$pattern" | while read fname; do
    local dest_file=${fname/"$base_dir"/$dest_dir}
    mkdir -p $(dirname "$dest_file")
    cp "$fname" "$dest_file"
  done
}

function mkdir_cd() {
  local dir=$1
  mkdir -p $dir && cd $dir
}
