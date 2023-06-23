FROM alpine:3.18.2
WORKDIR /opt/pjmcli
COPY . .
RUN apk update && apk add --no-cache build-base cmake curl-dev expat-dev
WORKDIR /opt/pjmcli/build
RUN cmake ..
RUN make

CMD ["./pjmcli"]
#CMD ["bash"]
