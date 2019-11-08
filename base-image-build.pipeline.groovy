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
        GCP_REGISTRY = 'gcr.io/jovial-meridian-249123'
        IMAGE = 'taraxa-node-base'
        SLACK_CHANNEL = 'jenkins'
        SLACK_TEAM_DOMAIN = 'phragmites'
        DOCKER_BRANCH_TAG = sh(script: './dockerfiles/scripts/docker_tag_from_branch.sh "${BRANCH_NAME}"', , returnStdout: true).trim()
    }
    stages {
        stage('Docker Registry Login') {
            steps {
                withCredentials([string(credentialsId: 'gcp_container_registry_key_json', variable: 'GCP_KEY')]) {
                  sh 'echo ${GCP_KEY} | docker login -u _json_key --password-stdin https://gcr.io'
                }
            }
        }
        stage('Build Docker Image') {
            steps {
                sh 'docker pull ${GCP_REGISTRY}/${IMAGE}|| echo "Image not found"'
                sh 'DOCKER_BUILDKIT=1 docker build --progress=plain --pull --cache-from=${GCP_REGISTRY}/${IMAGE} -t ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} -f dockerfiles/base.ubuntu.dockerfile .'
            }
        }
        stage('Push Docker Image') {
            when { branch 'master' }
            steps {
                sh '''
                  docker tag ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} ${GCP_REGISTRY}/${IMAGE}:${BUILD_NUMBER}
                  docker tag ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} ${GCP_REGISTRY}/${IMAGE}:latest

                  docker push ${GCP_REGISTRY}/${IMAGE}:${BUILD_NUMBER}
                  docker push ${GCP_REGISTRY}/${IMAGE}:latest
                '''
            }
        }
        stage('Push Docker Image ${BRANCH_NAME}') {
            when { not { branch 'master' } }
            steps {
              sh '''
                docker tag ${IMAGE}-${DOCKER_BRANCH_TAG}-${BUILD_NUMBER} ${GCP_REGISTRY}/${IMAGE}:${DOCKER_BRANCH_TAG}
                docker push ${GCP_REGISTRY}/${IMAGE}:${DOCKER_BRANCH_TAG}
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
