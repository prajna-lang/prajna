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
                // stage('x64-win11-release') {
                //     agent {
                //         label "Win11"
                //     }
                //     environment{
                //         BUILD_TYPE = 'release'
                //     }
                //     stages {
                //         stage('env') {
                //             steps {
                //                 bat 'git --version'
                //                 bat 'cmake --version'
                //                 bat 'git config --global --list'
                //             }
                //         }
                //         stage('build') {
                //             steps {
                //                 bat 'bash ./scripts/clone_submodules.sh -f --jobs=4 --depth=50'
                //                 bat 'call ./scripts/windows_build.bat %BUILD_TYPE%'
                //             }
                //         }
                //         stage('test') {
                //             steps {
                //                 bat 'bash ./scripts/test.sh %BUILD_TYPE%'
                //                 bat 'bash ./scripts/test_examples.sh %BUILD_TYPE%'
                //             }
                //         }
                //     }
                // }

                stage('x64-linux-release') {
                    agent {
                        dockerfile {
                            label 'Sunny'
                            filename 'ubuntu_dev_jenkins.dockerfile'
                            dir 'dockerfiles'
                            // 参数由宿主主机的jenkins账号决定
                            additionalBuildArgs '''\
                            --build-arg GID=125 \
                            --build-arg UID=124 \
                            --build-arg UNAME=jenkins \
                            '''
                            // args '--gpus all --network host'
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
                                // sh 'nvidia-smi'
                                sh 'pwd'
                                sh 'git --version'
                            }
                        }
                        stage('build') {
                            steps {
                                sh './scripts/clone_submodules.sh -f --jobs=4 --depth=50'
                                sh './scripts/configure.sh ${BUILD_TYPE} -DPRAJNA_WITH_JUPYTER=ON -DPRAJNA_DISABLE_ASSERT=OFF -DPRAJNA_WITH_CUDA=ON'
                                sh './scripts/build.sh ${BUILD_TYPE} install'
                            }
                        }
                        stage('test') {
                            steps {
                                sh './scripts/test.sh ${BUILD_TYPE} --gtest_filter=-*gpu*'
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

                stage('aarch64-osx-release') {
                    agent {
                        label 'Mac'
                    }
                    environment {
                        CXX = 'clang++'
                        CC = 'clang'
                        BUILD_TYPE = 'release'
                    }
                    stages {
                        stage('env') {
                            steps {
                                sh 'echo $USER'
                                sh 'uname -a'
                                sh 'cmake --version'
                                sh 'clang++ --version'
                                sh 'pwd'
                                sh 'git --version'
                                sh 'git config --global --list'
                            }
                        }
                        stage('build') {
                            steps {
                                sh './scripts/clone_submodules.sh -f --jobs=4 --depth=50'
                                sh './scripts/configure.sh ${BUILD_TYPE} -DPRAJNA_WITH_JUPYTER=ON -DPRAJNA_DISABLE_ASSERT=OFF'
                                sh './scripts/build.sh ${BUILD_TYPE} install'
                            }
                        }
                        stage('test') {
                            steps {
                                sh './scripts/test.sh ${BUILD_TYPE} --gtest_filter=-*gpu*'
                                sh './scripts/test_examples.sh ${BUILD_TYPE}'
                            }
                        }
                        // stage("leak-check") {
                        //     steps {
                        //         sh 'valgrind --leak-check=full  --num-callers=10 --trace-children=yes ./scripts/test.sh ${BUILD_TYPE} --gtest_filter=*tensor_test'
                        //     }
                        // }
                    }
                }
            }
        }
    }
}
