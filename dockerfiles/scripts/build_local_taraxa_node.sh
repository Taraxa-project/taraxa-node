#!/bin/bash

source scripts/common.sh
source ./taraxa.variables

if [ ! -d "../../taraxa-node" ];
  then
	  echo -e $RED"Source code for taraxa-node not found in parent directory. Unable to build."$COLOR_END
	  exit 1
	else
	  echo -e "Local source code for taraxa-node found in ../../taraxa-node."
fi

# Load taraxa-builder docker image
LoadTaraxaBuilderImage

echo $CYAN"Building Taraxa node, branch/tag: \""TODO"\"" $COLOR_END

## Clone & init taraxa-node git repository
## private submodules: submodules/taraxa-aleth and submodules/taraxa-evm are updated separately as docker does not wait for credentials input when running update for all submodules at once
#docker run -ti --rm -v ${HOME}/.ssh:/root -v $(pwd):/git -w /git/taraxa-node alpine/git submodule update --init --recursive submodules/taraxa-aleth
#docker run -ti --rm -v ${HOME}/.ssh:/root -v $(pwd):/git -w /git/taraxa-node alpine/git submodule update --init --recursive submodules/taraxa-evm
#docker run -ti --rm -v ${HOME}/.ssh:/root -v $(pwd):/git -w /git/taraxa-node alpine/git submodule update --init --recursive

# Build taraxa-node using taraxa-build-image
#TODO: use ccache for building
docker run -it --rm \
    -v $(pwd)/..:/taraxa-node \
    -w /taraxa-node \
    $taraxa_builder_image \
    /bin/bash -l -c 'mkdir -p dockerfiles/'$node_local_build_dir'/install;
                     cd dockerfiles/'$node_local_build_dir';

                     cmake -DCMAKE_BUILD_TYPE=Release \
                           -DTARAXAD_INSTALL_DIR=./install \
                           -DTARAXAD_CONF_INSTALL_DIR=./install \
                           ../../;
                     make -j$(nproc) all  # automatically runs cppech and clang-format-check
                     make install;

                     cd tests/
                     ctest'

# strip taraxad binary
strip $node_local_build_dir/install/taraxad

# Change the ownership of build directory
ChownDir $node_local_build_dir

echo $CYAN"Building Taraxa node, branch/tag: \""TODO"\" Completed." $COLOR_END