ARG DOCKER_TAG=v6.0.1
FROM espressif/idf:${DOCKER_TAG}

ARG DEBIAN_FRONTEND=nointeractive
ARG CONTAINER_USER=esp
ARG USER_UID=1050
ARG USER_GID=1050

ARG INSTALL_OPENCODE=false
ARG OPENCODE_VERSION=2.0.22

ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8

RUN apt-get update \
  && apt-get install -y --no-install-recommends \
  udev \
  cmake \
  git \
  clang-tidy \
  clang-format \
  # llvm \
  # uncrustify \
  cppcheck \
  # iwyu \
  # cpplint \
  qrencode \
  python3-venv \
  && rm -rf /var/lib/apt/lists/*

# Remove any existing user with the target UID/GID (e.g., ubuntu:1000)
# Also explicitly remove 'ubuntu' user (UID 1000) from base image since updateRemoteUserUID will try to use 1000
RUN if getent passwd $USER_UID > /dev/null; then \
        EXISTING_USER=$(getent passwd $USER_UID | cut -d: -f1); \
        echo "Removing existing user $EXISTING_USER with UID $USER_UID"; \
        deluser --remove-home $EXISTING_USER || true; \
    fi \
    && if getent group $USER_GID > /dev/null; then \
        EXISTING_GROUP=$(getent group $USER_GID | cut -d: -f1); \
        echo "Removing existing group $EXISTING_GROUP with GID $USER_GID"; \
        delgroup $EXISTING_GROUP || true; \
    fi \
    && if getent passwd 1000 > /dev/null; then \
        EXISTING_USER=$(getent passwd 1000 | cut -d: -f1); \
        echo "Removing existing user $EXISTING_USER with UID 1000"; \
        deluser --remove-home $EXISTING_USER || true; \
    fi \
    && if getent group 1000 > /dev/null; then \
        EXISTING_GROUP=$(getent group 1000 | cut -d: -f1); \
        echo "Removing existing group $EXISTING_GROUP with GID 1000"; \
        delgroup $EXISTING_GROUP || true; \
    fi \
    && groupadd --gid $USER_GID $CONTAINER_USER \
    && adduser \
        --uid $USER_UID \
        --gid $USER_GID \
        --disabled-password \
        --gecos "" \
        $CONTAINER_USER \
    && usermod -a -G dialout $CONTAINER_USER


# RUN chmod -R 775 /opt/esp/python_env/

RUN python3 -m venv /opt/dev-venv \
    && /opt/dev-venv/bin/pip install --no-cache-dir \
        pre-commit \
    && chown -R ${CONTAINER_USER}:${CONTAINER_USER} /opt/dev-venv

# Ensure .local directory exists with correct ownership
RUN mkdir -p /home/${CONTAINER_USER}/.local/share /home/${CONTAINER_USER}/.local/state /home/${CONTAINER_USER}/.local/bin \
    && chown -R ${CONTAINER_USER}:${CONTAINER_USER} /home/${CONTAINER_USER}/.local

ENV PATH="/opt/dev-venv/bin:/home/${CONTAINER_USER}/.local/bin:${PATH}"


USER ${CONTAINER_USER}
ENV USER=${CONTAINER_USER}
ENV PATH="/home/${CONTAINER_USER}/.local/bin:/home/${CONTAINER_USER}/.opencode/bin:${PATH}"
WORKDIR /home/${CONTAINER_USER}


RUN echo "source /opt/esp/idf/export.sh > /dev/null 2>&1" >> ~/.bashrc

RUN if [ "${INSTALL_OPENCODE}" = "true" ]; then \
        curl -fsSL https://opencode.ai/v2/install | OPENCODE_VERSION="${OPENCODE_VERSION}" bash -s -- --no-modify-path; \
    else \
        echo "OpenCode installation disabled"; \
    fi

ENTRYPOINT [ "/opt/esp/entrypoint.sh" ]

CMD ["/bin/bash", "-c"]
