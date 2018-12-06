#FROM gcc:latest
FROM archlinux/base:latest

COPY . /usr/src/lal

WORKDIR /usr/src/lal

RUN pacman -Syu base-devel iputils libbsd libpqxx valgrind --noconfirm
RUN make testpp

CMD ["./bin/lalpp"]
