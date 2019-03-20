pipeline {
    agent any
    environment {
        AWS = credentials('AWS')
    }
    stages {
        stage('Submodules update') {
            steps {
                sh 'git submodule update --init --recursive'
            }                    
        }
        stage('Build Docker Image') {
            steps {
                sh 'eval $(docker run -e AWS_ACCESS_KEY_ID=$AWS_USR -e AWS_SECRET_ACCESS_KEY=$AWS_PSW xueshanf/awscli aws ecr get-login --region us-west-2 --no-include-email)'
                sh 'docker build -t taraxa-node -f dockerfiles/Dockerfile .'
            }                    
        }
        stage('Push Docker Image') {
            steps {
                sh 'docker tag taraxa-node 258198740961.dkr.ecr.us-west-2.amazonaws.com/taraxa-node/${env.BRANCH_NAME}:${env.BUILD_NUMBER}'
                sh 'docker push 258198740961.dkr.ecr.us-west-2.amazonaws.com/taraxa-node/${env.BRANCH_NAME}:${env.BUILD_NUMBER}'
            }                    
        }
    }   
}

  