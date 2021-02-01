// This file is for internal AMD use.
// If you are interested in running your own Jenkins, please raise a github issue for assistance.

def runCompileCommand(platform, project, jobName, boolean debug=false, boolean staticLibrary=false) {
    project.paths.construct_build_prefix()

    String buildTypeArg = debug ? '-DCMAKE_BUILD_TYPE=Debug' : '-DCMAKE_BUILD_TYPE=Release'
    String buildTypeDir = debug ? 'debug' : 'release'

    String osInfo = ''
    String update = ''
    String installPackage = ''
    String cmake = ''

    if (platform.jenkinsLabel.contains('centos')) {
        osInfo = 'cat /etc/os-release && uname -r'
        update = 'sudo yum -y update'
        installPackage = 'python MIVisionX-setup.py --installer yum --ffmpeg yes'
        cmake = 'cmake3'
    }
    else if (platform.jenkinsLabel.contains('sles')) {
        osInfo = 'cat /etc/lsb-release && uname -r'
        update = 'sudo zypper -y update'
        installPackage = 'sudo zypper install -y cmake gcc g++ opencv ffmpeg-4'
        cmake = 'cmake'
    }
    else {
        osInfo = 'cat /etc/lsb-release && uname -r'
        update = 'sudo apt -y update'
        installPackage = 'python MIVisionX-setup.py --ffmpeg yes'
        cmake = 'cmake'
    }

    def command = """#!/usr/bin/env bash
                set -x
                ${osInfo}
                ${update}
                echo Install MIVisionX Prerequisites
                cd ${project.paths.project_build_prefix}
                ${installPackage}
                echo Build MIVisionX - ${buildTypeDir}
                mkdir -p build/${buildTypeDir} && cd build/${buildTypeDir}
                ${cmake} ${buildTypeArg} ../..
                make -j\$(nproc)
                sudo make install
                sudo make package
                """

    platform.runCommand(this, command)
}

def runTestCommand (platform, project) {
    def command = """#!/usr/bin/env bash
                set -x
                ldd -v /opt/rocm/mivisionx/lib/libopenvx.so
                cd ${project.paths.project_build_prefix}/build/release
                python ${project.paths.project_build_prefix}/tests/vision_tests/runVisionTests.py --runvx_directory ./bin --hardware_mode CPU --num_frames 100
                python ${project.paths.project_build_prefix}/tests/vision_tests/runVisionTests.py --runvx_directory ./bin --hardware_mode GPU --num_frames 100
                python ${project.paths.project_build_prefix}/tests/neural_network_tests/runNeuralNetworkTests.py
                """

    platform.runCommand(this, command)
}

def runPackageCommand(platform, project) {
    def packageHelper = platform.makePackage(platform.jenkinsLabel, "${project.paths.project_build_prefix}/build/release")

    String packageType = ""
    String packageInfo = ""

    if (platform.jenkinsLabel.contains('centos')) {
        packageType = 'rpm'
        packageInfo = 'rpm -qlp'
    }
    else {
        packageType = 'deb'
        packageInfo = 'dpkg -c'
    }

    def command = """#!/usr/bin/env bash
                set -x
                export HOME=/home/jenkins
                echo Make RPP Package
                cd ${project.paths.project_build_prefix}/build/release
                sudo make package
                mkdir -p package
                mv *.md package/
                mv *.${packageType} package/
                ${packageInfo} package/*.${packageType}
                """

    platform.runCommand(this, command)
    platform.archiveArtifacts(this, packageHelper[1])
}

return this
