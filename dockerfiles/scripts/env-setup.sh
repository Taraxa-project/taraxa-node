#!/usr/bin/env bash

RED="\033[1;31m"
GREEN="\033[1;32m"
RESET="\033[0m"

echo_green() {
    local msg="${@}"
    printf "${GREEN}%b${RESET}\n" "${msg}"
}

_CONFIG_FILE=env_variables.sh
# Confirm continue if file exists
if [ -f "$_CONFIG_FILE" ] ; then
    echo_green "The file exists ${_CONFIG_FILE}"
    read -p "Please confirm that you want to continue (y/n) " _ANS
	if [ "$_ANS" != "y" ]; then
	    echo_green "Exiting..."
	    exit 0
	fi
fi
echo "Creating ${_CONFIG_FILE}"
AWS_PROFILE=${AWS_PROFILE:-default}
if [ -z "${AWS_ACCESS_KEY_ID}" ]; then
AWS_ACCESS_KEY_ID=$(aws configure get ${AWS_PROFILE}.aws_access_key_id)
fi
if [ -z "${AWS_SECRET_ACCESS_KEY}" ]; then
AWS_SECRET_ACCESS_KEY=$(aws configure get ${AWS_PROFILE}.aws_secret_access_key)
fi
if [ -z "${AWS_DEFAULT_REGION}" ]; then
    AWS_DEFAULT_REGION="us-west-2"
fi
cat << EOF > ${_CONFIG_FILE}
#!/usr/bin/env bash

# AWS variables
export AWS_ACCESS_KEY_ID="${AWS_ACCESS_KEY_ID}"
export AWS_SECRET_ACCESS_KEY="${AWS_SECRET_ACCESS_KEY}"
export AWS_DEFAULT_REGION="${AWS_DEFAULT_REGION}"
EOF