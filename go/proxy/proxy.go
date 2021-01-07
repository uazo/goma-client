// Copyright 2020 Google LLC. All Rights Reserved.

// proxy.go is an HTTP/1.1 to HTTP/2.0 proxy to reduce the number of
// connections made by compiler_proxy.
// Note that Go http library automatically use HTTP/2.0 if the protocol is
// supported by a server.
package main

import (
	"flag"
	"fmt"
	"log"
	"net/http"
	"net/http/httputil"
	"os"
	"os/signal"
)

var (
	serverHost = flag.String("server-host", "goma.chromium.org", "server host to connect to")
	listenPort = flag.Int("port", 19080, "port to listen connections from compiler_proxy")
)

func main() {
	signal.Ignore(os.Interrupt)
	log.SetOutput(os.Stderr)
	flag.Parse()
	listenHostPort := fmt.Sprintf("127.0.0.1:%d", *listenPort)
	proxy := &httputil.ReverseProxy{
		Director: func(req *http.Request) {
			req.URL.Scheme = "https"
			req.URL.Host = *serverHost
			req.Host = *serverHost
		},
	}
	if err := http.ListenAndServe(listenHostPort, proxy); err != nil {
		log.Fatal(err)
	}
}
