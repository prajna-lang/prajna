pipeline{
    agent none

    options {
        timeout(time: 2, unit: 'HOURS')
    }
    triggers {
        cron('30 07 * * *') // run at 07:30
    }

    stages {
        stage('PRAJNA CI'){
            // failFast true
            parallel {
                stage('x64-win11-release') {
                    agent {
                        label "Win11"
                    }
                    environment{
                        BUILD_TYPE = 'release'
                    }
                    stages {
                        stage('env') {
                            steps {
                                bat 'git --version'
                                bat 'cmake --version'
                                bat 'git config --global --list'
                            }
                        }
                        stage('build') {
                            steps {
                                bat 'bash ./scripts/clone_submodules.sh'
                                bat 'call ./scripts/windows_build.bat %BUILD_TYPE%'
                            }
                        }
                        stage('test') {
                            steps {
                                bat 'bash ./scripts/test.sh %BUILD_TYPE%'
                                bat 'bash ./scripts/test_examples.sh %BUILD_TYPE%'
                            }
                        }
                    }
                }

                stage('x64-linux-nvgpu-debug') {
                    agent {
                        dockerfile {
                            filename 'ubuntu_dev_nvgpu.dockerfile'
                            dir 'dockerfiles'
                            label 'Sunny'
                            //需要jenkins也用root权限启动, 目前使用其他权限会有很多问题, root权限存在极大的安全隐患
                            args '--gpus all -u root:root --network host'
                        }
                    }
                    environment {
                        CXX = 'clang++'
                        CC = 'clang'
                        BUILD_TYPE = 'debug'
                    }
                    stages {
                        stage('env') {
                            steps {
                                sh 'uname -a'
                                sh 'echo $USER'
                                sh 'cmake --version'
                                sh 'clang++ --version'
                                sh 'nvidia-smi'
                                sh 'pwd'
                                sh 'git --version'
                                sh 'git config --global http.version HTTP/1.1'
                                sh 'git config --global http.postBuffer 524288000'
                                sh 'git config --global http.lowSpeedLimit 0'
                                sh 'git config --global http.lowSpeedTime 999999'
                                sh 'git config --global --add safe.directory "*"'
                                sh 'git config --global --list'
                            }
                        }
                        stage('build') {
                            steps {
                                sh './scripts/clone_submodules.sh -f'
                                sh './scripts/configure.sh ${BUILD_TYPE} -DWITH_TLS=OFF -DPRAJNA_WITH_JUPYTER=OFF'
                                sh './scripts/build.sh ${BUILD_TYPE} install'
                                // 需要安装llc
                                   // 需要安装llc
                                sh 'cp build_${BUILD_TYPE}/bin/llc /usr/bin'
                            }
                        }
                        stage('test') {
                            steps {
                                sh './scripts/test.sh ${BUILD_TYPE}'
                                sh './scripts/test_examples.sh ${BUILD_TYPE}'
                            }
                        }
                    }
                }

                stage('x64-linux-nvgpu-release') {
                    agent {
                        dockerfile {
                            filename 'ubuntu_dev_nvgpu.dockerfile'
                            dir 'dockerfiles'
                            label 'Sunny'
                            //需要jenkins也用root权限启动, 目前使用其他权限会有很多问题, root权限存在极大的安全隐患
                            args '--gpus all -u root:root --network host'
                        }
                    }
                    environment {
                        CXX = 'clang++'
                        CC = 'clang'
                        BUILD_TYPE = 'release'
                    }
                    stages {
                        stage('env') {
                            steps {
                                sh 'uname -a'
                                sh 'echo $USER'
                                sh 'cmake --version'
                                sh 'clang++ --version'
                                sh 'nvidia-smi'
                                sh 'pwd'
                                sh 'git --version'
                                sh 'git config --global http.version HTTP/1.1'
                                sh 'git config --global http.postBuffer 524288000'
                                sh 'git config --global http.lowSpeedLimit 0'
                                sh 'git config --global http.lowSpeedTime 999999'
                                sh 'git config --global --add safe.directory "*"'
                                sh 'git config --global --list'
                            }
                        }
                        stage('build') {
                            steps {
                                sh './scripts/clone_submodules.sh -f'
                                sh './scripts/configure.sh ${BUILD_TYPE} -DPRAJNA_WITH_JUPYTER=ON -DPRAJNA_DISABLE_ASSERTS=ON'
                                sh './scripts/build.sh ${BUILD_TYPE} install'
                                // 需要安装llc
                                sh 'cp build_${BUILD_TYPE}/bin/llc /usr/bin'
                            }
                        }
                        stage('test') {
                            steps {
                                sh './scripts/test.sh ${BUILD_TYPE}'
                                sh './scripts/test_examples.sh ${BUILD_TYPE}'
                            }
                        }
                        stage("leak-check") {
                            steps {
                                sh 'valgrind --leak-check=full  --num-callers=10 --trace-children=yes ./scripts/test.sh ${BUILD_TYPE} --gtest_filter=*tensor_test'
                            }
                        }
                    }
                }
            }
        }
    }

    post {
         always {
             script {
                if (env.BRANCH_NAME == 'main' || env.BRANCH_NAME == 'dev'){
                    cleanWs()
               }
            }
        }
    }
}
