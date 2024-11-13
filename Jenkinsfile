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
            parallel {
                stage('x64-win11-release') {
                    agent {
                        label "Win11"
                    }
                    stages {
                        stage('env') {
                            steps {
                                bat 'git --version'
                                bat 'cmake --version'
                                // docker内存也需要设置代理, 上面设置了使用host网络
                                // gnutls错误, 先这样设置吧
                                bat 'git config --global http.postBuffer 1048576000'
                                bat "git config --global --add safe.directory '*'"
                                bat 'git config http.sslVerify false'
                            }
                        }
                        stage('build') {
                            steps {
                                bat 'bash ./scripts/clone_submodules.sh --jobs=32'
                                bat 'cl --help'
                                bat 'call "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat"'
                                bat 'bash ./scripts/configure.sh ${BUILD_TYPE} -DWITH_TLS=OFF -DPRAJNA_WITH_JUPYTER=ON'
                                bat 'bash ./scripts/build.sh ${BUILD_TYPE} install'
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

                // stage('x64-linux-nvgpu-debug') {
                //     agent {
                //         dockerfile {
                //             filename 'ubuntu_dev_nvgpu.dockerfile'
                //             dir 'dockerfiles'
                //             label 'Sunny'
                //             //需要jenkins也用root权限启动, 目前使用其他权限会有很多问题, root权限存在极大的安全隐患
                //             args '--gpus all -u root:root --network host'
                //         }
                //     }
                //     environment {
                //         CXX = 'clang++'
                //         CC = 'clang'
                //         BUILD_TYPE = 'debug'
                //     }
                //     stages {
                //         stage('env') {
                //             steps {
                //                 sh 'uname -a'
                //                 sh 'echo $USER'
                //                 sh 'cmake --version'
                //                 sh 'clang++ --version'
                //                 sh 'nvidia-smi'
                //                 sh 'pwd'
                //                 sh 'git --version'
                //                 // docker内存也需要设置代理, 上面设置了使用host网络
                //                 sh 'git config --global http.proxy localhost:1087'
                //                 // gnutls错误, 先这样设置吧
                //                 sh 'git config --global http.postBuffer 1048576000'
                //                 sh "git config --global --add safe.directory '*'"
                //                 sh 'git config http.sslVerify false'
                //             }
                //         }
                //         stage('build') {
                //             steps {
                //                 sh 'git config --global --list'
                //                 sh './scripts/clone_submodules.sh -f --jobs=32'
                //                 sh './scripts/configure.sh ${BUILD_TYPE} -DWITH_TLS=OFF -DPRAJNA_WITH_JUPYTER=OFF'
                //                 sh './scripts/build.sh ${BUILD_TYPE} install'
                //                 // 需要安装llc
                //                    // 需要安装llc
                //                 sh 'cp build_${BUILD_TYPE}/bin/llc /usr/bin'
                //             }
                //         }
                //         stage('test') {
                //             steps {
                //                 sh './scripts/test.sh ${BUILD_TYPE}'
                //                 sh './scripts/test_examples.sh ${BUILD_TYPE}'
                //             }
                //         }
                //     }
                // }

                // stage('x64-linux-nvgpu-release') {
                //     agent {
                //         dockerfile {
                //             filename 'ubuntu_dev_nvgpu.dockerfile'
                //             dir 'dockerfiles'
                //             label 'Sunny'
                //             //需要jenkins也用root权限启动, 目前使用其他权限会有很多问题, root权限存在极大的安全隐患
                //             args '--gpus all -u root:root --network host'
                //         }
                //     }
                //     environment {
                //         CXX = 'clang++'
                //         CC = 'clang'
                //         BUILD_TYPE = 'release'
                //     }
                //     stages {
                //         stage('env') {
                //             steps {
                //                 sh 'uname -a'
                //                 sh 'echo $USER'
                //                 sh 'cmake --version'
                //                 sh 'clang++ --version'
                //                 sh 'nvidia-smi'
                //                 sh 'pwd'
                //                 sh 'git --version'
                //                 // docker内存也需要设置代理, 上面设置了使用host网络
                //                 sh 'git config --global http.proxy localhost:1087'
                //                 sh 'git config --global http.postBuffer 500M'
                //                 sh 'git config --global http.maxRequestBuffer 100M'
                //                 sh 'git config --global core.compression 0'
                //                 sh "git config --global --add safe.directory '*'"
                //                 // gnutls错误, 先这样设置吧
                //                 sh 'git config http.sslVerify false'
                //             }
                //         }
                //         stage('build') {
                //             steps {
                //                 sh 'git config --global --list'
                //                 sh './scripts/clone_submodules.sh -f --jobs=32'
                //                 sh './scripts/configure.sh ${BUILD_TYPE} -DPRAJNA_WITH_JUPYTER=ON -DPRAJNA_DISABLE_ASSERTS=ON'
                //                 sh './scripts/build.sh ${BUILD_TYPE} install'
                //                 // 需要安装llc
                //                 sh 'cp build_${BUILD_TYPE}/bin/llc /usr/bin'
                //             }
                //         }
                //         stage('test') {
                //             steps {
                //                 sh './scripts/test.sh ${BUILD_TYPE}'
                //                 sh './scripts/test_examples.sh ${BUILD_TYPE}'
                //             }
                //         }
                //         stage("leak-check") {
                //             steps {
                //                 sh 'valgrind --leak-check=full  --num-callers=10 --trace-children=yes ./scripts/test.sh ${BUILD_TYPE} --gtest_filter=*tensor_test'
                //             }
                //         }
                //     }
                // }
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
