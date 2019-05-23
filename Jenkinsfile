pipeline {
    agent any
    environment {
        AWS = credentials('AWS')
        REGISTRY = '541656622270.dkr.ecr.us-west-2.amazonaws.com'
        IMAGE = 'taraxa-node'
        BASE_IMAGE = 'taraxa-node-base'
        SLACK_CHANNEL = 'jenkins'
        SLACK_TEAM_DOMAIN = 'phragmites'
        BRANCH_NAME_LOWER_CASE = sh(script: 'echo "${BRANCH_NAME}" | tr "[:upper:]" "[:lower:]"', , returnStdout: true).trim()
    }
    stages {
        stage('Docker Registry Login') {
            steps {
                sh 'eval $(docker run --rm -e AWS_ACCESS_KEY_ID=$AWS_USR -e AWS_SECRET_ACCESS_KEY=$AWS_PSW mendrugory/awscli aws ecr get-login --region us-west-2 --no-include-email)'
            }                    
        }     
        stage('Unit Tests') {
            agent {
                docker {
                    image '${REGISTRY}/${BASE_IMAGE}'
                }
            }
            steps {
                sh 'git submodule update --init --recursive'
                sh 'make run_test'
            }                    
        }            
        stage('Build Docker Image') {
            steps {
                sh 'git submodule update --init --recursive'
                sh 'docker build --pull -t ${IMAGE}-${BRANCH_NAME_LOWER_CASE}-${BUILD_NUMBER} -f dockerfiles/Dockerfile .'
            }                    
        }
        stage('Smoke Test') {
            steps {
                sh 'docker network create --driver=bridge \
                    --subnet=172.100.1.0/24 --gateway=172.100.1.1 \
                    --ip-range=172.100.1.2/25 smoke-test-net'
                sh 'docker run --rm -d --name taraxa-node-smoke-test --net smoke-test-net ${IMAGE}-${BRANCH_NAME_LOWER_CASE}-${BUILD_NUMBER}'
                sh ''' docker run --rm --net smoke-test-net byrnedo/alpine-curl -d \"{ 
                        \"action\": \"insert_stamped_dag_block\", 
                        \"pivot\": \"0000000000000000000000000000000000000000000000000000000000000000\", 
                        \"hash\": \"0000000000000000000000000000000000000000000000000000000000000001\", 
                        \"sender\":\"000000000000000000000000000000000000000000000000000000000000000F\", 
                        \"tips\": [], \"stamp\": 43}\" \
                        taraxa-node-smoke-test:7777
                   '''
            } 
            post {
                always {
                    sh 'docker kill taraxa-node-smoke-test || true'
                    sh 'docker network rm smoke-test-net-${BRANCH_NAME} || true'
                }
            }                   
        }        
        stage('Push Docker Image') {
            when {branch 'master'}            
            steps {
                sh 'docker tag ${IMAGE}-${BRANCH_NAME_LOWER_CASE}-${BUILD_NUMBER} ${REGISTRY}/${IMAGE}:${BUILD_NUMBER}'
                sh 'docker tag ${IMAGE}-${BRANCH_NAME_LOWER_CASE}-${BUILD_NUMBER} ${REGISTRY}/${IMAGE}'
                sh 'docker push ${REGISTRY}/${IMAGE}:${BUILD_NUMBER}'
                sh 'docker push ${REGISTRY}/${IMAGE}'
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