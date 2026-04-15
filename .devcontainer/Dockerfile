FROM debian:11

ARG LLVM_VERSION=16
ARG WORK_SPACE=/root

WORKDIR ${WORK_SPACE}

RUN apt update \
    && apt install -y \
        lsb-release \
        software-properties-common \
        curl wget \
        vim git \
        rsync \
        gnupg \
    # [CMake]
    && apt install -y cmake \
    && apt install -y libssl-dev libcurl4-openssl-dev \
    # [LLVM]
    && wget https://apt.llvm.org/llvm.sh \
    && chmod +x llvm.sh \
    && ./llvm.sh ${LLVM_VERSION} all \
    # [ICU]
    && apt install -y libicu67 libicu-dev 

RUN apt update \
    # [Python3]
    && apt install -y python3 python3-pip \
    # [Node.js]: nvm
    && curl https://raw.githubusercontent.com/creationix/nvm/master/install.sh | bash \
    && . ~/.bashrc \
    && nvm install node --lts \
    # [Node.js]: yarn
    && apt install -y yarn \
    # [LLVM - Lit]
    && pip install lit

RUN apt update \
    # [GitHub Client]
    && curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg | dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg \
    && chmod go+r /usr/share/keyrings/githubcli-archive-keyring.gpg \
    && echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" | tee /etc/apt/sources.list.d/github-cli.list > /dev/null \
    && apt update \
    && apt install gh -y \
    # [Styio]
    && git clone https://github.com/SymPolicy/styio.git \
    # [Extra]
    && git clone https://github.com/SymPolicy/Unka-Malloc-env.git \
    && rsync -a Unka-Malloc-env/docker/extra/.vscode ${WORK_SPACE} \
    && rm -rf Unka-Malloc-env

# [System Environment Variables]
ENV clang=/usr/bin/clang-${LLVM_VERSION}
ENV clang++=/usr/bin/clang++-${LLVM_VERSION}
ENV FileCheck=/usr/bin/FileCheck-${LLVM_VERSION}

# [System Environment Variables]: CMake
ENV CC=/usr/bin/clang-${LLVM_VERSION}
ENV CXX=/usr/bin/clang++-${LLVM_VERSION}

# [Alias]
RUN echo "alias clang=/usr/bin/clang-${LLVM_VERSION}" >>/root/.bashrc
RUN echo "alias clang++=/usr/bin/clang++-${LLVM_VERSION}" >>/root/.bashrc
RUN echo "alias FileCheck=/usr/bin/FileCheck-${LLVM_VERSION}" >>/root/.bashrc
    
WORKDIR ${WORK_SPACE}/styio