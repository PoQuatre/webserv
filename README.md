*This project has been created as part of the 42 curriculum by `uanglade`, `mle-flem`, `nlaporte`.*

# Webserv

---

## Description


**Webserv** is a configurable HTTP server written entirely in **C++98**.
A lightweight HTTP/1.x web server, developed as part of the 42 curriculum. The objective of this project is to understand how web servers work internally by implementing the HTTP protocol, socket programming, event-driven I/O, and CGI execution without relying on external networking libraries.
Rather than using an existing web server such as NGINX or Apache, this project consists of implementing one from scratch while respecting the HTTP protocol and the constraints imposed by the project subject.

The server is designed to:

* Handle multiple simultaneous clients using non-blocking I/O.
* Serve static websites.
* Process HTTP requests.
* Execute CGI scripts.
* Upload and delete files.
* Support multiple server configurations through a configuration file.

The project focuses on low-level networking concepts including:

* TCP/IP sockets
* HTTP request parsing and generation
* Event driven systems
* Configuration parsing
* CGI communication

Mandatory features

* Non-blocking sockets
* Single event loop using `epoll()`
* Multiple listening ports
* Configurable server through configuration files
* Static website hosting
* HTTP methods:
  * GET
  * POST
  * DELETE
* File uploads
* Custom error pages
* Directory listing (autoindex)
* Route configuration
* HTTP redirections
* CGI execution (PHP, Python, etc.)
* Browser compatibility
* Proper HTTP status codes

Bonus features

* Cookie support
* Session management
* Multiple CGI implementations

---

## Instructions

### Compilation

```bash
make
```

Useful Makefile targets:

```bash
make
make clean
make fclean
make re
```

For developpement uses:

```bash
make debug-san
make debug
make test
```

### Running the server

Using a custom configuration:

```bash
./webserv demo.conf
```

For a list of options:

```bash
./webserv -h
```

### Testing

Open a browser and navigate to:

```
http://localhost:8080
```

The server can also be tested with:

```bash
curl http://localhost:8080/
```

---

## Resources

### HTTP

* RFC 7230 – Hypertext Transfer Protocol (HTTP/1.1): Message Syntax and Routing
* RFC 7231 – HTTP Semantics
* Mozilla Developer Network (MDN) HTTP Documentation
* Beej's Guide to Network Programming
* NGINX Documentation
* RFC 3875 – Common Gateway Interface (CGI)

---

## AI Usage

Artificial Intelligence tools were used exclusively as learning and documentation assistants.
No AI-generated code was copied directly into the final implementation without being fully reviewed, understood, tested, and adapted.
