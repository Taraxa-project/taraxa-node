basedir=$(realpath $(dirname "$0")/..)
cpu_cnt=$(${basedir}/../../scripts/cpu_count.sh)
dst=${basedir}/system_home_override
workdir=${basedir}/downloads

mkdir -p ${workdir} ${dst}
cd ${workdir}

function brew_install() {
  package_name=$1
  workdir_=${workdir}/${package_name}
  echo "installing ${package_name} via brew..."
  if [ ! -e ${workdir_} ]; then
    mkdir -p ${workdir_}
    brew install ${package_name}
    echo "Running command as sudo..."
    sudo cp -r $(brew --prefix ${package_name})/* ${workdir_}/
    sudo chown -R $(whoami) ${workdir_}
  fi
  cp -r ${workdir_}/* ${dst}/
}
