# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
#
# The environment the CI builds and emulates in (.github/workflows/build.yml): everything
# a job used to install, so that a job installs nothing. On 2026-09-24 the apt mirrors of
# the runners slowed down so far that installing the ARM toolchain took 13 and 19 minutes
# in two jobs, and more than ten again in one after a first fix, which retried; sixteen
# jobs then each installed it on every run. The image is built by
# .github/workflows/ci-image.yml and pulled from ghcr.io, which the mirrors do not serve.
#
# Every version is pinned here; changing one is a commit, a new tag of the image in both
# workflows, and a run of ci-image.yml by hand. The toolchain is Ubuntu's package, as the
# runners had it: 15:14.2.rel1-1 when the first image was built, on 2026-09-24.

FROM ubuntu:26.04

ARG RENODE_PORTABLE=1.17.0
ARG RENODE_RP2040=1.16.1
ARG RP2040_MODELS_COMMIT=5aca847c9f57ed96603e55d28e8ceeefa09e55f6

ENV DEBIAN_FRONTEND=noninteractive \
    DOTNET_ROOT=/usr/share/dotnet \
    DOTNET_CLI_TELEMETRY_OPTOUT=1 \
    DOTNET_NOLOGO=1 \
    PATH=/opt/venv/bin:/usr/share/dotnet:$PATH

# The toolchain as the runners had it (Ubuntu's gcc-arm-none-eabi, 14.2.rel1), the tools of
# the jobs, and what .NET needs.
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
        gcc-arm-none-eabi make git curl ca-certificates xz-utils \
        python3 python3-venv libicu-dev \
 && rm -rf /var/lib/apt/lists/*

# .NET 10: the linux-dotnet package of Renode runs on it, and the RP2040 models build
# against it.
RUN curl -fsSL https://dot.net/v1/dotnet-install.sh -o /tmp/dotnet-install.sh \
 && bash /tmp/dotnet-install.sh --channel 10.0 --install-dir /usr/share/dotnet \
 && rm /tmp/dotnet-install.sh

# Renode twice: the portable package for the STM32F4 and RP2350 suites, the linux-dotnet
# one for the RP2040 models, which are built against its assemblies (emulation/renode/RP2040.md).
RUN for v in "$RENODE_PORTABLE linux-portable" "$RENODE_RP2040 linux-dotnet"; do \
        set -- $v; \
        mkdir -p /opt/renode-$1; \
        curl -fsSL "https://github.com/renode/renode/releases/download/v$1/renode-$1.$2.tar.gz" \
            | tar -xz -C /opt/renode-$1 --strip-components=1; \
    done

# The RP2040 models, built once; a job links them where the suite looks for them.
RUN git clone --quiet https://github.com/matgla/Renode_RP2040.git /opt/rp2040 \
 && git -C /opt/rp2040 checkout --quiet "$RP2040_MODELS_COMMIT" \
 && dotnet build /opt/rp2040/emulation/Peripherals.csproj -c Release \
        -p:RenodePath=/opt/renode-$RENODE_RP2040/bin

# What renode-test needs, apart from the system's Python.
RUN python3 -m venv /opt/venv \
 && pip install --quiet robotframework==6.1 robotframework-retryfailed psutil pyyaml

LABEL org.opencontainers.image.source=https://github.com/beber007/escapement
