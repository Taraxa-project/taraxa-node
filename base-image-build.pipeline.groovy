pipeline {
    agent any
    environment {
        AWS = credentials('AWS')
        REGISTRY = '541656622270.dkr.ecr.us-west-2.amazonaws.com'
        IMAGE = 'taraxa-node-base'
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
        stage('Build Docker Image') {
            steps {
                sh 'docker build --pull -t ${IMAGE}-${BRANCH_NAME_LOWER_CASE}-${BUILD_NUMBER} -f dockerfiles/base.ubuntu.dockerfile .'
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
