FROM alpine:3.18.2 AS builder
WORKDIR /opt/pjmcli
COPY . .
RUN apk update && apk add --no-cache build-base cmake curl-dev expat-dev
WORKDIR /opt/pjmcli/build
RUN cmake ..
RUN make

FROM alpine:3.18.2
RUN apk update && apk add --no-cache curl expat
WORKDIR /opt/pjmcli
COPY --from=builder /opt/pjmcli/build ./
CMD ["./pjmcli"]
