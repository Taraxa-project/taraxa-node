library identifier: 'jenkinsfile-library@master', retriever: modernSCM(
    [$class: 'GitSCMSource',
     remote: 'https://github.com/Taraxa-project/jenkinsfile-library.git',
     credentialsId: 'a9e63ab7-4c38-4644-8829-5f1144995c44'])

def getChart(){
        dir('taraxa-testnet') {
                git(
                    branch: 'development',
                    url: 'https://github.com/Taraxa-project/taraxa-testnet.git',
                    credentialsId: 'a9e63ab7-4c38-4644-8829-5f1144995c44'
                )
            }
}

// TODO: Add helm as a container
// TODO: Configure kubeconfig with jenkins variables

def helmVersion() {
    println "Checking client/server version"
    sh "helm version"
}

def helmInstallChart(String test_name, String image, String tag) {
    dir ('taraxa-testnet/tests') {
        println "Installing helm chart"
        sh """
            helm install --name ${test_name} taraxa-node \
                --wait \
                --atomic \
                --timeout 1200 \
                --namespace ${test_name} \
                --set replicaCount=5 \
                --set test.pythontester.script=jenkins.py \
                --set image.repository=${image} \
                --set image.tag=${tag} \
                -f taraxa-node/values.yaml
        """
    }
}

def helmTestChart(String test_name) {
    dir ('taraxa-testnet/tests') {
        println "Running helm test"
        sh """
            helm test ${test_name} \
                --timeout 3600 \
                --cleanup
        """
    }
}

def helmCleanChart(String test_name) {
    println "Cleaning helm test ${test_name}"
    sh "helm delete --purge ${test_name} || true"
    println "Cleaning namespace ${test_name}"
    sh "kubectl delete ns ${test_name} || true"
}

pipeline {
    agent any
    environment {
        GCP_REGISTRY = 'gcr.io/jovial-meridian-249123'
        IMAGE = 'taraxa-node'
        DOCKER_BRANCH_TAG = sh(script: './scripts/docker_tag_from_branch.sh "${BRANCH_NAME}"', , returnStdout: true).trim()
        HELM_TEST_NAME = sh(script: 'echo ${BRANCH_NAME} | sed "s/[^A-Za-z0-9\\-]*//g" | tr "[:upper:]" "[:lower:]"', returnStdout: true).trim()
        KIBANA_URL='kibana.gcp.taraxa.io'
        START_TIME = sh(returnStdout: true, script: 'date +%Y%m%d_%Hh%Mm%Ss').trim()
    }
    options {
      ansiColor('xterm')
      disableConcurrentBuilds()
    }
    stages {
        stage('Docker GCP Registry Login') {
            steps {
                withCredentials([string(credentialsId: 'gcp_container_registry_key_json', variable: 'GCP_KEY')]) {
                  sh 'echo ${GCP_KEY} | docker login -u _json_key --password-stdin https://gcr.io'
                }
            }
        }
        stage('Docker-Hub Registry Login') {
            steps {
                withCredentials([string(credentialsId: 'docker_hub_taraxa_pass', variable: 'HUB_PASS')]) {
                  sh 'echo ${HUB_PASS} | docker login -u taraxa --password-stdin'
                }
            }
        }
        stage('Build Docker Image') {
            steps {
                sh '''
                    DOCKER_BUILDKIT=1 docker build \
                    -t ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} .
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
                sh 'docker run --rm -d --name taraxa-node-smoke-test-${DOCKER_BRANCH_TAG} --net smoke-test-net-${DOCKER_BRANCH_TAG} ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} single'
                sh '''
                    mkdir -p  $PWD/test_build-d/
                    sleep 30
                    http_code=$(docker run --rm --net smoke-test-net-${DOCKER_BRANCH_TAG}  -v $PWD/test_build-d:/data byrnedo/alpine-curl \
                                       -sS --fail -w '%{http_code}' -o /data/http.out \
                                       --url taraxa-node-smoke-test-${DOCKER_BRANCH_TAG}:7777 \
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
                                                    "secret": "3800b2875669d9b2053c1aff9224ecfdc411423aac5b5a73d7a45ced1c3b9dcd"
                                                }]
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
                    sh 'docker kill taraxa-node-smoke-test-${DOCKER_BRANCH_TAG} || true'
                    sh 'docker network rm smoke-test-net-${DOCKER_BRANCH_TAG} || true'
                }
            }
        }
        stage('Push Docker Image For test') {
            when { branch 'PR-*' }
            steps {
                sh 'docker tag ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} ${GCP_REGISTRY}/${IMAGE}:${HELM_TEST_NAME}-build-${BUILD_NUMBER}'
                sh 'docker tag ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} ${GCP_REGISTRY}/${IMAGE}:${GIT_COMMIT}'
                sh 'docker push ${GCP_REGISTRY}/${IMAGE}:${HELM_TEST_NAME}-build-${BUILD_NUMBER}'
                sh 'docker push ${GCP_REGISTRY}/${IMAGE}:${GIT_COMMIT}'
            }
        }
        stage('Run kubernetes tests') {
            when { branch 'PR-*' }
            steps {
                getChart()
                helmVersion()
                helmInstallChart("${HELM_TEST_NAME}", "${GCP_REGISTRY}/${IMAGE}", "${HELM_TEST_NAME}-build-${BUILD_NUMBER}")
                helmTestChart("${HELM_TEST_NAME}")
            }
            post {
                always {
                    sh './scripts/kibana-url.sh || true'
                    helmCleanChart("${HELM_TEST_NAME}")
                }
            }
        }
        stage('Push Docker Image') {
            when {branch 'master'}
            steps {
                sh 'docker tag ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} ${GCP_REGISTRY}/${IMAGE}:${GIT_COMMIT}'
                sh 'docker tag ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} ${GCP_REGISTRY}/${IMAGE}:${START_TIME}'
                sh 'docker tag ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} ${GCP_REGISTRY}/${IMAGE}:${START_TIME}-${GIT_COMMIT}'
                sh 'docker tag ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} ${GCP_REGISTRY}/${IMAGE}:latest'
                sh 'docker tag ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} ${GCP_REGISTRY}/${IMAGE}'
                sh 'docker tag ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} taraxa/${IMAGE}:${GIT_COMMIT}'
                sh 'docker tag ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} taraxa/${IMAGE}:${START_TIME}'
                sh 'docker tag ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} taraxa/${IMAGE}:${START_TIME}-${GIT_COMMIT}'
                sh 'docker tag ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} taraxa/${IMAGE}:latest'
                sh 'docker tag ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} taraxa/${IMAGE}'
                sh 'docker push ${GCP_REGISTRY}/${IMAGE}'
                sh 'docker push taraxa/${IMAGE}'
            }
        }
    }
post {
    always {
        cleanWs()
    }
    success {
      slackSendCustom(status: 'success')
    }
    failure {
      slackSendCustom(status: 'failed')
    }
  }
}
