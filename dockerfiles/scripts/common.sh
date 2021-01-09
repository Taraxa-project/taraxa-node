source ./taraxa.variables

RED=$'\033[0;31m'
CYAN=$'\e[1;96m'
COLOR_END=$'\033[0m' # No Color

function LoadTaraxaBuilderImage {
  if [[ "$(docker images -q $taraxa_builder_image 2> /dev/null)" == "" ]];
  then
    echo "Taraxa build image " $taraxa_builder_image " not found. Creating one..."
    DOCKER_BUILDKIT=1 docker build --no-cache --progress=plain -t $taraxa_builder_image -f taraxa_builder.dockerfile .
  else
    echo "Taraxa build image " $taraxa_builder_image " found."
  fi
}

function ChownDir {
    docker run -it --rm -v $(pwd)/$1:/$1 -w / -e QUID=$UID -e QGID=$(id -g $USER) golang:1.12.7 /bin/sh -c 'chown -R $QUID:$QGID '$1
}
