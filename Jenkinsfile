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
                sh 'eval $(AWS_ACCESS_KEY_ID=$AWS_USR AWS_SECRET_ACCESS_KEY=$AWS_PSW aws ecr get-login --region us-west-2 --no-include-email)'
                sh 'docker build -t taraxa-node -f dockerfiles/Dockerfile'
            }                    
        }
    }   
}