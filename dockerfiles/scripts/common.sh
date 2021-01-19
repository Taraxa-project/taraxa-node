source ./taraxa_variables.conf

RED=$'\033[0;31m'
CYAN=$'\e[1;96m'
COLOR_END=$'\033[0m' # No Color

# Checks if taraxa_builder image exists, if it does not - it creates it
function LoadTaraxaBuilderImage {
  if [[ "$(docker images -q $taraxa_builder_image 2> /dev/null)" == "" ]];
  then
    echo "Taraxa build image " $taraxa_builder_image " not found. Creating one..."
    ./scripts/build_taraxa_builder_image.sh
  else
    echo "Taraxa build image " $taraxa_builder_image " found."
  fi
}

# Load config variable from taraxa_variables.conf file and return it in form:
# "--build-arg ARG_NAME=ARG_VALUE"
# Call this function with ARG_NAME argument, e.g  LoadConfigVar TARAXA_BUILD_DIR
function LoadConfigVar {
  ARG=$1

  if [[ -z $ARG ]]; then
    echo -e $RED"LoadArg called without provided argument."$COLOR_END
	  exit 1
  fi

  ret_val="--build-arg $ARG=${!ARG}"
  echo "$ret_val"
}

function ChownDir {
    docker run -it --rm -v $(pwd)/$1:/$1 -w / -e QUID=$UID -e QGID=$(id -g $USER) golang:1.12.7 /bin/sh -c 'chown -R $QUID:$QGID '$1
}