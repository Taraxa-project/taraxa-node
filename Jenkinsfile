pipeline {
    agent any
    environment {
        GCP_REGISTRY = 'gcr.io/jovial-meridian-249123'
        IMAGE = 'taraxa-node'
        BASE_IMAGE = 'taraxa-node-base'
        SLACK_CHANNEL = 'jenkins'
        SLACK_TEAM_DOMAIN = 'phragmites'
        DOCKER_BRANCH_TAG = sh(script: './dockerfiles/scripts/docker_tag_from_branch.sh "${BRANCH_NAME}"', , returnStdout: true).trim()
    }
    options {
      ansiColor('xterm')
      disableConcurrentBuilds()
    }
    stages {
        stage('Validate C++ formatting') {
            steps {
               sh './scripts/validate_format_project_files_cxx.sh'
            }
        }
        stage('Trigger Base Image Build') {
            steps {
              build job: "docker-base-image/${BRANCH_NAME}", parameters: [
                  string(name: 'upsteam_project_name', value: env.NAME)
              ], propagate: true, wait: true

            }
        }
        stage('Docker Registry Login') {
            steps {
                withCredentials([string(credentialsId: 'gcp_container_registry_key_json', variable: 'GCP_KEY')]) {
                  sh 'echo ${GCP_KEY} | docker login -u _json_key --password-stdin https://gcr.io'
                }
            }
        }
        stage('Unit Tests') {
            agent {
              docker {
                alwaysPull true
                image "${GCP_REGISTRY}/${BASE_IMAGE}:${DOCKER_BRANCH_TAG}"
                reuseNode true
              }
            }
            steps {
                sh 'make DEBUG=1 -j 4 run_test'
            }
        }
        stage('Build Docker Image') {
            steps {
                sh '''
                    git submodule update --init --recursive
                    docker build --pull --cache-from=${GCP_REGISTRY}/${IMAGE} \
                    -t ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} \
                    --build-arg BASE_IMAGE=${GCP_REGISTRY}/${BASE_IMAGE}:${DOCKER_BRANCH_TAG} \
                    -f dockerfiles/Dockerfile .
                '''
            }
        }
        stage('Smoke Test') {
            steps {
                sh '''
                    if [ ! -z "$(docker network list --format '{{.Name}}' | grep -o smoke-test-net-${DOCKER_BRANCH_TAG})" ]; then
                      docker network rm \
                        smoke-test-net-${DOCKER_BRANCH_TAG} >/dev/null;
                    fi
                    docker network create --driver=bridge \
                      smoke-test-net-${DOCKER_BRANCH_TAG}
                '''
                sh 'docker run --rm -d --name taraxa-node-smoke-test --net smoke-test-net-${DOCKER_BRANCH_TAG} ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER}'
                sh '''
                    mkdir -p  $PWD/test_build-d/
                    http_code=$(docker run --rm --net smoke-test-net-${DOCKER_BRANCH_TAG}  -v $PWD/test_build-d:/data byrnedo/alpine-curl \
                                       -sS --fail -w '%{http_code}' -o /data/http.out \
                                       --url taraxa-node-smoke-test:7777 \
                                       -d '{
                                            "jsonrpc": "2.0",
                                            "id":"0",
                                            "method": "send_coin_transaction",
                                            "params":[{
                                            "nonce": 0,
                                            "value": 0,
                                            "gas": 0,
                                            "gas_price": 0,
                                            "receiver": "973ecb1c08c8eb5a7eaa0d3fd3aab7924f2838b0",
                                            "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd"]
                                            }')
                    cat $PWD/test_build-d/http.out || true
                    if [[ $http_code -eq 200 ]] ; then
                        exit 0
                    else
                        exit $http_code
                    fi

                   '''
            }
            post {
                always {
                    sh 'docker kill taraxa-node-smoke-test || true'
                    sh 'docker network rm smoke-test-net-${DOCKER_BRANCH_TAG} || true'
                }
            }
        }
        stage('Push Docker Image') {
            when {branch 'master'}
            steps {
                sh 'docker tag ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} ${GCP_REGISTRY}/${IMAGE}:${BUILD_NUMBER}'
                sh 'docker tag ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} ${GCP_REGISTRY}/${IMAGE}'
                sh 'docker push ${GCP_REGISTRY}/${IMAGE}:${BUILD_NUMBER}'
                sh 'docker push ${GCP_REGISTRY}/${IMAGE}'
            }
        }
    }
post {
    always {
        cleanWs()
    }
    success {
      slackSend (channel: "${SLACK_CHANNEL}", teamDomain: "${SLACK_TEAM_DOMAIN}", tokenCredentialId: 'SLACK_TOKEN_ID',
                color: '#00FF00', message: "SUCCESSFUL: Job '${JOB_NAME} [${BUILD_NUMBER}]' (${BUILD_URL})")
    }
    failure {
      slackSend (channel: "${SLACK_CHANNEL}", teamDomain: "${SLACK_TEAM_DOMAIN}", tokenCredentialId: 'SLACK_TOKEN_ID',
                color: '#FF0000', message: "FAILED: Job '${JOB_NAME} [${BUILD_NUMBER}]' (${BUILD_URL})")
    }
  }
}
