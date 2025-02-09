FROM archlinux:latest as build

RUN pacman -Syu --noconfirm && \
    pacman -S --noconfirm base-devel git qt6-base qt6-mqtt cmake\
     extra-cmake-modules kcoreaddons kdbusaddons kidletime kglobalaccel knotifications

COPY . /src

RUN mkdir -p /src/build
WORKDIR /src/build

RUN cmake ..
RUN make
RUN chmod +x /src/build/bin/kiot

FROM archlinux:latest as runtime
RUN pacman -Syu --noconfirm && \
    pacman -S --noconfirm qt6-base qt6-mqtt kcoreaddons kdbusaddons kidletime kglobalaccel knotifications
COPY --from=build /src/build/bin/kiot /src/build/bin/kiot
ENV QT_QPA_PLATFORM=offscreen
ENV XDG_CONFIG_HOME=/etc/kiot/
ENTRYPOINT ["/src/build/bin/kiot"]
