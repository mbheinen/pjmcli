FROM alpine:3.18.2
WORKDIR /opt/pjm-mg-util
COPY . .
RUN apk update && apk add --no-cache build-base cmake curl-dev expat-dev
WORKDIR /opt/pjm-mg-util/build
RUN cmake ..
RUN make

CMD ["./pjm-mg-util"]
#CMD ["bash"]
