FROM debian:13

ARG LLVM_MAJOR=18
ARG CMAKE_VERSION=3.31.6
ARG PYTHON_VERSION=3.13.5
ARG LIT_VERSION=18.1.8
ARG NODE_VERSION=24.15.0

WORKDIR /workspaces/styio-nightly

RUN apt-get update \
    && env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        clang-${LLVM_MAJOR} \
        curl \
        git \
        gnupg \
        libcurl4-openssl-dev \
        libedit-dev \
        libffi-dev \
        libicu-dev \
        libssl-dev \
        libxml2-dev \
        libzstd-dev \
        lld-${LLVM_MAJOR} \
        llvm-${LLVM_MAJOR}-dev \
        llvm-${LLVM_MAJOR}-tools \
        ninja-build \
        pkg-config \
        python3 \
        python3-pip \
        python3-venv \
        rsync \
        unzip \
        wget \
        xz-utils \
        zip \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m pip install --break-system-packages --upgrade pip \
    && python3 -m pip install --break-system-packages \
        "cmake==${CMAKE_VERSION}" \
        "lit==${LIT_VERSION}"

RUN set -eux; \
    arch="$(dpkg --print-architecture)"; \
    case "${arch}" in \
      amd64) node_arch="x64" ;; \
      arm64) node_arch="arm64" ;; \
      *) echo "unsupported architecture: ${arch}" >&2; exit 1 ;; \
    esac; \
    archive="node-v${NODE_VERSION}-linux-${node_arch}.tar.xz"; \
    url="https://nodejs.org/dist/v${NODE_VERSION}/${archive}"; \
    mkdir -p /usr/local/lib/nodejs; \
    wget -qO "/tmp/${archive}" "${url}"; \
    tar -xJf "/tmp/${archive}" -C /usr/local/lib/nodejs; \
    ln -sf "/usr/local/lib/nodejs/node-v${NODE_VERSION}-linux-${node_arch}/bin/node" /usr/local/bin/node; \
    ln -sf "/usr/local/lib/nodejs/node-v${NODE_VERSION}-linux-${node_arch}/bin/npm" /usr/local/bin/npm; \
    ln -sf "/usr/local/lib/nodejs/node-v${NODE_VERSION}-linux-${node_arch}/bin/npx" /usr/local/bin/npx; \
    ln -sf "/usr/local/lib/nodejs/node-v${NODE_VERSION}-linux-${node_arch}/bin/corepack" /usr/local/bin/corepack; \
    rm -f "/tmp/${archive}"

RUN curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg \
      | dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg \
    && chmod go+r /usr/share/keyrings/githubcli-archive-keyring.gpg \
    && echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" \
      > /etc/apt/sources.list.d/github-cli.list \
    && apt-get update \
    && env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends gh \
    && rm -rf /var/lib/apt/lists/*

ENV CC=/usr/bin/clang-18
ENV CXX=/usr/bin/clang++-18
ENV LLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm
ENV STYIO_TOOLCHAIN_DEBIAN_STANDARD_VERSION=13
ENV STYIO_TOOLCHAIN_CMAKE_STANDARD_VERSION=${CMAKE_VERSION}
ENV STYIO_TOOLCHAIN_PYTHON_STANDARD_VERSION=${PYTHON_VERSION}
ENV STYIO_TOOLCHAIN_NODE_STANDARD_VERSION=${NODE_VERSION}
