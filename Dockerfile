#FROM gcc:latest
FROM archlinux/base:latest

COPY . /usr/src/lal

WORKDIR /usr/src/lal

RUN pacman -Syyu base-devel libbsd --noconfirm
RUN make test

CMD ["./bin/lal"]
