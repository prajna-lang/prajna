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
                                bat 'echo %PATH%'
                                bat 'cd'
                                bat 'git --version'
                                bat 'cmake --version'
                                bat 'git config --global --list'
                            }
                        }
                        stage('clean') {
                            when {
                                allOf {
                                    anyOf { branch 'main'; branch 'dev' }
                                    triggeredBy "TimerTrigger"
                                }
                            }
                            steps {
                                bat 'git submodule deinit  --force --all'
                                bat 'git clean -xdf .'
                            }
                        }
                        stage('build') {
                            steps {
                                bat 'bash ./scripts/clone_submodules.sh -f --jobs=4 --depth=50'
                                bat 'call ./scripts/windows_build.bat %BUILD_TYPE%'
                            }
                        }
                        stage('test') {
                            steps {
                                bat 'bash ./scripts/test.sh %BUILD_TYPE% --gtest_filter=-*gpu*'
                                bat 'bash ./scripts/test_examples.sh %BUILD_TYPE%'
                            }
                        }
                    }
                }

                stage('x64-linux-release') {
                    agent {
                        dockerfile {
                            label 'Sunny'
                            filename 'ubuntu_dev_gpu.dockerfile'
                            dir 'dockerfiles'
                            args '-u root:root --device /dev/kfd --device /dev/dri --security-opt seccomp=unconfined --gpus all --network host'
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
                                sh 'whoami'
                                sh 'echo $PATH'
                                sh 'cmake --version'
                                sh 'clang++ --version'
                                sh 'nvidia-smi'
                                sh 'rocm-smi'
                                sh 'pwd'
                                sh 'git --version'
                                // we use -u root:root in docker container, but the repo is owned by jenkins user, so we need to add the repo to the safe.directory
                                sh 'git config --global --add safe.directory "*"'
                            }
                        }
                        stage('clean') {
                                when {
                                    anyOf {
                                        branch 'main'
                                        branch 'dev'
                                    }
                                 }
                            steps {
                                sh 'git submodule deinit  --force --all'
                                sh 'git clean -xdf .'
                            }
                        }
                        stage('build') {
                            steps {
                                sh './scripts/clone_submodules.sh -f --jobs=4 --depth=50'
                                sh './scripts/configure.sh ${BUILD_TYPE} -DPRAJNA_WITH_JUPYTER=ON -DPRAJNA_DISABLE_ASSERT=OFF -DPRAJNA_WITH_CUDA=ON -DPRAJNA_WITH_ROCM=ON'
                                sh './scripts/build.sh ${BUILD_TYPE} install'
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
                                sh 'whoami'
                                sh 'echo $PATH'
                                sh 'uname -a'
                                sh 'cmake --version'
                                sh 'clang++ --version'
                                sh 'pwd'
                                sh 'git --version'
                                sh 'git config --global --list'
                            }
                        }
                        stage('clean') {
                                when {
                                    anyOf {
                                        branch 'main'
                                        branch 'dev'
                                    }
                                }
                            steps {
                                sh 'git submodule deinit  --force --all'
                                sh 'git clean -xdf .'
                            }
                        }
                        stage('large-files-check') {
                            steps {
                                sh 'echo "Checking for large files..."'
                                sh 'chmod +x ./scripts/check_large_files.sh'
                                sh './scripts/check_large_files.sh'
                            }
                        }
                        stage('format') {
                            steps {
                                sh 'clang-format --version'
                                sh 'echo "Running clang-format check..."'
                                sh './scripts/check_clang_format.sh'
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
                    }
                }
            }
        }
    }
}
