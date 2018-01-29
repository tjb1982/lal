#FROM gcc:latest
FROM archlinux/base:latest

COPY . /usr/src/lal

WORKDIR /usr/src/lal

RUN pacman -Syyu base-devel libbsd libpqxx --noconfirm
RUN make testpp

CMD ["./bin/lalpp"]
