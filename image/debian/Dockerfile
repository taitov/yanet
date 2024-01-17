FROM yanetplatform/builder AS builder

ARG YANET_VERSION_MAJOR=0
ARG YANET_VERSION_MINOR=0
ARG YANET_VERSION_REVISION=0
ARG YANET_VERSION_HASH=00000000
ARG YANET_VERSION_CUSTOM=develop

COPY . /project
RUN meson setup --prefix=/target \
                -Dtarget=release \
                -Dyanet_config=release,firewall,l3balancer,low_memory \
                -Darch=corei7,broadwell,knl \
                -Dversion_major=$YANET_VERSION_MAJOR \
                -Dversion_minor=$YANET_VERSION_MINOR \
                -Dversion_revision=$YANET_VERSION_REVISION \
                -Dversion_hash=$YANET_VERSION_HASH \
                -Dversion_custom=$YANET_VERSION_CUSTOM \
                build_release

RUN meson compile -C build_release
RUN meson install -C build_release

RUN mkdir /target/DEBIAN && \
    cp debian/control.in /target/DEBIAN/control && \
    sed -i "s/__VERSION__/${YANET_VERSION_MAJOR}.${YANET_VERSION_MINOR}/g" /target/DEBIAN/control

RUN mkdir /export
RUN dpkg-deb -b "/target" /export/yanet_${YANET_VERSION_MAJOR}.${YANET_VERSION_MINOR}_ubuntu22.04.deb


FROM ubuntu:22.04

COPY --from=builder /export /target
