ARG DOCKER_TAG=v5.5.3
FROM espressif/idf:${DOCKER_TAG}

ARG DEBIAN_FRONTEND=nointeractive
ARG CONTAINER_USER=esp
ARG USER_UID=1050
ARG USER_GID=$USER_UID

ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8

RUN apt update -y \
  && apt install -y -q \
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
  qrencode

RUN groupadd --gid $USER_GID $CONTAINER_USER \
  && adduser --uid $USER_UID --gid $USER_GID --disabled-password --gecos "" ${CONTAINER_USER} \
  && usermod -a -G root $CONTAINER_USER && usermod -a -G dialout $CONTAINER_USER

RUN chmod -R 775 /opt/esp/python_env/

USER ${CONTAINER_USER}
ENV USER=${CONTAINER_USER}
WORKDIR /home/${CONTAINER_USER}

RUN echo "source /opt/esp/idf/export.sh > /dev/null 2>&1" >> ~/.bashrc

ENTRYPOINT [ "/opt/esp/entrypoint.sh" ]

CMD ["/bin/bash", "-c"]
