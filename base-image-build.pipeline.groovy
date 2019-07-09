def branchSlug(branch='') {
  def slug = ""
  if (branch == '') {
    try {
        branch = env.BRANCH_NAME
    } catch (e) {
        echo "BRANCH_NAME not in scope, probably triggered from another job"
    }
  }
  slug = branch.toLowerCase()
  slug = slug.replaceAll(/origin\//, "").replaceAll(/\W/, "-")
  return slug
}

pipeline {
    agent any
    environment {
        AWS = credentials('AWS')
        REGISTRY = '541656622270.dkr.ecr.us-west-2.amazonaws.com'
        IMAGE = 'taraxa-node-base'
        SLACK_CHANNEL = 'jenkins'
        SLACK_TEAM_DOMAIN = 'phragmites'
        DOCKER_BRANCH_TAG = sh(script: './dockerfiles/scripts/docker_tag_from_branch.sh "${BRANCH_NAME}"', , returnStdout: true).trim()
    }
    stages {
        stage('Docker Registry Login') {
            steps {
                sh 'eval $(docker run --rm -e AWS_ACCESS_KEY_ID=$AWS_USR -e AWS_SECRET_ACCESS_KEY=$AWS_PSW mendrugory/awscli aws ecr get-login --region us-west-2 --no-include-email)'
            }
        }
        stage('Build Docker Image') {
            steps {
                sh 'docker pull ${REGISTRY}/${IMAGE}|| echo "Image not found"'
                sh 'docker build --pull --cache-from=${REGISTRY}/${IMAGE} -t ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} -f dockerfiles/base.ubuntu.dockerfile .'
            }
        }
        stage('Push Docker Image') {
            when { branch 'master' }
            steps {
                sh '''
                  docker tag ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} ${REGISTRY}/${IMAGE}:${BUILD_NUMBER}
                  docker tag ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER}
                  docker push ${REGISTRY}/${IMAGE}:${BUILD_NUMBER}
                  docker push ${REGISTRY}/${IMAGE}
                '''
            }
        }
        stage('Push Docker Image ${BRANCH_NAME}') {
            when { not { branch 'master' } }
            steps {
              sh '''
                docker tag ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} ${REGISTRY}/${IMAGE}:${DOCKER_BRANCH_TAG}
                docker push ${REGISTRY}/${IMAGE}:${DOCKER_BRANCH_TAG}
              '''
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
