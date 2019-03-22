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
        stage('Docker Registry Login') {
            steps {
                sh 'eval $(docker run -e AWS_ACCESS_KEY_ID=$AWS_USR -e AWS_SECRET_ACCESS_KEY=$AWS_PSW xueshanf/awscli aws ecr get-login --region us-west-2 --no-include-email)'
            }                    
        }     
        stage('Unit Tests') {
            agent {
                docekrfile {
                    filename 'dockerfiles/base.ubuntu.dockerfile'
                }
            }
            steps {
                sh 'make runt_test'
            }                    
        }            
        stage('Build Docker Image') {
            steps {
                sh 'docker build -t taraxa-node -f dockerfiles/Dockerfile .'
            }                    
        }
        stage('Push Docker Image') {
            steps {
                sh 'docker tag taraxa-node 541656622270.dkr.ecr.us-west-2.amazonaws.com/taraxa-node/${BRANCH_NAME}:${BUILD_NUMBER}'
                sh 'docker push 541656622270.dkr.ecr.us-west-2.amazonaws.com/taraxa-node/${BRANCH_NAME}:${BUILD_NUMBER}'
            }                    
        }
    }   
}

  