FROM ubuntu:jammy

ARG UNAME user
ARG UID=1000
ARG GID=1000

ENV TZ=Europe/Berlin
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# RUN groupadd -g $GID -o $UNAME
# RUN useradd -m -u $UID -g $GID -o -s /usr/bin/zsh $UNAME
# RUN gpasswd -a $UNAME sudo

RUN bash -c "apt -y update"
RUN bash -c "apt -y install python3"

RUN bash -c "apt -y install cmake"
RUN bash -c "apt -y install git"
RUN bash -c "apt -y install build-essential"
RUN bash -c "apt -y install libboost-all-dev"
RUN bash -c "apt -y install gdb"
RUN bash -c "apt -y install curl"
RUN bash -c "apt -y install openjdk-17-jre-headless"

RUN bash -c "apt-get -qq update && apt-get -qq install linux-tools-generic"
RUN bash -c "rm /usr/bin/perf"
RUN bash -c "ln -s /usr/lib/linux-tools/6.8.0-60-generic/perf /usr/bin/perf"

USER $UNAME

