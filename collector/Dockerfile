FROM gcc:14-bookworm

WORKDIR /app
COPY main.cpp .
RUN g++ -std=c++20 -Wall -Wextra -Wpedantic -O2 main.cpp -o collector

RUN mkdir -p /data
EXPOSE 8080
CMD ["./collector"]
