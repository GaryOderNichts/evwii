FROM ghcr.io/wiiu-env/devkitppc:20230218

COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:20230215 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libmocha:20220903 /artifacts $DEVKITPRO

RUN \
mkdir wut && \
cd wut && \
git init . && \
git remote add origin https://github.com/devkitPro/wut.git && \
git fetch --depth 1 origin 451a1828f7646053b59ebacd813135e0300c67e8 && \
git checkout FETCH_HEAD
WORKDIR /wut
RUN make -j$(nproc)
RUN make install

WORKDIR /project
