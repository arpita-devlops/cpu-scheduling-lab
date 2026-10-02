FROM emscripten/emsdk:3.1.74 AS build
WORKDIR /src
COPY CMakeLists.txt ./
COPY include ./include
COPY src ./src
COPY app ./app
COPY tests ./tests
COPY testcases ./testcases
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build -j \
 && ./build/scheduler_tests testcases

FROM ubuntu:22.04
COPY --from=build /src/build/scheduler /usr/local/bin/scheduler
COPY --from=build /src/testcases /testcases
USER nobody
ENTRYPOINT ["scheduler"]
